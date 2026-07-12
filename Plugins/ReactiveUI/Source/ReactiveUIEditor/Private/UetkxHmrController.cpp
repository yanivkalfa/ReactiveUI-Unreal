// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxHmrController.h"

#include "Framework/Notifications/NotificationManager.h"
#include "ILiveCodingModule.h"
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
	if (!bActive)
	{
		return;
	}
	// Honour the current setting (the window checkbox / Project Settings drive this).
	bDisableSessionOnStop = GetDefault<UReactiveUetkxEditorSettings>()->bDisableSessionOnStop;
	if (ILiveCodingModule* LC = LiveCoding())
	{
		if (PatchCompleteHandle.IsValid())
		{
			LC->GetOnPatchCompleteDelegate().Remove(PatchCompleteHandle);
		}
		if (bDisableSessionOnStop && LC->IsEnabledForSession())
		{
			LC->EnableForSession(false); // restore normal external builds
		}
	}
	PatchCompleteHandle.Reset();
	bActive = false;
	bDirtyAgain = false;
	UE_LOG(LogUetkxHmr, Display, TEXT("[RUI HMR] stopped"));
	OnStatusChanged.Broadcast();
}

void FUetkxHmrController::Shutdown()
{
	Stop();
	OnStatusChanged.Clear();
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
