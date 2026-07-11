// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxFileScan — the .uetkx FILE grammar: preamble directives, `component Name(params) {}`
// declarations, the body splitter (setup vs the LAST top-level markup `return ( ... )` —
// family T1.4 useLastReturn parity), and the hook-call sequence (order + kinds) that both the
// compiler (bakes __RUI_HOOK_SIG) and the Phase 4 interpreter (computes live) hash for the
// deliberate state-reset-on-hook-shape-change semantics. Lives in Interp per D-27.
//
// Params use the family decl grammar: `Name: Type = Default, ...` (types are C++ type names).
// Diagnostics carry family numbers (2100/2101/2102/0108/0303/0304); UE-only limits use the
// UETKX3xxx space (D-18).

#pragma once

#include "CoreMinimal.h"
#include "UetkxMarkup.h"

struct REACTIVEUIINTERP_API FUetkxDiag
{
	FString Code;
	int32 Severity = 0; // 0 err / 1 warn / 2 hint (sidecar schema v2)
	FString Message;
	int32 Offset = -1; // code points into the ORIGINAL source; -1 = whole file
	int32 Length = 1;
};

struct REACTIVEUIINTERP_API FUetkxParam
{
	FString Name;
	FString Type;
	FString Default; // empty = required
};

struct REACTIVEUIINTERP_API FUetkxComponentDecl
{
	FString Name;
	int32 At = -1; // the `component` keyword offset
	int32 NameAt = -1;
	TArray<FUetkxParam> Params;
	FString Setup; // body text before the chosen markup return (verbatim C++)
	int32 SetupAt = -1;
	FString Body; // entire body text (formatter/diagnostics)
	int32 BodyAt = -1;
	TSharedPtr<FUetkxNode> Root; // the single render root
	TArray<TSharedPtr<FUetkxNode>> WindowNodes;
	TArray<FString> HookCalls; // ordered hook kinds in setup ("UseState", "UseEffect", ...)
	int32 Next = -1;		   // just past the closing brace
};

struct REACTIVEUIINTERP_API FUetkxFileScanResult
{
	TArray<FString> PreambleIncludes; // verbatim `#include ...` lines from the preamble
	TArray<FUetkxComponentDecl> Components;
	TArray<FUetkxDiag> Diags;

	bool HasError() const
	{
		for (const FUetkxDiag& Diag : Diags)
		{
			if (Diag.Severity == 0)
			{
				return true;
			}
		}
		return false;
	}
};

/** The (last) top-level markup `return ( ... )` inside a body — the ONE splitter shared by
 *  the file scan, the emitter, and the formatter (they must never disagree on the window). */
struct REACTIVEUIINTERP_API FUetkxSplitReturn
{
	bool bOk = false;
	int32 ReturnAt = -1; // offset of `return`
	int32 MStart = -1;	 // first char inside `(`
	int32 MEnd = -1;	 // index OF the closing `)`
	int32 AfterParen = -1;
};

class REACTIVEUIINTERP_API FUetkxFileScan
{
public:
	/** The 23 hook names (auto-prefix + signature scanning). */
	static const TArray<FString>& HookNames();

	/** FNV-1a (2166136261/16777619, 32-bit) over the ordered hook-kind sequence. */
	static uint32 HookSignature(const TArray<FString>& HookCalls);

	/** Scan a whole .uetkx source: preamble + every component declaration. */
	static FUetkxFileScanResult Scan(const FString& Source, const FString& Basename);

	/** Find the LAST top-level `return ( ... )` in a body (T1.4 semantics). With
	 *  bRequireMarkupPeek the window must START like markup (`<`/`@`/`{`, after leading markup
	 *  comments) — the component scan's rule; without it any parenthesized return counts — the
	 *  directive-body rule (EmitBody/formatter). */
	static FUetkxSplitReturn SplitMarkupReturn(const TArray<int32>& Body, bool bRequireMarkupPeek);
};
