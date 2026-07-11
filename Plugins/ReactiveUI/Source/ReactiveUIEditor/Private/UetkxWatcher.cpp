// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxWatcher.h"

#include "DirectoryWatcherModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IDirectoryWatcher.h"
#include "Logging/MessageLog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RUICompileCommandlet.h"
#include "RuiHmr.h"
#include "UetkxDriver.h"

#include "HAL/IConsoleManager.h"
#include "ILiveCodingModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogUetkxWatcher, Log, All);

namespace
{
	constexpr double PollSeconds = 2.0;
	constexpr double DeadmanSeconds = 30.0;
	const FName MessageLogName(TEXT("ReactiveUI"));

	/** Off by default: when a swap reports fallback notes ("rebuild required for full
	 *  behavior"), optionally fire a Live Coding compile so the compiled regions catch up. */
	TAutoConsoleVariable<bool> CVarAutoLiveCoding(TEXT("rui.Hmr.AutoLiveCoding"), false,
												  TEXT("Trigger Live Coding when a .uetkx hot-swap needs a rebuild "
													   "for full behavior (default off)."));

	void MaybeTriggerLiveCoding()
	{
		if (!CVarAutoLiveCoding.GetValueOnGameThread())
		{
			return;
		}
		if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding")))
		{
			if (LiveCoding->IsEnabledForSession() && !LiveCoding->IsCompiling())
			{
				LiveCoding->Compile(ELiveCodingCompileFlags::None, nullptr);
			}
		}
	}
} // namespace

void FUetkxWatcher::Start()
{
	// trigger 1: directory watcher on each sweep root
	FDirectoryWatcherModule& WatcherModule =
		FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	if (IDirectoryWatcher* Watcher = WatcherModule.Get())
	{
		for (const FString& Root : URUICompileCommandlet::DefaultRoots())
		{
			FDelegateHandle Handle;
			Watcher->RegisterDirectoryChangedCallback_Handle(
				Root, IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FUetkxWatcher::OnDirectoryChanged), Handle,
				IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);
			WatchHandles.Add(Root, Handle);
		}
	}

	// trigger 2: window activation (external-editor edits without a change notification)
	if (FSlateApplication::IsInitialized())
	{
		ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddLambda(
			[this](bool bActive)
			{
				if (bActive)
				{
					bPendingSweep = true;
				}
			});
	}

	// trigger 3: the standing poll — also drains pending sweeps from triggers 1+2
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[this](float)
			{
				if (bPendingSweep || !bFirstSweepDone || FUetkxDriver::FingerprintMismatch())
				{
					bPendingSweep = false;
					Sweep(TEXT("poll"));
					return true;
				}
				for (const FString& Root : URUICompileCommandlet::DefaultRoots())
				{
					if (FUetkxDriver::HasStale(Root))
					{
						Sweep(TEXT("stale-poll"));
						break;
					}
				}
				return true;
			}),
		PollSeconds);
}

void FUetkxWatcher::Stop()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	if (ActivationHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle);
	}
	if (FModuleManager::Get().IsModuleLoaded(TEXT("DirectoryWatcher")))
	{
		FDirectoryWatcherModule& WatcherModule =
			FModuleManager::GetModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		if (IDirectoryWatcher* Watcher = WatcherModule.Get())
		{
			for (const TPair<FString, FDelegateHandle>& Pair : WatchHandles)
			{
				Watcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value);
			}
		}
	}
	WatchHandles.Empty();
}

void FUetkxWatcher::OnDirectoryChanged(const TArray<FFileChangeData>& Changes)
{
	for (const FFileChangeData& Change : Changes)
	{
		if (Change.Filename.EndsWith(TEXT(".uetkx")))
		{
			bPendingSweep = true;
			return;
		}
	}
}

void FUetkxWatcher::ReportDiags(const FString& UetkxPath, const TArray<FString>& Errors)
{
	const FString Body = FString::Join(Errors, TEXT("\n"));
	FString* Last = LastDiags.Find(UetkxPath);
	if (Errors.IsEmpty())
	{
		if (Last)
		{
			// the green resolved line — silence would leave the dock's last word an error
			FMessageLog(MessageLogName)
				.Info(FText::FromString(FString::Printf(TEXT("%s: compile errors resolved"), *UetkxPath)));
			UE_LOG(LogUetkxWatcher, Display, TEXT("%s: compile errors resolved"), *UetkxPath);
			LastDiags.Remove(UetkxPath);
		}
		return;
	}
	if (Last && *Last == Body)
	{
		return; // dedup: this exact error set was already reported
	}
	LastDiags.Add(UetkxPath, Body);
	FMessageLog Log(MessageLogName);
	for (const FString& Error : Errors)
	{
		Log.Error(FText::FromString(Error));
	}
	Log.Notify(FText::FromString(
				   FString::Printf(TEXT("ReactiveUI: %s failed to compile"), *FPaths::GetCleanFilename(UetkxPath))),
			   EMessageSeverity::Error, /*bForce*/ false);
}

int32 FUetkxWatcher::Sweep(const TCHAR* Reason)
{
	const double Now = FPlatformTime::Seconds();
	if (bBusy)
	{
		if (Now - BusySince < DeadmanSeconds)
		{
			bPendingSweep = true; // re-entrancy guard: queue instead
			return 0;
		}
		UE_LOG(LogUetkxWatcher, Warning, TEXT("sweep deadman fired after %.0fs — clearing the busy latch"),
			   Now - BusySince);
	}
	bBusy = true;
	BusySince = Now;

	int32 Compiled = 0;
	int32 Errors = 0;
	const bool bForce = FUetkxDriver::FingerprintMismatch();
	for (const FString& Root : URUICompileCommandlet::DefaultRoots())
	{
		const FUetkxSweepResult Result = FUetkxDriver::CompileAll(Root, bForce);
		Compiled += Result.Compiled;
		Errors += Result.Errors;
		for (const FUetkxFileResult& File : Result.Files)
		{
			if (File.bSkipped)
			{
				continue;
			}
			TArray<FString> FileErrors;
			for (const FUetkxDiag& Diag : File.Diags)
			{
				if (Diag.Severity == 0)
				{
					FileErrors.Add(FString::Printf(TEXT("%s: %s: %s (offset %d)"), *File.UetkxPath, *Diag.Code,
												   *Diag.Message, Diag.Offset));
				}
			}
			ReportDiags(File.UetkxPath, FileErrors);
			if (File.bOk)
			{
				// the live half of the loop: swap the definition under the running UI
				const FRuiHmrStatus Status = FRuiHmr::ApplyFile(File.UetkxPath);
				FMessageLog(MessageLogName).Info(FText::FromString(Status.ToString()));
				if (Status.Notes.Num() > 0)
				{
					MaybeTriggerLiveCoding();
				}
			}
		}
	}
	if (!bFirstSweepDone || Compiled > 0 || Errors > 0)
	{
		// cold-open proof of life: a silent Output means the watcher is NOT running
		UE_LOG(LogUetkxWatcher, Display, TEXT("[RUI] sweep (%s): %d compiled, %d error(s)"), Reason, Compiled, Errors);
	}
	bFirstSweepDone = true;
	bBusy = false;
	return Compiled;
}
