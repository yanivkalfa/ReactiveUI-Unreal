// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Config CVars, diagnostics counters, and the cooperative render-error latch — the small
// always-on core services (config.gd + diagnostics.gd + the D-10 latch, one header).

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

// ─────────────────────────────────────────────────────────────────────────────────────────
// STATGROUP_ReactiveUI (D-14) — `stat ReactiveUI` shows per-frame reconciler activity.
// ─────────────────────────────────────────────────────────────────────────────────────────

DECLARE_STATS_GROUP(TEXT("ReactiveUI"), STATGROUP_ReactiveUI, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Renders"), STAT_RuiRenders, STATGROUP_ReactiveUI, REACTIVEUICORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Commits"), STAT_RuiCommits, STATGROUP_ReactiveUI, REACTIVEUICORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Placements"), STAT_RuiPlacements, STATGROUP_ReactiveUI, REACTIVEUICORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Updates"), STAT_RuiUpdates, STATGROUP_ReactiveUI, REACTIVEUICORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Deletions"), STAT_RuiDeletions, STATGROUP_ReactiveUI, REACTIVEUICORE_API);

// ─────────────────────────────────────────────────────────────────────────────────────────
// Config (rui.* CVars — dotted PascalCase per D-14; defaults mirror config.gd)
// ─────────────────────────────────────────────────────────────────────────────────────────

struct REACTIVEUICORE_API FRuiConfig
{
	/** Chunk the render phase across frames on a budget (commit stays atomic). Off by
	 *  default — synchronous renders are simplest and fast for normal UIs. */
	static bool IsTimeSlicing();
	static float FrameBudgetMs();

	/** Host-node pooling opt-out (A/B measurement). */
	static bool IsHostNodePoolEnabled();

	/** Hook-order validation + set-during-render warnings (dev builds default-on). */
	static bool IsHookValidationEnabled();
	static bool IsStrictDiagnosticsEnabled();

	/** StrictMode double-render (dev-only; flushes impure renders + stale captures, D-14). */
	static bool IsStrictModeEnabled();
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Diagnostics counters (diagnostics.gd) — cheap, opt-in, test-assertable
// ─────────────────────────────────────────────────────────────────────────────────────────

struct REACTIVEUICORE_API FRuiDiagnostics
{
	static bool bEnabled;

	/** Message capture for headless tests (validators emit here when on). */
	static bool bCapture;
	static TArray<FString> Messages;
	static void Emit(const FString& Msg);
	static void ClearMessages();

	static int32 Renders; // component render-fn invocations (excludes bailouts)
	static int32 Commits;
	static int32 Placements;
	static int32 Updates;
	static int32 Deletions;

	static void Reset();
	static void OnRender()
	{
		INC_DWORD_STAT(STAT_RuiRenders);
		if (bEnabled)
		{
			++Renders;
		}
	}
	static void OnCommit()
	{
		INC_DWORD_STAT(STAT_RuiCommits);
		if (bEnabled)
		{
			++Commits;
		}
	}
	static void OnPlacement()
	{
		INC_DWORD_STAT(STAT_RuiPlacements);
		if (bEnabled)
		{
			++Placements;
		}
	}
	static void OnUpdate()
	{
		INC_DWORD_STAT(STAT_RuiUpdates);
		if (bEnabled)
		{
			++Updates;
		}
	}
	static void OnDeletion()
	{
		INC_DWORD_STAT(STAT_RuiDeletions);
		if (bEnabled)
		{
			++Deletions;
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Cooperative render-error latch (D-10). UE ships without C++ exceptions, so there is no
// throw path: a failing render CALLS RUI::FailRender(...) and returns whatever it can; the
// reconciler checks the latch after each component render and unwinds the WIP subtree to
// the nearest error boundary (safe pre-commit — double buffering keeps `current` intact).
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace RUI
{
	/** Signal "this render failed" (usable via the RUI_RENDER_FAIL macro for file/line). */
	REACTIVEUICORE_API void FailRender(const FString& Reason);

	/** Reconciler-side: consume the latch (returns unset optional when no failure). */
	REACTIVEUICORE_API TOptional<FString> ConsumeRenderFailure();

	/** True while a component render is on the stack (debug in-render assert support). */
	REACTIVEUICORE_API bool IsRendering();
	REACTIVEUICORE_API void SetRendering(bool bInRendering);
} // namespace RUI

#define RUI_RENDER_FAIL(Fmt, ...)                                                                                      \
	RUI::FailRender(FString::Printf(TEXT("%s(%d): ") Fmt, TEXT(__FILE__), __LINE__, ##__VA_ARGS__))
