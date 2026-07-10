// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxJsxScan.h"

#include "UetkxLexer.h"

#include "UetkxChars.h"

using namespace UetkxChars;

namespace
{
} // namespace

TArray<FUetkxMarkupRange> FUetkxJsxScan::FindMarkupRanges(const TArray<int32>& Src, int32 Start, int32 End)
{
	TArray<FUetkxMarkupRange> Out;
	int32 i = Start;
	// markup at the very start of the expression (e.g. an attr value that IS markup)
	const int32 S0 = SkipWs(Src, Start, End);
	if (MarkupAt(Src, S0, End))
	{
		const int32 E0 = FindElementEnd(Src, S0, End);
		if (E0 == -1)
		{
			Out.Add({S0, -1, FString(), Start});
			return Out;
		}
		Out.Add({S0, E0, FString(), Start});
		i = E0;
	}
	while (i < End)
	{
		const int32 j = FUetkxLexer::SkipNoncode(Src, i);
		if (j != i)
		{
			i = j;
			continue;
		}
		const int32 C = Src[i];
		if (C == C_LPAREN || C == C_LBRACKET || C == C_COMMA || C == C_QUESTION)
		{
			i = Try(Src, i + 1, End, TEXT(""), i, Out, i + 1);
			continue;
		}
		if (C == C_EQ && IsSimpleAssign(Src, i, End))
		{
			i = Try(Src, i + 1, End, TEXT(""), i, Out, i + 1);
			continue;
		}
		if (C == C_AMP && i + 1 < End && Src[i + 1] == C_AMP)
		{
			i = Try(Src, i + 2, End, TEXT("&&"), i, Out, i + 2);
			continue;
		}
		if (C == C_PIPE && i + 1 < End && Src[i + 1] == C_PIPE)
		{
			i = Try(Src, i + 2, End, TEXT("||"), i, Out, i + 2);
			continue;
		}
		// ternary else-branch `:` (never `::`, never preceded by `:`)
		if (C == C_COLON && !(i + 1 < End && Src[i + 1] == C_COLON) && !(i > 0 && Src[i - 1] == C_COLON))
		{
			i = Try(Src, i + 1, End, TEXT(""), i, Out, i + 1);
			continue;
		}
		if ((C == 'r' || C == 'e') && IsIdentBoundary(Src, i))
		{
			// keyword boundaries: return / else
			if (FUetkxLexer::KeywordAt(Src, i, TEXT("return")))
			{
				i = Try(Src, i + 6, End, TEXT(""), i, Out, i + 6);
				continue;
			}
			if (FUetkxLexer::KeywordAt(Src, i, TEXT("else")))
			{
				i = Try(Src, i + 4, End, TEXT(""), i, Out, i + 4);
				continue;
			}
		}
		++i;
	}
	return Out;
}

int32 FUetkxJsxScan::Try(const TArray<int32>& Src, int32 After, int32 End, const TCHAR* Op, int32 OpPos,
						 TArray<FUetkxMarkupRange>& Out, int32 Fallback)
{
	const int32 P = SkipWs(Src, After, End);
	if (MarkupAt(Src, P, End))
	{
		const int32 E = FindElementEnd(Src, P, End);
		if (E == -1)
		{
			Out.Add({P, -1, Op, OpPos});
			return End;
		}
		Out.Add({P, E, Op, OpPos});
		return E;
	}
	return Fallback;
}

