// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiCoreMisc.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"

// ── CVars (rui.*, dotted PascalCase — D-14) ──────────────────────────────────────────────

static TAutoConsoleVariable<bool> CVarRuiTimeSlicing(
	TEXT("rui.TimeSlicing"), false,
	TEXT("Chunk the ReactiveUI render phase across frames on a budget (commit stays atomic)."));

static TAutoConsoleVariable<float> CVarRuiFrameBudgetMs(
	TEXT("rui.FrameBudgetMs"), 8.0f,
	TEXT("Render-phase work per frame before parking, when rui.TimeSlicing is on."));

static TAutoConsoleVariable<bool> CVarRuiHostNodePool(
	TEXT("rui.HostNodePool"), true,
	TEXT("Recycle childless leaf widgets across keyed-list churn (GO-05). Off to A/B."));

static TAutoConsoleVariable<bool> CVarRuiHookValidation(
	TEXT("rui.HookValidation"),
#if UE_BUILD_SHIPPING
	false,
#else
	true,
#endif
	TEXT("Hook-order mismatch detection (hooks in branches/loops desync slots)."));

static TAutoConsoleVariable<bool> CVarRuiStrictDiagnostics(
	TEXT("rui.StrictDiagnostics"),
#if UE_BUILD_SHIPPING
	false,
#else
	true,
#endif
	TEXT("Warn on state updates during render and similar misuse."));

static TAutoConsoleVariable<bool> CVarRuiStrictMode(
	TEXT("rui.StrictMode"), false,
	TEXT("Dev double-render: render functions run twice, first result discarded (flushes impure renders and stale captures)."));

bool FRuiConfig::IsTimeSlicing() { return CVarRuiTimeSlicing.GetValueOnGameThread(); }
float FRuiConfig::FrameBudgetMs() { return CVarRuiFrameBudgetMs.GetValueOnGameThread(); }
bool FRuiConfig::IsHostNodePoolEnabled() { return CVarRuiHostNodePool.GetValueOnGameThread(); }
bool FRuiConfig::IsHookValidationEnabled() { return CVarRuiHookValidation.GetValueOnGameThread(); }
bool FRuiConfig::IsStrictDiagnosticsEnabled() { return CVarRuiStrictDiagnostics.GetValueOnGameThread(); }
bool FRuiConfig::IsStrictModeEnabled()
{
#if UE_BUILD_SHIPPING
	return false; // never in shipping, regardless of the CVar
#else
	return CVarRuiStrictMode.GetValueOnGameThread();
#endif
}

// ── Diagnostics ──────────────────────────────────────────────────────────────────────────

bool FRuiDiagnostics::bEnabled = false;
bool FRuiDiagnostics::bCapture = false;
TArray<FString> FRuiDiagnostics::Messages;
int32 FRuiDiagnostics::Renders = 0;
int32 FRuiDiagnostics::Commits = 0;
int32 FRuiDiagnostics::Placements = 0;
int32 FRuiDiagnostics::Updates = 0;
int32 FRuiDiagnostics::Deletions = 0;

void FRuiDiagnostics::Emit(const FString& Msg)
{
	if (bCapture)
	{
		Messages.Add(Msg);
	}
}

void FRuiDiagnostics::ClearMessages()
{
	Messages.Empty();
}

void FRuiDiagnostics::Reset()
{
	Renders = Commits = Placements = Updates = Deletions = 0;
}

// ── Render-error latch (D-10) ────────────────────────────────────────────────────────────

namespace
{
	// Game-thread only (checkf'd at the public entry points) — plain statics suffice and
	// keep the latch visible in a debugger.
	TOptional<FString> GRuiRenderFailure;
	bool bGRuiIsRendering = false;
}

namespace RUI
{
	void FailRender(const FString& Reason)
	{
		if (!GRuiRenderFailure.IsSet()) // first failure wins (nested failures are fallout)
		{
			GRuiRenderFailure = Reason;
		}
	}

	TOptional<FString> ConsumeRenderFailure()
	{
		TOptional<FString> Out = MoveTemp(GRuiRenderFailure);
		GRuiRenderFailure.Reset();
		return Out;
	}

	bool IsRendering() { return bGRuiIsRendering; }
	void SetRendering(bool bInRendering) { bGRuiIsRendering = bInRendering; }
}
