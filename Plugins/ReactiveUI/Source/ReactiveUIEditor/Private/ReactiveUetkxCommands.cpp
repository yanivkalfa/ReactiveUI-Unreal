// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "ReactiveUetkxCommands.h"

#include "Framework/Docking/TabManager.h"
#include "Logging/LogMacros.h"
#include "ReactiveUetkxMenu.h"
#include "Styling/AppStyle.h"
#include "UetkxHmrController.h"

#define LOCTEXT_NAMESPACE "ReactiveUetkx"

DEFINE_LOG_CATEGORY_STATIC(LogRuiCmd, Log, All);

FReactiveUetkxCommands::FReactiveUetkxCommands()
	: TCommands<FReactiveUetkxCommands>(TEXT("ReactiveUetkx"), LOCTEXT("ContextDesc", "ReactiveUetkx"),
										NAME_None, FAppStyle::GetAppStyleSetName())
{
}

void FReactiveUetkxCommands::RegisterCommands()
{
	// Default UNBOUND — FInputChord() is empty, so nothing fires until the owner binds it.
	UI_COMMAND(ToggleHmr, "Toggle HMR", "Start or stop the ReactiveUetkx Hot Reload mode.",
			   EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ToggleHmrWindow, "Toggle HMR Window", "Open or close the ReactiveUetkx Hot Reload window.",
			   EUserInterfaceActionType::Button, FInputChord());
}

void FReactiveUetkxCommands::ExecuteToggleHmr()
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
			UE_LOG(LogRuiCmd, Warning, TEXT("[ReactiveUetkx] Toggle HMR — start failed: %s"), *Error);
		}
	}
}

void FReactiveUetkxCommands::ExecuteToggleHmrWindow()
{
	const FTabId TabId(ReactiveUetkxTabs::HmrWindow);
	if (TSharedPtr<SDockTab> Existing = FGlobalTabmanager::Get()->FindExistingLiveTab(TabId))
	{
		Existing->RequestCloseTab();
	}
	else
	{
		FGlobalTabmanager::Get()->TryInvokeTab(ReactiveUetkxTabs::HmrWindow);
	}
}

bool FReactiveUetkxInputProcessor::HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& KeyEvent)
{
	// Ignore bare modifier presses — wait for the real key that completes the chord.
	const FKey Key = KeyEvent.GetKey();
	if (Key.IsModifierKey())
	{
		return false;
	}
	const FInputChord Chord(Key, KeyEvent.IsShiftDown(), KeyEvent.IsControlDown(), KeyEvent.IsAltDown(),
							KeyEvent.IsCommandDown());

	const FReactiveUetkxCommands& Commands = FReactiveUetkxCommands::Get();
	if (Commands.ToggleHmr.IsValid() && Commands.ToggleHmr->HasActiveChord(Chord))
	{
		FReactiveUetkxCommands::ExecuteToggleHmr();
		return true; // consume — this was our shortcut
	}
	if (Commands.ToggleHmrWindow.IsValid() && Commands.ToggleHmrWindow->HasActiveChord(Chord))
	{
		FReactiveUetkxCommands::ExecuteToggleHmrWindow();
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
