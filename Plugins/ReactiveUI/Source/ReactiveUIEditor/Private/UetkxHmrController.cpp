// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxHmrController.h"

#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ILiveCodingModule.h"
#include "Modules/ModuleManager.h"
#include "ReactiveUetkxEditorSettings.h"
#include "RuiReconciler.h"
#include "Widgets/Notifications/SNotificationList.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogUetkxHmr, Log, All);

namespace
{
	ILiveCodingModule* LiveCoding()
	{
		return FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding"));
	}

#if PLATFORM_WINDOWS
	// Epic's Live Coding console is a separate process; its window title is "<Project> - Live Coding".
	HWND GLiveCodingConsoleHwnd = nullptr;	  // the console window, once discovered
	HWINEVENTHOOK GConsoleShowHook = nullptr; // fires when Epic re-shows the console → we hide it instantly

	BOOL CALLBACK FindLiveCodingConsoleProc(HWND Hwnd, LPARAM)
	{
		WCHAR Title[256] = {};
		::GetWindowTextW(Hwnd, Title, UE_ARRAY_COUNT(Title));
		if (FString(Title).EndsWith(TEXT(" - Live Coding")))
		{
			GLiveCodingConsoleHwnd = Hwnd;
			return 0; // found it — stop enumerating (FALSE)
		}
		return 1; // keep enumerating (TRUE)
	}

	// Event-driven re-hide: the console's process raises EVENT_OBJECT_SHOW when BringToFront un-hides it on a
	// compile; we SW_HIDE it in the same beat, so it never actually paints on screen. WINEVENT_OUTOFCONTEXT
	// delivers this on the editor's message-pumping thread.
	void CALLBACK OnConsoleShowEvent(HWINEVENTHOOK, DWORD Event, HWND Hwnd, LONG idObject, LONG, DWORD, DWORD)
	{
		if (Event == EVENT_OBJECT_SHOW && idObject == OBJID_WINDOW && Hwnd == GLiveCodingConsoleHwnd && Hwnd != nullptr)
		{
			::ShowWindow(Hwnd, SW_HIDE);
		}
	}
#endif
} // namespace

FUetkxHmrController& FUetkxHmrController::Get()
{
	static FUetkxHmrController Instance;
	return Instance;
}

bool FUetkxHmrController::IsCompiling() const
{
	ILiveCodingModule* LC = LiveCoding();
	return LC != nullptr && LC->IsCompiling();
}

bool FUetkxHmrController::Start(FString& OutError)
{
	if (bActive)
	{
		return true;
	}
	ILiveCodingModule* LC = LiveCoding();
	if (LC == nullptr)
	{
		OutError = TEXT("Live Coding module is not available in this editor build.");
		return false;
	}
	// Enable the session if it isn't already (the "turn the mode on" step). Respect a hard block.
	if (!LC->IsEnabledForSession())
	{
		if (!LC->CanEnableForSession())
		{
			OutError = LC->GetEnableErrorText().ToString();
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Live Coding cannot be enabled for this session.");
			}
			return false;
		}
		LC->EnableForSession(true);
	}
	// Refresh the live UI whenever Live Coding finishes a patch (whatever triggered it).
	PatchCompleteHandle = LC->GetOnPatchCompleteDelegate().AddRaw(this, &FUetkxHmrController::OnPatchComplete);
	bActive = true;
	bDirtyAgain = false;
	StartConsoleHider(); // keep Epic's console window hidden while HMR drives the compiles (opt-out setting)
	UE_LOG(LogUetkxHmr, Display, TEXT("[RUI HMR] started (Live Coding mode ON — external builds pause while active)"));
	OnStatusChanged.Broadcast();
	return true;
}

void FUetkxHmrController::Stop()
{
	// Honour the current setting (the window checkbox / Project Settings drive this).
	StopInternal(GetDefault<UReactiveUetkxEditorSettings>()->bDisableSessionOnStop);
}

