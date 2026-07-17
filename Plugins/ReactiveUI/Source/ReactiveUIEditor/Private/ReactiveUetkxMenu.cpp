// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "ReactiveUetkxMenu.h"

#include "Framework/Docking/TabManager.h"
#include "Logging/LogMacros.h"
#include "Logging/MessageLog.h"
#include "RuiReconciler.h"
#include "ToolMenus.h"
#include "UetkxHmrController.h"

#define LOCTEXT_NAMESPACE "ReactiveUetkx"

DEFINE_LOG_CATEGORY_STATIC(LogRuiMenu, Log, All);

namespace ReactiveUetkxTabs
{
	const FName HmrWindow(TEXT("ReactiveUetkxHmr"));
	const FName Preview(TEXT("ReactiveUIPreview"));
} // namespace ReactiveUetkxTabs

namespace
{
	const FName GMainMenuName(TEXT("LevelEditor.MainMenu"));
	const FName GRootMenuName(TEXT("LevelEditor.MainMenu.ReactiveUetkx"));

	void InvokeTab(FName TabId)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(TabId);
	}

	void CheckRegistry()
	{
		const FUetkxHmrController& Controller = FUetkxHmrController::Get();
		int32 LiveRoots = 0;
		FRuiReconciler::ForEachLive([&LiveRoots](FRuiReconciler&) { ++LiveRoots; });
		const FUetkxHmrStatus& Status = Controller.GetStatus();
		UE_LOG(LogRuiMenu, Display, TEXT("[ReactiveUetkx] HMR %s — live roots: %d, swaps: %d, errors: %d"),
			   Controller.IsActive() ? TEXT("ACTIVE") : TEXT("idle"), LiveRoots, Status.Swaps, Status.Errors);
	}

	void FillRootMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Main = Menu->FindOrAddSection("Main");
		Main.AddMenuEntry("HmrMode", LOCTEXT("HmrMode", "HMR Mode"),
						  LOCTEXT("HmrModeTip", "Open the ReactiveUetkx Hot Reload window (Start/Stop HMR)."),
						  FSlateIcon(),
						  FUIAction(FExecuteAction::CreateStatic(&InvokeTab, ReactiveUetkxTabs::HmrWindow)));
		Main.AddMenuEntry("Preview", LOCTEXT("Preview", "Preview"),
						  LOCTEXT("PreviewTip", "Open the read-only .uetkx component preview."), FSlateIcon(),
						  FUIAction(FExecuteAction::CreateStatic(&InvokeTab, ReactiveUetkxTabs::Preview)));
		// §3 (MARKUP_EVERYWHERE_PLAN): one-click access to the compile-error page (TB-8's
		// clickable rows live there) — the engine parks it under Window's LOG section header,
		// which the owner scrolled straight past.
		Main.AddMenuEntry(
			"MessageLog", LOCTEXT("MessageLog", "Message Log"),
			LOCTEXT("MessageLogTip", "Open the ReactiveUI Message Log page (.uetkx compile errors with jump-to-file links)."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda(
				[]() { FMessageLog(TEXT("ReactiveUI")).Open(EMessageSeverity::Info, /*bOpenEvenIfEmpty*/ true); })));

		FToolMenuSection& Debug = Menu->FindOrAddSection("Debug");
		Debug.AddSubMenu("Debug", LOCTEXT("Debug", "Debug"), LOCTEXT("DebugTip", "ReactiveUetkx diagnostics."),
						 FNewToolMenuDelegate::CreateLambda(
							 [](UToolMenu* Sub)
							 {
								 FToolMenuSection& S = Sub->FindOrAddSection("DebugSection");
								 S.AddMenuEntry("CheckRegistry", LOCTEXT("CheckRegistry", "Check Registry"),
												LOCTEXT("CheckRegistryTip",
														"Log the HMR status + live reconciler roots to LogRuiEditor."),
												FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&CheckRegistry)));
							 }));
	}
} // namespace

void FReactiveUetkxMenu::Register()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (ToolMenus == nullptr || ToolMenus->IsMenuRegistered(GRootMenuName))
	{
		return;
	}

	// One top-level menu on the main bar (next to Window / Tools / Help).
	UToolMenu* MainMenu = ToolMenus->ExtendMenu(GMainMenuName);
	if (MainMenu == nullptr)
	{
		return;
	}
	FToolMenuSection& Section = MainMenu->FindOrAddSection("ReactiveUetkx");
	Section.AddSubMenu("ReactiveUetkx", LOCTEXT("MenuLabel", "ReactiveUetkx"),
					   LOCTEXT("MenuTip", "ReactiveUI (.uetkx) — Hot Reload, preview, diagnostics."),
					   FNewToolMenuDelegate::CreateStatic(&FillRootMenu));
}

void FReactiveUetkxMenu::Unregister()
{
	if (UToolMenus* ToolMenus = UToolMenus::Get())
	{
		ToolMenus->RemoveMenu(GRootMenuName);
	}
}

#undef LOCTEXT_NAMESPACE
