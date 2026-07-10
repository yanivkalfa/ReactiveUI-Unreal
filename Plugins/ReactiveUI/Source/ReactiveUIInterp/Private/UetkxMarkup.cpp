// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Line-for-line port of guitkx_markup.gd — change BOTH (all three dialects) or neither.

#include "UetkxMarkup.h"

#include "UetkxLexer.h"

#include "UetkxChars.h"

using namespace UetkxChars;

namespace
{
} // namespace

FUetkxParseResult FUetkxMarkup::Parse(const TArray<int32>& InSrc, int32 Start, int32 End)
{
	Src = &InSrc;
	ErrCode.Empty();
	ErrMsg.Empty();
	ErrAt = -1;
	LineStarts.Reset();
	LineStarts.Add(0);
	for (int32 i = 0; i < InSrc.Num(); ++i)
	{
		if (InSrc[i] == C_NL)
		{
			LineStarts.Add(i + 1);
		}
	}
	FNodesResult R = ParseNodes(Start, End);
	FUetkxParseResult Out;
	Out.Nodes = MoveTemp(R.Nodes);
	Out.ErrorCode = ErrCode;
	Out.ErrorMsg = ErrMsg;
	Out.ErrorAt = ErrAt;
	return Out;
}

void FUetkxMarkup::Fail(const TCHAR* Code, const FString& Msg, int32 At)
{
	ErrCode = Code;
	ErrMsg = Msg;
	ErrAt = At;
}

FUetkxMarkup::FNodesResult FUetkxMarkup::ParseNodes(int32 Start, int32 End)
{
	FNodesResult Out;
	const TArray<int32>& S = *Src;
	int32 i = Start;
	while (i < End && ErrCode.IsEmpty())
	{
		i = SkipWs(i, End);
		if (i >= End)
		{
			break;
		}
		const int32 C = S[i];
		if (C == C_LT)
		{
			// `<!-- ... -->` comment (checked before `</` — both start with `<`).
			if (i + 3 < End && S[i + 1] == C_BANG && S[i + 2] == C_DASH && S[i + 3] == C_DASH)
			{
				const int32 Hce = FindSeq(TEXT("-->"), i + 4);
				if (Hce == -1 || Hce + 3 > End)
				{
					Fail(TEXT("UETKX0304"), TEXT("unclosed `<!--` comment"), i);
					break;
				}
				TSharedPtr<FUetkxNode> Node = MakeShared<FUetkxNode>();
				Node->Type = EUetkxNodeType::Comment;
				Node->At = i;
				Node->Value = Slice(i, Hce + 3);
				Out.Nodes.Add(Node);
				i = Hce + 3;
				continue;
			}
			if (i + 1 < End && S[i + 1] == C_SLASH)
			{
				break; // a closing tag belongs to the caller
			}
			FNodeResult R = ParseElement(i, End);
			if (!ErrCode.IsEmpty())
			{
				break;
			}
			Out.Nodes.Add(R.Node);
			i = R.Next;
		}
		else if (C == C_SLASH && i + 1 < End && (S[i + 1] == C_SLASH || S[i + 1] == C_STAR))
		{
			// `// line` / `/* block */` comments at node-start position only.
			TSharedPtr<FUetkxNode> Node = MakeShared<FUetkxNode>();
			Node->Type = EUetkxNodeType::Comment;
			Node->At = i;
			if (S[i + 1] == C_SLASH)
			{
				int32 Le = -1;
				for (int32 k = i; k < End; ++k)
				{
					if (S[k] == C_NL)
					{
						Le = k;
						break;
					}
				}
				const int32 Stop = (Le == -1) ? End : Le;
				Node->Value = Slice(i, Stop);
				Out.Nodes.Add(Node);
				i = Stop;
			}
			else
			{
				const int32 Bce = FindSeq(TEXT("*/"), i + 2);
				if (Bce == -1 || Bce + 2 > End)
				{
					Fail(TEXT("UETKX0304"), TEXT("unclosed `/*` comment"), i);
					break;
				}
				Node->Value = Slice(i, Bce + 2);
				Out.Nodes.Add(Node);
				i = Bce + 2;
			}
		}
		else if (C == C_AT)
		{
			FNodeResult R = ParseDirective(i, End);
			if (!ErrCode.IsEmpty())
			{
				break;
			}
			Out.Nodes.Add(R.Node);
			i = R.Next;
		}
		else if (C == C_LBRACE)
		{
			const int32 Close = FUetkxLexer::FindMatching(S, i);
			if (Close == -1 || Close >= End)
			{
				Fail(TEXT("UETKX0304"), TEXT("unclosed `{` expression"), i);
				break;
			}
			TSharedPtr<FUetkxNode> Node = MakeShared<FUetkxNode>();
			Node->Type = EUetkxNodeType::Expr;
			Node->At = i;
			Node->Vat = SkipWs(i + 1, Close);
			Node->Code = SliceTrimmed(i + 1, Close);
			Out.Nodes.Add(Node);
			i = Close + 1;
		}
		else
		{
			FNodeResult R = ParseText(i, End);
			if (R.Node.IsValid())
			{
				Out.Nodes.Add(R.Node);
			}
			i = R.Next;
		}
	}
	Out.Next = i;
	return Out;
}