void FUetkxHmrController::StopInternal(bool bForceDisableSession)
{
	if (!bActive)
	{
		return;
	}
	bDisableSessionOnStop = bForceDisableSession;
	if (ILiveCodingModule* LC = LiveCoding())
	{
		if (PatchCompleteHandle.IsValid())
		{
			LC->GetOnPatchCompleteDelegate().Remove(PatchCompleteHandle);
		}
		if (bForceDisableSession && LC->IsEnabledForSession())
		{
			LC->EnableForSession(false); // restore normal external builds
		}
	}
	PatchCompleteHandle.Reset();
	StopConsoleHider();
	bActive = false;
	bDirtyAgain = false;
	UE_LOG(LogUetkxHmr, Display, TEXT("[RUI HMR] stopped%s"),
		   bForceDisableSession ? TEXT(" (Live Coding session disabled — external builds restored)") : TEXT(""));
	OnStatusChanged.Broadcast();
}

void FUetkxHmrController::Shutdown()
{
	// Module shutdown runs AFTER the UObject system has exited at editor close — so touch NO UObjects here.
	// Stop() reads the settings CDO (GetDefault<...>), which is gone by now → crash. Call the core directly
	// with a literal (we only need to detach our patch-complete delegate; Live Coding tears its own session
	// down on exit — no need to disable the session here).
	UnregisterPieHooks();
	StopInternal(/*bForceDisableSession*/ false);
	OnStatusChanged.Clear();
}

void FUetkxHmrController::StartConsoleHider()
{
	if (ConsoleHiderHandle.IsValid() || !GetDefault<UReactiveUetkxEditorSettings>()->bHideLiveCodingConsole)
	{
		return;
	}
	// A SLOW poll (1s) only discovers the console window and installs the event hook; the hook does the
	// instant re-hiding (the console may not exist until the first compile, hence the discovery loop).
	ConsoleHiderHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FUetkxHmrController::ConsoleHiderTick), 1.0f);
	ConsoleHiderTick(0.0f); // discover + hide immediately if it's already up
}

void FUetkxHmrController::StopConsoleHider()
{
	if (ConsoleHiderHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ConsoleHiderHandle);
		ConsoleHiderHandle.Reset();
	}
#if PLATFORM_WINDOWS
	if (GConsoleShowHook != nullptr)
	{
		::UnhookWinEvent(GConsoleShowHook);
		GConsoleShowHook = nullptr;
	}
	GLiveCodingConsoleHwnd = nullptr;
#endif
	// We do NOT restore the window: Live Coding re-shows it (BringToFront) on the user's next compile, so
	// stopping the hider is enough — their own C++ builds get Epic's console back with no action from us.
}

bool FUetkxHmrController::ConsoleHiderTick(float)
{
#if PLATFORM_WINDOWS
	if (GLiveCodingConsoleHwnd == nullptr || !::IsWindow(GLiveCodingConsoleHwnd))
	{
		GLiveCodingConsoleHwnd = nullptr;
		if (GConsoleShowHook != nullptr) // window went away — drop the stale hook, we'll reinstall on rediscovery
		{
			::UnhookWinEvent(GConsoleShowHook);
			GConsoleShowHook = nullptr;
		}
		::EnumWindows(&FindLiveCodingConsoleProc, 0); // sets GLiveCodingConsoleHwnd if found
	}
	if (GLiveCodingConsoleHwnd != nullptr)
	{
		if (GConsoleShowHook == nullptr) // install the event hook scoped to the console's process
		{
			DWORD ProcessId = 0;
			::GetWindowThreadProcessId(GLiveCodingConsoleHwnd, &ProcessId);
			GConsoleShowHook = ::SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, &OnConsoleShowEvent,
												 ProcessId, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
		}
		if (::IsWindowVisible(GLiveCodingConsoleHwnd))
		{
			::ShowWindow(GLiveCodingConsoleHwnd, SW_HIDE);
		}
	}
#endif
	return true; // keep the slow discovery poll alive while active
}

void FUetkxHmrController::RefreshConsoleHiderState()
{
	if (!bActive)
	{
		return; // only manage the console while HMR is running
	}
	if (GetDefault<UReactiveUetkxEditorSettings>()->bHideLiveCodingConsole)
	{
		StartConsoleHider();
	}
	else
	{
		StopConsoleHider();
	}
}

void FUetkxHmrController::RegisterPieHooks()
{
	// FEditorDelegates are static multicasts — safe to subscribe at module load (before GEditor is up).
	if (PiePostStartedHandle.IsValid())
	{
		return;
	}
	PiePostStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FUetkxHmrController::OnPiePostStarted);
	PieEndedHandle = FEditorDelegates::EndPIE.AddRaw(this, &FUetkxHmrController::OnPieEnded);
}

