// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// HMR v2 Phase 2 — the ReactiveUetkx Hot Reload window (nomad tab). The C++ sibling of the Unity
// toolkit's UitkxHmrWindow: a Start/Stop control over FUetkxHmrController plus a live read-out
// (ACTIVE/Idle, watched globs, swaps/errors/last, RAM, the "external builds pause" warning, and a
// recent-errors tail). It only reads the controller — all HMR behaviour lives there. Repaints on the
// controller's OnStatusChanged and on a slow timer (IsCompiling + RAM), never per frame.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

class SReactiveUetkxHmrPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReactiveUetkxHmrPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SReactiveUetkxHmrPanel() override;

	// Keyboard capture for the in-window shortcut recorder.
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	// --- Start/Stop ---
	FReply OnToggleClicked();
	FText GetToggleLabel() const;

	// --- status line ---
	FText GetStateText() const;
	FSlateColor GetStateColor() const;

	// --- stats ---
	FText GetSwapsText() const;
	FText GetErrorsText() const;
	FText GetLastText() const;
	FText GetRamText() const;

	// --- settings checkboxes ---
	ECheckBoxState IsNotificationsChecked() const;
	void OnNotificationsChanged(ECheckBoxState NewState);
	ECheckBoxState IsVerboseChecked() const;
	void OnVerboseChanged(ECheckBoxState NewState);

	// --- shortcut recorder (RecordIndex: 0 = Toggle HMR, 1 = Toggle Window) ---
	TSharedRef<class SWidget> BuildShortcutRow(int32 RecordIndex, const FText& Label);
	FText GetShortcutText(int32 RecordIndex) const;
	FReply OnRecordClicked(int32 RecordIndex);
	FReply OnClearShortcut(int32 RecordIndex);
	TSharedPtr<class FUICommandInfo> CommandFor(int32 RecordIndex) const;

	// --- recent errors ---
	void RebuildErrorList();

	/** The controller fired OnStatusChanged → repaint the parts that are not attribute-bound. */
	void OnControllerStatusChanged();

	TSharedPtr<SVerticalBox> ErrorListBox;
	FDelegateHandle StatusChangedHandle;
	uint64 BaselineRamBytes = 0;	 // RAM at window open, for the "+N since open" delta
	int32 RecordingIndex = INDEX_NONE; // which shortcut row is capturing a key (INDEX_NONE = not recording)
};
