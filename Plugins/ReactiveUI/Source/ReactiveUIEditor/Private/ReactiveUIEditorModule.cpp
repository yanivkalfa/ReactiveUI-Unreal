// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "ReactiveUetkxCommands.h"
#include "ReactiveUetkxMenu.h"
#include "SReactiveUetkxHmrPanel.h"
#include "SUetkxPreviewPanel.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "UetkxCodegen.h"
#include "ToolMenus.h"
#include "UetkxHmrController.h"
#include "UetkxWatcher.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiEditor, Log, All);

namespace
{
	const FName GRuiPreviewTabId(TEXT("ReactiveUIPreview"));
	const FName GRuiHmrTabId(TEXT("ReactiveUetkxHmr"));
} // namespace

class FReactiveUIEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// R13 — inject the engine's FCoreStyle brush set into the toolchain: brush-name attrs
		// (BorderImage) resolve at runtime EXCLUSIVELY through this fixed style set, so the
		// valid names are closed per engine. The toolchain has no Slate dependency (D-27) —
		// enumeration happens here and feeds both the UETKX0106 compile check (RUICompile,
		// the watcher) and the schema export (`brushNames` — LSP validation + completion).
		{
			// Candidate names come from EVERY registered style set (GetStyleKeys does not walk
			// the parent chain — WhiteBrush lives in the true core style behind the editor
			// style's parent link); a candidate counts only if it RESOLVES through the exact
			// lookup the adapter performs (GetOptionalBrush follows the chain). Note this is
			// the EDITOR environment's resolvable set — a superset of a shipped game's; typos
			// are what it exists to kill.
			const ISlateStyle& CoreStyle = FCoreStyle::Get();
			TSet<FName> Candidates;
			FSlateStyleRegistry::IterateAllStyles(
				[&Candidates](const ISlateStyle& Style)
				{
					Candidates.Append(Style.GetStyleKeys());
					return true;
				});
			Candidates.Append(CoreStyle.GetStyleKeys());
			TArray<FString> BrushNames;
			for (const FName& Key : Candidates)
			{
				if (CoreStyle.GetOptionalBrush(Key, nullptr, nullptr) != nullptr)
				{
					BrushNames.Add(Key.ToString());
				}
			}
			FUetkxCodegen::SetEnvironmentBrushNames(MoveTemp(BrushNames));
		}

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
		RegisterHmrWindowTab();

		// HMR v2 DX: let "Follow Play" drive the session from the PIE Play/Stop buttons. (Epic's Live
		// Coding console is hidden only while HMR is active — see FUetkxHmrController's console hider —
		// so the plugin never rewrites the user's Live Coding config.)
		FUetkxHmrController::Get().RegisterPieHooks();

		// Rebindable, default-unbound shortcuts (D-HMR-6) + a global key preprocessor to fire them.
		FReactiveUetkxCommands::Register();
		if (FSlateApplication::IsInitialized())
		{
			InputProcessor = MakeShared<FReactiveUetkxInputProcessor>();
			FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
		}

		// The main-menu bar isn't up yet at module load — register once ToolMenus is ready.
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateStatic(&FReactiveUetkxMenu::Register));
		UE_LOG(LogRuiEditor, Display,
			   TEXT("ReactiveUIEditor started — .uetkx watcher armed; ReactiveUetkx menu + HMR window; "
					"console: ReactiveUetkx.HMR.Start/Stop/Toggle"));
	}

	virtual void ShutdownModule() override
	{
		FReactiveUetkxMenu::Unregister();
		UToolMenus::UnRegisterStartupCallback(this);
		if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}
		InputProcessor.Reset();
		FReactiveUetkxCommands::Unregister();
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
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GRuiHmrTabId);
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

	/** Register the ReactiveUetkx Hot Reload window as a nomad tab (opened from the menu, HMR v2 Phase 2). */
	void RegisterHmrWindowTab()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return; // headless (commandlet) — no window UI
		}
		FGlobalTabmanager::Get()
			->RegisterNomadTabSpawner(GRuiHmrTabId, FOnSpawnTab::CreateRaw(this, &FReactiveUIEditorModule::SpawnHmrTab))
			.SetDisplayName(NSLOCTEXT("ReactiveUetkx", "HmrTabTitle", "ReactiveUetkx Hot Reload"))
			.SetTooltipText(
				NSLOCTEXT("ReactiveUetkx", "HmrTabTooltip", "Start/Stop .uetkx Hot Module Reload (Live Coding)"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetMenuType(ETabSpawnerMenuType::Enabled);
	}

	TSharedRef<SDockTab> SpawnHmrTab(const FSpawnTabArgs&)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SReactiveUetkxHmrPanel)];
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
	TArray<TUniquePtr<FAutoConsoleCommand>> HmrCommands;	 // ReactiveUetkx.HMR.Start/Stop/Toggle
	TSharedPtr<FReactiveUetkxInputProcessor> InputProcessor; // global shortcut handler (Phase 3)
};

IMPLEMENT_MODULE(FReactiveUIEditorModule, ReactiveUIEditor)