// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// HMR v2 Phase 3 (D-HMR-6) — the two ReactiveUetkx editor commands, the C++ sibling of the Unity
// toolkit's UitkxHmrKeybinds. Both default UNBOUND (empty chord); the owner binds them in Editor
// Preferences ▸ Keyboard Shortcuts or the in-window recorder — the binding manager is the single store.
// FReactiveUetkxInputProcessor makes them fire globally (any focused editor window), the way Unity's
// global event handler does, since these aren't scoped to one widget's command list.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Commands/Commands.h"

class FReactiveUetkxCommands : public TCommands<FReactiveUetkxCommands>
{
public:
	FReactiveUetkxCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> ToggleHmr;		 // Start/Stop the HMR mode
	TSharedPtr<FUICommandInfo> ToggleHmrWindow;	 // open/close the Hot Reload window

	/** Run the action a command is bound to (shared by the input processor + any menu wiring). */
	static void ExecuteToggleHmr();
	static void ExecuteToggleHmrWindow();
};

/** A global key preprocessor: on every KeyDown it matches the event against the two commands' active
 *  chords and fires the action. Unbound commands (empty chord) never match. */
class FReactiveUetkxInputProcessor : public IInputProcessor
{
public:
	virtual ~FReactiveUetkxInputProcessor() override = default;

	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("ReactiveUetkxInput"); }
};