FUetkxMarkup::FNodeResult FUetkxMarkup::ParseElement(int32 OpenIndex, int32 End)
{
	const TArray<int32>& S = *Src;
	int32 i = OpenIndex + 1;
	const int32 Line = LineOf(OpenIndex);
	// tag name (empty -> fragment)
	const int32 NameStart = i;
	while (i < End && IsTagChar(S[i]))
	{
		++i;
	}
	const FString Tag = Slice(NameStart, i);
	// A `<` must be directly followed by a tag name, or `>` for a fragment.
	if (Tag.IsEmpty() && (i >= End || S[i] != C_GT))
	{
		Fail(TEXT("UETKX0300"), TEXT("invalid tag name -- `<` must be followed by a tag name, or `<>` for a fragment"),
			 OpenIndex);
		return {nullptr, End};
	}
	// A tag cannot start with a digit.
	if (!Tag.IsEmpty() && Tag[0] >= '0' && Tag[0] <= '9')
	{
		Fail(TEXT("UETKX0300"), FString::Printf(TEXT("tag name cannot start with a digit (<%s>)"), *Tag), OpenIndex);
		return {nullptr, End};
	}
	// attributes up to ">" or "/>"
	TArray<FUetkxAttr> Attrs;
	while (i < End)
	{
		i = SkipWs(i, End);
		if (i >= End)
		{
			Fail(TEXT("UETKX0303"), FString::Printf(TEXT("unexpected EOF in <%s>"), *Tag), OpenIndex);
			return {nullptr, End};
		}
		const int32 C = S[i];
		if (C == C_SLASH && i + 1 < End && S[i + 1] == C_GT)
		{
			return {MakeEl(Tag, MoveTemp(Attrs), {}, Line, OpenIndex), i + 2}; // self-closing
		}
		if (C == C_GT)
		{
			++i;
			break;
		}
		FAttrResult Ar = ParseAttribute(i, End);
		if (!ErrCode.IsEmpty())
		{
			return {nullptr, End};
		}
		Attrs.Add(MoveTemp(Ar.Attr));
		i = Ar.Next;
	}
	// paired: parse children; ParseNodes stops exactly on the matching "</" (or End).
	FNodesResult Cr = ParseNodes(i, End);
	if (!ErrCode.IsEmpty())
	{
		return {nullptr, End};
	}
	const int32 j = Cr.Next;
	// A truncated `<Box><` at EOF must fail on its own (family G-04).
	if (j >= End || S[j] != C_LT || j + 1 >= End || S[j + 1] != C_SLASH)
	{
		Fail(TEXT("UETKX0301"), FString::Printf(TEXT("unclosed tag <%s>"), *Tag), OpenIndex);
		return {nullptr, End};
	}
	// j points at "</": read the close name to ">".
	const int32 Ce = [&]()
	{
		for (int32 k = j; k < End; ++k)
		{
			if (S[k] == C_GT)
			{
				return k;
			}
		}
		return -1;
	}();
	if (Ce == -1)
	{
		Fail(TEXT("UETKX0303"), FString::Printf(TEXT("malformed closing tag for <%s>"), *Tag), j);
		return {nullptr, End};
	}
	const FString CloseName = SliceTrimmed(j + 2, Ce);
	if (CloseName != Tag)
	{
		Fail(TEXT("UETKX0302"), FString::Printf(TEXT("mismatched tag </%s> (expected </%s>)"), *CloseName, *Tag), j);
		return {nullptr, End};
	}
	return {MakeEl(Tag, MoveTemp(Attrs), MoveTemp(Cr.Nodes), Line, OpenIndex), Ce + 1};
}

