// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiDemoScreens.h"

#include "RuiContext.h"
#include "RuiDemoShared.h"
#include "RuiStyle.h"

using namespace RuiDemo;

namespace RuiDemo
{
	const TArray<FRuiDemoEntry>& GetGalleryEntries()
	{
		static const TArray<FRuiDemoEntry> Entries = {
			{TEXT("Hello World"), &HelloWorldScreen}, {TEXT("Counter"), &CounterScreen},
			{TEXT("Text Field"), &TextFieldScreen},	  {TEXT("Use Effect"), &UseEffectScreen},
			{TEXT("Signals"), &SignalCounterScreen},  {TEXT("Context"), &ContextScreen},
			{TEXT("Keyed Diff"), &KeyedDiffScreen},	  {TEXT("Styled Panels"), &StyledPanelsScreen},
			{TEXT("Tic Tac Toe"), &TicTacToeScreen},  {TEXT("Custom Draw"), &CustomDrawScreen},
			{TEXT("Stress Test"), &StressTestScreen},
		};
		return Entries;
	}
} // namespace RuiDemo

// ── the shell: menu column + selected screen (switching remounts — cleanups exercised) ────

static FRuiNodeArray GalleryShellComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Selected, SetSelected] = Ctx.UseState<int32>(0);
	TFunction<void(int32)> Select = SetSelected;
	const int32 SelectedNow = Selected;

	Ctx.UseEffect(
		[]()
		{
			FRuiStyleDict Dim;
			Dim.Add(FName(TEXT("RenderOpacity")), FRuiValue(0.35f));
			RUI::Slate::RegisterStyleClass(FName(TEXT("rui-demo-dim")), MoveTemp(Dim));
		},
		RUI::Deps());

	const TArray<FRuiDemoEntry>& Entries = GetGalleryEntries();

	TArray<FRuiNode> MenuRows;
	MenuRows.Add(StyledText(TEXT("Examples"), 14.0f, FLinearColor(0.7f, 0.7f, 0.8f)));
	MenuRows.Add(Gap(4.0f));
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		FRuiButtonProps P;
		P.SetOnClicked(FRuiCallback::Create([Select, i]() { Select(i); }));
		P.SetContentPadding(FMargin(10.0f, 3.0f));
		FRuiNode Row = RUI::Slate::Button(
			MoveTemp(P), {RUI::TextBlock((i == SelectedNow ? TEXT("> ") : TEXT("  ")) + Entries[i].Name)});
		Row.Key = FRuiKey(i);
		FRuiNode Spaced = WithSlot(MoveTemp(Row), FName(TEXT("Slot.Padding")), FRuiValue(TEXT("0,0,0,3")));
		MenuRows.Add(WithSlot(MoveTemp(Spaced), FName(TEXT("Slot.HAlign")), FRuiValue(FName(TEXT("fill")))));
	}

	FRuiBorderProps MenuCard;
	MenuCard.SetPadding(FMargin(8.0f));
	MenuCard.SetBorderImage(FName(TEXT("WhiteBrush")));
	MenuCard.SetBorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f));

	FRuiBoxProps MenuWidth;
	MenuWidth.SetWidthOverride(180.0f);
	MenuWidth.SetVAlign(FName(TEXT("top")));

	// Key the content by index: switching demos fully unmounts the old screen (cleanups run).
	FRuiNode Content = Entries[SelectedNow].Make();
	Content.Key = FRuiKey(1000 + SelectedNow);
	FRuiNode ContentSlot = WithSlot(MoveTemp(Content), FName(TEXT("Slot.Padding")), FRuiValue(TEXT("10,0,0,0")));

	FRuiBoxProps Root;
	Root.SetHAlign(FName(TEXT("left")));
	Root.SetVAlign(FName(TEXT("top")));
	return {RUI::Slate::Box(
		MoveTemp(Root), {RUI::Slate::VerticalBox(
							FRuiVerticalBoxProps(),
							{StyledText(TEXT("ReactiveUI for Unreal — example gallery"), 20.0f), Gap(4.0f),
							 RUI::Slate::HorizontalBox(
								 FRuiHorizontalBoxProps(),
								 {RUI::Slate::Box(MoveTemp(MenuWidth),
												  {RUI::Slate::Border(MoveTemp(MenuCard),
																	  {RUI::Slate::VerticalBox(FRuiVerticalBoxProps(),
																							   MoveTemp(MenuRows))})}),
								  MoveTemp(ContentSlot)})})})};
}
RUI_COMPONENT(GalleryShellComp)

namespace RuiDemo
{
	FRuiNode GalleryRoot()
	{
		return RUI::FC(&GalleryShellComp);
	}
} // namespace RuiDemo
