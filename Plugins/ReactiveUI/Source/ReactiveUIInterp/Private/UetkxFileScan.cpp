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

	/** Advance past the rest of the current line (error resync for a malformed preamble import). */
	int32 AdvancePastLine(const TArray<int32>& Src, int32 i)
	{
		const int32 N = Src.Num();
		while (i < N && Src[i] != C_NL)
		{
			++i;
		}
		return i < N ? i + 1 : i;
	}

	/** Parse a preamble `import { A, B } from "spec"` at `Start` (the `import` keyword). Records the
	 *  FUetkxImportDecl + duplicate-import diagnostics (UETKX2303) into Out; ImportedFrom tracks
	 *  name -> first specifier across the whole file. Returns the index just past the import (or the
	 *  resync point on a malformed import). Named exports only — no `*`, no default (A1). */
	int32 ParseImport(const TArray<int32>& Src, int32 Start, FUetkxFileScanResult& Out,
					  TMap<FString, FString>& ImportedFrom)
	{
		const int32 N = Src.Num();
		FUetkxImportDecl Imp;
		Imp.At = Start;
		int32 k = SkipWsOnly(Src, Start + 6); // past "import"
		if (k >= N || Src[k] != C_LBRACE)
		{
			AddDiag(Out.Diags, TEXT("UETKX0303"), 0, TEXT("import expects `{ Name, ... }` after `import`"),
					FMath::Min(k, N - 1));
			return AdvancePastLine(Src, k);
		}
		const int32 Bclose = FUetkxLexer::FindMatching(Src, k);
		if (Bclose == -1)
		{
			AddDiag(Out.Diags, TEXT("UETKX0304"), 0, TEXT("unclosed `{` in import list"), k);
			return N;
		}
		for (int32 p = k + 1; p < Bclose;)
		{
			p = SkipWsOnly(Src, p);
			if (p >= Bclose)
			{
				break;
			}
			if (Src[p] == C_COMMA)
			{
				++p;
				continue;
			}
			const int32 s = p;
			while (p < Bclose && FUetkxLexer::IsIdentCode(Src[p]))
			{
				++p;
			}
			if (p == s)
			{
				AddDiag(Out.Diags, TEXT("UETKX0300"), 0,
						TEXT("import list expects bare names — named exports only (no `*`, no default)"), s);
				return AdvancePastLine(Src, Bclose);
			}
			Imp.Names.Add(FUetkxLexer::FromCodePoints(Src, s, p - s));
			Imp.NameAts.Add(s);
		}
		if (Imp.Names.IsEmpty())
		{
			AddDiag(Out.Diags, TEXT("UETKX0303"), 0,
					TEXT("import must name at least one binding: `import { Name } from \"...\"`"), k);
		}
		int32 f = SkipWsOnly(Src, Bclose + 1);
		if (!FUetkxLexer::KeywordAt(Src, f, TEXT("from")))
		{
			AddDiag(Out.Diags, TEXT("UETKX0303"), 0, TEXT("import expects `from \"specifier\"`"), FMath::Min(f, N - 1));
			return AdvancePastLine(Src, f);
		}
		f = SkipWsOnly(Src, f + 4);
		if (f >= N || (Src[f] != C_QUOTE && Src[f] != C_APOS))
		{
			AddDiag(Out.Diags, TEXT("UETKX0303"), 0, TEXT("import specifier must be a quoted path, e.g. `\"./Foo\"`"),
					FMath::Min(f, N - 1));
			return AdvancePastLine(Src, f);
		}
		const int32 Quote = Src[f];
		Imp.SpecifierAt = f;
		int32 q = f + 1;
		while (q < N && Src[q] != Quote && Src[q] != C_NL)
		{
			++q;
		}
		if (q >= N || Src[q] != Quote)
		{
			AddDiag(Out.Diags, TEXT("UETKX0300"), 0, TEXT("unterminated import specifier string"), f);
			return AdvancePastLine(Src, f);
		}
		Imp.Specifier = FUetkxLexer::FromCodePoints(Src, f + 1, q - (f + 1));
		const int32 End = q + 1;

		// Duplicate-import diagnostics (2303): a name already imported earlier in this file, or
		// repeated within this same import's braces.
		TSet<FString> ThisImport;
		for (int32 idx = 0; idx < Imp.Names.Num(); ++idx)
		{
			const FString& Name = Imp.Names[idx];
			const int32 NameAt = Imp.NameAts[idx];
			if (const FString* Prev = ImportedFrom.Find(Name))
			{
				AddDiag(Out.Diags, TEXT("UETKX2303"), 0,
						FString::Printf(TEXT("duplicate import of `%s` (already imported from %s)"), *Name, **Prev),
						NameAt, Name.Len());
			}
			else if (ThisImport.Contains(Name))
			{
				AddDiag(Out.Diags, TEXT("UETKX2303"), 0,
						FString::Printf(TEXT("duplicate import of `%s` (already imported from %s)"), *Name,
										*Imp.Specifier),
						NameAt, Name.Len());
			}
			else
			{
				ThisImport.Add(Name);
			}
		}
		for (const FString& Name : ThisImport)
		{
			ImportedFrom.Add(Name, Imp.Specifier);
		}
		Out.Imports.Add(MoveTemp(Imp));
		return End;
	}

	/** Parse a `component` decl at `Ci` (the `component` keyword; a preceding `export` is passed as
	 *  bExported). Pushes to Out.Components + Out.Order on success and returns the index past the
	 *  closing brace, or -1 on a fatal structural error (diag already recorded — stop the scan). */
	int32 ParseComponent(const TArray<int32>& Src, int32 Ci, bool bExported, FUetkxFileScanResult& Out)
	{
		const int32 N = Src.Num();
		int32 k = SkipWsOnly(Src, Ci + 9);
		const int32 Ns = k;
		while (k < N && FUetkxLexer::IsIdentCode(Src[k]))
		{
			++k;
		}
		FUetkxComponentDecl Decl;
		Decl.bExported = bExported;
		Decl.At = Ci;
		Decl.Name = FUetkxLexer::FromCodePoints(Src, Ns, k - Ns);
		Decl.NameAt = Ns;
		if (Decl.Name.IsEmpty())
		{
			AddDiag(Out.Diags, TEXT("UETKX0300"), 0, TEXT("missing component name"), k);
			return -1;
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
				return -1;
			}
			Decl.Params = ParseParams(FUetkxLexer::FromCodePoints(Src, k + 1, Pc - k - 1));
			k = Pc + 1;
		}
		k = SkipWsOnly(Src, k);
		if (k >= N || Src[k] != C_LBRACE)
		{
			AddDiag(Out.Diags, TEXT("UETKX0303"), 0, TEXT("component body `{ ... }` expected"), FMath::Min(k, N - 1));
			return -1;
		}
		const int32 Bclose = FUetkxLexer::FindMatchingMarkup(Src, k);
		if (Bclose == -1)
		{
			AddDiag(Out.Diags, TEXT("UETKX0304"), 0, TEXT("unclosed component body"), k);
			return -1;
		}
		const int32 BodyAt = k + 1;
		Decl.Body = FUetkxLexer::FromCodePoints(Src, BodyAt, Bclose - BodyAt);
		Decl.BodyAt = BodyAt;
		Decl.Next = Bclose + 1;

		const TArray<int32> Body = FUetkxLexer::ToCodePoints(Decl.Body);
		const FUetkxSplitReturn Split = FUetkxFileScan::SplitMarkupReturn(Body, /*bRequireMarkupPeek*/ true);
		if (!Split.bOk)
		{
			AddDiag(Out.Diags, TEXT("UETKX2101"), 0, TEXT("component has no `return ( ... )` markup return"), Ci, 9);
			return -1;
		}
		Decl.Setup = FUetkxLexer::FromCodePoints(Body, 0, Split.ReturnAt);
		Decl.SetupAt = BodyAt;
		Decl.HookCalls = ScanHookCalls(FUetkxLexer::ToCodePoints(Decl.Setup));

		FUetkxMarkup Parser;
		FUetkxParseResult Pr = Parser.Parse(Body, Split.MStart, Split.MEnd);
		if (!Pr.IsOk())
		{
			AddDiag(Out.Diags, *Pr.ErrorCode, 0, Pr.ErrorMsg, BodyAt + FMath::Max(0, Pr.ErrorAt));
			return -1;
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
			return -1;
		}
		Decl.Root = RenderRoots[0];

		const int32 Idx = Out.Components.Num();
		Out.Components.Add(MoveTemp(Decl));
		Out.Order.Emplace(EUetkxDeclKind::Component, Idx);
		return Bclose + 1;
	}

	/** Parse a `hook UseName(params) [-> Ret] { body }` decl at `Hi`. Params `( )` REQUIRED (family
	 *  UITKX2201). Returns Next past the body, or -1 on a fatal error. */
	int32 ParseHook(const TArray<int32>& Src, int32 Hi, bool bExported, FUetkxFileScanResult& Out)
	{
		const int32 N = Src.Num();
		FUetkxHookDecl Decl;
		Decl.bExported = bExported;
		Decl.At = Hi;
		int32 k = SkipWsOnly(Src, Hi + 4);
		Decl.NameAt = k;
		while (k < N && FUetkxLexer::IsIdentCode(Src[k]))
		{
			++k;
		}
		Decl.Name = FUetkxLexer::FromCodePoints(Src, Decl.NameAt, k - Decl.NameAt);
		if (Decl.Name.IsEmpty())
		{
			AddDiag(Out.Diags, TEXT("UETKX2200"), 0, TEXT("missing hook name"), k);
			return -1;
		}
		if (!Decl.Name.StartsWith(TEXT("Use")) || Decl.Name.Len() < 4 || !FChar::IsUpper(Decl.Name[3]))
		{
			AddDiag(Out.Diags, TEXT("UETKX2203"), 1,
					FString::Printf(TEXT("hook name `%s` should start with `Use`"), *Decl.Name), Decl.NameAt,
					Decl.Name.Len());
		}
		k = SkipWsOnly(Src, k);
		if (k < N && Src[k] == '<')
		{
			AddDiag(Out.Diags, TEXT("UETKX3006"), 0,
					TEXT("template `hook` declarations are not supported — write a C++ template in a `module`"), k);
			return -1;
		}
		if (k >= N || Src[k] != C_LPAREN)
		{
			AddDiag(Out.Diags, TEXT("UETKX2201"), 0,
					FString::Printf(TEXT("hook `%s` is missing its parameter list `( ... )`"), *Decl.Name), k);
			return -1;
		}
		const int32 Pc = FUetkxLexer::FindMatching(Src, k);
		if (Pc == -1)
		{
			AddDiag(Out.Diags, TEXT("UETKX0304"), 0, TEXT("unclosed `(` in hook params"), k);
			return -1;
		}
		Decl.Params = FUetkxLexer::FromCodePoints(Src, k + 1, Pc - k - 1).TrimStartAndEnd();
		k = SkipWsOnly(Src, Pc + 1);
		if (k + 1 < N && Src[k] == '-' && Src[k + 1] == '>')
		{
			const int32 Rh = k + 2;
			while (k < N && Src[k] != C_LBRACE)
			{
				++k;
			}
			Decl.Ret = FUetkxLexer::FromCodePoints(Src, Rh, k - Rh).TrimStartAndEnd();
		}
		k = SkipWsOnly(Src, k);
		if (k >= N || Src[k] != C_LBRACE)
		{
			AddDiag(Out.Diags, TEXT("UETKX2202"), 0,
					FString::Printf(TEXT("hook `%s` body `{ ... }` expected"), *Decl.Name), FMath::Min(k, N - 1));
			return -1;
		}
		const int32 Bclose = FUetkxLexer::FindMatching(Src, k);
		if (Bclose == -1)
		{
			AddDiag(Out.Diags, TEXT("UETKX0304"), 0, TEXT("unclosed hook body"), k);
			return -1;
		}
		Decl.Body = FUetkxLexer::FromCodePoints(Src, k + 1, Bclose - k - 1);
		Decl.BodyAt = k + 1;
		Decl.Next = Bclose + 1;
		const int32 Idx = Out.Hooks.Num();
		Out.Hooks.Add(MoveTemp(Decl));
		Out.Order.Emplace(EUetkxDeclKind::Hook, Idx);
		return Bclose + 1;
	}

	/** Parse a `module Name { verbatim C++ }` decl at `Mi` (no parameter list, family UITKX2205).
	 *  Returns Next past the body, or -1 on a fatal error. */
	int32 ParseModule(const TArray<int32>& Src, int32 Mi, bool bExported, FUetkxFileScanResult& Out)
	{
		const int32 N = Src.Num();
		FUetkxModuleDecl Decl;
		Decl.bExported = bExported;
		Decl.At = Mi;
		int32 k = SkipWsOnly(Src, Mi + 6);
		Decl.NameAt = k;
		while (k < N && FUetkxLexer::IsIdentCode(Src[k]))
		{
			++k;
		}
		Decl.Name = FUetkxLexer::FromCodePoints(Src, Decl.NameAt, k - Decl.NameAt);
		if (Decl.Name.IsEmpty())
		{
			AddDiag(Out.Diags, TEXT("UETKX2204"), 0, TEXT("missing module name"), k);
			return -1;
		}
		k = SkipWsOnly(Src, k);
		if (k >= N || Src[k] != C_LBRACE)
		{
			AddDiag(Out.Diags, TEXT("UETKX2205"), 0,
					FString::Printf(TEXT("module `%s` is missing a body — expected `{` after the name (modules take "
										 "no `( )`)"),
									*Decl.Name),
					FMath::Min(k, N - 1));
			return -1;
		}
		const int32 Bclose = FUetkxLexer::FindMatching(Src, k);
		if (Bclose == -1)
		{
			AddDiag(Out.Diags, TEXT("UETKX0304"), 0, TEXT("unclosed module body"), k);
			return -1;
		}
		Decl.Body = FUetkxLexer::FromCodePoints(Src, k + 1, Bclose - k - 1);
		Decl.BodyAt = k + 1;
		Decl.Next = Bclose + 1;
		const int32 Idx = Out.Modules.Num();
		Out.Modules.Add(MoveTemp(Decl));
		Out.Order.Emplace(EUetkxDeclKind::Module, Idx);
		return Bclose + 1;
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

FUetkxSplitReturn FUetkxFileScan::SplitMarkupReturn(const TArray<int32>& Body, bool bRequireMarkupPeek)
{
	FUetkxSplitReturn Out;
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
				bool bMarkup = true;
				if (bRequireMarkupPeek)
				{
					// markup window? peek first real char inside (past leading markup comments)
					int32 First = SkipWsOnly(Body, P + 1);
					while (First < N)
					{
						const int32 Skipped = FUetkxLexer::SkipNoncodeMarkup(Body, First);
						if (Skipped == First)
						{
							break;
						}
						First = SkipWsOnly(Body, Skipped);
					}
					bMarkup = First < N && (Body[First] == C_LT || Body[First] == C_AT || Body[First] == C_LBRACE);
				}
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

FUetkxFileScanResult FUetkxFileScan::Scan(const FString& Source, const FString& Basename)
{
	FUetkxFileScanResult Out;
	const TArray<int32> Src = FUetkxLexer::ToCodePoints(Source);
	const int32 N = Src.Num();

	TMap<FString, FString> ImportedFrom; // name -> first specifier (UETKX2303 across the file)

	// ── Preamble: verbatim #include lines + `import { … } from "…"`, until the first declaration.
	// Imports are PREAMBLE ONLY (A1); an `export`/`component`/`hook`/`module` keyword ends it.
	int32 i = 0;
	while (i < N)
	{
		const int32 j = FUetkxLexer::SkipNoncode(Src, i);
		if (j != i)
		{
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
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("import")))
		{
			i = ParseImport(Src, i, Out, ImportedFrom);
			continue;
		}
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("export")) || FUetkxLexer::KeywordAt(Src, i, TEXT("component")) ||
			FUetkxLexer::KeywordAt(Src, i, TEXT("hook")) || FUetkxLexer::KeywordAt(Src, i, TEXT("module")))
		{
			break;
		}
		++i;
	}

	// ── Declarations: a SEQUENCE of components/hooks/modules in any order (FULL MIXED-DECL v1).
	// De-binarized (A1): no first-keyword dispatch — at each decl keyword we parse that kind, push
	// it to its array + Order, and continue.
	while (i < N)
	{
		const int32 j = FUetkxLexer::SkipNoncode(Src, i);
		if (j != i)
		{
			i = j;
			continue;
		}
		const int32 C = Src[i];
		if (C == C_SPACE || C == C_TAB || C == C_NL || C == C_CR)
		{
			++i;
			continue;
		}
		// imports are preamble-only: one appearing among the declarations is UETKX2309.
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("import")))
		{
			AddDiag(Out.Diags, TEXT("UETKX2309"), 0,
					TEXT("import must appear in the preamble, before the first declaration"), i);
			i = ParseImport(Src, i, Out, ImportedFrom); // consume it so the scan can resync
			continue;
		}
		bool bExported = false;
		const int32 DeclStart = i;
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("export")))
		{
			bExported = true;
			i = SkipWsOnly(Src, i + 6);
		}
		int32 Next = -1;
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("component")))
		{
			Next = ParseComponent(Src, i, bExported, Out);
			if (Next >= 0 && bExported)
			{
				Out.Components.Last().ExportAt = DeclStart; // the decl's true start (the `export`)
			}
		}
		else if (FUetkxLexer::KeywordAt(Src, i, TEXT("hook")))
		{
			Next = ParseHook(Src, i, bExported, Out);
		}
		else if (FUetkxLexer::KeywordAt(Src, i, TEXT("module")))
		{
			Next = ParseModule(Src, i, bExported, Out);
		}
		else
		{
			// `export` not followed by a declaration, or unparseable content where a declaration
			// was expected — the mixed-decl replacement for the old "content after body" hard error.
			AddDiag(Out.Diags, TEXT("UETKX2101"), 0,
					bExported ? TEXT("`export` must be followed by `component`, `hook`, or `module`")
							  : TEXT("expected a declaration (`component`, `hook`, or `module`)"),
					DeclStart);
			return Out;
		}
		if (Next < 0)
		{
			return Out; // fatal decl parse error (diag already recorded)
		}
		i = Next;
	}

	// No declarations at all → the family "empty file" error (imports alone are not a unit).
	if (Out.Components.IsEmpty() && Out.Hooks.IsEmpty() && Out.Modules.IsEmpty())
	{
		AddDiag(Out.Diags, TEXT("UETKX2101"), 0, TEXT("no `component`, `hook`, or `module` declaration found"), -1);
		return Out;
	}

	// Component/file-name nudge (0103) — kept for the one-component ergonomic. Companion suffixes
	// (`X.style.uetkx`) compare against the first dot-segment; multi-component files are flagged by
	// the convention warn (2105) instead, not by name churn.
	if (Out.Components.Num() == 1)
	{
		FString BaseCmp = Basename;
		int32 DotAt = -1;
		if (BaseCmp.FindChar(TEXT('.'), DotAt))
		{
			BaseCmp = BaseCmp.Left(DotAt);
		}
		const FUetkxComponentDecl& Only = Out.Components[0];
		if (Only.Name != BaseCmp && !BaseCmp.IsEmpty())
		{
			AddDiag(Out.Diags, TEXT("UETKX0103"), 1,
					FString::Printf(TEXT("component `%s` differs from file name `%s`"), *Only.Name, *BaseCmp),
					Only.NameAt, Only.Name.Len());
		}
	}

	// ONE component per file is now a CONVENTION (A3): >1 component is a LINT warn (sev 1), not a
	// hard error — the file still compiles (React parity: eslint-plugin-react-refresh).
	if (Out.Components.Num() > 1)
	{
		AddDiag(Out.Diags, TEXT("UETKX2105"), 1,
				FString::Printf(TEXT("%d components in one file — one component per file (convention); move siblings "
									 "into components/<Name>/<Name>.uetkx"),
								Out.Components.Num()),
				Out.Components[1].At);
	}
	return Out;
}

