// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Widgets.* — contract coverage for the batch-2 widgets: right Slate type
// mounted, prop rows applied (via engine getters where they exist), events through the
// real bound delegates, and the two D-16 controlled-input rules (editable-text caret
// skip-when-equal, self-notifying setter skips).

#include "Misc/AutomationTest.h"
#include "RuiContext.h"
#include "RuiRoot.h"
#include "RuiSlateElements.h"
#include "RuiSlateHost.h"
#include "SRuiCanvas.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/SRichTextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_WIDGET_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace WidgetTest
{
	static TFunction<void(int32)> IntSetter;
	static TSharedPtr<FString> Events;
	static FRuiHostHandle CapturedNode;

	static void Reset()
	{
		IntSetter = nullptr;
		Events = MakeShared<FString>();
		CapturedNode.Reset();
	}

	static TSharedPtr<SWidget> RootChild(FRuiRoot& Root, int32 Index = 0)
	{
		FChildren* Children = Root.GetWidget()->GetRootPanel()->GetChildren();
		return Children->Num() > Index ? TSharedPtr<SWidget>(Children->GetChildAt(Index)) : nullptr;
	}
} // namespace WidgetTest

// ── layout leaves + containers: Border(Box(Spacer/Image/ProgressBar)) ─────────────────────

static FRuiNodeArray WidgetsLayoutComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Pct, SetPct] = Ctx.UseState<int32>(25);
	WidgetTest::IntSetter = SetPct;

	FRuiBorderProps BorderProps;
	BorderProps.SetPadding(FMargin(8.0f));
	BorderProps.SetBorderBackgroundColor(FLinearColor::Red);
	BorderProps.SetHAlign(FName(TEXT("center")));

	FRuiBoxProps BoxProps;
	BoxProps.SetWidthOverride(240.0f);
	BoxProps.SetHeightOverride(120.0f);

	FRuiSpacerProps SpacerProps;
	SpacerProps.SetSize(FVector2D(10.0f, 20.0f));

	FRuiImageProps ImageProps;
	ImageProps.SetColorAndOpacity(FLinearColor::Green);
	ImageProps.SetDesiredSizeOverride(FVector2D(32.0f, 32.0f));

	FRuiProgressBarProps BarProps;
	BarProps.SetPercent(Pct / 100.0f);

	return {RUI::Slate::Border(
		MoveTemp(BorderProps),
		{RUI::Slate::Box(
			MoveTemp(BoxProps),
			{RUI::Slate::VerticalBox(FRuiVerticalBoxProps(), {RUI::Slate::Spacer(MoveTemp(SpacerProps)),
															  RUI::Slate::Image(MoveTemp(ImageProps)),
															  RUI::Slate::ProgressBar(MoveTemp(BarProps))})})})};
}
RUI_COMPONENT(WidgetsLayoutComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiWidgetsLayoutTest, "ReactiveUI.Widgets.Layout", RUI_WIDGET_TEST_FLAGS)
bool FRuiWidgetsLayoutTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&WidgetsLayoutComp));
	TSharedPtr<SWidget> BorderW = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("border mounted"), BorderW.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("SBorder type"), BorderW->GetType(), FName(TEXT("SBorder")));

	TSharedRef<SWidget> BoxW = BorderW->GetChildren()->GetChildAt(0);
	TestEqual(TEXT("SBox type"), BoxW->GetType(), FName(TEXT("SBox")));
	TSharedRef<SWidget> PanelW = BoxW->GetChildren()->GetChildAt(0);
	TestEqual(TEXT("inner panel"), PanelW->GetType(), FName(TEXT("SVerticalBox")));
	TestEqual(TEXT("three leaves"), PanelW->GetChildren()->Num(), 3);
	TestEqual(TEXT("spacer type"), PanelW->GetChildren()->GetChildAt(0)->GetType(), FName(TEXT("SSpacer")));
	TestEqual(TEXT("image type"), PanelW->GetChildren()->GetChildAt(1)->GetType(), FName(TEXT("SImage")));
	TestEqual(TEXT("progress type"), PanelW->GetChildren()->GetChildAt(2)->GetType(), FName(TEXT("SProgressBar")));

	// The box sizes the layout: prepass then check desired size honors the overrides.
	BorderW->SlatePrepass(1.0f);
	TestEqual(TEXT("SBox width override"), BoxW->GetDesiredSize().X, 240.0f);
	TestEqual(TEXT("SBox height override"), BoxW->GetDesiredSize().Y, 120.0f);

	// Regression (owner playtest): box-panel slots must be AUTO-size by default — Slate's
	// own default is FILL, which squeezes every row (clipped titles, crushed inputs). An
	// auto VerticalBox's desired height must be at least the sum of its children's desired heights.
	float ChildSum = 0.0f;
	for (int32 i = 0; i < PanelW->GetChildren()->Num(); ++i)
	{
		ChildSum += PanelW->GetChildren()->GetChildAt(i)->GetDesiredSize().Y;
	}
	TestTrue(TEXT("auto slots: panel desired height covers all children"),
			 PanelW->GetDesiredSize().Y >= ChildSum - 0.5f);
	return true;
}