TSharedPtr<FUetkxNode> FUetkxMarkup::MakeEl(const FString& Tag, TArray<FUetkxAttr>&& Attrs,
											TArray<TSharedPtr<FUetkxNode>>&& Children, int32 Line, int32 At)
{
	TSharedPtr<FUetkxNode> Node = MakeShared<FUetkxNode>();
	Node->At = At;
	Node->Children = MoveTemp(Children);
	if (Tag.IsEmpty())
	{
		Node->Type = EUetkxNodeType::Frag;
		return Node;
	}
	// <Fragment> is a named alias of <> (case-insensitive); spelling + attrs kept for the
	// formatter round-trip and the emitter's `key` handling.
	if (Tag.ToLower() == TEXT("fragment"))
	{
		Node->Type = EUetkxNodeType::Frag;
		Node->Named = Tag;
		Node->Attrs = MoveTemp(Attrs);
		return Node;
	}
	Node->Type = EUetkxNodeType::El;
	Node->Tag = Tag;
	Node->Attrs = MoveTemp(Attrs);
	Node->Line = Line;
	return Node;
}

FUetkxMarkup::FAttrResult FUetkxMarkup::ParseAttribute(int32 Start, int32 End)
{
	const TArray<int32>& S = *Src;
	int32 i = Start;
	// spread attribute `{...expr}` — merged into props at codegen. Also `{/* comment */}`.
	if (S[i] == C_LBRACE)
	{
		const int32 Probe = SkipWs(i + 1, End);
		if (Probe + 1 < End && S[Probe] == C_SLASH && S[Probe + 1] == C_STAR)
		{
			const int32 Ce2 = FindSeq(TEXT("*/"), Probe + 2);
			if (Ce2 == -1 || Ce2 + 2 > End)
			{
				Fail(TEXT("UETKX0304"), TEXT("unclosed comment in attribute list"), i);
				return {{}, false, End};
			}
			const int32 After = SkipWs(Ce2 + 2, End);
			if (After >= End || S[After] != C_RBRACE)
			{
				Fail(TEXT("UETKX0303"), TEXT("attribute comment must close with `*/}`"), i);
				return {{}, false, End};
			}
			FUetkxAttr Attr;
			Attr.Kind = EUetkxAttrKind::Comment;
			Attr.Value = Slice(i, After + 1);
			Attr.At = i;
			Attr.Vat = -1;
			Attr.End = After + 1;
			return {MoveTemp(Attr), true, After + 1};
		}
		const int32 Sclose = FUetkxLexer::FindMatching(S, i);
		if (Sclose == -1 || Sclose >= End)
		{
			Fail(TEXT("UETKX0304"), TEXT("unclosed `{` in spread attribute"), i);
			return {{}, false, End};
		}
		const FString Inner = SliceTrimmed(i + 1, Sclose);
		if (!Inner.StartsWith(TEXT("...")))
		{
			Fail(TEXT("UETKX0300"), TEXT("expected `...spread` or an attribute name"), i);
			return {{}, false, End};
		}
		const int32 Svat = SkipWs(SkipWs(i + 1, Sclose) + 3, Sclose); // first expr char after `...`
		FUetkxAttr Attr;
		Attr.Kind = EUetkxAttrKind::Spread;
		Attr.Value = Inner.Mid(3).TrimStartAndEnd();
		Attr.At = i;
		Attr.Vat = Svat;
		Attr.End = Sclose + 1;
		return {MoveTemp(Attr), true, Sclose + 1};
	}
	const int32 Ns = i;
	while (i < End && IsAttrNameChar(S[i]))
	{
		++i;
	}
	const FString Name = Slice(Ns, i);
	const int32 NameEnd = i;
	if (Name.IsEmpty())
	{
		Fail(TEXT("UETKX0300"), TEXT("unexpected token in attributes"), i);
		return {{}, false, End};
	}
	if (Name.StartsWith(TEXT(".")) || Name.StartsWith(TEXT("-")))
	{
		Fail(
			TEXT("UETKX0300"),
			FString::Printf(TEXT("unexpected `%c` in attributes -- dotted/namespaced tags are not supported"), Name[0]),
			Ns);
		return {{}, false, End};
	}
	i = SkipWs(i, End);
	if (i >= End || S[i] != C_EQ)
	{
		FUetkxAttr Attr; // boolean shorthand
		Attr.Name = Name;
		Attr.Kind = EUetkxAttrKind::Bool;
		Attr.Value = TEXT("true");
		Attr.At = Ns;
		Attr.Vat = -1;
		Attr.End = NameEnd;
		return {MoveTemp(Attr), true, i};
	}
	++i; // past "="
	i = SkipWs(i, End);
	if (i >= End)
	{
		Fail(TEXT("UETKX0303"), FString::Printf(TEXT("missing attribute value for '%s'"), *Name), Ns);
		return {{}, false, End};
	}
	const int32 C = S[i];
	if (C == C_QUOTE || C == C_APOS)
	{
		const int32 Se = FUetkxLexer::SkipNoncodeMarkup(S, i); // markup string skip (same as _skip_string)
		if (Se <= i + 1 || Se > End || S[Se - 1] != C)
		{
			Fail(TEXT("UETKX0300"), FString::Printf(TEXT("unterminated string in attribute '%s'"), *Name), i);
			return {{}, false, End};
		}
		FUetkxAttr Attr;
		Attr.Name = Name;
		Attr.Kind = EUetkxAttrKind::Str;
		Attr.Value = Slice(i + 1, Se - 1);
		Attr.At = Ns;
		Attr.Vat = i + 1;
		Attr.End = Se;
		return {MoveTemp(Attr), true, Se};
	}
	if (C == C_LBRACE)
	{
		const int32 Close = FUetkxLexer::FindMatching(S, i);
		if (Close == -1 || Close >= End)
		{
			Fail(TEXT("UETKX0304"), FString::Printf(TEXT("unclosed `{` in attribute '%s'"), *Name), i);
			return {{}, false, End};
		}
		FUetkxAttr Attr;
		Attr.Name = Name;
		Attr.Kind = EUetkxAttrKind::Expr;
		Attr.Value = SliceTrimmed(i + 1, Close);
		Attr.At = Ns;
		Attr.Vat = SkipWs(i + 1, Close);
		Attr.End = Close + 1;
		return {MoveTemp(Attr), true, Close + 1};
	}
	Fail(TEXT("UETKX0300"), FString::Printf(TEXT("attribute '%s' value must be a string or {expr}"), *Name), i);
	return {{}, false, End};
}