bool FUetkxJsxScan::MarkupAt(const TArray<int32>& Src, int32 Index, int32 End)
{
	if (Index >= End || Src[Index] != C_LT)
	{
		return false;
	}
	if (Index + 1 >= End)
	{
		return false;
	}
	const int32 C = Src[Index + 1];
	return C == C_GT || C == '_' || (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z');
}

int32 FUetkxJsxScan::FindElementEnd(const TArray<int32>& Src, int32 Open, int32 End)
{
	int32 Depth = 0;
	int32 i = Open;
	while (i < End)
	{
		// Markup lexis over tag names/text/attributes; `{…}` holes route through FindMatching
		// (C++ lexis) below.
		const int32 j = FUetkxLexer::SkipNoncodeMarkup(Src, i);
		if (j != i)
		{
			i = j;
			continue;
		}
		const int32 C = Src[i];
		if (C == C_LBRACE)
		{
			const int32 Close = FUetkxLexer::FindMatching(Src, i); // skip an attr/child {…} hole whole
			if (Close == -1 || Close >= End)
			{
				return -1;
			}
			i = Close + 1;
			continue;
		}
		if (C == C_LT)
		{
			if (i + 1 < End && Src[i + 1] == C_SLASH)
			{
				// closing tag </...> or fragment </>
				--Depth;
				int32 Gt = -1;
				for (int32 k = i; k < End; ++k)
				{
					if (Src[k] == C_GT)
					{
						Gt = k;
						break;
					}
				}
				if (Gt == -1)
				{
					return -1;
				}
				i = Gt + 1;
				if (Depth == 0)
				{
					return i;
				}
				continue;
			}
			if (i + 1 < End && Src[i + 1] == C_GT)
			{
				++Depth; // fragment open <>
				i += 2;
				continue;
			}
			if (MarkupAt(Src, i, End))
			{
				int32 Gt = -1;
				bool bSelfClosing = false;
				ScanOpenTag(Src, i, End, Gt, bSelfClosing);
				if (Gt == -1)
				{
					return -1;
				}
				i = Gt + 1;
				if (bSelfClosing)
				{
					if (Depth == 0)
					{
						return i;
					}
				}
				else
				{
					++Depth;
				}
				continue;
			}
		}
		++i;
	}
	return -1;
}

void FUetkxJsxScan::ScanOpenTag(const TArray<int32>& Src, int32 Lt, int32 End, int32& OutGt, bool& bOutSelfClosing)
{
	OutGt = -1;
	bOutSelfClosing = false;
	int32 i = Lt + 1;
	while (i < End && FUetkxLexer::IsIdentCode(Src[i]))
	{
		++i; // tag name
	}
	while (i < End)
	{
		const int32 j = FUetkxLexer::SkipNoncodeMarkup(Src, i);
		if (j != i)
		{
			i = j;
			continue;
		}
		const int32 C = Src[i];
		if (C == C_LBRACE)
		{
			const int32 Close = FUetkxLexer::FindMatching(Src, i);
			if (Close == -1 || Close >= End)
			{
				return;
			}
			i = Close + 1;
			continue;
		}
		if (C == C_SLASH && i + 1 < End && Src[i + 1] == C_GT)
		{
			OutGt = i + 1;
			bOutSelfClosing = true;
			return;
		}
		if (C == C_GT)
		{
			OutGt = i;
			return;
		}
		++i;
	}
}

int32 FUetkxJsxScan::SkipWs(const TArray<int32>& Src, int32 Index, int32 End)
{
	while (Index < End)
	{
		const int32 C = Src[Index];
		if (C != C_SPACE && C != C_TAB && C != C_NL && C != C_CR)
		{
			break;
		}
		++Index;
	}
	return Index;
}

bool FUetkxJsxScan::IsIdentBoundary(const TArray<int32>& Src, int32 Index)
{
	return Index == 0 || !FUetkxLexer::IsIdentCode(Src[Index - 1]);
}

bool FUetkxJsxScan::IsSimpleAssign(const TArray<int32>& Src, int32 Index, int32 End)
{
	// Not ==, <=, >=, !=, +=, -=, *=, /=, %=, &=, |=, ^= (and never the 2nd `=` of `==`).
	if (Index + 1 < End && Src[Index + 1] == C_EQ)
	{
		return false;
	}
	if (Index == 0)
	{
		return true;
	}
	const int32 P = Src[Index - 1];
	return !(P == C_EQ || P == C_BANG || P == C_LT || P == C_GT || P == C_PLUS || P == C_DASH || P == C_STAR ||
			 P == C_SLASH || P == C_PERCENT || P == C_AMP || P == C_PIPE || P == C_CARET);
}