// ── controlled editable text: the D-16 caret rule ─────────────────────────────────────────

static FRuiNodeArray WidgetsEditComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Gen, SetGen] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetGen;
	TSharedPtr<FString> Log = WidgetTest::Events;

	FRuiEditableTextBoxProps Props;
	Props.SetText(FText::FromString(Gen == 0 ? TEXT("alpha") : TEXT("beta")));
	Props.SetHintText(FText::FromString(TEXT("type here")));
	Props.SetOnTextChanged(
		FRuiCallback::Create([Log](const FRuiValue& V) { *Log += TEXT("chg:") + V.TextValue.ToString() + TEXT(";"); }));
	Props.SetOnTextCommitted(FRuiCallback::Create([Log](const FRuiValue& V)
												  { *Log += TEXT("commit:") + V.TextValue.ToString() + TEXT(";"); }));
	Props.Ref = [](const FRuiHostHandle& H) { WidgetTest::CapturedNode = H; };
	return {RUI::Slate::EditableTextBox(MoveTemp(Props))};
}
RUI_COMPONENT(WidgetsEditComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiWidgetsEditableTest, "ReactiveUI.Widgets.EditableText", RUI_WIDGET_TEST_FLAGS)
bool FRuiWidgetsEditableTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&WidgetsEditComp));
	TSharedPtr<SWidget> W = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("editable mounted"), W.IsValid()))
	{
		return false;
	}
	SEditableTextBox& Edit = static_cast<SEditableTextBox&>(*W);
	TestEqual(TEXT("initial text applied"), Edit.GetText().ToString(), FString(TEXT("alpha")));

	AddInfo(TEXT("[caret] user-typed text that matches state is NOT re-set"));
	// Simulate the typing round-trip: the widget already holds the value the state will
	// re-render with. The adapter must skip SetText (compares against the WIDGET).
	Edit.SetText(FText::FromString(TEXT("beta")));
	// SetText is SELF-NOTIFYING (the D-16 premise): the real OnTextChanged delegate fired
	// through the proxy into the user closure.
	TestEqual(TEXT("programmatic SetText notified through the proxy"), *WidgetTest::Events, FString(TEXT("chg:beta;")));
	WidgetTest::Events->Empty();
	WidgetTest::IntSetter(1); // state now renders "beta" too
	Root->FlushSync();
	TestEqual(TEXT("text still beta"), Edit.GetText().ToString(), FString(TEXT("beta")));
	TestEqual(TEXT("the equal-value re-render did NOT re-notify (caret rule)"), *WidgetTest::Events, FString());

	AddInfo(TEXT("[events] the real bound delegates reach the swapped closures"));
	FRuiSlateNode* Node = FRuiSlateHost::Resolve(WidgetTest::CapturedNode);
	if (!TestNotNull(TEXT("ref captured the slate node"), Node) ||
		!TestTrue(TEXT("proxy exists"), Node->Proxy.IsValid()))
	{
		return false;
	}
	Node->Proxy->HandleText(FText::FromString(TEXT("x")),
							static_cast<int32>(FRuiEditableTextBoxProps::OnTextChanged_Bit));
	Node->Proxy->HandleTextCommit(FText::FromString(TEXT("y")), ETextCommit::OnEnter,
								  static_cast<int32>(FRuiEditableTextBoxProps::OnTextCommitted_Bit));
	TestEqual(TEXT("both handlers fired"), *WidgetTest::Events, FString(TEXT("chg:x;commit:y;")));
	return true;
}

