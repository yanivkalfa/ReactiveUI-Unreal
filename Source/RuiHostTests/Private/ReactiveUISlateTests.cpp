// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Slate.* — the Slate host over REAL widgets, headless (widgets construct and
// mutate fine under NullRHI; nothing here needs painting). What the mock suites can't
// prove: adapter create/diff rows, panel child mechanics, slot.* props, the event proxy's
// bind-once-swap-inner chain, widget POINTER IDENTITY across re-renders (the D-12 "a
// re-render never reconstructs" claim), and FRuiRoot mount/unmount surfaces.

#include "Misc/AutomationTest.h"
#include "RuiContext.h"
#include "RuiRoot.h"
#include "RuiSlateElements.h"
#include "RuiSlateHost.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_SLATE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace SlateTest
{
	static TFunction<void(int32)> IntSetter;
	static int32 Clicks = 0;
	static FString LastClickTag;

	static void Reset()
	{
		IntSetter = nullptr;
		Clicks = 0;
		LastClickTag.Empty();
	}

	/** The SRuiRoot inner overlay's first slot widget. */
	static TSharedPtr<SWidget> RootChild(FRuiRoot& Root, int32 Index = 0)
	{
		FChildren* Children = Root.GetWidget()->GetRootPanel()->GetChildren();
		return Children->Num() > Index ? TSharedPtr<SWidget>(Children->GetChildAt(Index)) : nullptr;
	}

	static FString TextOfChildAt(SWidget& Panel, int32 Index)
	{
		FChildren* Children = Panel.GetChildren();
		if (Index >= Children->Num())
		{
			return TEXT("<no child>");
		}
		TSharedRef<SWidget> W = Children->GetChildAt(Index);
		if (W->GetType() != FName(TEXT("STextBlock")))
		{
			return TEXT("<not text>");
		}
		return StaticCastSharedRef<STextBlock>(W)->GetText().ToString();
	}
} // namespace SlateTest

// ── mount: component -> real widgets ──────────────────────────────────────────────────────

static FRuiNodeArray SlateMountComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Count, SetCount] = Ctx.UseState<int32>(1);
	SlateTest::IntSetter = SetCount;
	TArray<FRuiNode> Rows;
	for (int32 i = 0; i < Count; ++i)
	{
		Rows.Add(RUI::Text(FString::Printf(TEXT("row %d"), i)));
	}
	return {RUI::Slate::VBox(FRuiVerticalBoxProps(), MoveTemp(Rows))};
}
RUI_COMPONENT(SlateMountComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiSlateMountTest, "ReactiveUI.Slate.Mount", RUI_SLATE_TEST_FLAGS)
bool FRuiSlateMountTest::RunTest(const FString&)
{
	SlateTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&SlateMountComp));

	TSharedPtr<SWidget> Panel = SlateTest::RootChild(*Root);
	if (!TestTrue(TEXT("a panel mounted under the root overlay"), Panel.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("panel is SVerticalBox"), Panel->GetType(), FName(TEXT("SVerticalBox")));
	TestEqual(TEXT("one text row"), Panel->GetChildren()->Num(), 1);
	TestEqual(TEXT("text content"), SlateTest::TextOfChildAt(*Panel, 0), FString(TEXT("row 0")));

	AddInfo(TEXT("[slate] update grows the list through the frame queue"));
	SlateTest::IntSetter(3);
	Root->FlushSync();
	TestEqual(TEXT("three rows after update"), Panel->GetChildren()->Num(), 3);
	TestEqual(TEXT("row 2 text"), SlateTest::TextOfChildAt(*Panel, 2), FString(TEXT("row 2")));

	AddInfo(TEXT("[slate] unmount empties the overlay"));
	Root->Unmount();
	TestEqual(TEXT("root overlay empty"), Root->GetWidget()->GetRootPanel()->GetChildren()->Num(), 0);
	TestEqual(TEXT("zero live fibers"), Root->GetReconciler().NumLiveFibers(), 0);
	return true;
}

// ── pointer identity: updates NEVER reconstruct (D-12) ────────────────────────────────────