FUetkxMarkup::FNodeResult FUetkxMarkup::ParseText(int32 Start, int32 End)
{
	const TArray<int32>& S = *Src;
	int32 i = Start;
	while (i < End)
	{
		const int32 Tc = S[i];
		if (Tc == C_LT || Tc == C_AT)
		{
			break;
		}
		++i;
	}
	const FString Trimmed = SliceTrimmed(Start, i);
	if (Trimmed.IsEmpty())
	{
		return {nullptr, i}; // whitespace-only collapses to nothing
	}
	TSharedPtr<FUetkxNode> Node = MakeShared<FUetkxNode>();
	Node->Type = EUetkxNodeType::Text;
	Node->At = Start;
	Node->Value = Trimmed;
	return {Node, i};
}

FUetkxMarkup::FNodeResult FUetkxMarkup::ParseDirective(int32 At, int32 End)
{
	const TArray<int32>& S = *Src;
	if (FUetkxLexer::KeywordAt(S, At + 1, TEXT("if")))
	{
		return ParseIf(At, End);
	}
	if (FUetkxLexer::KeywordAt(S, At + 1, TEXT("for")))
	{
		return ParseLoop(At, End, EUetkxNodeType::For, 4);
	}
	if (FUetkxLexer::KeywordAt(S, At + 1, TEXT("while")))
	{
		return ParseLoop(At, End, EUetkxNodeType::While, 6);
	}
	if (FUetkxLexer::KeywordAt(S, At + 1, TEXT("match")))
	{
		return ParseMatch(At, End);
	}
	Fail(TEXT("UETKX0305"), TEXT("unknown @directive"), At);
	return {nullptr, End};
}

