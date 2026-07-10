// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxFileScan.h"

#include "UetkxChars.h"
#include "UetkxLexer.h"

using namespace UetkxChars;

namespace
{
	void AddDiag(TArray<FUetkxDiag>& Diags, const TCHAR* Code, int32 Severity, const FString& Msg, int32 Offset,
				 int32 Length = 1)
	{
		FUetkxDiag D;
		D.Code = Code;
		D.Severity = Severity;
		D.Message = Msg;
		D.Offset = Offset;
		D.Length = Length;
		Diags.Add(MoveTemp(D));
	}

	int32 SkipWsOnly(const TArray<int32>& S, int32 i)
	{
		while (i < S.Num() && (S[i] == C_SPACE || S[i] == C_TAB || S[i] == C_NL || S[i] == C_CR))
		{
			++i;
		}
		return i;
	}

	/** Split the family param list `Name: Type = Default, ...` (respecting <>/()/{} nesting
	 *  and default-value commas via a small depth walk). */
	TArray<FUetkxParam> ParseParams(const FString& ParamText)
	{
		TArray<FUetkxParam> Out;
		TArray<FString> Pieces;
		int32 Depth = 0;
		FString Cur;
		for (int32 i = 0; i < ParamText.Len(); ++i)
		{
			const TCHAR C = ParamText[i];
			if (C == '(' || C == '[' || C == '{' || C == '<')
			{
				++Depth;
			}
			else if (C == ')' || C == ']' || C == '}' || C == '>')
			{
				--Depth;
			}
			if (C == ',' && Depth == 0)
			{
				Pieces.Add(Cur);
				Cur.Empty();
				continue;
			}
			Cur.AppendChar(C);
		}
		if (!Cur.TrimStartAndEnd().IsEmpty())
		{
			Pieces.Add(Cur);
		}
		for (const FString& Piece : Pieces)
		{
			FUetkxParam P;
			FString Rest = Piece;
			FString DefaultPart;
			int32 EqIdx = INDEX_NONE;
			// first top-level '=' (defaults may hold nested commas but not top-level '=')
			int32 D2 = 0;
			for (int32 i = 0; i < Rest.Len(); ++i)
			{
				const TCHAR C = Rest[i];
				if (C == '(' || C == '[' || C == '{' || C == '<')
				{
					++D2;
				}
				else if (C == ')' || C == ']' || C == '}' || C == '>')
				{
					--D2;
				}
				else if (C == '=' && D2 == 0 && (i + 1 >= Rest.Len() || Rest[i + 1] != '='))
				{
					EqIdx = i;
					break;
				}
			}
			if (EqIdx != INDEX_NONE)
			{
				DefaultPart = Rest.Mid(EqIdx + 1).TrimStartAndEnd();
				Rest = Rest.Left(EqIdx);
			}
			FString Name, Type;
			if (!Rest.Split(TEXT(":"), &Name, &Type))
			{
				Name = Rest; // untyped param — emitter diagnoses
			}
			P.Name = Name.TrimStartAndEnd();
			P.Type = Type.TrimStartAndEnd();
			P.Default = DefaultPart;
			if (!P.Name.IsEmpty())
			{
				Out.Add(MoveTemp(P));
			}
		}
		return Out;
	}

	/** The LAST top-level `return (` whose parens hold markup (T1.4). Returns m_start/m_end
	 *  bounding the inside of the parens, and the return keyword's offset. */
	struct FSplitReturn
	{
		bool bOk = false;
		int32 ReturnAt = -1; // offset of `return`
		int32 MStart = -1;	 // first char inside `(`
		int32 MEnd = -1;	 // index OF the closing `)`
		int32 AfterParen = -1;
	};

