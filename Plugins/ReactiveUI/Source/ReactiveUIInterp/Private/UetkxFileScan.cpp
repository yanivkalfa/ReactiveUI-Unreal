// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxFileScan.h"

#include "UetkxChars.h"
#include "UetkxJsxScan.h"
#include "UetkxLexer.h"
#include "UetkxMarkup.h"

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

	/** §4 markup-everywhere: if code point `i` lands inside a jsx markup range, the index to jump
	 *  to (the range end); -1 otherwise. Walkers use this to keep CODE lexis off markup text. */
	int32 JsxSkipTo(const TArray<FUetkxMarkupRange>& Ranges, int32 i)
	{
		for (const FUetkxMarkupRange& R : Ranges)
		{
			if (i >= R.Start && i < R.End)
			{
				return R.End;
			}
		}
		return -1;
	}

	/** TB-12 / UETKX0114, NARROWED by §4 markup-everywhere: markup-as-value is now first-class
	 *  (`auto X = (<VerticalBox>…);` lowers in place), so the old detector is gone. What stays
	 *  illegal is a PAREN-LESS markup return at statement level (`return <Tag/>;`) — the family
	 *  return spelling is `return ( <markup> );` (that's what the collector recognizes; a bare
	 *  form would otherwise die as an FRuiNode→FRuiNodeArray conversion error inside the .inl).
	 *  Lambda-nested returns (ParenDepth > 0, deduced return type) lower fine and stay legal. */
	void DiagnoseBareMarkupReturn(const TArray<int32>& Body, const TArray<FUetkxMarkupRange>& Ranges, int32 BodyAt,
								  TArray<FUetkxDiag>& Diags)
	{
		using namespace UetkxChars;
		const int32 N = Body.Num();
		int32 ParenDepth = 0;
		int32 i = 0;
		while (i < N)
		{
			const int32 Skip = JsxSkipTo(Ranges, i);
			if (Skip != -1)
			{
				i = Skip;
				continue;
			}
			const int32 j = FUetkxLexer::SkipNoncode(Body, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			const int32 C = Body[i];
			if (C == C_LPAREN || C == C_LBRACKET)
			{
				++ParenDepth;
				++i;
				continue;
			}
			if (C == C_RPAREN || C == C_RBRACKET)
			{
				--ParenDepth;
				++i;
				continue;
			}
			if (ParenDepth == 0 && C == 'r' && FUetkxLexer::KeywordAt(Body, i, TEXT("return")))
			{
				int32 P = i + 6;
				while (P < N && (Body[P] == C_SPACE || Body[P] == C_TAB || Body[P] == C_NL || Body[P] == C_CR))
				{
					++P;
				}
				for (const FUetkxMarkupRange& R : Ranges)
				{
					if (R.Start == P)
					{
						AddDiag(Diags, TEXT("UETKX0114"), 0,
								TEXT("a markup return must be parenthesized — write `return ( <...> );`"), BodyAt + P);
						return;
					}
				}
				i += 6;
				continue;
			}
			++i;
		}
	}

	/** TB-5 / UETKX0107 — Unity's UnreachableAfterReturn (UITKX0107), family-numbered: dead code
	 *  after the FIRST top-level `return` statement of a component body. Handles BOTH shapes —
	 *  a top-level markup `return ( … )` before the end of the body, and a plain C++ early
	 *  `return x;` in setup (the collector never sees those — they carry no markup). Severity 2
	 *  (hint); the LSP attaches the Unnecessary tag so editors FADE the range instead of
	 *  squiggling it. Comment-only tails don't count as content. */
	void DiagnoseUnreachableAfterReturn(const TArray<int32>& Body, const TArray<FUetkxReturnSpan>& Returns,
										const TArray<FUetkxMarkupRange>& Ranges, int32 BodyAt,
										TArray<FUetkxDiag>& Diags)
	{
		using namespace UetkxChars;
		const int32 N = Body.Num();
		int32 DeadStart = -1;
		int32 Depth = 0;
		int32 i = 0;
		while (i < N)
		{
			const int32 Skip = JsxSkipTo(Ranges, i);
			if (Skip != -1)
			{
				i = Skip;
				continue;
			}
			const int32 j = FUetkxLexer::SkipNoncode(Body, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			const int32 C = Body[i];
			if (C == C_LBRACE || C == C_LPAREN || C == C_LBRACKET)
			{
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
			if (Depth == 0 && FUetkxLexer::KeywordAt(Body, i, TEXT("return")))
			{
				const FUetkxReturnSpan* Span = nullptr;
				for (const FUetkxReturnSpan& S : Returns)
				{
					if (S.ReturnAt == i)
					{
						Span = &S;
						break;
					}
				}
				int32 End;
				if (Span)
				{
					End = Span->AfterParen;
					while (End < N &&
						   (Body[End] == C_SPACE || Body[End] == C_TAB || Body[End] == C_NL || Body[End] == C_CR))
					{
						++End;
					}
					if (End < N && Body[End] == ';')
					{
						++End;
					}
				}
				else
				{
					// A plain C++ `return expr;` — scan to its statement-ending top-level `;`.
					// Markup ranges jump whole (a `;` inside markup text is not a terminator).
					int32 D2 = 0;
					int32 k = i + 6;
					while (k < N)
					{
						const int32 Skip2 = JsxSkipTo(Ranges, k);
						if (Skip2 != -1)
						{
							k = Skip2;
							continue;
						}
						const int32 j2 = FUetkxLexer::SkipNoncode(Body, k);
						if (j2 != k)
						{
							k = j2;
							continue;
						}
						const int32 C2 = Body[k];
						if (C2 == C_LBRACE || C2 == C_LPAREN || C2 == C_LBRACKET)
						{
							++D2;
						}
						else if (C2 == C_RBRACE || C2 == C_RPAREN || C2 == C_RBRACKET)
						{
							--D2;
						}
						else if (C2 == ';' && D2 == 0)
						{
							++k;
							break;
						}
						++k;
					}
					End = k;
				}
				DeadStart = End;
				break;
			}
			++i;
		}
		if (DeadStart < 0)
		{
			return;
		}
		int32 First = -1;
		i = DeadStart;
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Body, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			const int32 C = Body[i];
			if (C == C_SPACE || C == C_TAB || C == C_NL || C == C_CR)
			{
				++i;
				continue;
			}
			First = i;
			break;
		}
		if (First < 0)
		{
			return;
		}
		int32 Last = N;
		while (Last > First && (Body[Last - 1] == C_SPACE || Body[Last - 1] == C_TAB || Body[Last - 1] == C_NL ||
								Body[Last - 1] == C_CR))
		{
			--Last;
		}
		AddDiag(Diags, TEXT("UETKX0107"), 2, TEXT("Unreachable code after 'return'."), BodyAt + First, Last - First);
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
		// Angle brackets nest ONLY in TYPE position (before a param's `=`): `TArray<TMap<A,B>>`. In a
		// DEFAULT value (after `=`) a `<`/`>`/`<<`/`>>` is a comparison/shift OPERATOR, not nesting —
		// counting it there unbalanced the depth and merged/dropped later params (bughunt SCAN-1). A
		// default's own commas are protected by ()/[]/{} which Depth still tracks; the flag resets at
		// each top-level comma (the next param starts fresh in type position).
		bool bInDefault = false;
		FString Cur;
		for (int32 i = 0; i < ParamText.Len(); ++i)
		{
			const TCHAR C = ParamText[i];
			if (C == '(' || C == '[' || C == '{')
			{
				++Depth;
			}
			else if (C == ')' || C == ']' || C == '}')
			{
				--Depth;
			}
			else if (!bInDefault && C == '<')
			{
				++Depth;
			}
			else if (!bInDefault && C == '>')
			{
				--Depth;
			}
			else if (C == '=' && Depth == 0 && (i + 1 >= ParamText.Len() || ParamText[i + 1] != '=') &&
					 (i == 0 || (ParamText[i - 1] != '=' && ParamText[i - 1] != '!' && ParamText[i - 1] != '<' &&
								 ParamText[i - 1] != '>')))
			{
				bInDefault = true; // entered this param's default value
			}
			if (C == ',' && Depth == 0)
			{
				Pieces.Add(Cur);
				Cur.Empty();
				bInDefault = false;
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

	/** Collect the ordered hook-call kinds in setup (word-boundary, not after . or ::).
	 *  §4.3 HMR protection: markup ranges are EXCLUDED — hooks are illegal inside markup
	 *  (UETKX0013), so markup text must never perturb the hook signature: editing a value-markup
	 *  child would otherwise flip the signature and spuriously reset live state on a HMR swap. */
	TArray<FString> ScanHookCalls(const TArray<int32>& Setup, const TArray<FUetkxMarkupRange>* SkipRanges = nullptr)
	{
		TArray<FString> Out;
		const TArray<FString>& Hooks = FUetkxFileScan::HookNames();
		int32 i = 0;
		const int32 N = Setup.Num();
		while (i < N)
		{
			if (SkipRanges != nullptr)
			{
				const int32 Skip = JsxSkipTo(*SkipRanges, i);
				if (Skip != -1)
				{
					i = Skip;
					continue;
				}
			}
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

	// ── §4 rules of hooks — the family port (Unity UITKX0013-0016, Godot T2.5), C++-shaped ──────
	// Hooks must run unconditionally at the top level of the component body: 0013 conditional
	// (if/else — and any hook inside markup, which renders conditionally by construction), 0014
	// loop (for/while/@for/@while), 0015 match branch (switch/@match), 0016 callback/lambda
	// (incl. event attributes). Every violating call is reported, not just the first.

	enum class EHookBlock : uint8
	{
		None,
		Conditional,
		Loop,
		Match,
		Callback,
		Markup // plain attr/child expression — 0013 with the in-markup wording
	};

	const TCHAR* HookBlockCode(EHookBlock Kind)
	{
		switch (Kind)
		{
		case EHookBlock::Loop:
			return TEXT("UETKX0014");
		case EHookBlock::Match:
			return TEXT("UETKX0015");
		case EHookBlock::Callback:
			return TEXT("UETKX0016");
		default:
			return TEXT("UETKX0013");
		}
	}

	FString HookBlockMessage(EHookBlock Kind)
	{
		const TCHAR* What = TEXT("conditionally (inside an if/else)");
		switch (Kind)
		{
		case EHookBlock::Loop:
			What = TEXT("inside a loop");
			break;
		case EHookBlock::Match:
			What = TEXT("inside a switch/match branch");
			break;
		case EHookBlock::Callback:
			What = TEXT("inside a callback/lambda");
			break;
		case EHookBlock::Markup:
			What = TEXT("inside markup");
			break;
		default:
			break;
		}
		return FString::Printf(
			TEXT("hook called %s — hooks must run unconditionally at the top level of the component body"), What);
	}

	/** Hook-call match at `i` (same rule as ScanHookCalls): a known hook name followed by `(` or
	 *  a `<` template-arg list. Returns the name length, or 0. */
	int32 HookCallLenAt(const TArray<int32>& Src, int32 i)
	{
		const int32 N = Src.Num();
		if (i >= N || (Src[i] != 'U' && Src[i] != 'P'))
		{
			return 0;
		}
		for (const FString& Hook : FUetkxFileScan::HookNames())
		{
			if (FUetkxLexer::KeywordAt(Src, i, *Hook))
			{
				const int32 K = SkipWsOnly(Src, i + Hook.Len());
				if (K < N && (Src[K] == C_LPAREN || Src[K] == C_LT))
				{
					return Hook.Len();
				}
			}
		}
		return 0;
	}

	void ValidateMarkupNodeHooks(const FUetkxNode& Node, int32 Base, int32 FallbackAt, EHookBlock Enclosing,
								 TArray<FUetkxDiag>& Diags);

	/** Code embedded in markup (attr/child expressions, directive headers/bodies): flag hook
	 *  calls with `Kind`, then recurse into any markup ranges nested in the code (jsx scan).
	 *  `Base` is the absolute offset of Code[0], or -1 when unknown (diags fall back to
	 *  FallbackAt). */
	void ScanMarkupCodeForHooks(const FString& Code, int32 Base, int32 FallbackAt, EHookBlock Kind,
								TArray<FUetkxDiag>& Diags)
	{
		const TArray<int32> Src = FUetkxLexer::ToCodePoints(Code);
		TArray<FUetkxMarkupRange> Ranges = FUetkxJsxScan::FindMarkupRanges(Src, 0, Src.Num());
		for (FUetkxMarkupRange& R : Ranges)
		{
			if (R.End == -1)
			{
				R.End = Src.Num();
			}
		}
		const int32 N = Src.Num();
		int32 i = 0;
		while (i < N)
		{
			const int32 Jump = JsxSkipTo(Ranges, i);
			if (Jump != -1)
			{
				i = Jump;
				continue;
			}
			const int32 j = FUetkxLexer::SkipNoncode(Src, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			const int32 HookLen = HookCallLenAt(Src, i);
			if (HookLen > 0)
			{
				AddDiag(Diags, HookBlockCode(Kind), 0, HookBlockMessage(Kind), Base < 0 ? FallbackAt : Base + i,
						HookLen);
				i += HookLen;
				continue;
			}
			if (FUetkxLexer::IsIdentCode(Src[i]))
			{
				while (i < N && FUetkxLexer::IsIdentCode(Src[i]))
				{
					++i;
				}
				continue;
			}
			++i;
		}
		for (const FUetkxMarkupRange& R : Ranges)
		{
			FUetkxMarkup Parser;
			FUetkxParseResult Pr = Parser.Parse(Src, R.Start, R.End);
			if (!Pr.IsOk())
			{
				continue; // the emitter owns markup parse errors (UETKX2508 / family codes)
			}
			for (const TSharedPtr<FUetkxNode>& Nd : Pr.Nodes)
			{
				if (Nd.IsValid())
				{
					ValidateMarkupNodeHooks(*Nd, Base, Base < 0 ? FallbackAt : Base + R.Start, Kind, Diags);
				}
			}
		}
	}

	/** A directive body — statements + `return ( <markup> )` (family 0.7): validate the lead code
	 *  (jsx-aware), then every root of the return window. Mirrors EmitBody's split exactly. */
	void ValidateDirectiveBodyHooks(const FString& BodyMarkup, int32 Base, int32 FallbackAt, EHookBlock Enclosing,
									TArray<FUetkxDiag>& Diags)
	{
		const TArray<int32> Body = FUetkxLexer::ToCodePoints(BodyMarkup);
		const FUetkxSplitReturn Split = FUetkxFileScan::SplitMarkupReturn(Body, /*bRequireMarkupPeek*/ false);
		if (!Split.bOk)
		{
			ScanMarkupCodeForHooks(BodyMarkup, Base, FallbackAt, Enclosing, Diags);
			return;
		}
		ScanMarkupCodeForHooks(FUetkxLexer::FromCodePoints(Body, 0, Split.ReturnAt), Base, FallbackAt, Enclosing,
							   Diags);
		FUetkxMarkup Parser;
		FUetkxParseResult Pr = Parser.Parse(Body, Split.MStart, Split.MEnd);
		if (Pr.IsOk())
		{
			for (const TSharedPtr<FUetkxNode>& Nd : Pr.Nodes)
			{
				if (Nd.IsValid())
				{
					ValidateMarkupNodeHooks(*Nd, Base, Base < 0 ? FallbackAt : Base + Split.MStart, Enclosing, Diags);
				}
			}
		}
	}

	/** Walk one parsed markup node: attr/child expressions, directive headers and bodies.
	 *  `Base` is the absolute anchor of this tree's offset frame (node.At/Vat/BodyAt compose on
	 *  top of it), or -1 when the frame is detached (diags fall back to FallbackAt). */
	void ValidateMarkupNodeHooks(const FUetkxNode& Node, int32 Base, int32 FallbackAt, EHookBlock Enclosing,
								 TArray<FUetkxDiag>& Diags)
	{
		const int32 NodeAt = Base < 0 ? FallbackAt : Base + Node.At;
		switch (Node.Type)
		{
		case EUetkxNodeType::El:
		case EUetkxNodeType::Frag:
			for (const FUetkxAttr& Attr : Node.Attrs)
			{
				if (Attr.Kind != EUetkxAttrKind::Expr && Attr.Kind != EUetkxAttrKind::Spread)
				{
					continue;
				}
				EHookBlock Kind = Enclosing;
				if (Attr.Name.Len() > 2 && Attr.Name.StartsWith(TEXT("On")))
				{
					Kind = EHookBlock::Callback; // event attrs run as callbacks
				}
				else if (Kind == EHookBlock::None)
				{
					Kind = EHookBlock::Markup;
				}
				ScanMarkupCodeForHooks(Attr.Value, Base < 0 || Attr.Vat < 0 ? -1 : Base + Attr.Vat, NodeAt, Kind,
									   Diags);
			}
			for (const TSharedPtr<FUetkxNode>& Child : Node.Children)
			{
				if (Child.IsValid())
				{
					ValidateMarkupNodeHooks(*Child, Base, NodeAt, Enclosing, Diags);
				}
			}
			break;
		case EUetkxNodeType::Expr:
			ScanMarkupCodeForHooks(Node.Code, Base < 0 || Node.Vat < 0 ? -1 : Base + Node.Vat, NodeAt,
								   Enclosing == EHookBlock::None ? EHookBlock::Markup : Enclosing, Diags);
			break;
		case EUetkxNodeType::If:
			for (const FUetkxIfBranch& Branch : Node.Branches)
			{
				ScanMarkupCodeForHooks(Branch.Cond, -1, NodeAt, EHookBlock::Conditional, Diags);
				ValidateDirectiveBodyHooks(Branch.BodyMarkup, Base < 0 || Branch.BodyAt < 0 ? -1 : Base + Branch.BodyAt,
										   NodeAt, EHookBlock::Conditional, Diags);
			}
			if (Node.ElseBody.IsSet())
			{
				ValidateDirectiveBodyHooks(Node.ElseBody.GetValue(),
										   Base < 0 || Node.ElseBodyAt < 0 ? -1 : Base + Node.ElseBodyAt, NodeAt,
										   EHookBlock::Conditional, Diags);
			}
			break;
		case EUetkxNodeType::For:
		case EUetkxNodeType::While:
			ScanMarkupCodeForHooks(Node.Header, -1, NodeAt, EHookBlock::Loop, Diags);
			ValidateDirectiveBodyHooks(Node.BodyMarkup, Base < 0 || Node.BodyAt < 0 ? -1 : Base + Node.BodyAt, NodeAt,
									   EHookBlock::Loop, Diags);
			break;
		case EUetkxNodeType::Match:
			ScanMarkupCodeForHooks(Node.Subject, -1, NodeAt, EHookBlock::Match, Diags);
			for (const FUetkxMatchCase& Case : Node.Cases)
			{
				ScanMarkupCodeForHooks(Case.Value, -1, NodeAt, EHookBlock::Match, Diags);
				ValidateDirectiveBodyHooks(Case.BodyMarkup, Base < 0 || Case.BodyAt < 0 ? -1 : Base + Case.BodyAt,
										   NodeAt, EHookBlock::Match, Diags);
			}
			if (Node.DefaultBody.IsSet())
			{
				ValidateDirectiveBodyHooks(Node.DefaultBody.GetValue(),
										   Base < 0 || Node.DefaultBodyAt < 0 ? -1 : Base + Node.DefaultBodyAt, NodeAt,
										   EHookBlock::Match, Diags);
			}
			break;
		default:
			break;
		}
	}

	/** The C++ brace-stack walk over the body's CODE (markup regions skipped): flag hook calls
	 *  under if/else/for/while/do/switch blocks (incl. brace-less bodies + headers) and inside
	 *  lambdas. */
	void CodeWalkHooks(const TArray<int32>& Body, const TArray<FUetkxMarkupRange>& Skip, int32 BodyAt,
					   TArray<FUetkxDiag>& Diags)
	{
		TArray<EHookBlock> Stack; // one entry per '{'
		TSet<int32> LambdaBraces; // '{' offsets recognized as lambda bodies
		EHookBlock Pending = EHookBlock::None;
		int32 ParenDepth = 0;
		const int32 N = Body.Num();
		int32 i = 0;
		while (i < N)
		{
			const int32 Jump = JsxSkipTo(Skip, i);
			if (Jump != -1)
			{
				i = Jump;
				continue;
			}
			const int32 j = FUetkxLexer::SkipNoncode(Body, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			const int32 C = Body[i];
			if (C == C_LPAREN)
			{
				++ParenDepth;
				++i;
				continue;
			}
			if (C == C_RPAREN)
			{
				--ParenDepth;
				++i;
				continue;
			}
			if (C == C_LBRACKET)
			{
				// lambda intro? peek non-destructively: [captures] (params)? specifiers? '{'
				const int32 CloseB = FUetkxLexer::FindMatching(Body, i);
				if (CloseB != -1)
				{
					int32 k = SkipWsOnly(Body, CloseB + 1);
					if (k < N && Body[k] == C_LPAREN)
					{
						const int32 CloseP = FUetkxLexer::FindMatching(Body, k);
						k = CloseP == -1 ? N : SkipWsOnly(Body, CloseP + 1);
					}
					int32 Guard = 0;
					while (k < N && Body[k] != C_LBRACE && Guard++ < 64 &&
						   (FUetkxLexer::IsIdentCode(Body[k]) || Body[k] == C_SPACE || Body[k] == C_TAB ||
							Body[k] == C_NL || Body[k] == C_CR || Body[k] == C_DASH || Body[k] == C_GT ||
							Body[k] == ':' || Body[k] == C_AMP || Body[k] == C_STAR || Body[k] == C_LT ||
							Body[k] == C_COMMA))
					{
						++k;
					}
					if (k < N && Body[k] == C_LBRACE)
					{
						LambdaBraces.Add(k);
					}
				}
				++i; // walk the capture list normally — init-captures run unconditionally
				continue;
			}
			if (C == C_LBRACE)
			{
				EHookBlock Kind = EHookBlock::None;
				if (LambdaBraces.Contains(i))
				{
					Kind = EHookBlock::Callback;
				}
				else if (Pending != EHookBlock::None)
				{
					Kind = Pending;
				}
				Stack.Push(Kind);
				Pending = EHookBlock::None;
				++i;
				continue;
			}
			if (C == C_RBRACE)
			{
				if (Stack.Num() > 0)
				{
					Stack.Pop();
				}
				++i;
				continue;
			}
			if (C == ';' && ParenDepth == 0)
			{
				Pending = EHookBlock::None; // a brace-less `if (x) stmt;` body ended
				++i;
				continue;
			}
			if (FUetkxLexer::IsIdentCode(C) && (i == 0 || !FUetkxLexer::IsIdentCode(Body[i - 1])))
			{
				const int32 HookLen = HookCallLenAt(Body, i);
				if (HookLen > 0)
				{
					EHookBlock Ctx = Pending;
					for (int32 S = Stack.Num() - 1; Ctx == EHookBlock::None && S >= 0; --S)
					{
						Ctx = Stack[S];
					}
					if (Ctx != EHookBlock::None)
					{
						AddDiag(Diags, HookBlockCode(Ctx), 0, HookBlockMessage(Ctx), BodyAt + i, HookLen);
					}
					i += HookLen;
					continue;
				}
				if (FUetkxLexer::KeywordAt(Body, i, TEXT("if")) || FUetkxLexer::KeywordAt(Body, i, TEXT("else")))
				{
					Pending = EHookBlock::Conditional;
				}
				else if (FUetkxLexer::KeywordAt(Body, i, TEXT("for")) ||
						 FUetkxLexer::KeywordAt(Body, i, TEXT("while")) || FUetkxLexer::KeywordAt(Body, i, TEXT("do")))
				{
					Pending = EHookBlock::Loop;
				}
				else if (FUetkxLexer::KeywordAt(Body, i, TEXT("switch")))
				{
					Pending = EHookBlock::Match;
				}
				while (i < N && FUetkxLexer::IsIdentCode(Body[i]))
				{
					++i;
				}
				continue;
			}
			++i;
		}
	}

	/** §4 rules-of-hooks driver: the code walk (outside markup), then every markup region's
	 *  parsed tree — return windows use their already-parsed roots; value-markup ranges parse
	 *  here (tolerantly — the emitter owns parse errors). */
	void ValidateHookPlacement(const TArray<int32>& Body, const TArray<FUetkxMarkupRange>& JsxRanges,
							   const TArray<FUetkxReturnSpan>& Returns, int32 BodyAt, TArray<FUetkxDiag>& Diags)
	{
		TArray<FUetkxMarkupRange> Skip = JsxRanges;
		for (const FUetkxReturnSpan& Span : Returns)
		{
			FUetkxMarkupRange R;
			R.Start = Span.MStart;
			R.End = Span.MEnd;
			Skip.Add(R); // directive-form windows (`return ( @if … )`) are not jsx ranges
		}
		CodeWalkHooks(Body, Skip, BodyAt, Diags);
		for (const FUetkxReturnSpan& Span : Returns)
		{
			if (Span.Root.IsValid())
			{
				ValidateMarkupNodeHooks(*Span.Root, BodyAt, BodyAt + Span.MStart, EHookBlock::None, Diags);
			}
		}
		for (const FUetkxMarkupRange& R : JsxRanges)
		{
			bool bInWindow = false;
			for (const FUetkxReturnSpan& Span : Returns)
			{
				if (R.Start >= Span.MStart && R.Start < Span.MEnd)
				{
					bInWindow = true;
					break;
				}
			}
			if (bInWindow)
			{
				continue;
			}
			FUetkxMarkup Parser;
			FUetkxParseResult Pr = Parser.Parse(Body, R.Start, R.End);
			if (!Pr.IsOk())
			{
				continue;
			}
			for (const TSharedPtr<FUetkxNode>& Nd : Pr.Nodes)
			{
				if (Nd.IsValid())
				{
					ValidateMarkupNodeHooks(*Nd, BodyAt, BodyAt + R.Start, EHookBlock::None, Diags);
				}
			}
		}
	}

	/** The quoted or angle-bracket header path inside a `#include "X.h"` / `#include <X.h>` line,
	 *  or empty if malformed (UETKX2317 redundancy hint, INCLUDE_RETIREMENT_PLAN.md §B). */
	FString ExtractIncludeHeader(const FString& Line)
	{
		int32 Open = INDEX_NONE;
		TCHAR CloseChar = TEXT('"');
		if (Line.FindChar(TEXT('"'), Open))
		{
			CloseChar = TEXT('"');
		}
		else if (Line.FindChar(TEXT('<'), Open))
		{
			CloseChar = TEXT('>');
		}
		else
		{
			return FString();
		}
		int32 Close = INDEX_NONE;
		if (!Line.Mid(Open + 1).FindChar(CloseChar, Close))
		{
			return FString();
		}
		return Line.Mid(Open + 1, Close);
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

	/** Parse a preamble import at `Start` (the `import` keyword) — either the NAMED form
	 *  `import { A, B } from "spec"` (named exports only — no `*`, no default, A1) or the HOST
	 *  INCLUDE form `import "@Header.h"` (INCLUDE_RETIREMENT_PLAN.md §B — no braces, no `from`,
	 *  no names). Records the FUetkxImportDecl + duplicate-import diagnostics (UETKX2303) into
	 *  Out; ImportedFrom tracks name -> first specifier for named imports and payload -> payload
	 *  for host includes (the keyspaces cannot collide). Returns the index just past the import
	 *  (or the resync point on a malformed one). */
	int32 ParseImport(const TArray<int32>& Src, int32 Start, FUetkxFileScanResult& Out,
					  TMap<FString, FString>& ImportedFrom)
	{
		const int32 N = Src.Num();
		FUetkxImportDecl Imp;
		Imp.At = Start;
		int32 k = SkipWsOnly(Src, Start + 6); // past "import"

		// INCLUDE_RETIREMENT_PLAN.md §B: `import "@Header.h"` — a nameless HOST INCLUDE. The named
		// form always starts `{`; a leading quote routes here instead (no `from` clause).
		if (k < N && (Src[k] == C_QUOTE || Src[k] == C_APOS))
		{
			const int32 Quote = Src[k];
			const int32 QuoteAt = k;
			int32 q = k + 1;
			while (q < N && Src[q] != Quote && Src[q] != C_NL)
			{
				++q;
			}
			if (q >= N || Src[q] != Quote)
			{
				AddDiag(Out.Diags, TEXT("UETKX0300"), 0, TEXT("unterminated import specifier string"), QuoteAt);
				return AdvancePastLine(Src, QuoteAt);
			}
			const FString Payload = FUetkxLexer::FromCodePoints(Src, QuoteAt + 1, q - (QuoteAt + 1));
			const int32 End = q + 1;
			if (!Payload.StartsWith(TEXT("@")))
			{
				AddDiag(Out.Diags, TEXT("UETKX0303"), 0,
						TEXT("import expects `{ Name, ... } from \"...\"` or a `\"@Header.h\"` host include"), QuoteAt);
				return AdvancePastLine(Src, End);
			}
			FUetkxImportDecl HostImp;
			HostImp.At = Start;
			HostImp.bHostInclude = true;
			HostImp.SpecifierAt = QuoteAt;
			HostImp.Specifier = Payload.Mid(1); // strip the leading '@'
			// Duplicate-payload check (2303 rule, applied to host includes): ImportedFrom is keyed by
			// NAME for named imports and by PAYLOAD here — the keyspaces never collide (names are bare
			// identifiers; header payloads always contain `/` or `.`).
			if (ImportedFrom.Contains(HostImp.Specifier))
			{
				AddDiag(Out.Diags, TEXT("UETKX2303"), 0,
						FString::Printf(TEXT("duplicate host include `%s`"), *HostImp.Specifier), QuoteAt,
						Payload.Len());
			}
			else
			{
				ImportedFrom.Add(HostImp.Specifier, HostImp.Specifier);
			}
			Out.Imports.Add(MoveTemp(HostImp));
			return End;
		}

		if (k >= N || Src[k] != C_LBRACE)
		{
			AddDiag(Out.Diags, TEXT("UETKX0303"), 0,
					TEXT("import expects `{ Name, ... }` or a `\"@Header.h\"` host include after `import`"),
					FMath::Min(k, N - 1));
			return AdvancePastLine(Src, k);
		}
		const int32 Bclose = FUetkxLexer::FindMatching(Src, k);
		if (Bclose == -1)
		{
			AddDiag(Out.Diags, TEXT("UETKX0304"), 0, TEXT("unclosed `{` in import list"), k);
			return N;
		}
		// Skip whitespace AND comments between names — a `//`/`/* */` inside the brace list must not be
		// misread as a bad specifier that drops the whole import (bughunt SCAN-2).
		auto SkipWsAndComments = [&Src](int32 p)
		{
			for (;;)
			{
				p = SkipWsOnly(Src, p);
				const int32 nc = FUetkxLexer::SkipNoncode(Src, p);
				if (nc == p)
				{
					return p;
				}
				p = nc;
			}
		};
		for (int32 p = k + 1; p < Bclose;)
		{
			p = SkipWsAndComments(p);
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
				AddDiag(
					Out.Diags, TEXT("UETKX2303"), 0,
					FString::Printf(TEXT("duplicate import of `%s` (already imported from %s)"), *Name, *Imp.Specifier),
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
		// Wave G: collect ALL markup returns (early returns splice verbatim, Unity model). The
		// FINAL span must be top-level — it anchors the window/setup split (T1.4 last-wins) and
		// guarantees the emitted impl always ends in a return statement.
		Decl.Returns = FUetkxFileScan::CollectMarkupReturns(Body);
		// §4 markup-everywhere: the jsx-range list of record for this body (element-form markup at
		// every family boundary position — value, argument, ternary, short-circuit, return). Range
		// coords are body-relative; unbalanced ranges clamp to the body end. Computed before the
		// no-return early-out so a paren-less `return <Tag/>` gets its precise 0114 instead of a
		// bare 2101.
		TArray<FUetkxMarkupRange> JsxRanges = FUetkxJsxScan::FindMarkupRanges(Body, 0, Body.Num());
		for (FUetkxMarkupRange& R : JsxRanges)
		{
			if (R.End == -1)
			{
				R.End = Body.Num();
			}
		}
		DiagnoseBareMarkupReturn(Body, JsxRanges, BodyAt, Out.Diags);
		if (Decl.Returns.IsEmpty())
		{
			AddDiag(Out.Diags, TEXT("UETKX2101"), 0, TEXT("component has no `return ( ... )` markup return"), Ci, 9);
			return -1;
		}
		if (!Decl.Returns.Last().bTopLevel)
		{
			AddDiag(Out.Diags, TEXT("UETKX3007"), 0,
					TEXT("the component's final markup `return ( ... )` must be at the top level of the body"),
					BodyAt + Decl.Returns.Last().ReturnAt, 6);
			return -1;
		}
		const FUetkxReturnSpan& Final = Decl.Returns.Last();
		Decl.Setup = FUetkxLexer::FromCodePoints(Body, 0, Final.ReturnAt);
		Decl.SetupAt = BodyAt;
		Decl.HookCalls = ScanHookCalls(FUetkxLexer::ToCodePoints(Decl.Setup), &JsxRanges);
		DiagnoseUnreachableAfterReturn(Body, Decl.Returns, JsxRanges, BodyAt, Out.Diags);

		for (FUetkxReturnSpan& Span : Decl.Returns)
		{
			FUetkxMarkup Parser;
			FUetkxParseResult Pr = Parser.Parse(Body, Span.MStart, Span.MEnd);
			if (!Pr.IsOk())
			{
				AddDiag(Out.Diags, *Pr.ErrorCode, 0, Pr.ErrorMsg, BodyAt + FMath::Max(0, Pr.ErrorAt));
				return -1;
			}
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
				AddDiag(Out.Diags, TEXT("UETKX0108"), 0,
						FString::Printf(TEXT("a component must return exactly one root element (got %d)"),
										RenderRoots.Num()),
						BodyAt + Span.MStart);
				return -1;
			}
			Span.Root = RenderRoots[0];
			if (&Span == &Decl.Returns.Last())
			{
				Decl.WindowNodes = Pr.Nodes; // the final window keeps the formatter contract
			}
		}
		// §4 rules of hooks (family 0013-0016) — needs the parsed window roots, so it runs last.
		ValidateHookPlacement(Body, JsxRanges, Decl.Returns, BodyAt, Out.Diags);
		Decl.Root = Decl.Returns.Last().Root;

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
				// A declaration keyword at a token boundary before the body `{` means the hook's body is
				// missing — stop so the NEXT declaration is not swallowed as this hook's return/body
				// (bughunt SCAN-3); the `{`-expected check below then reports UETKX2202.
				const bool bBoundary = (k == Rh) || Src[k - 1] == C_SPACE || Src[k - 1] == C_TAB ||
									   Src[k - 1] == C_NL || Src[k - 1] == C_CR;
				if (bBoundary &&
					(FUetkxLexer::KeywordAt(Src, k, TEXT("component")) ||
					 FUetkxLexer::KeywordAt(Src, k, TEXT("hook")) || FUetkxLexer::KeywordAt(Src, k, TEXT("module")) ||
					 FUetkxLexer::KeywordAt(Src, k, TEXT("export")) || FUetkxLexer::KeywordAt(Src, k, TEXT("import"))))
				{
					break;
				}
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
										  TEXT("UseStableFunc"),
										  TEXT("UseStableAction"),
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

const TArray<FString>& FUetkxFileScan::AutoIncludedHeaders()
{
	// INCLUDE_RETIREMENT_PLAN.md §A — MUST match UetkxDriver.cpp's aggregator prelude and
	// virtualDoc.ts's PRELUDE exactly (comments there point back here).
	static const TArray<FString> Headers = {
		TEXT("CoreMinimal.h"),
		TEXT("RuiContext.h"),
		TEXT("RuiCoreElements.h"),
		TEXT("RuiSignal.h"),
		TEXT("RuiSlateElements.h"),
		TEXT("RuiStyle.h"),
		TEXT("RuiRouter.h"),
		TEXT("RuiAssetBrush.h"),
		TEXT("RuiFieldHooks.h"),
		TEXT("RuiUmgElement.h"),
		TEXT("RuiSignalViewModel.h"),
		TEXT("RuiHostWidget.h"),
		TEXT("RuiWorldSubsystem.h"),
		TEXT("RuiActivation.h"),
		TEXT("RuiActivatableScreen.h"),
		TEXT("RuiMvvmViewModel.h"),
		TEXT("UObject/StrongObjectPtr.h"),
		TEXT("Engine/World.h"),
	};
	return Headers;
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

TArray<FUetkxReturnSpan> FUetkxFileScan::CollectMarkupReturns(const TArray<int32>& Body)
{
	TArray<FUetkxReturnSpan> Out;
	const int32 N = Body.Num();
	int32 ParenDepth = 0; // parens + brackets hide returns (call args); braces do NOT (if/else)
	int32 BraceDepth = 0;
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
		if (C == C_LPAREN || C == C_LBRACKET)
		{
			++ParenDepth;
			++i;
			continue;
		}
		if (C == C_RPAREN || C == C_RBRACKET)
		{
			--ParenDepth;
			++i;
			continue;
		}
		if (C == C_LBRACE)
		{
			++BraceDepth;
			++i;
			continue;
		}
		if (C == C_RBRACE)
		{
			--BraceDepth;
			++i;
			continue;
		}
		if (ParenDepth == 0 && C == 'r' && FUetkxLexer::KeywordAt(Body, i, TEXT("return")))
		{
			const int32 P = SkipWsOnly(Body, i + 6);
			if (P < N && Body[P] == C_LPAREN)
			{
				// peek first real char inside (past leading markup comments)
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
				const bool bTopLevel = BraceDepth == 0;
				const bool bMarkup =
					First < N && (Body[First] == C_LT || Body[First] == C_AT || (Body[First] == C_LBRACE && bTopLevel));
				const int32 Close =
					bMarkup ? FUetkxLexer::FindMatchingMarkup(Body, P) : FUetkxLexer::FindMatching(Body, P);
				if (Close == -1)
				{
					++i;
					continue;
				}
				if (bMarkup)
				{
					FUetkxReturnSpan Span;
					Span.ReturnAt = i;
					Span.MStart = P + 1;
					Span.MEnd = Close;
					Span.AfterParen = Close + 1;
					Span.bTopLevel = bTopLevel;
					Out.Add(MoveTemp(Span));
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
					Out.PreambleIncludeAts.Add(i);
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

	// ── UETKX2317 (hint): a #include or `import "@X.h"` naming a header the generated prelude
	// already provides (INCLUDE_RETIREMENT_PLAN.md §B — the family's redundant-using hint,
	// ported per-leg). Severity 2, so it never contributes to HasError().
	{
		const TArray<FString>& AutoHeaders = AutoIncludedHeaders();
		for (int32 n = 0; n < Out.PreambleIncludes.Num(); ++n)
		{
			const FString Header = ExtractIncludeHeader(Out.PreambleIncludes[n]);
			if (!Header.IsEmpty() && AutoHeaders.Contains(Header))
			{
				AddDiag(Out.Diags, TEXT("UETKX2317"), 2,
						FString::Printf(TEXT("`%s` is auto-included by the generated prelude — this line is redundant"),
										*Header),
						Out.PreambleIncludeAts[n], Out.PreambleIncludes[n].Len());
			}
		}
		for (const FUetkxImportDecl& Imp : Out.Imports)
		{
			if (Imp.bHostInclude && AutoHeaders.Contains(Imp.Specifier))
			{
				AddDiag(Out.Diags, TEXT("UETKX2317"), 2,
						FString::Printf(TEXT("`%s` is auto-included by the generated prelude — this line is redundant"),
										*Imp.Specifier),
						Imp.SpecifierAt, Imp.Specifier.Len() + 3); // +3: '@' + the two quotes
			}
		}
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
			if (Next >= 0 && bExported)
			{
				Out.Hooks.Last().ExportAt = DeclStart; // the decl's true start (the `export`)
			}
		}
		else if (FUetkxLexer::KeywordAt(Src, i, TEXT("module")))
		{
			Next = ParseModule(Src, i, bExported, Out);
			if (Next >= 0 && bExported)
			{
				Out.Modules.Last().ExportAt = DeclStart; // the decl's true start (the `export`)
			}
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
