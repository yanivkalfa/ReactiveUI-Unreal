// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxFormatter — canonical-form formatter for .uetkx (family guitkx_formatter.gd port).
// AST-driven re-emit through the single parser of record (FUetkxFileScan/FUetkxMarkup), NOT
// regex post-processing. On ANY parse error the source returns VERBATIM (never corrupt) with
// bFellBack=true (G-06: distinguishable from "already canonical"). Embedded C++ (setup,
// directive-body statements) keeps its internal structure — only base-indent is re-anchored
// (depth-based with unit inference; lines inside multi-line strings/comments stay byte-
// verbatim, G-02). Idempotent: Format(Format(x).Text).Text == Format(x).Text, corpus-pinned.
//
// Options resolve from a `uetkx.config.json` walk-up (nearest ancestor wins); .uetkx default
// is TABS. The TS port (Phase 5) runs the same uetkx-formatter-cases.json goldens.

#pragma once

#include "CoreMinimal.h"

struct REACTIVEUITOOLCHAIN_API FUetkxFormatOptions
{
	int32 PrintWidth = 100;
	FString IndentStyle = TEXT("tab"); // "tab" | "space"
	int32 IndentSize = 2;			   // spaces per level when IndentStyle == "space"
	bool bSingleAttributePerLine = false;
	bool bInsertSpaceBeforeSelfClose = true;
};

struct REACTIVEUITOOLCHAIN_API FUetkxFormatResult
{
	bool bOk = true;
	FString Text;
	bool bChanged = false;
	bool bFellBack = false; // parse error / unsafe re-emit: Text == Source byte-identical
};

class REACTIVEUITOOLCHAIN_API FUetkxFormatter
{
public:
	static FUetkxFormatResult Format(const FString& Source, const FUetkxFormatOptions& Options = FUetkxFormatOptions());

	/** Nearest `uetkx.config.json` walking UP from StartDir; defaults when none found. */
	static FUetkxFormatOptions ResolveOptions(const FString& StartDir);
};