static FRuiNodeArray SlateIdentityComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Value, SetValue] = Ctx.UseState<int32>(0);
	SlateTest::IntSetter = SetValue;
	return {RUI::Slate::VBox(FRuiVerticalBoxProps(), {RUI::Text(FString::Printf(TEXT("v=%d"), Value))})};
}
RUI_COMPONENT(SlateIdentityComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiSlateIdentityTest, "ReactiveUI.Slate.PointerIdentity", RUI_SLATE_TEST_FLAGS)
bool FRuiSlateIdentityTest::RunTest(const FString&)
{
	SlateTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&SlateIdentityComp));
	TSharedPtr<SWidget> Panel = SlateTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	SWidget* PanelBefore = Panel.Get();
	SWidget* TextBefore = &Panel->GetChildren()->GetChildAt(0).Get();

	SlateTest::IntSetter(42);
	Root->FlushSync();

	TSharedPtr<SWidget> PanelAfter = SlateTest::RootChild(*Root);
	TestTrue(TEXT("panel pointer identical after re-render"), PanelAfter.Get() == PanelBefore);
	TestTrue(TEXT("text pointer identical after re-render"),
			 &PanelAfter->GetChildren()->GetChildAt(0).Get() == TextBefore);
	TestEqual(TEXT("text updated in place"), SlateTest::TextOfChildAt(*PanelAfter, 0), FString(TEXT("v=42")));
	return true;
}

// ── keyed reorder on a real panel ─────────────────────────────────────────────────────────

static FRuiNodeArray SlateReorderComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Flip, SetFlip] = Ctx.UseState<int32>(0);
	SlateTest::IntSetter = SetFlip;
	TArray<int32> Order = Flip != 0 ? TArray<int32>{2, 0, 1} : TArray<int32>{0, 1, 2};
	TArray<FRuiNode> Rows;
	for (int32 Id : Order)
	{
		Rows.Add(RUI::Text(FString::Printf(TEXT("item %d"), Id)));
		Rows.Last().Key = FRuiKey(Id);
	}
	return {RUI::Slate::VBox(FRuiVerticalBoxProps(), MoveTemp(Rows))};
}
RUI_COMPONENT(SlateReorderComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiSlateReorderTest, "ReactiveUI.Slate.KeyedReorder", RUI_SLATE_TEST_FLAGS)
bool FRuiSlateReorderTest::RunTest(const FString&)
{
	SlateTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&SlateReorderComp));
	TSharedPtr<SWidget> Panel = SlateTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	SWidget* Item2Before = &Panel->GetChildren()->GetChildAt(2).Get(); // "item 2"

	SlateTest::IntSetter(1);
	Root->FlushSync();

	TestEqual(TEXT("order after flip [2,0,1] — slot 0"), SlateTest::TextOfChildAt(*Panel, 0), FString(TEXT("item 2")));
	TestEqual(TEXT("order after flip — slot 1"), SlateTest::TextOfChildAt(*Panel, 1), FString(TEXT("item 0")));
	TestEqual(TEXT("order after flip — slot 2"), SlateTest::TextOfChildAt(*Panel, 2), FString(TEXT("item 1")));
	TestTrue(TEXT("keyed identity preserved (item 2 moved, not rebuilt)"),
			 &Panel->GetChildren()->GetChildAt(0).Get() == Item2Before);
	return true;
}

// ── the event proxy: bind-once-swap-inner ─────────────────────────────────────────────────