// ── checkbox + slider: self-notifying skips + events ──────────────────────────────────────

static FRuiNodeArray WidgetsInputComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [State, SetState] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetState;
	TSharedPtr<FString> Log = WidgetTest::Events;

	FRuiCheckBoxProps Check;
	Check.SetbIsChecked(State != 0);
	Check.SetOnCheckStateChanged(
		FRuiCallback::Create([Log](const FRuiValue& V) { *Log += V.BoolValue ? TEXT("on;") : TEXT("off;"); }));

	FRuiSliderProps Slide;
	Slide.SetValue(State != 0 ? 0.75f : 0.25f);
	Slide.SetOnValueChanged(
		FRuiCallback::Create([Log](const FRuiValue& V) { *Log += FString::Printf(TEXT("v%.2f;"), V.FloatValue); }));

	return {RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{RUI::Slate::CheckBox(MoveTemp(Check), {RUI::TextBlock(TEXT("opt"))}), RUI::Slate::Slider(MoveTemp(Slide))})};
}
RUI_COMPONENT(WidgetsInputComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiWidgetsInputTest, "ReactiveUI.Widgets.CheckSlider", RUI_WIDGET_TEST_FLAGS)
bool FRuiWidgetsInputTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&WidgetsInputComp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	SCheckBox& Check = static_cast<SCheckBox&>(Panel->GetChildren()->GetChildAt(0).Get());
	SSlider& Slide = static_cast<SSlider&>(Panel->GetChildren()->GetChildAt(1).Get());
	TestEqual(TEXT("checkbox type"), Check.GetType(), FName(TEXT("SCheckBox")));
	TestEqual(TEXT("slider type"), Slide.GetType(), FName(TEXT("SSlider")));
	TestFalse(TEXT("unchecked at state 0"), Check.IsChecked());
	TestEqual(TEXT("slider value applied"), Slide.GetValue(), 0.25f);

	AddInfo(TEXT("[inputs] state flip drives both widgets"));
	WidgetTest::IntSetter(1);
	Root->FlushSync();
	TestTrue(TEXT("checked at state 1"), Check.IsChecked());
	TestEqual(TEXT("slider moved"), Slide.GetValue(), 0.75f);
	return true;
}

// ── scrollbox + canvas ────────────────────────────────────────────────────────────────────

static FRuiNodeArray WidgetsScrollCanvasComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	// Draw fn wrapped ONCE via UseMemo — identity survives re-renders (the D-12 rule).
	const TSharedPtr<FRuiDrawFn>& Draw = Ctx.UseMemo<TSharedPtr<FRuiDrawFn>>(
		[]()
		{
			return RUI::Slate::MakeDrawFn([](const FGeometry&, FSlateWindowElementList&, int32 LayerId) -> int32
										  { return LayerId; });
		},
		RUI::Deps());

	FRuiCanvasProps CanvasProps;
	CanvasProps.SetDrawFn(Draw);
	CanvasProps.SetCanvasSize(FVector2D(64.0f, 48.0f));

	FRuiScrollBoxProps ScrollProps;
	ScrollProps.SetOrientation(FName(TEXT("vertical")));

	TArray<FRuiNode> Items;
	for (int32 i = 0; i < 5; ++i)
	{
		Items.Add(RUI::TextBlock(FString::Printf(TEXT("item %d"), i)));
	}
	return {
		RUI::Slate::VerticalBox(FRuiVerticalBoxProps(), {RUI::Slate::ScrollBox(MoveTemp(ScrollProps), MoveTemp(Items)),
														 RUI::Slate::RuiCanvas(MoveTemp(CanvasProps))})};
}
RUI_COMPONENT(WidgetsScrollCanvasComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiWidgetsScrollCanvasTest, "ReactiveUI.Widgets.ScrollCanvas", RUI_WIDGET_TEST_FLAGS)
bool FRuiWidgetsScrollCanvasTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&WidgetsScrollCanvasComp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> Scroll = Panel->GetChildren()->GetChildAt(0);
	TSharedRef<SWidget> Canvas = Panel->GetChildren()->GetChildAt(1);
	TestEqual(TEXT("scrollbox type"), Scroll->GetType(), FName(TEXT("SScrollBox")));
	TestEqual(TEXT("canvas type"), Canvas->GetType(), FName(TEXT("SRuiCanvas")));

	Canvas->SlatePrepass(1.0f);
	TestTrue(TEXT("canvas desired size"), Canvas->GetDesiredSize() == FVector2D(64.0f, 48.0f));
	return true;
}

