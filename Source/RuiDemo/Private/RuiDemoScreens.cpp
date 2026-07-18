// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiDemoScreens.h"

#include "RuiContext.h"
#include "RuiDemoShared.h"
#include "RuiStyle.h"

using namespace RuiDemo;

namespace RuiDemo
{
	// Every screen is a COMPILED .uetkx component (Screens/*.uetkx -> committed .uetkx.inl,
	// built through RuiDemo.Uetkx.gen.cpp). The generated code self-registers named factories;
	// entries resolve through RUI::Named — the wrappers themselves are TU-local by design.
	const TArray<FRuiDemoEntry>& GetGalleryEntries()
	{
		static const TArray<FRuiDemoEntry> Entries = {
			{TEXT("Hello World"), +[]() { return RUI::Named(FName(TEXT("HelloWorld"))); }},
			{TEXT("Counter"), +[]() { return RUI::Named(FName(TEXT("SimpleCounter"))); }},
			{TEXT("Click Counter"), +[]() { return RUI::Named(FName(TEXT("ClickCounter"))); }},
			{TEXT("Text Field"), +[]() { return RUI::Named(FName(TEXT("SimpleTextField"))); }},
			{TEXT("Use Effect"), +[]() { return RUI::Named(FName(TEXT("SimpleUseEffect"))); }},
			{TEXT("Signals"), +[]() { return RUI::Named(FName(TEXT("SignalCounter"))); }},
			{TEXT("Context"), +[]() { return RUI::Named(FName(TEXT("ContextDemo"))); }},
			{TEXT("Keyed Diff"), +[]() { return RUI::Named(FName(TEXT("KeyedDiff"))); }},
			{TEXT("Styled Panels"), +[]() { return RUI::Named(FName(TEXT("StyledPanels"))); }},
			{TEXT("Tic Tac Toe"), +[]() { return RUI::Named(FName(TEXT("TicTacToe"))); }},
			{TEXT("Custom Draw"), +[]() { return RUI::Named(FName(TEXT("CustomDraw"))); }},
			{TEXT("Stress Test"), +[]() { return RUI::Named(FName(TEXT("StressTest"))); }},
			{TEXT("Router"), +[]() { return RUI::Named(FName(TEXT("RouterDemo"))); }},
			{TEXT("Doom"), +[]() { return RUI::Named(FName(TEXT("DoomGame"))); }},
			{TEXT("Acceptance Lab"), +[]() { return RUI::Named(FName(TEXT("AcceptanceLab"))); }},
			// Epic-interop pillars (compiled .uetkx like everything else).
			{TEXT("MVVM (data feed)"), +[]() { return RUI::Named(FName(TEXT("MvvmDemo"))); }},
			{TEXT("CommonUI (activation)"), +[]() { return RUI::Named(FName(TEXT("CommonUiDemo"))); }},
			{TEXT("UMG Host & Reverse MVVM"), +[]() { return RUI::Named(FName(TEXT("UmgHostDemo"))); }},
			{TEXT("Interop — all 4 pillars"), +[]() { return RUI::Named(FName(TEXT("InteropShowcase"))); }},
		};
		return Entries;
	}

	const TArray<FName>& GetCompiledScreenNames()
	{
		static const TArray<FName> Names = {
			FName(TEXT("HelloWorld")),		FName(TEXT("SimpleCounter")),	FName(TEXT("ClickCounter")),
			FName(TEXT("SimpleTextField")), FName(TEXT("SimpleUseEffect")), FName(TEXT("SignalCounter")),
			FName(TEXT("ContextDemo")),		FName(TEXT("KeyedDiff")),		FName(TEXT("StyledPanels")),
			FName(TEXT("TicTacToe")),		FName(TEXT("CustomDraw")),		FName(TEXT("StressTest")),
			FName(TEXT("RouterDemo")),		FName(TEXT("AcceptanceLab")),	FName(TEXT("MvvmDemo")),
			FName(TEXT("CommonUiDemo")),	FName(TEXT("UmgHostDemo")),		FName(TEXT("InteropShowcase")),
			FName(TEXT("DoomGame")),
		};
		return Names;
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
	// The content slot FILLS the remaining viewport (Slot.Fill both axes) — card-style demo
	// screens are top-left content and look the same, but screens that scale to their area
	// (Doom's ScaleBox letterbox) get the real estate they need to reach the window bottom.
	FRuiNode Content = Entries[SelectedNow].Make();
	Content.Key = FRuiKey(1000 + SelectedNow);
	FRuiNode ContentSlot = WithSlot(MoveTemp(Content), FName(TEXT("Slot.Padding")), FRuiValue(TEXT("10,0,0,0")));
	ContentSlot = WithSlot(MoveTemp(ContentSlot), FName(TEXT("Slot.Fill")), FRuiValue(1.0f));

	FRuiNode BodyRow = RUI::Slate::HorizontalBox(
		FRuiHorizontalBoxProps(),
		{RUI::Slate::Box(MoveTemp(MenuWidth),
						 {RUI::Slate::Border(MoveTemp(MenuCard),
											 {RUI::Slate::VerticalBox(FRuiVerticalBoxProps(), MoveTemp(MenuRows))})}),
		 MoveTemp(ContentSlot)});
	BodyRow = WithSlot(MoveTemp(BodyRow), FName(TEXT("Slot.Fill")), FRuiValue(1.0f));

	return {RUI::Slate::Box(
		FRuiBoxProps(), {RUI::Slate::VerticalBox(FRuiVerticalBoxProps(),
												 {StyledText(TEXT("ReactiveUI for Unreal — example gallery"), 20.0f),
												  Gap(4.0f), MoveTemp(BodyRow)})})};
}
RUI_COMPONENT(GalleryShellComp)

namespace RuiDemo
{
	FRuiNode GalleryRoot()
	{
		return RUI::FC(&GalleryShellComp);
	}
} // namespace RuiDemo
