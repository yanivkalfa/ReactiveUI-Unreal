// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-006 — the preview tab's Slate panel: a `.uetkx` path box + Load, a live preview area (an
// FUetkxPreview mounted root), and a scrolling message list (parse diagnostics + interp notes).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FUetkxPreview;
class SBorder;
class SEditableTextBox;
class SVerticalBox;

class SUetkxPreviewPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUetkxPreviewPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Load + preview a `.uetkx` file (also the entry the tab uses); refreshes the preview + messages. */
	void PreviewFile(const FString& FilePath);

private:
	FReply OnLoadClicked();
	void Rebuild();

	TSharedPtr<SEditableTextBox> PathBox;
	TSharedPtr<SBorder> PreviewArea;
	TSharedPtr<SVerticalBox> MessageList;
	TSharedPtr<FUetkxPreview> Preview;
};
