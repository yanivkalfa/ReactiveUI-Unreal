// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuiHmr — the D-20 dev-loop applier: save Foo.uetkx mid-PIE → parse → build interpreter
// definitions → swap them under their ComponentIds (reconciler override seam) → refresh every
// live root. The status line reports exactly what happened (the family's honesty rule):
// `reloaded / refreshed / reset / linked / global / ms`.
//
// State rules (honest set): interp→interp swaps preserve hook state when the hook-signature
// hash is unchanged; the FIRST swap of a compiled component always resets (compiled cells are
// typed, interp cells are FRuiValue — representation change; TD-019 tracks value migration);
// a signature change resets by design. Parse error → per-file isolation: the old definition
// keeps rendering, errors reported, nothing swapped.

#pragma once

#include "CoreMinimal.h"

struct REACTIVEUIINTERP_API FRuiHmrStatus
{
	int32 Reloaded = 0;	 // definitions swapped
	int32 Refreshed = 0; // live roots re-rendered
	int32 Reset = 0;	 // components whose hook state was deliberately reset
	int32 Linked = 0;	 // components hot-LINKED (name unseen before this session swap)
	bool bGlobal = true; // v1 refresh granularity is global (HmrRefreshAll)
	double Ms = 0.0;
	TArray<FString> Errors;
	TArray<FString> Notes; // interp fallback notes ("rebuild required for full behavior")

	bool Ok() const { return Errors.IsEmpty(); }
	FString ToString() const;
};

class REACTIVEUIINTERP_API FRuiHmr
{
public:
	/** Apply one saved .uetkx: swap/link its components, refresh live roots. Importers = the
	 *  project-relative names of files that import this one (the blast radius named in the
	 *  hook/module rebuild note, M9) — the watcher supplies them from the driver's import graph. */
	static FRuiHmrStatus ApplyFile(const FString& UetkxPath, const TArray<FString>& Importers = {});

	/** Apply from source text (tests / in-memory). Basename names the file binding. */
	static FRuiHmrStatus ApplySource(const FString& Source, const FString& Basename,
									 const TArray<FString>& Importers = {});

	/** Drop every interp override (a real rebuild landed — compiled definitions rule again). */
	static void ResetSession();
};
