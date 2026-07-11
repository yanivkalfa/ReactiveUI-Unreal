// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxLexer — the load-bearing scanner for the .uetkx toolchain (lives in Interp per D-27: the
// dev-build interpreter parses .uetkx at runtime; the UncookedOnly Toolchain consumes it): the C++-LEXIS port of the
// family's string/char/comment-skipping state machine (guitkx_lexer.gd carries the GDScript
// lexis, uitkx the C# lexis; the MARKUP grammar is byte-compatible across all three).
// Everything that balances delimiters (the `return (...)` markup window, `{expr}` holes,
// `<Tag>` children) MUST route through SkipNoncode so quotes/comments/braces inside embedded
// C++ never confuse balancing.
//
// C++ lexis handled by SkipNoncode: `//` line comments, `/* */` block comments, preprocessor
// LINES (`#` first-non-ws on the line — skipped to EOL, honoring `\` line continuations),
// "…" strings and '…' char literals with escapes, R"delim(…)delim" raw strings, and the
// L / u8 / u / U prefixes (alone or before R) — prefix only at a TOKEN START, never after a
// value (so `a & b` and `x % y` stay operators… C++ has no &"" forms, but the token-start
// discipline is kept for the identifier-adjacency rule: `LR"(x)"` yes, `FOOL"(x)"` no).
//
// Markup lexis (SkipNoncodeMarkup) is the family's, verbatim: `//`, `/* */`, `<!-- -->`
// comments and quoted strings; `#` is literal text in markup.
//
// UNITS: all indices are Unicode CODE POINTS (D-18 — sidecar offsets and corpus cases are
// code points; the corpus includes non-BMP cases). Callers convert once via ToCodePoints.
// Contract corpus: ide-extensions/lsp-server/test-fixtures/uetkx-scanner-cases.json — run by
// the ReactiveUI.Uetkx.Scanner automation test AND lsp-server's node --test (D-22).

#pragma once

#include "CoreMinimal.h"

class REACTIVEUIINTERP_API FUetkxLexer
{
public:
	/** Decode UTF-16 FString → code points (surrogate pairs collapse to one element). */
	static TArray<int32> ToCodePoints(FStringView Source);

	/** Re-encode a code-point slice back to FString (diagnostics/snippets). */
	static FString FromCodePoints(const TArray<int32>& Src, int32 Start, int32 Count);

	/** If `i` sits at the start of a C++ comment/preprocessor-line/string/char literal, skip
	 *  the whole token and return the index just past it; else return `i` unchanged. */
	static int32 SkipNoncode(const TArray<int32>& Src, int32 Index);

	/** Markup-lexis skip: `//`, `/* *​/`, `<!-- -->`, quoted strings; `#` is literal. */
	static int32 SkipNoncodeMarkup(const TArray<int32>& Src, int32 Index);

	/** Matching close delimiter for the ()/{}/[] at OpenIndex, C++ lexis. -1 if unbalanced. */
	static int32 FindMatching(const TArray<int32>& Src, int32 OpenIndex);

	/** Mode-aware counterpart for spans mixing MARKUP with embedded C++ (component/directive
	 *  bodies, `return (...)` windows, `{expr}` holes) — the family's G-01/G-23 semantics:
	 *  body levels line-classify (markup line vs C++ prelude line), `return (` opens a MARKUP
	 *  level, other `(`/naked `{` open CODE levels, `[` inherits. -1 if unbalanced. */
	static int32 FindMatchingMarkup(const TArray<int32>& Src, int32 OpenIndex);

	/** True when `Word` occurs at Index bounded by non-identifier chars (a real keyword). */
	static bool KeywordAt(const TArray<int32>& Src, int32 Index, const TCHAR* Word);

	static bool IsIdentCode(int32 C)
	{
		return C == '_' || (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9');
	}

	/** Code-point offsets of each line head (index 0 = line 1). LineOf maps an offset to its 1-based
	 *  source line — the basis for the `#line` directives that make VS breakpoints in a .uetkx bind
	 *  to the generated `.inl` (M7). */
	static TArray<int32> BuildLineStarts(const TArray<int32>& Src);
	static int32 LineOf(const TArray<int32>& LineStarts, int32 Offset);

private:
	static int32 SkipString(const TArray<int32>& Src, int32 QuoteIndex);
	static int32 SkipRawString(const TArray<int32>& Src, int32 RIndex);
	static bool IsMarkupLine(const TArray<int32>& Src, int32 LineStart, int32 LineEnd);
	static bool IsPreprocessorLine(const TArray<int32>& Src, int32 Index);
};
