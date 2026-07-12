// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "SUetkxPreviewPanel.h"
#include "Styling/AppStyle.h"
#include "UetkxHmrController.h"
#include "UetkxWatcher.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiEditor, Log, All);

namespace
{
	const FName GRuiPreviewTabId(TEXT("ReactiveUIPreview"));
}

class FReactiveUIEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
		FMessageLogInitializationOptions Options;
		Options.bShowFilters = true;
		Options.bShowPages = false;
		MessageLogModule.RegisterLogListing(TEXT("ReactiveUI"),
											NSLOCTEXT("ReactiveUI", "MessageLogLabel", "ReactiveUI"), Options);
		Watcher = MakeUnique<FUetkxWatcher>();
		Watcher->Start();

		RegisterHmrConsoleCommands();
		RegisterPreviewTab();
		UE_LOG(LogRuiEditor, Display,
			   TEXT("ReactiveUIEditor started — .uetkx watcher armed; HMR via ReactiveUetkx.HMR.Start/Stop/Toggle"));
	}

	virtual void ShutdownModule() override
	{
		FUetkxHmrController::Get().Shutdown();
		HmrCommands.Reset();
		if (Watcher.IsValid())
		{
			Watcher->Stop();
			Watcher.Reset();
		}
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GRuiPreviewTabId);
		}
		if (FModuleManager::Get().IsModuleLoaded(TEXT("MessageLog")))
		{
			FModuleManager::GetModuleChecked<FMessageLogModule>(TEXT("MessageLog"))
				.UnregisterLogListing(TEXT("ReactiveUI"));
		}
	}

private:
	/** Register the read-only `.uetkx` preview as a nomad tab + a Tools-menu entry (TD-006). */
	void RegisterPreviewTab()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return; // headless (commandlet) — no tab UI
		}
		// ONE registration only: a nomad tab grouped under Window ▸ Tools. Previously it was ALSO added
		// via a separate LevelEditor.MainMenu.Tools entry, and — with no WorkspaceMenu group — the
		// ungrouped Enabled tab floated to the top of the Window menu and double-listed. Grouping it
		// lists it exactly once, in the conventional place, and the redundant Tools-menu entry is gone.
		FGlobalTabmanager::Get()
			->RegisterNomadTabSpawner(GRuiPreviewTabId,
									  FOnSpawnTab::CreateRaw(this, &FReactiveUIEditorModule::SpawnPreviewTab))
			.SetDisplayName(NSLOCTEXT("ReactiveUI", "PreviewTabTitle", "ReactiveUI Preview"))
			.SetTooltipText(
				NSLOCTEXT("ReactiveUI", "PreviewTabTooltip", "Read-only live preview of a .uetkx component"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetMenuType(ETabSpawnerMenuType::Enabled);
	}

	TSharedRef<SDockTab> SpawnPreviewTab(const FSpawnTabArgs&)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SUetkxPreviewPanel)];
	}

	/** Start/Stop/Toggle the HMR mode from the console (the Phase-2 window + shortcuts drive the same
	 *  FUetkxHmrController; these keep it usable/scriptable meanwhile). */
	void RegisterHmrConsoleCommands()
	{
		HmrCommands.Add(MakeUnique<FAutoConsoleCommand>(
			TEXT("ReactiveUetkx.HMR.Start"), TEXT("Start ReactiveUetkx HMR (enables Live Coding mode)."),
			FConsoleCommandDelegate::CreateLambda(
				[]()
				{
					FString Error;
					if (!FUetkxHmrController::Get().Start(Error))
					{
						UE_LOG(LogRuiEditor, Warning, TEXT("[RUI HMR] start failed: %s"), *Error);
					}
				})));
		HmrCommands.Add(MakeUnique<FAutoConsoleCommand>(
			TEXT("ReactiveUetkx.HMR.Stop"), TEXT("Stop ReactiveUetkx HMR."),
			FConsoleCommandDelegate::CreateLambda([]() { FUetkxHmrController::Get().Stop(); })));
		HmrCommands.Add(MakeUnique<FAutoConsoleCommand>(
			TEXT("ReactiveUetkx.HMR.Toggle"), TEXT("Toggle ReactiveUetkx HMR on/off."),
			FConsoleCommandDelegate::CreateLambda(
				[]()
				{
					FUetkxHmrController& Controller = FUetkxHmrController::Get();
					if (Controller.IsActive())
					{
						Controller.Stop();
					}
					else
					{
						FString Error;
						if (!Controller.Start(Error))
						{
							UE_LOG(LogRuiEditor, Warning, TEXT("[RUI HMR] start failed: %s"), *Error);
						}
					}
				})));
	}

	TUniquePtr<FUetkxWatcher> Watcher;
	TArray<TUniquePtr<FAutoConsoleCommand>> HmrCommands; // ReactiveUetkx.HMR.Start/Stop/Toggle
};

IMPLEMENT_MODULE(FReactiveUIEditorModule, ReactiveUIEditor)