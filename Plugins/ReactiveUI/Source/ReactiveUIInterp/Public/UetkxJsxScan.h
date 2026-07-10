// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxJsxScan — finds markup nested INSIDE an embedded C++ expression, e.g.
// `bOpen && <Panel/>`, `Cond ? <A/> : <B/>`, `Items.Map([](auto It){ return <Row/>; })`.
// Port of guitkx_jsx_scan.gd with the C++ operator set.
//
// The hard problem is telling a markup `<` from a less-than operator. Like the family we do
// NOT general-disambiguate — a POSITION-GATED whitelist: a `<` begins markup ONLY when it
// follows (ws-skipped) a boundary token that can only precede an expression, AND the char
// after `<` is a tag-name start (letter/underscore) or `>` (fragment). `a < b` never matches
// because no boundary precedes it. Boundaries (C++ set): start-of-expression, `(`, `[`, `,`,
// simple `=`, `&&`, `||`, `?`, ternary `:` (not `::`), `return`, `else`.
//
// All string/comment skipping routes through FUetkxLexer; this module adds NO lexis.

#pragma once

#include "CoreMinimal.h"

/** One markup range inside an expression. End == -1 marks UNBALANCED markup (the caller must
 *  diagnose — after a boundary, `<Tag` is never valid C++ either; family T1.2). Op is "" or
 *  the short-circuit operator ("&&" / "||") that must desugar to a conditional at emit. */
struct REACTIVEUIINTERP_API FUetkxMarkupRange
{
	int32 Start = -1;
	int32 End = -1;
	FString Op;
	int32 OpPos = -1;
};

class REACTIVEUIINTERP_API FUetkxJsxScan
{
public:
	/** Scan [Start, End) of Src for embedded markup ranges (sorted, non-overlapping). */
	static TArray<FUetkxMarkupRange> FindMarkupRanges(const TArray<int32>& Src, int32 Start, int32 End);

	/** True when a markup element/fragment opens at Index (`<` + tag-start or `>`). */
	static bool MarkupAt(const TArray<int32>& Src, int32 Index, int32 End);

	/** From a `<` at Open, the index just past the outermost element close; -1 unbalanced. */
	static int32 FindElementEnd(const TArray<int32>& Src, int32 Open, int32 End);

private:
	static int32 Try(const TArray<int32>& Src, int32 After, int32 End, const TCHAR* Op, int32 OpPos,
					 TArray<FUetkxMarkupRange>& Out, int32 Fallback);
	static void ScanOpenTag(const TArray<int32>& Src, int32 Lt, int32 End, int32& OutGt, bool& bOutSelfClosing);
	static int32 SkipWs(const TArray<int32>& Src, int32 Index, int32 End);
	static bool IsIdentBoundary(const TArray<int32>& Src, int32 Index);
	static bool IsSimpleAssign(const TArray<int32>& Src, int32 Index, int32 End);
};