FUetkxPreambleScan FUetkxFileScan::ScanPreamble(const FString& Source)
{
	FUetkxPreambleScan Out;
	const TArray<int32> Src = FUetkxLexer::ToCodePoints(Source);
	const int32 N = Src.Num();

	// Preamble: imports only (reusing ParseImport; #include is irrelevant to resolution).
	TMap<FString, FString> ImportedFrom;
	FUetkxFileScanResult Tmp;
	int32 i = 0;
	while (i < N)
	{
		const int32 j = FUetkxLexer::SkipNoncode(Src, i);
		if (j != i)
		{
			i = j;
			continue;
		}
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("import")))
		{
			i = ParseImport(Src, i, Tmp, ImportedFrom);
			continue;
		}
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("export")) || FUetkxLexer::KeywordAt(Src, i, TEXT("component")) ||
			FUetkxLexer::KeywordAt(Src, i, TEXT("hook")) || FUetkxLexer::KeywordAt(Src, i, TEXT("module")))
		{
			break;
		}
		++i;
	}
	Out.Imports = MoveTemp(Tmp.Imports);

	// Declarations: capture [export] component/hook/module Name and SKIP the body (no markup parse).
	// Best-effort: stop at the first unparseable header (the target's real compile reports it).
	while (i < N)
	{
		const int32 j = FUetkxLexer::SkipNoncode(Src, i);
		if (j != i)
		{
			i = j;
			continue;
		}
		const int32 C = Src[i];
		if (C == C_SPACE || C == C_TAB || C == C_NL || C == C_CR)
		{
			++i;
			continue;
		}
		const int32 DeclStart = i;
		bool bExported = false;
		int32 ExportAt = -1;
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("export")))
		{
			bExported = true;
			ExportAt = i;
			i = SkipWsOnly(Src, i + 6);
		}
		FUetkxPreambleDecl D;
		D.bExported = bExported;
		D.ExportAt = ExportAt;
		D.At = i;
		int32 KeywordLen = 0;
		if (FUetkxLexer::KeywordAt(Src, i, TEXT("component")))
		{
			D.Kind = EUetkxDeclKind::Component;
			KeywordLen = 9;
		}
		else if (FUetkxLexer::KeywordAt(Src, i, TEXT("hook")))
		{
			D.Kind = EUetkxDeclKind::Hook;
			KeywordLen = 4;
		}
		else if (FUetkxLexer::KeywordAt(Src, i, TEXT("module")))
		{
			D.Kind = EUetkxDeclKind::Module;
			KeywordLen = 6;
		}
		else
		{
			break; // junk / import-after-decl / export-without-decl
		}
		int32 k = SkipWsOnly(Src, i + KeywordLen);
		const int32 Ns = k;
		while (k < N && FUetkxLexer::IsIdentCode(Src[k]))
		{
			++k;
		}
		D.Name = FUetkxLexer::FromCodePoints(Src, Ns, k - Ns);
		k = SkipWsOnly(Src, k);
		// component/hook take a `(...)` param list; skip it.
		if (D.Kind != EUetkxDeclKind::Module && k < N && Src[k] == C_LPAREN)
		{
			const int32 Pc = FUetkxLexer::FindMatching(Src, k);
			if (Pc == -1)
			{
				break;
			}
			k = SkipWsOnly(Src, Pc + 1);
		}
		// hooks may carry `-> Ret` before the body.
		if (D.Kind == EUetkxDeclKind::Hook && k + 1 < N && Src[k] == '-' && Src[k + 1] == '>')
		{
			while (k < N && Src[k] != C_LBRACE)
			{
				++k;
			}
		}
		k = SkipWsOnly(Src, k);
		if (k >= N || Src[k] != C_LBRACE)
		{
			break;
		}
		// component bodies mix markup + C++ (FindMatchingMarkup); hooks/modules are pure C++.
		const int32 Bclose = D.Kind == EUetkxDeclKind::Component ? FUetkxLexer::FindMatchingMarkup(Src, k)
																 : FUetkxLexer::FindMatching(Src, k);
		if (Bclose == -1)
		{
			break;
		}
		if (!D.Name.IsEmpty())
		{
			if (Out.FirstDeclAt < 0)
			{
				Out.FirstDeclAt = DeclStart;
			}
			Out.Decls.Add(MoveTemp(D));
		}
		i = Bclose + 1;
	}
	return Out;
}
