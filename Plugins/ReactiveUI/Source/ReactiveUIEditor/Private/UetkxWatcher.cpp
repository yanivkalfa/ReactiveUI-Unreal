// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxWatcher.h"

#include "DirectoryWatcherModule.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "IDirectoryWatcher.h"
#include "Logging/MessageLog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RUICompileCommandlet.h"
#include "ReactiveUetkxEditorSettings.h"
#include "UetkxDriver.h"
#include "UetkxHmrController.h"

DEFINE_LOG_CATEGORY_STATIC(LogUetkxWatcher, Log, All);

namespace
{
	// HMR v2 (D-HMR-4): a fast ticker + a quiet-window debounce so a save is picked up in ~300 ms
	// (coalesced), not on the slow fallback poll. The stale/fingerprint fallback stays throttled.
	constexpr float TickSeconds = 0.15f;			// FTSTicker delay is float (UE 5.8 warns on the double)
	constexpr double DefaultDebounceSeconds = 0.30; // quiet window after the last .uetkx event (settings override)
	constexpr double StalePollSeconds = 2.0;		// throttle for the (file-I/O) HasStale fallback
	constexpr double DeadmanSeconds = 30.0;
	const FName MessageLogName(TEXT("ReactiveUI"));

	// The quiet window after the last event, from settings (clamped to something sane).
	double DebounceSeconds()
	{
		const int32 Ms = GetDefault<UReactiveUetkxEditorSettings>()->DebounceMs;
		return Ms > 0 ? FMath::Clamp(Ms, 0, 2000) / 1000.0 : DefaultDebounceSeconds;
	}

	bool VerboseWatcher()
	{
		return GetDefault<UReactiveUetkxEditorSettings>()->bVerboseWatcher;
	}
} // namespace

void FUetkxWatcher::Start()
{
	// Watched roots: settings.WatchedRoots (project-relative), else the default Source/ + Plugins/.
	// Read at Start — changing the roots takes effect on the next editor start (they anchor the FS watchers).
	Roots.Reset();
	for (const FString& Rel : GetDefault<UReactiveUetkxEditorSettings>()->WatchedRoots)
	{
		const FString Full = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Rel);
		if (IFileManager::Get().DirectoryExists(*Full))
		{
			Roots.Add(Full);
		}
	}
	if (Roots.Num() == 0)
	{
		Roots = URUICompileCommandlet::DefaultRoots();
	}

	// trigger 1: directory watcher on each sweep root
	FDirectoryWatcherModule& WatcherModule =
		FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	if (IDirectoryWatcher* Watcher = WatcherModule.Get())
	{
		for (const FString& Root : Roots)
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

	// trigger 3: a fast ticker that debounces the change/activation triggers and, throttled, runs the
	// HasStale fallback poll (external edit while the editor sits in background).
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[this](float)
			{
				const double Now = FPlatformTime::Seconds();
				// First sweep + fingerprint drift are immediate; a pending change sweeps once the quiet
				// window has elapsed (coalescing a burst of save events into one compile).
				if (!bFirstSweepDone || FUetkxDriver::FingerprintMismatch())
				{
					bPendingSweep = false;
					Sweep(TEXT("poll"));
					return true;
				}
				if (bPendingSweep && (Now - LastChangeSeconds) >= DebounceSeconds())
				{
					bPendingSweep = false;
					Sweep(TEXT("save"));
					return true;
				}
				// Throttled fallback: catch a stale file the FS watcher missed.
				if ((Now - LastStaleCheckSeconds) >= StalePollSeconds)
				{
					LastStaleCheckSeconds = Now;
					for (const FString& Root : Roots)
					{
						if (FUetkxDriver::HasStale(Root))
						{
							Sweep(TEXT("stale-poll"));
							break;
						}
					}
				}
				return true;
			}),
		TickSeconds);
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
	// ANY .uetkx event (add / modify / remove — the watcher includes directory changes) arms a
	// debounced sweep. CompileAll then handles the whole tree: new files compile, removed files
	// orphan-sweep, changed files + their importers recompile (HMR v2 D-HMR-4).
	for (const FFileChangeData& Change : Changes)
	{
		if (Change.Filename.EndsWith(TEXT(".uetkx")))
		{
			bPendingSweep = true;
			LastChangeSeconds = FPlatformTime::Seconds();
			if (VerboseWatcher())
			{
				UE_LOG(LogUetkxWatcher, Display, TEXT("[RUI watcher] event: %s (armed debounced sweep)"),
					   *Change.Filename);
			}
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
	for (const FString& Root : Roots)
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
		}
	}
	// HMR v2: codegen regenerated the committed .inl (keeps the tree fresh regardless of HMR). Hand the
	// result to the controller — when HMR mode is ACTIVE it triggers a Live Coding compile → patch →
	// refresh; when stopped, this is a no-op (D-HMR-2/3).
	FUetkxHmrController::Get().NotifyCodegen(Compiled, Errors, Reason);
	if (!bFirstSweepDone || Compiled > 0 || Errors > 0)
	{
		// cold-open proof of life: a silent Output means the watcher is NOT running
		UE_LOG(LogUetkxWatcher, Display, TEXT("[RUI] sweep (%s): %d compiled, %d error(s)"), Reason, Compiled, Errors);
	}
	bFirstSweepDone = true;
	bBusy = false;
	return Compiled;
}
