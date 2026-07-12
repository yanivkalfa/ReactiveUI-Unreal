// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxHmrController.h"

#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ILiveCodingModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "ReactiveUetkxEditorSettings.h"
#include "RuiReconciler.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogUetkxHmr, Log, All);

namespace
{
	ILiveCodingModule* LiveCoding()
	{
		return FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding"));
	}
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
	bActive = false;
	bDirtyAgain = false;
	UE_LOG(LogUetkxHmr, Display, TEXT("[RUI HMR] stopped%s"),
		   bForceDisableSession ? TEXT(" (Live Coding session disabled — external builds restored)") : TEXT(""));
	OnStatusChanged.Broadcast();
}

void FUetkxHmrController::Shutdown()
{
	UnregisterPieHooks();
	Stop();
	OnStatusChanged.Clear();
}

void FUetkxHmrController::ApplyConsoleVisibilitySetting()
{
	// Live Coding's console window is governed by its Startup mode (EditorPerProjectUserSettings, and
	// ConfigRestartRequired). We can't call the engine's private HideConsole(), so we steer that config:
	//   AutomaticButHidden → the console runs as the compile server but shows no window (our window is UI).
	//   Automatic          → Epic's console shows (the stock behaviour).
	// A user who deliberately chose "Manual" is left untouched.
	static const TCHAR* Section = TEXT("/Script/LiveCoding.LiveCodingSettings");
	FString Current;
	GConfig->GetString(Section, TEXT("Startup"), Current, GEditorPerProjectIni);
	if (Current == TEXT("Manual"))
	{
		return;
	}
	const bool bHide = GetDefault<UReactiveUetkxEditorSettings>()->bHideLiveCodingConsole;
	const FString Desired = bHide ? TEXT("AutomaticButHidden") : TEXT("Automatic");
	if (Current != Desired)
	{
		GConfig->SetString(Section, TEXT("Startup"), *Desired, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
		UE_LOG(LogUetkxHmr, Display,
			   TEXT("[RUI HMR] Live Coding console startup set to '%s' — restart the editor for it to take effect."),
			   *Desired);
	}
}

void FUetkxHmrController::RegisterPieHooks()
{
	// FEditorDelegates are static multicasts — safe to subscribe at module load (before GEditor is up).
	if (PiePostStartedHandle.IsValid())
	{
		return;
	}
	PiePostStartedHandle =
		FEditorDelegates::PostPIEStarted.AddRaw(this, &FUetkxHmrController::OnPiePostStarted);
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
		RecentErrors.Insert({FDateTime::Now().ToString(TEXT("%H:%M")),
							  FString::Printf(TEXT("%s — %d error(s)"), *Reason, NumErrors)},
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
		FNotificationInfo Info(FText::FromString(FString::Printf(
			TEXT("HMR: patched %s (%.0f ms)"),
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