	FSplitReturn SplitReturn(const TArray<int32>& Body)
	{
		FSplitReturn Out;
		const int32 N = Body.Num();
		int32 Depth = 0;
		int32 i = 0;
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Body, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			const int32 C = Body[i];
			if (C == C_LBRACE || C == C_LPAREN || C == C_LBRACKET)
			{
				// a top-level markup return's window is skipped WHOLE below; every other
				// bracket tracks depth so nested returns (lambdas) don't count as top-level
				++Depth;
				++i;
				continue;
			}
			if (C == C_RBRACE || C == C_RPAREN || C == C_RBRACKET)
			{
				--Depth;
				++i;
				continue;
			}
			if (Depth == 0 && C == 'r' && FUetkxLexer::KeywordAt(Body, i, TEXT("return")))
			{
				const int32 P = SkipWsOnly(Body, i + 6);
				if (P < N && Body[P] == C_LPAREN)
				{
					// markup window? peek first real char inside
					int32 First = SkipWsOnly(Body, P + 1);
					// skip leading markup comments
					while (First < N)
					{
						const int32 Skipped = FUetkxLexer::SkipNoncodeMarkup(Body, First);
						if (Skipped == First)
						{
							break;
						}
						First = SkipWsOnly(Body, Skipped);
					}
					const bool bMarkup =
						First < N && (Body[First] == C_LT || Body[First] == C_AT || Body[First] == C_LBRACE);
					const int32 Close =
						bMarkup ? FUetkxLexer::FindMatchingMarkup(Body, P) : FUetkxLexer::FindMatching(Body, P);
					if (Close == -1)
					{
						++i;
						continue;
					}
					if (bMarkup)
					{
						Out.bOk = true; // keep scanning: the LAST one wins
						Out.ReturnAt = i;
						Out.MStart = P + 1;
						Out.MEnd = Close;
						Out.AfterParen = Close + 1;
					}
					i = Close + 1;
					continue;
				}
				i += 6;
				continue;
			}
			++i;
		}
		return Out;
	}

	/** Collect the ordered hook-call kinds in setup (word-boundary, not after . or ::). */
	TArray<FString> ScanHookCalls(const TArray<int32>& Setup)
	{
		TArray<FString> Out;
		const TArray<FString>& Hooks = FUetkxFileScan::HookNames();
		int32 i = 0;
		const int32 N = Setup.Num();
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Setup, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			if (Setup[i] == 'U' || Setup[i] == 'P')
			{
				for (const FString& Hook : Hooks)
				{
					if (FUetkxLexer::KeywordAt(Setup, i, *Hook))
					{
						// must be a CALL: next non-ws is `(` or `<` (template arg)
						int32 K = SkipWsOnly(Setup, i + Hook.Len());
						if (K < N && (Setup[K] == C_LPAREN || Setup[K] == C_LT))
						{
							Out.Add(Hook);
							i += Hook.Len();
							break;
						}
					}
				}
			}
			++i;
		}
		return Out;
	}
} // namespace

const TArray<FString>& FUetkxFileScan::HookNames()
{
	static const TArray<FString> Names = {TEXT("UseState"),
										  TEXT("UseReducer"),
										  TEXT("UseRef"),
										  TEXT("UseMemo"),
										  TEXT("UseCallback"),
										  TEXT("UseEffect"),
										  TEXT("UseLayoutEffect"),
										  TEXT("UseContext"),
										  TEXT("ProvideContext"),
										  TEXT("UseDeferredValue"),
										  TEXT("UseTransition"),
										  TEXT("UseStableCallback"),
										  TEXT("UseImperativeHandle"),
										  TEXT("UseSafeArea"),
										  TEXT("UseSignal"),
										  TEXT("UseSignalKey"),
										  TEXT("UseTween"),
										  TEXT("UseAnimate"),
										  TEXT("UseTweenValue"),
										  TEXT("UseSfx")};
	return Names;
}

uint32 FUetkxFileScan::HookSignature(const TArray<FString>& HookCalls)
{
	uint32 Hash = 2166136261u;
	for (const FString& Call : HookCalls)
	{
		for (int32 i = 0; i < Call.Len(); ++i)
		{
			Hash = (Hash ^ static_cast<uint32>(Call[i])) * 16777619u;
		}
		Hash = (Hash ^ static_cast<uint32>(';')) * 16777619u;
	}
	return Hash;
}

