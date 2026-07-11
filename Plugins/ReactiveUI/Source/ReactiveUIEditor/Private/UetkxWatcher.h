// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxWatcher — compile-on-save (the family's three-trigger design, D-19):
//   1. FDirectoryWatcher change notifications on Source/ + Plugins/ (.uetkx edits)
//   2. window activation (tab back into the editor after an external-IDE edit)
//   3. a standing 2s HasStale poll ("save in VS Code while the editor sits in background" —
//      a stale file once survived 40 min unnoticed in the field)
// A sweep compiles stale files (FUetkxDriver), pushes de-duplicated diagnostics to the
// "ReactiveUI" MessageLog (append-only docks spam — each distinct error set once, a green
// resolved line when it heals), and hands every compiled file to FRuiHmr (definition swap).
// Busy guard with a 30s deadman (a crashed sweep costs one 30s outage, not the session);
// the FIRST sweep logs unconditionally (cold-open proof of life).

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"

class FUetkxWatcher
{
public:
	void Start();
	void Stop();

	/** One sweep NOW (also the test seam). Returns files compiled. */
	int32 Sweep(const TCHAR* Reason);

private:
	void OnDirectoryChanged(const TArray<struct FFileChangeData>& Changes);
	void ReportDiags(const FString& UetkxPath, const TArray<FString>& Errors);

	FTSTicker::FDelegateHandle TickerHandle;
	TMap<FString, FDelegateHandle> WatchHandles; // root dir -> directory-watcher handle
	FDelegateHandle ActivationHandle;
	TMap<FString, FString> LastDiags; // path -> last-reported body (dedup)
	bool bPendingSweep = false;
	bool bBusy = false;
	double BusySince = 0.0;
	bool bFirstSweepDone = false;
};
