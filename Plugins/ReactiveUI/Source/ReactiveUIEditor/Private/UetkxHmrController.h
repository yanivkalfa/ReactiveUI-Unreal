// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// HMR v2 (plans/HMR_V2_PLAN.md) — the Live-Coding-driven Hot Module Reload controller. The C++
// sibling of the Unity toolkit's UitkxHmrController: a Start/Stop MODE. START enables the Unreal
// Live Coding session (external builds pause while active — the honest tradeoff, owned explicitly);
// while active, ANY .uetkx change (the watcher already regenerates the .inl) triggers a Live Coding
// compile, and on the patch-complete delegate we refresh every live reconciler root so it re-renders
// with the freshly-patched COMPILED code — full fidelity (imports, hooks, effects), state preserved
// (hook cells are heap-resident, untouched by a code patch). No interpreter, no per-edit branching.
//
// Not headless-testable: Live Coding needs a running editor + compiler. The engine is owner-verified
// in-editor; the watcher's codegen + debounce are covered by the automation/driver suites.

#pragma once

#include "CoreMinimal.h"

/** One HMR reload's outcome — surfaced to the Message Log and (Phase 2) the HMR window. */
struct FUetkxHmrStatus
{
	int32 Swaps = 0;	 // successful Live Coding patches applied this session
	int32 Errors = 0;	 // compile failures this session
	FString LastReason;	 // what triggered the last cycle (a file path / "sweep")
	double LastMs = 0.0; // wall-clock of the last compile→patch→refresh
};

/** A recent codegen error, for the HMR window's "Recent Errors" list (full detail is in the
 *  "ReactiveUI" Message Log — this is the at-a-glance tail). */
struct FUetkxHmrError
{
	FString When;	 // HH:MM the error was reported
	FString Summary; // "<reason> — N error(s)"
};

/** Editor-only HMR controller (singleton). The watcher feeds it codegen results; it drives Live
 *  Coding and the reconciler refresh. Phase 2 adds the ReactiveUetkx window on top of this. */
class FUetkxHmrController
{
public:
	static FUetkxHmrController& Get();

	/** Broadcast after every state change (start/stop, a compile, a patch) so the window repaints. */
	DECLARE_MULTICAST_DELEGATE(FOnHmrStatusChanged);
	FOnHmrStatusChanged OnStatusChanged;

	/** START the HMR mode: enable the Live Coding session (if available) + subscribe to its
	 *  patch-complete delegate. Returns false + a reason when Live Coding is unavailable/disabled. */
	bool Start(FString& OutError);

	/** STOP the mode: unsubscribe; per bDisableSessionOnStop, leave or disable the Live Coding session. */
	void Stop();

	/** Apply the ReactiveUetkx "Hide the Live Coding console" setting to Live Coding's startup-mode config
	 *  (AutomaticButHidden ↔ Automatic). Respects a user who chose Manual. Takes effect next editor start. */
	void ApplyConsoleVisibilitySetting();

	/** Subscribe / unsubscribe the PIE begin+end hooks that drive "Follow Play" (bFollowPie). */
	void RegisterPieHooks();
	void UnregisterPieHooks();

	bool IsActive() const { return bActive; }
	const FUetkxHmrStatus& GetStatus() const { return Status; }
	const TArray<FUetkxHmrError>& GetRecentErrors() const { return RecentErrors; }
	bool IsCompiling() const;

	/** The watcher calls this after a codegen sweep. When active and something regenerated, it triggers
	 *  a Live Coding compile (coalesced: if a compile is already running, it re-fires on completion). */
	void NotifyCodegen(int32 NumChanged, int32 NumErrors, const FString& Reason);

	/** Teardown (module shutdown): Stop() + drop delegates. */
	void Shutdown();

	/** When true, Stop() also disables the Live Coding session (restores normal external builds). */
	bool bDisableSessionOnStop = false;

private:
	FUetkxHmrController() = default;

	void StopInternal(bool bForceDisableSession); // Stop() core; PIE end forces the session off (frees builds)
	void TriggerCompile();
	void OnPatchComplete();	 // Live Coding finished a patch → refresh the live UI
	void RefreshLiveRoots(); // ForEachLive → HmrRefreshAll + FlushSync (re-render with patched code)
	void OnPiePostStarted(bool bSimulating);
	void OnPieEnded(bool bSimulating);

	bool bActive = false;
	bool bDirtyAgain = false; // a change arrived mid-compile → run one more cycle on completion
	double CycleStartSeconds = 0.0;
	FUetkxHmrStatus Status;
	TArray<FUetkxHmrError> RecentErrors; // newest-first, capped (see NotifyCodegen)
	FDelegateHandle PatchCompleteHandle;
	FDelegateHandle PiePostStartedHandle;
	FDelegateHandle PieEndedHandle;
};