// ── batch 2 (Phase 7): WidgetSwitcher + ScaleBox + Throbber + WrapBox ──────────────────────

static FRuiNodeArray WidgetsBatch2Comp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Page, SetPage] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetPage;

	FRuiWidgetSwitcherProps SwitcherProps;
	SwitcherProps.SetWidgetIndex(Page);

	FRuiScaleBoxProps ScaleProps;
	ScaleProps.SetStretch(FName(TEXT("scaleToFit")));
	ScaleProps.SetStretchDirection(FName(TEXT("downOnly")));

	FRuiThrobberProps ThrobProps;
	ThrobProps.SetNumPieces(5);
	ThrobProps.SetAnimate(FName(TEXT("verticalAndOpacity")));

	FRuiWrapBoxProps WrapProps;
	WrapProps.SetOrientation(FName(TEXT("horizontal")));
	WrapProps.SetWrapSize(120.0f);

	return {RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{RUI::Slate::WidgetSwitcher(
			 MoveTemp(SwitcherProps),
			 {RUI::TextBlock(TEXT("page A")), RUI::TextBlock(TEXT("page B")), RUI::TextBlock(TEXT("page C"))}),
		 RUI::Slate::ScaleBox(MoveTemp(ScaleProps), {RUI::TextBlock(TEXT("scaled"))}),
		 RUI::Slate::Throbber(MoveTemp(ThrobProps)),
		 RUI::Slate::WrapBox(MoveTemp(WrapProps),
							 {RUI::TextBlock(TEXT("w0")), RUI::TextBlock(TEXT("w1")), RUI::TextBlock(TEXT("w2"))})})};
}
RUI_COMPONENT(WidgetsBatch2Comp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiWidgetsBatch2Test, "ReactiveUI.Widgets.Batch2", RUI_WIDGET_TEST_FLAGS)
bool FRuiWidgetsBatch2Test::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&WidgetsBatch2Comp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> SwitcherW = Panel->GetChildren()->GetChildAt(0);
	TSharedRef<SWidget> ScaleW = Panel->GetChildren()->GetChildAt(1);
	TSharedRef<SWidget> ThrobW = Panel->GetChildren()->GetChildAt(2);
	TSharedRef<SWidget> WrapW = Panel->GetChildren()->GetChildAt(3);

	// Right Slate types mounted.
	TestEqual(TEXT("switcher type"), SwitcherW->GetType(), FName(TEXT("SWidgetSwitcher")));
	TestEqual(TEXT("scalebox type"), ScaleW->GetType(), FName(TEXT("SScaleBox")));
	TestEqual(TEXT("throbber type"), ThrobW->GetType(), FName(TEXT("SThrobber")));
	TestEqual(TEXT("wrapbox type"), WrapW->GetType(), FName(TEXT("SWrapBox")));

	// WidgetSwitcher: three pages, index prop applied.
	SWidgetSwitcher& Switcher = static_cast<SWidgetSwitcher&>(*SwitcherW);
	TestEqual(TEXT("switcher has 3 pages"), Switcher.GetNumWidgets(), 3);
	TestEqual(TEXT("active index applied"), Switcher.GetActiveWidgetIndex(), 0);

	// ScaleBox wraps one child; WrapBox flowed three children.
	TestEqual(TEXT("scalebox has content"), ScaleW->GetChildren()->Num(), 1);
	TestEqual(TEXT("wrapbox has 3 children"), WrapW->GetChildren()->Num(), 3);

	// State flip drives the switcher index (runtime setter path).
	AddInfo(TEXT("[switcher] state flip retargets the active page"));
	WidgetTest::IntSetter(2);
	Root->FlushSync();
	TestEqual(TEXT("active index retargeted"), Switcher.GetActiveWidgetIndex(), 2);
	return true;
}