void FUetkxHmrController::UnregisterPieHooks()
{
	if (PiePostStartedHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(PiePostStartedHandle);
		PiePostStartedHandle.Reset();
	}
	if (PieEndedHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(PieEndedHandle);
		PieEndedHandle.Reset();
	}
}

void FUetkxHmrController::OnPiePostStarted(bool /*bSimulating*/)
{
	if (!GetDefault<UReactiveUetkxEditorSettings>()->bFollowPie || bActive)
	{
		return;
	}
	FString Error;
	if (!Start(Error))
	{
		UE_LOG(LogUetkxHmr, Warning, TEXT("[RUI HMR] Follow Play: could not start on PIE — %s"), *Error);
	}
}

void FUetkxHmrController::OnPieEnded(bool /*bSimulating*/)
{
	if (!GetDefault<UReactiveUetkxEditorSettings>()->bFollowPie || !bActive)
	{
		return;
	}
	// Leaving Play: fully release Live Coding so external builds work again while you edit (the point of
	// Follow Play). This overrides bDisableSessionOnStop for the PIE-driven stop.
	StopInternal(/*bForceDisableSession*/ true);
}

void FUetkxHmrController::NotifyCodegen(int32 NumChanged, int32 NumErrors, const FString& Reason)
{
	Status.Errors += NumErrors;
	if (NumErrors > 0)
	{
		// Keep a short, newest-first tail for the window; the "ReactiveUI" Message Log has the detail.
		RecentErrors.Insert(
			{FDateTime::Now().ToString(TEXT("%H:%M")), FString::Printf(TEXT("%s — %d error(s)"), *Reason, NumErrors)},
			0);
		constexpr int32 MaxRecent = 20;
		if (RecentErrors.Num() > MaxRecent)
		{
			RecentErrors.SetNum(MaxRecent);
		}
		OnStatusChanged.Broadcast(); // errors surface whether or not the mode is active
	}
	if (!bActive)
	{
		return; // codegen still runs (keeps the committed .inl fresh) but no live patch while stopped
	}
	if (NumChanged <= 0)
	{
		return;
	}
	Status.LastReason = Reason;
	if (IsCompiling())
	{
		bDirtyAgain = true; // don't overlap; re-fire once the in-flight patch completes (coalesce)
		return;
	}
	TriggerCompile();
}

void FUetkxHmrController::TriggerCompile()
{
	ILiveCodingModule* LC = LiveCoding();
	if (LC == nullptr || !LC->IsEnabledForSession())
	{
		return;
	}
	CycleStartSeconds = FPlatformTime::Seconds();
	bDirtyAgain = false;
	LC->Compile(ELiveCodingCompileFlags::None, nullptr); // async — the patch-complete delegate lands the result
	OnStatusChanged.Broadcast();
}

void FUetkxHmrController::OnPatchComplete()
{
	if (!bActive)
	{
		return;
	}
	RefreshLiveRoots();
	++Status.Swaps;
	Status.LastMs = (FPlatformTime::Seconds() - CycleStartSeconds) * 1000.0;
	UE_LOG(LogUetkxHmr, Display, TEXT("[RUI HMR] patch applied — refreshed live UI (%.0f ms), %d total"), Status.LastMs,
		   Status.Swaps);
	if (GetDefault<UReactiveUetkxEditorSettings>()->bShowNotifications)
	{
		FNotificationInfo Info(FText::FromString(
			FString::Printf(TEXT("HMR: patched %s (%.0f ms)"),
							Status.LastReason.IsEmpty() ? TEXT("live UI") : *Status.LastReason, Status.LastMs)));
		Info.ExpireDuration = 2.5f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	OnStatusChanged.Broadcast();
	if (bDirtyAgain) // changes arrived during the compile — land the latest
	{
		TriggerCompile();
	}
}

void FUetkxHmrController::RefreshLiveRoots()
{
	// The compiled component functions were patched in place; re-render every live root so the fibers
	// call the new code. Hook state lives on the heap (untouched by the patch) → preserved.
	FRuiReconciler::ForEachLive(
		[](FRuiReconciler& Reconciler)
		{
			Reconciler.HmrRefreshAll();
			Reconciler.FlushSync();
		});
}