FUetkxFileScanResult FUetkxFileScan::Scan(const FString& Source, const FString& Basename)
{
	FUetkxFileScanResult Out;
	const TArray<int32> Src = FUetkxLexer::ToCodePoints(Source);
	const int32 N = Src.Num();

	// Preamble: verbatim #include lines anywhere before the first declaration.
	int32 i = 0;
	while (i < N)
	{
		const int32 j = FUetkxLexer::SkipNoncode(Src, i);
		if (j != i)
		{
			// capture #include lines out of the skipped span
			if (Src[i] == C_HASH)
			{
				const FString Line = FUetkxLexer::FromCodePoints(Src, i, j - i).TrimStartAndEnd();
				if (Line.StartsWith(TEXT("#include")))
				{
					Out.PreambleIncludes.Add(Line);
				}
			}
			i = j;
			continue;
		}
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("component")))
		{
			break;
		}
		++i;
	}

	// component declarations
	bool bAny = false;
	while (i < N)
	{
		const int32 j = FUetkxLexer::SkipNoncode(Src, i);
		if (j != i)
		{
			i = j;
			continue;
		}
		if (!FUetkxLexer::KeywordAt(Src, i, TEXT("component")))
		{
			++i;
			continue;
		}
		bAny = true;
		const int32 Ci = i;
		int32 k = SkipWsOnly(Src, Ci + 9);
		const int32 Ns = k;
		while (k < N && FUetkxLexer::IsIdentCode(Src[k]))
		{
			++k;
		}
		FUetkxComponentDecl Decl;
		Decl.Name = FUetkxLexer::FromCodePoints(Src, Ns, k - Ns);
		Decl.NameAt = Ns;
		if (Decl.Name.IsEmpty())
		{
			AddDiag(Out.Diags, TEXT("UETKX0300"), 0, TEXT("missing component name"), k);
			return Out;
		}
		if (!(Decl.Name[0] >= 'A' && Decl.Name[0] <= 'Z'))
		{
			AddDiag(Out.Diags, TEXT("UETKX2100"), 0,
					FString::Printf(TEXT("component name `%s` must be PascalCase"), *Decl.Name), Ns, Decl.Name.Len());
		}
		k = SkipWsOnly(Src, k);
		if (k < N && Src[k] == C_LPAREN)
		{
			const int32 Pc = FUetkxLexer::FindMatching(Src, k);
			if (Pc == -1)
			{
				AddDiag(Out.Diags, TEXT("UETKX0304"), 0, TEXT("unclosed `(` in component params"), k);
				return Out;
			}
			Decl.Params = ParseParams(FUetkxLexer::FromCodePoints(Src, k + 1, Pc - k - 1));
			k = Pc + 1;
		}
		k = SkipWsOnly(Src, k);
		if (k >= N || Src[k] != C_LBRACE)
		{
			AddDiag(Out.Diags, TEXT("UETKX0303"), 0, TEXT("component body `{ ... }` expected"), FMath::Min(k, N - 1));
			return Out;
		}
		const int32 Bclose = FUetkxLexer::FindMatchingMarkup(Src, k);
		if (Bclose == -1)
		{
			AddDiag(Out.Diags, TEXT("UETKX0304"), 0, TEXT("unclosed component body"), k);
			return Out;
		}
		const int32 BodyAt = k + 1;
		Decl.Body = FUetkxLexer::FromCodePoints(Src, BodyAt, Bclose - BodyAt);
		Decl.BodyAt = BodyAt;
		Decl.Next = Bclose + 1;

		const TArray<int32> Body = FUetkxLexer::ToCodePoints(Decl.Body);
		const FSplitReturn Split = SplitReturn(Body);
		if (!Split.bOk)
		{
			AddDiag(Out.Diags, TEXT("UETKX2101"), 0, TEXT("component has no `return ( ... )` markup return"), Ci, 9);
			return Out;
		}
		Decl.Setup = FUetkxLexer::FromCodePoints(Body, 0, Split.ReturnAt);
		Decl.SetupAt = BodyAt;
		Decl.HookCalls = ScanHookCalls(FUetkxLexer::ToCodePoints(Decl.Setup));

		FUetkxMarkup Parser;
		FUetkxParseResult Pr = Parser.Parse(Body, Split.MStart, Split.MEnd);
		if (!Pr.IsOk())
		{
			AddDiag(Out.Diags, *Pr.ErrorCode, 0, Pr.ErrorMsg, BodyAt + FMath::Max(0, Pr.ErrorAt));
			return Out;
		}
		Decl.WindowNodes = Pr.Nodes;
		TArray<TSharedPtr<FUetkxNode>> RenderRoots;
		for (const TSharedPtr<FUetkxNode>& Node : Pr.Nodes)
		{
			if (Node.IsValid() && Node->Type != EUetkxNodeType::Comment)
			{
				RenderRoots.Add(Node);
			}
		}
		if (RenderRoots.Num() != 1)
		{
			AddDiag(
				Out.Diags, TEXT("UETKX0108"), 0,
				FString::Printf(TEXT("a component must return exactly one root element (got %d)"), RenderRoots.Num()),
				BodyAt + Split.MStart);
			return Out;
		}
		Decl.Root = RenderRoots[0];

		if (Decl.Name != Basename && !Basename.IsEmpty())
		{
			AddDiag(Out.Diags, TEXT("UETKX0103"), 1,
					FString::Printf(TEXT("component `%s` differs from file name `%s`"), *Decl.Name, *Basename), Ns,
					Decl.Name.Len());
		}
		Out.Components.Add(MoveTemp(Decl));
		i = Bclose + 1;
	}

	if (!bAny)
	{
		AddDiag(Out.Diags, TEXT("UETKX2101"), 0, TEXT("no `component` declaration found"), -1);
	}
	return Out;
}