// ── batch 2b: text inputs + safe containers + Separator (TD-011 reconstruct mask) ─────────

static FRuiNodeArray WidgetsBatch2bComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	// state 0: thin/white · 1: thin/red (runtime color only) · 2: thick/red (construct change)
	auto [Phase, SetPhase] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetPhase;

	FRuiMultiLineEditableTextBoxProps MultiProps;
	MultiProps.SetText(FText::FromString(TEXT("line one")));
	MultiProps.SetHintText(FText::FromString(TEXT("notes")));

	FRuiSearchBoxProps SearchProps;
	SearchProps.SetHintText(FText::FromString(TEXT("search")));

	FRuiSafeZoneProps SafeProps;
	SafeProps.SetbIsTitleSafe(true);

	FRuiDPIScalerProps DpiProps;
	DpiProps.SetDPIScale(1.5f);

	FRuiSeparatorProps SepProps;
	SepProps.SetThickness(Phase >= 2 ? 6.0f : 2.0f);
	SepProps.SetColorAndOpacity(Phase >= 1 ? FLinearColor::Red : FLinearColor::White);

	return {RUI::Slate::VerticalBox(FRuiVerticalBoxProps(),
									{RUI::Slate::MultiLineEditableTextBox(MoveTemp(MultiProps)),
									 RUI::Slate::SearchBox(MoveTemp(SearchProps)),
									 RUI::Slate::SafeZone(MoveTemp(SafeProps), {RUI::TextBlock(TEXT("safe"))}),
									 RUI::Slate::DPIScaler(MoveTemp(DpiProps), {RUI::TextBlock(TEXT("scaled"))}),
									 RUI::Slate::Separator(MoveTemp(SepProps))})};
}
RUI_COMPONENT(WidgetsBatch2bComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiWidgetsBatch2bTest, "ReactiveUI.Widgets.Batch2b", RUI_WIDGET_TEST_FLAGS)
bool FRuiWidgetsBatch2bTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&WidgetsBatch2bComp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> MultiW = Panel->GetChildren()->GetChildAt(0);
	TSharedRef<SWidget> SearchW = Panel->GetChildren()->GetChildAt(1);
	TSharedRef<SWidget> SafeW = Panel->GetChildren()->GetChildAt(2);
	TSharedRef<SWidget> DpiW = Panel->GetChildren()->GetChildAt(3);

	TestEqual(TEXT("multiline type"), MultiW->GetType(), FName(TEXT("SMultiLineEditableTextBox")));
	TestEqual(TEXT("searchbox type"), SearchW->GetType(), FName(TEXT("SSearchBox")));
	TestEqual(TEXT("safezone type"), SafeW->GetType(), FName(TEXT("SSafeZone")));
	TestEqual(TEXT("dpiscaler type"), DpiW->GetType(), FName(TEXT("SDPIScaler")));
	TestEqual(TEXT("multiline initial text"), static_cast<SMultiLineEditableTextBox&>(*MultiW).GetText().ToString(),
			  FString(TEXT("line one")));

	// ── TD-011 in production: the Separator's Thickness is construct-only ────────────────────
	SWidget* Sep0 = &Panel->GetChildren()->GetChildAt(4).Get();
	TestEqual(TEXT("separator type"), Sep0->GetType(), FName(TEXT("SSeparator")));

	AddInfo(TEXT("[reconstruct] a runtime-only change (ColorAndOpacity) must NOT replace the widget"));
	WidgetTest::IntSetter(1);
	Root->FlushSync();
	SWidget* Sep1 = &Panel->GetChildren()->GetChildAt(4).Get();
	TestEqual(TEXT("color-only change reused the same SSeparator"), (void*)Sep1, (void*)Sep0);

	AddInfo(TEXT("[reconstruct] a construct-only change (Thickness) MUST replace the widget"));
	WidgetTest::IntSetter(2);
	Root->FlushSync();
	SWidget* Sep2 = &Panel->GetChildren()->GetChildAt(4).Get();
	TestNotEqual(TEXT("thickness change replaced the SSeparator"), (void*)Sep2, (void*)Sep1);
	TestEqual(TEXT("replacement is still an SSeparator"), Sep2->GetType(), FName(TEXT("SSeparator")));
	return true;
}

