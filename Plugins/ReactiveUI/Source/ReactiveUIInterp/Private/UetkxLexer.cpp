// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxLexer.h"

namespace
{
	constexpr int32 C_NL = '\n';
	constexpr int32 C_CR = '\r';
	constexpr int32 C_TAB = '\t';
	constexpr int32 C_SPACE = ' ';
	constexpr int32 C_QUOTE = '"';
	constexpr int32 C_APOS = '\'';
	constexpr int32 C_HASH = '#';
	constexpr int32 C_SLASH = '/';
	constexpr int32 C_STAR = '*';
	constexpr int32 C_BSLASH = '\\';
	constexpr int32 C_LT = '<';
	constexpr int32 C_BANG = '!';
	constexpr int32 C_DASH = '-';
	constexpr int32 C_AT = '@';
	constexpr int32 C_LPAREN = '(';
	constexpr int32 C_RPAREN = ')';
	constexpr int32 C_LBRACE = '{';
	constexpr int32 C_RBRACE = '}';
	constexpr int32 C_LBRACKET = '[';
	constexpr int32 C_RBRACKET = ']';

	/** Can code `c` end a value/operand? (Then a following prefix char is NOT a prefix.) */
	bool IsValueEnd(int32 C)
	{
		return FUetkxLexer::IsIdentCode(C) || C == C_RPAREN || C == C_RBRACKET || C == C_QUOTE || C == C_APOS;
	}

	/** If Index starts a C++ string-literal PREFIX (L, u, U, u8 — optionally followed by R),
	 *  return the index of the quote it prefixes; else -1. Token-start rule applies. */
	int32 PrefixedQuoteAt(const TArray<int32>& Src, int32 Index, bool& bOutRaw)
	{
		bOutRaw = false;
		const int32 N = Src.Num();
		if (Index > 0 && FUetkxLexer::IsIdentCode(Src[Index - 1]))
		{
			return -1; // mid-identifier: FOOL"(" is not a prefix
		}
		int32 i = Index;
		const int32 C = Src[i];
		if (C == 'L' || C == 'U')
		{
			++i;
		}
		else if (C == 'u')
		{
			++i;
			if (i < N && Src[i] == '8')
			{
				++i;
			}
		}
		if (i < N && Src[i] == 'R')
		{
			bOutRaw = true;
			++i;
		}
		if (i > Index && i < N && Src[i] == C_QUOTE)
		{
			return i;
		}
		return -1;
	}
} // namespace

TArray<int32> FUetkxLexer::ToCodePoints(FStringView Source)
{
	TArray<int32> Out;
	Out.Reserve(Source.Len());
	for (int32 i = 0; i < Source.Len(); ++i)
	{
		const TCHAR C = Source[i];
		if (StringConv::IsHighSurrogate(C) && i + 1 < Source.Len() && StringConv::IsLowSurrogate(Source[i + 1]))
		{
			Out.Add(StringConv::EncodeSurrogate(C, Source[i + 1]));
			++i;
		}
		else
		{
			Out.Add(static_cast<int32>(C));
		}
	}
	return Out;
}

FString FUetkxLexer::FromCodePoints(const TArray<int32>& Src, int32 Start, int32 Count)
{
	FString Out;
	const int32 End = FMath::Min(Start + Count, Src.Num());
	for (int32 i = FMath::Max(Start, 0); i < End; ++i)
	{
		const uint32 Cp = static_cast<uint32>(Src[i]);
		if (Cp <= 0xFFFF)
		{
			Out.AppendChar(static_cast<TCHAR>(Cp));
		}
		else
		{
			const uint32 V = Cp - 0x10000u;
			Out.AppendChar(static_cast<TCHAR>(0xD800u + (V >> 10)));
			Out.AppendChar(static_cast<TCHAR>(0xDC00u + (V & 0x3FFu)));
		}
	}
	return Out;
}

bool FUetkxLexer::IsPreprocessorLine(const TArray<int32>& Src, int32 Index)
{
	// `#` counts as a preprocessor line iff it is the first non-ws char on its line.
	for (int32 k = Index - 1; k >= 0; --k)
	{
		const int32 C = Src[k];
		if (C == C_NL)
		{
			return true;
		}
		if (C != C_SPACE && C != C_TAB && C != C_CR)
		{
			return false;
		}
	}
	return true; // start of file
}

