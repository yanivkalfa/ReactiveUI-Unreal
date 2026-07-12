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

	// --- recent errors ---
	void RebuildErrorList();

	/** The controller fired OnStatusChanged → repaint the parts that are not attribute-bound. */
	void OnControllerStatusChanged();

	TSharedPtr<SVerticalBox> ErrorListBox;
	FDelegateHandle StatusChangedHandle;
	uint64 BaselineRamBytes = 0; // RAM at window open, for the "+N since open" delta
};
