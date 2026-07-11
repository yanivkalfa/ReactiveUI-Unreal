// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Shared building blocks for the demo screens (C++ builder sugar until Phase 3 markup).

#pragma once

#include "CoreMinimal.h"
#include "RuiCoreElements.h"
#include "RuiNode.h"
#include "RuiSlateElements.h"

namespace RuiDemo
{
	/** Section card: dark translucent backdrop + inner padding + a gap above (Slot.*). */
	inline FRuiNode Padded(FRuiNode Inner, float Padding = 8.0f)
	{
		FRuiBorderProps P;
		P.SetPadding(FMargin(Padding + 4.0f));
		P.SetBorderImage(FName(TEXT("WhiteBrush"))); // solid fill (the engine default brush is a thin frame)
		P.SetBorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f));
		FRuiNode Node = RUI::Slate::Border(MoveTemp(P), {MoveTemp(Inner)});
		TSharedRef<FRuiBorderProps> Props =
			MakeShared<FRuiBorderProps>(static_cast<const FRuiBorderProps&>(*Node.Props));
		Props->SlotProps = MakeShared<FRuiStyleDict>();
		Props->SlotProps->Add(FName(TEXT("Slot.Padding")), FRuiValue(TEXT("0,10,0,0")));
		Node.Props = Props;
		return Node;
	}

	/** Breathing room between rows inside a section. */
	inline FRuiNode Gap(float Height = 6.0f)
	{
		FRuiSpacerProps P;
		P.SetSize(FVector2D(1.0f, Height));
		return RUI::Slate::Spacer(MoveTemp(P));
	}

	inline FRuiNode StyledText(const FString& S, float FontSize, FLinearColor Color = FLinearColor::White)
	{
		FRuiNode Node = RUI::TextBlock(S);
		TSharedRef<FRuiTextBlockProps> Props =
			MakeShared<FRuiTextBlockProps>(static_cast<const FRuiTextBlockProps&>(*Node.Props));
		Props->Style = MakeShared<FRuiStyleDict>();
		Props->Style->Add(FName(TEXT("Font.Size")), FRuiValue(FontSize));
		Props->Style->Add(FName(TEXT("ColorAndOpacity")), FRuiValue(Color));
		Node.Props = Props;
		return Node;
	}

	inline FRuiNode LabeledButton(const FString& Label, TFunction<void()> OnClick)
	{
		FRuiButtonProps P;
		P.SetOnClicked(FRuiCallback::Create(MoveTemp(OnClick)));
		P.SetContentPadding(FMargin(12.0f, 4.0f));
		FRuiNode Node = RUI::Slate::Button(MoveTemp(P), {RUI::TextBlock(Label)});
		TSharedRef<FRuiButtonProps> Props =
			MakeShared<FRuiButtonProps>(static_cast<const FRuiButtonProps&>(*Node.Props));
		Props->SlotProps = MakeShared<FRuiStyleDict>();
		Props->SlotProps->Add(FName(TEXT("Slot.Padding")), FRuiValue(TEXT("0,0,6,0")));
		Node.Props = Props;
		return Node;
	}

	/** Attach Slot.* props to any node (per-node copy; demo authoring sugar). */
	inline FRuiNode WithSlot(FRuiNode Node, const FName Key, FRuiValue Value)
	{
		TSharedRef<FRuiPropsBase> Props = ConstCastSharedRef<FRuiPropsBase>(Node.Props.ToSharedRef());
		if (!Props->SlotProps.IsValid())
		{
			Props->SlotProps = MakeShared<FRuiStyleDict>();
		}
		Props->SlotProps->Add(Key, MoveTemp(Value));
		return Node;
	}

} // namespace RuiDemo