FUetkxMarkup::FSpanResult FUetkxMarkup::ReadParen(int32 Index, int32 End)
{
	const TArray<int32>& S = *Src;
	int32 i = SkipWs(Index, End);
	if (i >= End || S[i] != C_LPAREN)
	{
		Fail(TEXT("UETKX2506"), TEXT("directive expects `(...)`"), i);
		return {FString(), End, -1};
	}
	const int32 Close = FUetkxLexer::FindMatching(S, i);
	if (Close == -1 || Close >= End)
	{
		Fail(TEXT("UETKX0304"), TEXT("unclosed `(` in directive"), i);
		return {FString(), End, -1};
	}
	return {SliceTrimmed(i + 1, Close), Close + 1, -1};
}

FUetkxMarkup::FSpanResult FUetkxMarkup::ReadBraceBody(int32 Index, int32 End)
{
	const TArray<int32>& S = *Src;
	int32 i = SkipWs(Index, End);
	if (i >= End || S[i] != C_LBRACE)
	{
		Fail(TEXT("UETKX0303"), TEXT("directive expects `{ ... }` body"), i);
		return {FString(), End, -1};
	}
	// G-01: a directive BODY is markup — FindMatchingMarkup keeps `#` literal and markup comments.
	const int32 Close = FUetkxLexer::FindMatchingMarkup(S, i);
	if (Close == -1 || Close >= End)
	{
		Fail(TEXT("UETKX0304"), TEXT("unclosed `{` directive body"), i);
		return {FString(), End, -1};
	}
	return {Slice(i + 1, Close), Close + 1, i + 1};
}

FUetkxMarkup::FNodeResult FUetkxMarkup::ParseIf(int32 At, int32 End)
{
	const TArray<int32>& S = *Src;
	TSharedPtr<FUetkxNode> Node = MakeShared<FUetkxNode>();
	Node->Type = EUetkxNodeType::If;
	Node->At = At;
	int32 i = At + 3; // past "@if"
	FSpanResult P = ReadParen(i, End);
	if (!ErrCode.IsEmpty())
	{
		return {nullptr, End};
	}
	FSpanResult B = ReadBraceBody(P.Next, End);
	if (!ErrCode.IsEmpty())
	{
		return {nullptr, End};
	}
	Node->Branches.Add({P.Text, B.Text, B.At});
	i = B.Next;
	while (true)
	{
		const int32 K = SkipWs(i, End);
		// the `@` itself must be verified — a commented `elif` must not become a real branch.
		if (K >= End || S[K] != C_AT)
		{
			break;
		}
		if (K + 5 <= End && FUetkxLexer::KeywordAt(S, K + 1, TEXT("elif")))
		{
			FSpanResult Pe = ReadParen(K + 5, End);
			if (!ErrCode.IsEmpty())
			{
				return {nullptr, End};
			}
			FSpanResult Be = ReadBraceBody(Pe.Next, End);
			if (!ErrCode.IsEmpty())
			{
				return {nullptr, End};
			}
			Node->Branches.Add({Pe.Text, Be.Text, Be.At});
			i = Be.Next;
		}
		else if (K + 5 <= End && FUetkxLexer::KeywordAt(S, K + 1, TEXT("else")))
		{
			FSpanResult Bb = ReadBraceBody(K + 5, End);
			if (!ErrCode.IsEmpty())
			{
				return {nullptr, End};
			}
			Node->ElseBody = Bb.Text;
			Node->ElseBodyAt = Bb.At;
			i = Bb.Next;
			break;
		}
		else
		{
			break;
		}
	}
	return {Node, i};
}

FUetkxMarkup::FNodeResult FUetkxMarkup::ParseLoop(int32 At, int32 End, EUetkxNodeType Kind, int32 KeywordLen)
{
	TSharedPtr<FUetkxNode> Node = MakeShared<FUetkxNode>();
	Node->Type = Kind;
	Node->At = At;
	FSpanResult P = ReadParen(At + KeywordLen, End);
	if (!ErrCode.IsEmpty())
	{
		return {nullptr, End};
	}
	FSpanResult B = ReadBraceBody(P.Next, End);
	if (!ErrCode.IsEmpty())
	{
		return {nullptr, End};
	}
	Node->Header = P.Text;
	Node->BodyMarkup = B.Text;
	Node->BodyAt = B.At;
	return {Node, B.Next};
}