int32 FUetkxLexer::SkipNoncode(const TArray<int32>& Src, int32 Index)
{
	const int32 N = Src.Num();
	if (Index >= N)
	{
		return Index;
	}
	const int32 C = Src[Index];
	// // line comment
	if (C == C_SLASH && Index + 1 < N && Src[Index + 1] == C_SLASH)
	{
		int32 j = Index + 2;
		while (j < N && Src[j] != C_NL)
		{
			++j;
		}
		return j;
	}
	// /* block comment */
	if (C == C_SLASH && Index + 1 < N && Src[Index + 1] == C_STAR)
	{
		int32 j = Index + 2;
		while (j + 1 < N)
		{
			if (Src[j] == C_STAR && Src[j + 1] == C_SLASH)
			{
				return j + 2;
			}
			++j;
		}
		return N;
	}
	// preprocessor line (skipped as a line, honoring trailing-backslash continuations)
	if (C == C_HASH && IsPreprocessorLine(Src, Index))
	{
		int32 j = Index + 1;
		while (j < N)
		{
			if (Src[j] == C_NL)
			{
				// continuation? last non-CR char before NL is a backslash
				int32 k = j - 1;
				if (k >= 0 && Src[k] == C_CR)
				{
					--k;
				}
				if (k >= Index && Src[k] == C_BSLASH)
				{
					++j;
					continue;
				}
				return j;
			}
			++j;
		}
		return N;
	}
	// prefixed string/char (L"..", u8"..", LR"(..)", ...)
	bool bRaw = false;
	const int32 PrefQuote = PrefixedQuoteAt(Src, Index, bRaw);
	if (PrefQuote != -1)
	{
		return bRaw ? SkipRawString(Src, PrefQuote) : SkipString(Src, PrefQuote);
	}
	// R"delim(...)delim"
	if (C == 'R' && Index + 1 < N && Src[Index + 1] == C_QUOTE && (Index == 0 || !IsIdentCode(Src[Index - 1])))
	{
		return SkipRawString(Src, Index + 1);
	}
	// plain string / char literal
	if (C == C_QUOTE || C == C_APOS)
	{
		// token-start rule: an apostrophe right after a digit is a C++14 digit separator
		// (1'000'000) — not a char literal.
		if (C == C_APOS && Index > 0 && Src[Index - 1] >= '0' && Src[Index - 1] <= '9')
		{
			return Index;
		}
		return SkipString(Src, Index);
	}
	return Index;
}

int32 FUetkxLexer::SkipString(const TArray<int32>& Src, int32 QuoteIndex)
{
	const int32 N = Src.Num();
	const int32 Q = Src[QuoteIndex];
	// Markup attribute strings may be triple-quoted (family grammar); C++ has no triple form
	// but recognizing it here is harmless for code (`"""` is empty string + quote anyway) and
	// keeps the markup-side skipper byte-compatible with the shared corpus.
	if (QuoteIndex + 2 < N && Src[QuoteIndex + 1] == Q && Src[QuoteIndex + 2] == Q)
	{
		int32 j = QuoteIndex + 3;
		while (j < N)
		{
			if (Src[j] == C_BSLASH)
			{
				j += 2;
				continue;
			}
			if (Src[j] == Q && j + 2 < N && Src[j + 1] == Q && Src[j + 2] == Q)
			{
				return j + 3;
			}
			++j;
		}
		return N;
	}
	int32 k = QuoteIndex + 1;
	while (k < N)
	{
		const int32 Ch = Src[k];
		if (Ch == C_BSLASH)
		{
			k += 2;
			continue;
		}
		if (Ch == Q)
		{
			return k + 1;
		}
		if (Ch == C_NL) // single-line literals don't span newlines (family rule)
		{
			return k;
		}
		++k;
	}
	return N;
}

int32 FUetkxLexer::SkipRawString(const TArray<int32>& Src, int32 RQuoteIndex)
{
	// RQuoteIndex points at the OPENING QUOTE of R"delim( ... )delim".
	const int32 N = Src.Num();
	int32 j = RQuoteIndex + 1;
	TArray<int32> Delim;
	while (j < N && Src[j] != C_LPAREN && Src[j] != C_NL && (j - RQuoteIndex) <= 17)
	{
		Delim.Add(Src[j]);
		++j;
	}
	if (j >= N || Src[j] != C_LPAREN)
	{
		return SkipString(Src, RQuoteIndex); // malformed raw literal — degrade to plain string
	}
	++j; // past '('
	while (j < N)
	{
		if (Src[j] == C_RPAREN)
		{
			int32 k = j + 1;
			int32 d = 0;
			while (d < Delim.Num() && k < N && Src[k] == Delim[d])
			{
				++k;
				++d;
			}
			if (d == Delim.Num() && k < N && Src[k] == C_QUOTE)
			{
				return k + 1;
			}
		}
		++j;
	}
	return N;
}

