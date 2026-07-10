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
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"

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
	BorderProps.SetBorderColor(FLinearColor::Red);
	BorderProps.SetHAlign(FName(TEXT("center")));

	FRuiBoxProps BoxProps;
	BoxProps.SetWidth(240.0f);
	BoxProps.SetHeight(120.0f);

	FRuiSpacerProps SpacerProps;
	SpacerProps.SetSize(FVector2D(10.0f, 20.0f));

	FRuiImageProps ImageProps;
	ImageProps.SetColor(FLinearColor::Green);
	ImageProps.SetDesiredSize(FVector2D(32.0f, 32.0f));

	FRuiProgressBarProps BarProps;
	BarProps.SetPercent(Pct / 100.0f);

	return {RUI::Slate::Border(
		MoveTemp(BorderProps),
		{RUI::Slate::Box(MoveTemp(BoxProps),
						 {RUI::Slate::VBox(FRuiVerticalBoxProps(), {RUI::Slate::Spacer(MoveTemp(SpacerProps)),
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
	// auto VBox's desired height must be at least the sum of its children's desired heights.
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

	FRuiEditableTextProps Props;
	Props.SetText(FText::FromString(Gen == 0 ? TEXT("alpha") : TEXT("beta")));
	Props.SetHintText(FText::FromString(TEXT("type here")));
	Props.SetOnTextChanged(
		FRuiCallback::Create([Log](const FRuiValue& V) { *Log += TEXT("chg:") + V.TextValue.ToString() + TEXT(";"); }));
	Props.SetOnTextCommitted(FRuiCallback::Create([Log](const FRuiValue& V)
												  { *Log += TEXT("commit:") + V.TextValue.ToString() + TEXT(";"); }));
	Props.Ref = [](const FRuiHostHandle& H) { WidgetTest::CapturedNode = H; };
	return {RUI::Slate::EditableText(MoveTemp(Props))};
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
	Node->Proxy->HandleText(FText::FromString(TEXT("x")), static_cast<int32>(FRuiEditableTextProps::OnTextChanged_Bit));
	Node->Proxy->HandleTextCommit(FText::FromString(TEXT("y")), ETextCommit::OnEnter,
								  static_cast<int32>(FRuiEditableTextProps::OnTextCommitted_Bit));
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
	Check.SetOnCheckedChanged(
		FRuiCallback::Create([Log](const FRuiValue& V) { *Log += V.BoolValue ? TEXT("on;") : TEXT("off;"); }));

	FRuiSliderProps Slide;
	Slide.SetValue(State != 0 ? 0.75f : 0.25f);
	Slide.SetOnValueChanged(
		FRuiCallback::Create([Log](const FRuiValue& V) { *Log += FString::Printf(TEXT("v%.2f;"), V.FloatValue); }));

	return {RUI::Slate::VBox(FRuiVerticalBoxProps(), {RUI::Slate::CheckBox(MoveTemp(Check), {RUI::Text(TEXT("opt"))}),
													  RUI::Slate::Slider(MoveTemp(Slide))})};
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
		Items.Add(RUI::Text(FString::Printf(TEXT("item %d"), i)));
	}
	return {RUI::Slate::VBox(FRuiVerticalBoxProps(), {RUI::Slate::ScrollBox(MoveTemp(ScrollProps), MoveTemp(Items)),
													  RUI::Slate::Canvas(MoveTemp(CanvasProps))})};
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

#endif // WITH_DEV_AUTOMATION_TESTS