FUetkxMarkup::FNodeResult FUetkxMarkup::ParseMatch(int32 At, int32 End)
{
	const TArray<int32>& S = *Src;
	TSharedPtr<FUetkxNode> Node = MakeShared<FUetkxNode>();
	Node->Type = EUetkxNodeType::Match;
	Node->At = At;
	FSpanResult P = ReadParen(At + 6, End); // past "@match"
	if (!ErrCode.IsEmpty())
	{
		return {nullptr, End};
	}
	Node->Subject = P.Text;
	int32 Bi = SkipWs(P.Next, End);
	if (Bi >= End || S[Bi] != C_LBRACE)
	{
		Fail(TEXT("UETKX0303"), TEXT("@match expects `{ ... }` with @case/@default arms"), Bi);
		return {nullptr, End};
	}
	const int32 Bclose = FUetkxLexer::FindMatchingMarkup(S, Bi);
	if (Bclose == -1 || Bclose >= End)
	{
		Fail(TEXT("UETKX0304"), TEXT("unclosed @match body"), Bi);
		return {nullptr, End};
	}
	int32 j = Bi + 1;
	while (j < Bclose)
	{
		j = SkipWs(j, Bclose);
		if (j >= Bclose)
		{
			break;
		}
		if (S[j] == C_AT && FUetkxLexer::KeywordAt(S, j + 1, TEXT("case")))
		{
			FSpanResult Cp = ReadParen(j + 5, Bclose);
			if (!ErrCode.IsEmpty())
			{
				return {nullptr, End};
			}
			FSpanResult Cb = ReadBraceBody(Cp.Next, Bclose);
			if (!ErrCode.IsEmpty())
			{
				return {nullptr, End};
			}
			Node->Cases.Add({Cp.Text, Cb.Text, Cb.At});
			j = Cb.Next;
		}
		else if (S[j] == C_AT && FUetkxLexer::KeywordAt(S, j + 1, TEXT("default")))
		{
			FSpanResult Db = ReadBraceBody(j + 8, Bclose);
			if (!ErrCode.IsEmpty())
			{
				return {nullptr, End};
			}
			Node->DefaultBody = Db.Text;
			Node->DefaultBodyAt = Db.At;
			j = Db.Next;
		}
		else
		{
			Fail(TEXT("UETKX2506"), TEXT("@match body expects @case (...) { } or @default { }"), j);
			return {nullptr, End};
		}
	}
	return {Node, Bclose + 1};
}

// --- helpers ---

int32 FUetkxMarkup::SkipWs(int32 Index, int32 End) const
{
	const TArray<int32>& S = *Src;
	while (Index < End)
	{
		const int32 C = S[Index];
		if (C != C_SPACE && C != C_TAB && C != C_NL && C != C_CR)
		{
			break;
		}
		++Index;
	}
	return Index;
}

bool FUetkxMarkup::IsTagChar(int32 C)
{
	return C == '_' || (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9');
}

bool FUetkxMarkup::IsAttrNameChar(int32 C)
{
	return IsTagChar(C) || C == C_DASH || C == C_DOT;
}

int32 FUetkxMarkup::LineOf(int32 Index) const
{
	int32 Lo = 0;
	int32 Hi = LineStarts.Num() - 1;
	while (Lo < Hi)
	{
		const int32 Mid = (Lo + Hi + 1) >> 1;
		if (LineStarts[Mid] <= Index)
		{
			Lo = Mid;
		}
		else
		{
			Hi = Mid - 1;
		}
	}
	return Lo + 1;
}

FString FUetkxMarkup::Slice(int32 Start, int32 End) const
{
	return FUetkxLexer::FromCodePoints(*Src, Start, End - Start);
}

FString FUetkxMarkup::SliceTrimmed(int32 Start, int32 End) const
{
	return Slice(Start, End).TrimStartAndEnd();
}

int32 FUetkxMarkup::FindSeq(const TCHAR* Seq, int32 From) const
{
	const TArray<int32>& S = *Src;
	const int32 SeqLen = FCString::Strlen(Seq);
	for (int32 i = FMath::Max(From, 0); i + SeqLen <= S.Num(); ++i)
	{
		bool bMatch = true;
		for (int32 k = 0; k < SeqLen; ++k)
		{
			if (S[i + k] != static_cast<int32>(Seq[k]))
			{
				bMatch = false;
				break;
			}
		}
		if (bMatch)
		{
			return i;
		}
	}
	return -1;
}
