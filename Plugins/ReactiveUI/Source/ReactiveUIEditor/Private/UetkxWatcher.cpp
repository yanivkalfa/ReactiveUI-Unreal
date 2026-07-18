// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxWatcher.h"

#include "DirectoryWatcherModule.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "IDirectoryWatcher.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/FileHelper.h"
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

namespace
{
	/** 1-based line/col from a code-point offset (scanner offsets are code points; low
	 *  surrogates don't count). A jump aid, not a contract — astral chars shift col by design. */
	void UetkxLineColFromOffset(const FString& Contents, int32 CodePointOffset, int32& OutLine, int32& OutCol)
	{
		OutLine = 1;
		OutCol = 1;
		int32 Seen = 0;
		for (int32 i = 0; i < Contents.Len() && Seen < CodePointOffset; ++i)
		{
			const TCHAR C = Contents[i];
			if (C >= 0xDC00 && C <= 0xDFFF)
			{
				continue; // low surrogate — same code point as the preceding high surrogate
			}
			++Seen;
			if (C == TEXT('\n'))
			{
				++OutLine;
				OutCol = 1;
			}
			else
			{
				++OutCol;
			}
		}
	}

	/** TB-8: open the .uetkx at line:col in the owner's editor — `code --goto` when VS Code is
	 *  on PATH (checked lazily, once), else the OS default app for the file. */
	void UetkxOpenAtLine(const FString& UetkxPath, int32 Line, int32 Col)
	{
		static int32 CachedHasCode = -1; // -1 unknown, 0 no, 1 yes
		if (CachedHasCode == -1)
		{
			int32 ReturnCode = 1;
			FString StdOut, StdErr;
			FPlatformProcess::ExecProcess(TEXT("cmd.exe"), TEXT("/c where code"), &ReturnCode, &StdOut, &StdErr);
			CachedHasCode = ReturnCode == 0 ? 1 : 0;
		}
		if (CachedHasCode == 1)
		{
			const FString Args = FString::Printf(TEXT("/c code --goto \"%s:%d:%d\""), *UetkxPath, Line, Col);
			FProcHandle Handle = FPlatformProcess::CreateProc(TEXT("cmd.exe"), *Args, /*bLaunchDetached*/ true,
															  /*bLaunchHidden*/ true, /*bLaunchReallyHidden*/ true,
															  nullptr, 0, nullptr, nullptr);
			if (Handle.IsValid())
			{
				FPlatformProcess::CloseProc(Handle);
				return;
			}
		}
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*UetkxPath);
	}
} // namespace

void FUetkxWatcher::ReportDiags(const FString& UetkxPath, const TArray<FString>& Errors,
								const TArray<FUetkxDiag>* ErrorDiags)
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
	// TB-8 (TESTING_BUGS.md): tokenized rows — message text + a `File(line,col)` ACTION token
	// that opens the file at the error (Unity's double-click-the-console parity). Falls back to
	// the plain-text row when structured diags are unavailable.
	FString FileContents;
	const bool bHaveContents =
		ErrorDiags && ErrorDiags->Num() == Errors.Num() && FFileHelper::LoadFileToString(FileContents, *UetkxPath);
	for (int32 e = 0; e < Errors.Num(); ++e)
	{
		if (!bHaveContents)
		{
			Log.Error(FText::FromString(Errors[e]));
			continue;
		}
		int32 Line = 1;
		int32 Col = 1;
		UetkxLineColFromOffset(FileContents, (*ErrorDiags)[e].Offset, Line, Col);
		const FString Path = UetkxPath;
		TSharedRef<FTokenizedMessage> Msg = Log.Error(FText::FromString(Errors[e]));
		Msg->AddToken(FActionToken::Create(
			FText::FromString(FString::Printf(TEXT("→ %s(%d,%d)"), *FPaths::GetCleanFilename(UetkxPath), Line, Col)),
			FText::FromString(TEXT("Open this file at the error line in your editor")),
			FOnActionTokenExecuted::CreateLambda([Path, Line, Col]() { UetkxOpenAtLine(Path, Line, Col); })));
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
			TArray<FUetkxDiag> FileErrorDiags; // parallel to FileErrors — feeds the TB-8 jump tokens
			for (const FUetkxDiag& Diag : File.Diags)
			{
				if (Diag.Severity == 0)
				{
					FileErrors.Add(FString::Printf(TEXT("%s: %s: %s"), *File.UetkxPath, *Diag.Code, *Diag.Message));
					FileErrorDiags.Add(Diag);
				}
			}
			ReportDiags(File.UetkxPath, FileErrors, &FileErrorDiags);
			// ES-modules (M5): a recompiled SUPPORT file (values/hooks/modules/utils — no markup of
			// its own) patches THROUGH its importers; name them so the author knows the HMR blast
			// radius. Sidecar dep graph = the sweep that just ran, so the answer is current.
			if (File.bOk && File.bSupportFile)
			{
				const TArray<FString> Importers =
					FUetkxDriver::ImportersOf(FUetkxDriver::ProjectRelPath(File.UetkxPath), Roots);
				if (Importers.Num() > 0)
				{
					UE_LOG(LogUetkxWatcher, Display,
						   TEXT("[RUI] hmr: %s is a support file — the rebuild patches its "
								"importer(s): %s"),
						   *FPaths::GetCleanFilename(File.UetkxPath), *FString::Join(Importers, TEXT(", ")));
				}
			}
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
