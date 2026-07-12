// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Framework/Docking/TabManager.h"
#include "ILiveCodingModule.h"
#include "Logging/LogMacros.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "RuiHmr.h"
#include "SUetkxPreviewPanel.h"
#include "Styling/AppStyle.h"
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

		// After a Live Coding patch, the RECOMPILED compiled components carry the user's edits — clear the
		// interp overrides that would otherwise permanently shadow them, so "rebuild for full behavior"
		// actually takes effect within the session (bughunt HMR-2).
		if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding")))
		{
			LiveCodingPatchHandle = LiveCoding->GetOnPatchCompleteDelegate().AddStatic(&FRuiHmr::ResetSession);
		}

		RegisterPreviewTab();
		UE_LOG(LogRuiEditor, Display, TEXT("ReactiveUIEditor started — .uetkx watcher armed, preview tab registered"));
	}

	virtual void ShutdownModule() override
	{
		if (LiveCodingPatchHandle.IsValid())
		{
			if (ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding")))
			{
				LiveCoding->GetOnPatchCompleteDelegate().Remove(LiveCodingPatchHandle);
			}
			LiveCodingPatchHandle.Reset();
		}
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

	TUniquePtr<FUetkxWatcher> Watcher;
	FDelegateHandle LiveCodingPatchHandle; // clears interp overrides on a Live Coding rebuild (HMR-2)
};

IMPLEMENT_MODULE(FReactiveUIEditorModule, ReactiveUIEditor)