int32 FUetkxLexer::SkipNoncodeMarkup(const TArray<int32>& Src, int32 Index)
{
	const int32 N = Src.Num();
	if (Index >= N)
	{
		return Index;
	}
	const int32 C = Src[Index];
	if (C == C_SLASH && Index + 1 < N && Src[Index + 1] == C_SLASH)
	{
		int32 j = Index + 2;
		while (j < N && Src[j] != C_NL)
		{
			++j;
		}
		return j;
	}
	if (C == C_SLASH && Index + 1 < N && Src[Index + 1] == C_STAR)
	{
		int32 j = Index + 2;
		while (j + 1 < N)
		{
			if (Src[j] == C_STAR && Src[j + 1] == C_SLASH)
			{
				return j + 2;
			}
			++j;
		}
		return N;
	}
	if (C == C_LT && Index + 3 < N && Src[Index + 1] == C_BANG && Src[Index + 2] == C_DASH && Src[Index + 3] == C_DASH)
	{
		int32 j = Index + 4;
		while (j + 2 < N)
		{
			if (Src[j] == C_DASH && Src[j + 1] == C_DASH && Src[j + 2] == '>')
			{
				return j + 3;
			}
			++j;
		}
		return N;
	}
	if (C == C_QUOTE || C == C_APOS)
	{
		return SkipString(Src, Index);
	}
	return Index;
}

int32 FUetkxLexer::FindMatching(const TArray<int32>& Src, int32 OpenIndex)
{
	const int32 N = Src.Num();
	TArray<int32> Stack;
	int32 i = OpenIndex;
	while (i < N)
	{
		const int32 j = SkipNoncode(Src, i);
		if (j != i)
		{
			i = j;
			continue;
		}
		const int32 C = Src[i];
		if (C == C_LPAREN || C == C_LBRACE || C == C_LBRACKET)
		{
			Stack.Push(C);
		}
		else if (C == C_RPAREN || C == C_RBRACE || C == C_RBRACKET)
		{
			if (Stack.IsEmpty())
			{
				return -1;
			}
			const int32 Top = Stack.Pop(EAllowShrinking::No);
			if ((C == C_RPAREN && Top != C_LPAREN) || (C == C_RBRACE && Top != C_LBRACE) ||
				(C == C_RBRACKET && Top != C_LBRACKET))
			{
				return -1;
			}
			if (Stack.IsEmpty())
			{
				return i;
			}
		}
		++i;
	}
	return -1;
}

bool FUetkxLexer::IsMarkupLine(const TArray<int32>& Src, int32 LineStart, int32 LineEnd)
{
	int32 k = LineStart;
	while (k < LineEnd)
	{
		const int32 C = Src[k];
		if (C == C_SPACE || C == C_TAB || C == C_CR)
		{
			++k;
			continue;
		}
		if (C == C_LT || C == C_LBRACE)
		{
			return true;
		}
		if (C == C_SLASH && k + 1 < LineEnd)
		{
			const int32 Nx = Src[k + 1];
			return Nx == C_SLASH || Nx == C_STAR;
		}
		if (C == C_AT)
		{
			return KeywordAt(Src, k + 1, TEXT("if")) || KeywordAt(Src, k + 1, TEXT("elif")) ||
				   KeywordAt(Src, k + 1, TEXT("else")) || KeywordAt(Src, k + 1, TEXT("for")) ||
				   KeywordAt(Src, k + 1, TEXT("while")) || KeywordAt(Src, k + 1, TEXT("match")) ||
				   KeywordAt(Src, k + 1, TEXT("case")) || KeywordAt(Src, k + 1, TEXT("default"));
		}
		return false;
	}
	return false;
}