static FRuiNodeArray SlateButtonComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Gen, SetGen] = Ctx.UseState<int32>(0);
	SlateTest::IntSetter = SetGen;
	FRuiButtonProps Props;
	const int32 CapturedGen = Gen;
	Props.SetOnClicked(FRuiCallback::Create(
		[CapturedGen]()
		{
			++SlateTest::Clicks;
			SlateTest::LastClickTag = FString::Printf(TEXT("gen%d"), CapturedGen);
		}));
	Props.SetbEnabled(Gen % 2 == 0);
	return {RUI::Slate::Button(MoveTemp(Props), {RUI::Text(TEXT("press"))})};
}
RUI_COMPONENT(SlateButtonComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiSlateButtonTest, "ReactiveUI.Slate.EventProxy", RUI_SLATE_TEST_FLAGS)
bool FRuiSlateButtonTest::RunTest(const FString&)
{
	SlateTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&SlateButtonComp));
	TSharedPtr<SWidget> ButtonWidget = SlateTest::RootChild(*Root);
	if (!TestTrue(TEXT("button mounted"), ButtonWidget.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("widget is SButton"), ButtonWidget->GetType(), FName(TEXT("SButton")));
	TestTrue(TEXT("enabled at gen 0"), ButtonWidget->IsEnabled());
	SButton& Button = static_cast<SButton&>(*ButtonWidget);
	TestEqual(TEXT("button holds the text content"), SlateTest::TextOfChildAt(Button, 0), FString(TEXT("press")));

	// Fire the REAL Slate delegate path (bound once at construction to the proxy).
	Button.SimulateClick();
	TestEqual(TEXT("click reached the gen-0 closure"), SlateTest::LastClickTag, FString(TEXT("gen0")));
	TestEqual(TEXT("one click"), SlateTest::Clicks, 1);

	AddInfo(TEXT("[proxy] re-render swaps the inner callback without rebinding"));
	SlateTest::IntSetter(1);
	Root->FlushSync();
	TestFalse(TEXT("disabled at gen 1 (prop diff applied)"), ButtonWidget->IsEnabled());
	Button.SimulateClick();
	TestEqual(TEXT("click reached the gen-1 closure"), SlateTest::LastClickTag, FString(TEXT("gen1")));
	TestEqual(TEXT("two clicks total"), SlateTest::Clicks, 2);
	return true;
}

// ── slot.* props on a box panel ───────────────────────────────────────────────────────────

static FRuiNodeArray SlateSlotPropsComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Pad, SetPad] = Ctx.UseState<int32>(4);
	SlateTest::IntSetter = SetPad;
	FRuiNode Text = RUI::Text(TEXT("padded"));
	TSharedRef<FRuiTextProps> Props = MakeShared<FRuiTextProps>(static_cast<const FRuiTextProps&>(*Text.Props));
	Props->SlotProps = MakeShared<FRuiStyleDict>();
	Props->SlotProps->Add(FName(TEXT("slot.padding")), FRuiValue(static_cast<float>(Pad)));
	Props->SlotProps->Add(FName(TEXT("slot.halign")), FRuiValue(TEXT("center")));
	Text.Props = Props;
	return {RUI::Slate::VBox(FRuiVerticalBoxProps(), {MoveTemp(Text)})};
}
RUI_COMPONENT(SlateSlotPropsComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiSlateSlotPropsTest, "ReactiveUI.Slate.SlotProps", RUI_SLATE_TEST_FLAGS)
bool FRuiSlateSlotPropsTest::RunTest(const FString&)
{
	SlateTest::Reset();
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&SlateSlotPropsComp));
	TSharedPtr<SWidget> Panel = SlateTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	auto SlotOf = [&Panel](int32 i) -> const SVerticalBox::FSlot&
	{ return static_cast<const SVerticalBox::FSlot&>(Panel->GetChildren()->GetSlotAt(i)); };

	TestEqual(TEXT("slot padding applied at insert"), SlotOf(0).GetPadding().Top, 4.0f);
	TestEqual(TEXT("slot halign applied at insert"), static_cast<int32>(SlotOf(0).GetHorizontalAlignment()),
			  static_cast<int32>(HAlign_Center));

	AddInfo(TEXT("[slot] slot.* change re-applies through the parent adapter"));
	SlateTest::IntSetter(12);
	Root->FlushSync();
	TestEqual(TEXT("slot padding updated"), SlotOf(0).GetPadding().Top, 12.0f);
	TestEqual(TEXT("halign survives the slot update"), static_cast<int32>(SlotOf(0).GetHorizontalAlignment()),
			  static_cast<int32>(HAlign_Center));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