// ── batch 2c: SpinBox + UniformWrapPanel + RichTextBlock + Grid/UniformGrid panels ─────────

static FRuiNode CellBox(const TCHAR* Label, int32 Column, int32 Row)
{
	FRuiBoxProps BoxProps;
	TSharedRef<FRuiStyleDict> Slot = MakeShared<FRuiStyleDict>();
	Slot->Add(FName(TEXT("slot.column")), FRuiValue(Column));
	Slot->Add(FName(TEXT("slot.row")), FRuiValue(Row));
	BoxProps.SlotProps = Slot;
	return RUI::Slate::Box(MoveTemp(BoxProps), {RUI::TextBlock(Label)});
}

static FRuiNodeArray WidgetsBatch2cComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Val, SetVal] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetVal;

	FRuiSpinBoxProps SpinProps;
	SpinProps.SetValue(Val == 0 ? 0.25f : 0.75f);
	SpinProps.SetMinValue(0.0f);
	SpinProps.SetMaxValue(1.0f);

	FRuiRichTextBlockProps RichProps;
	RichProps.SetText(FText::FromString(TEXT("rich <b>text</>")));
	RichProps.SetbAutoWrapText(true);

	return {RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{RUI::Slate::SpinBox(MoveTemp(SpinProps)),
		 RUI::Slate::UniformWrapPanel(FRuiUniformWrapPanelProps(),
									  {RUI::TextBlock(TEXT("u0")), RUI::TextBlock(TEXT("u1"))}),
		 RUI::Slate::RichTextBlock(MoveTemp(RichProps)),
		 RUI::Slate::GridPanel(FRuiGridPanelProps(), {CellBox(TEXT("g00"), 0, 0), CellBox(TEXT("g11"), 1, 1)}),
		 RUI::Slate::UniformGridPanel(FRuiUniformGridPanelProps(),
									  {CellBox(TEXT("c00"), 0, 0), CellBox(TEXT("c01"), 0, 1)})})};
}
RUI_COMPONENT(WidgetsBatch2cComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiWidgetsBatch2cTest, "ReactiveUI.Widgets.Batch2c", RUI_WIDGET_TEST_FLAGS)
bool FRuiWidgetsBatch2cTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&WidgetsBatch2cComp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	SSpinBox<float>& Spin = static_cast<SSpinBox<float>&>(Panel->GetChildren()->GetChildAt(0).Get());
	TSharedRef<SWidget> WrapW = Panel->GetChildren()->GetChildAt(1);
	SRichTextBlock& Rich = static_cast<SRichTextBlock&>(Panel->GetChildren()->GetChildAt(2).Get());
	TSharedRef<SWidget> GridW = Panel->GetChildren()->GetChildAt(3);
	TSharedRef<SWidget> UGridW = Panel->GetChildren()->GetChildAt(4);

	TestEqual(TEXT("uniformwrap type"), WrapW->GetType(), FName(TEXT("SUniformWrapPanel")));
	TestEqual(TEXT("richtext type"), Rich.GetType(), FName(TEXT("SRichTextBlock")));
	TestEqual(TEXT("gridpanel type"), GridW->GetType(), FName(TEXT("SGridPanel")));
	TestEqual(TEXT("uniformgrid type"), UGridW->GetType(), FName(TEXT("SUniformGridPanel")));

	TestEqual(TEXT("spinbox value applied"), Spin.GetValue(), 0.25f);
	TestEqual(TEXT("richtext text applied"), Rich.GetText().ToString(), FString(TEXT("rich <b>text</>")));
	TestEqual(TEXT("uniformwrap has 2 children"), WrapW->GetChildren()->Num(), 2);
	TestEqual(TEXT("gridpanel placed 2 cells"), GridW->GetChildren()->Num(), 2);
	TestEqual(TEXT("uniformgrid placed 2 cells"), UGridW->GetChildren()->Num(), 2);

	AddInfo(TEXT("[spinbox] state flip retargets the value (self-notifying skip path)"));
	WidgetTest::IntSetter(1);
	Root->FlushSync();
	TestEqual(TEXT("spinbox value retargeted"), Spin.GetValue(), 0.75f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