int32 FUetkxLexer::FindMatchingMarkup(const TArray<int32>& Src, int32 OpenIndex)
{
	enum EMode : uint8
	{
		Body,
		Markup,
		Code
	};
	const int32 N = Src.Num();
	TArray<int32> Delims;
	TArray<uint8> Modes;
	Delims.Push(Src[OpenIndex]);
	Modes.Push(Src[OpenIndex] == C_LBRACE ? Body : Markup);
	bool bExpectBody = false;
	bool bExpectMarkupParen = false;

	int32 LineStart = 0;
	for (int32 k = OpenIndex - 1; k >= 0; --k)
	{
		if (Src[k] == C_NL)
		{
			LineStart = k + 1;
			break;
		}
	}
	int32 LineEnd = N;
	for (int32 k = OpenIndex; k < N; ++k)
	{
		if (Src[k] == C_NL)
		{
			LineEnd = k;
			break;
		}
	}
	bool bLineMarkup = IsMarkupLine(Src, LineStart, LineEnd);

	int32 i = OpenIndex + 1;
	while (i < N)
	{
		while (i > LineEnd)
		{
			LineStart = LineEnd + 1;
			LineEnd = N;
			for (int32 k = LineStart; k < N; ++k)
			{
				if (Src[k] == C_NL)
				{
					LineEnd = k;
					break;
				}
			}
			bLineMarkup = IsMarkupLine(Src, LineStart, LineEnd);
		}
		const uint8 Mode = Modes.Last();
		const bool bInCode = Mode == Code;
		const bool bMarkupLexis = Mode == Markup || (Mode == Body && bLineMarkup);
		const int32 j = bMarkupLexis ? SkipNoncodeMarkup(Src, i) : SkipNoncode(Src, i);
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
		if (!bInCode)
		{
			if (C == C_AT && KeywordAt(Src, i + 1, TEXT("else")))
			{
				bExpectBody = true;
				bExpectMarkupParen = false;
				i += 5;
				continue;
			}
			if (C == C_AT && KeywordAt(Src, i + 1, TEXT("default")))
			{
				bExpectBody = true;
				bExpectMarkupParen = false;
				i += 8;
				continue;
			}
			if (C == 'r' && KeywordAt(Src, i, TEXT("return")))
			{
				bExpectMarkupParen = true;
				bExpectBody = false;
				i += 6;
				continue;
			}
			if (C == C_LPAREN)
			{
				Delims.Push(C);
				Modes.Push(bExpectMarkupParen ? Markup : Code);
				bExpectBody = false;
				bExpectMarkupParen = false;
				++i;
				continue;
			}
			if (C == C_LBRACE)
			{
				Delims.Push(C);
				Modes.Push(bExpectBody ? Body : Code);
				bExpectBody = false;
				bExpectMarkupParen = false;
				++i;
				continue;
			}
			if (C == C_LBRACKET)
			{
				Delims.Push(C);
				Modes.Push(Mode); // inherit
				bExpectBody = false;
				bExpectMarkupParen = false;
				++i;
				continue;
			}
			if (C == C_RPAREN || C == C_RBRACE || C == C_RBRACKET)
			{
				if (Delims.IsEmpty())
				{
					return -1;
				}
				const int32 Top = Delims.Pop(EAllowShrinking::No);
				Modes.Pop(EAllowShrinking::No);
				if ((C == C_RPAREN && Top != C_LPAREN) || (C == C_RBRACE && Top != C_LBRACE) ||
					(C == C_RBRACKET && Top != C_LBRACKET))
				{
					return -1;
				}
				if (Delims.IsEmpty())
				{
					return i;
				}
				++i;
				continue;
			}
			bExpectBody = false;
			bExpectMarkupParen = false;
			++i;
			continue;
		}
		else
		{
			if (C == C_LPAREN || C == C_LBRACE || C == C_LBRACKET)
			{
				Delims.Push(C);
				Modes.Push(Code);
				++i;
				continue;
			}
			if (C == C_RPAREN || C == C_RBRACE || C == C_RBRACKET)
			{
				if (Delims.IsEmpty())
				{
					return -1;
				}
				const int32 Top = Delims.Pop(EAllowShrinking::No);
				Modes.Pop(EAllowShrinking::No);
				if ((C == C_RPAREN && Top != C_LPAREN) || (C == C_RBRACE && Top != C_LBRACE) ||
					(C == C_RBRACKET && Top != C_LBRACKET))
				{
					return -1;
				}
				if (Delims.IsEmpty())
				{
					return i;
				}
				if (Modes.Last() != Code)
				{
					bExpectBody = (Top == C_LPAREN); // a header close re-arms "next { is a body"
				}
				++i;
				continue;
			}
			++i;
			continue;
		}
	}
	return -1;
}

bool FUetkxLexer::KeywordAt(const TArray<int32>& Src, int32 Index, const TCHAR* Word)
{
	const int32 N = Src.Num();
	const int32 WordLen = FCString::Strlen(Word);
	if (Index < 0 || Index + WordLen > N)
	{
		return false;
	}
	for (int32 k = 0; k < WordLen; ++k)
	{
		if (Src[Index + k] != static_cast<int32>(Word[k]))
		{
			return false;
		}
	}
	if (Index > 0 && IsIdentCode(Src[Index - 1]))
	{
		return false;
	}
	if (Index + WordLen < N && IsIdentCode(Src[Index + WordLen]))
	{
		return false;
	}
	return true;
}
