// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "SUetkxPreviewPanel.h"

#include "UetkxPreview.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ReactiveUIPreview"

void SUetkxPreviewPanel::Construct(const FArguments&)
{
	// clang-format off
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SAssignNew(PathBox, SEditableTextBox)
				.HintText(LOCTEXT("PathHint", "path to a .uetkx file"))
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Type)
				{
					if (Type == ETextCommit::OnEnter) { OnLoadClicked(); }
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Load", "Load"))
				.OnClicked(this, &SUetkxPreviewPanel::OnLoadClicked)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4)
		[
			SAssignNew(PreviewArea, SBorder)
			[
				SNew(STextBlock).Text(LOCTEXT("Empty", "Load a .uetkx file to preview its component."))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().MaxHeight(160).Padding(4)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(MessageList, SVerticalBox)
			]
		]
	];
	// clang-format on
}

FReply SUetkxPreviewPanel::OnLoadClicked()
{
	if (PathBox.IsValid())
	{
		PreviewFile(PathBox->GetText().ToString());
	}
	return FReply::Handled();
}

void SUetkxPreviewPanel::PreviewFile(const FString& FilePath)
{
	Preview = FUetkxPreview::FromFile(FilePath);
	Rebuild();
}

void SUetkxPreviewPanel::Rebuild()
{
	if (PreviewArea.IsValid())
	{
		PreviewArea->SetContent(Preview.IsValid() ? Preview->GetWidget() : SNullWidget::NullWidget);
	}
	if (MessageList.IsValid())
	{
		MessageList->ClearChildren();
		if (Preview.IsValid())
		{
			for (const FString& Message : Preview->GetMessages())
			{
				MessageList->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Message))];
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
