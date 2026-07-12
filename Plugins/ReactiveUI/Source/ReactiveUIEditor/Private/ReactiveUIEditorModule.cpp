// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Framework/Docking/TabManager.h"
#include "Logging/LogMacros.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "SUetkxPreviewPanel.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UetkxWatcher.h"
#include "Widgets/Docking/SDockTab.h"

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

		RegisterPreviewTab();
		UE_LOG(LogRuiEditor, Display, TEXT("ReactiveUIEditor started — .uetkx watcher armed, preview tab registered"));
	}

	virtual void ShutdownModule() override
	{
		if (Watcher.IsValid())
		{
			Watcher->Stop();
			Watcher.Reset();
		}
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GRuiPreviewTabId);
		}
		UToolMenus::UnRegisterStartupCallback(this);
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
		FGlobalTabmanager::Get()
			->RegisterNomadTabSpawner(GRuiPreviewTabId,
									  FOnSpawnTab::CreateRaw(this, &FReactiveUIEditorModule::SpawnPreviewTab))
			.SetDisplayName(NSLOCTEXT("ReactiveUI", "PreviewTabTitle", "ReactiveUI Preview"))
			.SetTooltipText(
				NSLOCTEXT("ReactiveUI", "PreviewTabTooltip", "Read-only live preview of a .uetkx component"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"))
			.SetMenuType(ETabSpawnerMenuType::Enabled);

		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FReactiveUIEditorModule::RegisterToolsMenu));
	}

	void RegisterToolsMenu()
	{
		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))
		{
			FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Tools"));
			Section.AddMenuEntry(TEXT("OpenReactiveUIPreview"),
								 NSLOCTEXT("ReactiveUI", "OpenPreview", "ReactiveUI Preview"),
								 NSLOCTEXT("ReactiveUI", "OpenPreviewTip", "Open the read-only .uetkx preview tab"),
								 FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"),
								 FUIAction(FExecuteAction::CreateLambda(
									 []() { FGlobalTabmanager::Get()->TryInvokeTab(GRuiPreviewTabId); })));
		}
	}

	TSharedRef<SDockTab> SpawnPreviewTab(const FSpawnTabArgs&)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SUetkxPreviewPanel)];
	}

	TUniquePtr<FUetkxWatcher> Watcher;
};

IMPLEMENT_MODULE(FReactiveUIEditorModule, ReactiveUIEditor)