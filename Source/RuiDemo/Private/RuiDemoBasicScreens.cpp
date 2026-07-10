// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Gallery screens ported from the Unity sibling's Samples (ReactiveUIToolKit): the basics —
// HelloWorld, SimpleCounter, SimpleTextField, SimpleUseEffect, SignalCounter, ContextDemo.

#include "RuiContext.h"
#include "RuiDemoShared.h"
#include "RuiSignal.h"

using namespace RuiDemo;

// ── HelloWorldFunc ────────────────────────────────────────────────────────────────────────

static FRuiNodeArray HelloWorldComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	return {Padded(RUI::TextBlock(TEXT("Hello, world! (Functional Component)")))};
}
RUI_COMPONENT(HelloWorldComp)

// ── SimpleCounterFunc ─────────────────────────────────────────────────────────────────────

static FRuiNodeArray SimpleCounterComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Count, SetCount] = Ctx.UseState<int32>(0);
	TFunction<void(int32)> Set = SetCount;
	const int32 Current = Count;
	return {Padded(RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{RUI::TextBlock(FString::Printf(TEXT("Count: %d"), Count)), Gap(),
		 RUI::Slate::HorizontalBox(FRuiHorizontalBoxProps(),
								   {LabeledButton(TEXT("+"), [Set, Current]() { Set(Current + 1); })})}))};
}
RUI_COMPONENT(SimpleCounterComp)

// ── SimpleTextFieldFunc (controlled input) ────────────────────────────────────────────────

static FRuiNodeArray SimpleTextFieldComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Text, SetText] = Ctx.UseState<FString>(FString());
	TFunction<void(FString)> Set = SetText;

	FRuiEditableTextBoxProps Field;
	Field.SetText(FText::FromString(Text));
	Field.SetHintText(FText::FromString(TEXT("Type here...")));
	Field.SetOnTextChanged(FRuiCallback::Create([Set](const FRuiValue& V) { Set(V.TextValue.ToString()); }));

	return {Padded(RUI::Slate::VerticalBox(FRuiVerticalBoxProps(),
										   {RUI::Slate::EditableTextBox(MoveTemp(Field)), Gap(),
											RUI::TextBlock(FString::Printf(TEXT("You typed: %s"), *Text))}))};
}
RUI_COMPONENT(SimpleTextFieldComp)

// ── SimpleUseEffectFunc ───────────────────────────────────────────────────────────────────

static FRuiNodeArray SimpleUseEffectComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Message, SetMessage] = Ctx.UseState<FString>(FString(TEXT("Waiting...")));
	TFunction<void(FString)> Set = SetMessage;
	Ctx.UseEffect([Set]() { Set(FString(TEXT("Effect ran!"))); }, RUI::Deps());
	return {Padded(RUI::TextBlock(FString::Printf(TEXT("Message: %s"), *Message)))};
}
RUI_COMPONENT(SimpleUseEffectComp)

// ── SignalCounterDemoFunc (cross-component shared state) ─────────────────────────────────

static const FName GDemoCounterSignal(TEXT("demo.counter"));

static FRuiNodeArray SignalPanelComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	const int32 Count = RUI::UseSignalKey<int32>(Ctx, GDemoCounterSignal, 0);
	return {RUI::TextBlock(FString::Printf(TEXT("Count: %d"), Count))};
}
RUI_COMPONENT(SignalPanelComp)

static FRuiNodeArray SignalCounterComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	TSharedRef<TRuiSignal<int32>> Signal = RUI::GetOrCreateSignal<int32>(GDemoCounterSignal, 0);
	return {Padded(RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{StyledText(TEXT("Signal Counter (shared state)"), 14.0f), Gap(),
		 // Two INDEPENDENT subscriber panels — both follow the same signal.
		 RUI::FC(&SignalPanelComp), RUI::FC(&SignalPanelComp), Gap(),
		 RUI::Slate::HorizontalBox(
			 FRuiHorizontalBoxProps(),
			 {LabeledButton(TEXT("Increment"), [Signal]() { Signal->Update([](const int32& V) { return V + 1; }); }),
			  LabeledButton(TEXT("Decrement"), [Signal]() { Signal->Update([](const int32& V) { return V - 1; }); }),
			  LabeledButton(TEXT("Reset"), [Signal]() { Signal->Set(0); })})}))};
}
RUI_COMPONENT(SignalCounterComp)

// ── ContextDemoFunc + ContextConsumer ─────────────────────────────────────────────────────

struct FRuiDemoLabelProps final : public FRuiPropsBase
{
	RUI_PROP(FString, Label, 0)
	RUI_PROPS_BODY(FRuiDemoLabelProps, RUI_EQ(Label))
};

static TRuiContext<FLinearColor> GDemoThemeCtx(FLinearColor(0.23f, 0.65f, 0.95f), FName(TEXT("DemoTheme")));

static FRuiNodeArray ContextConsumerComp(FRuiContext& Ctx, const FRuiDemoLabelProps& Props, const TArray<FRuiNode>&)
{
	const FLinearColor Theme = Ctx.UseContext(GDemoThemeCtx);
	const FString Label = Props.HasLabel() ? Props.Label : FString(TEXT("Primary Panel"));

	FRuiBorderProps Panel;
	Panel.SetPadding(FMargin(10.0f));
	Panel.SetBorderImage(FName(TEXT("WhiteBrush")));
	Panel.SetBorderBackgroundColor(Theme);
	FRuiNode Node = RUI::Slate::Border(
		MoveTemp(Panel),
		{RUI::TextBlock(FString::Printf(TEXT("%s: rgba(%.2f, %.2f, %.2f)"), *Label, Theme.R, Theme.G, Theme.B))});
	return {WithSlot(MoveTemp(Node), FName(TEXT("Slot.Padding")), FRuiValue(TEXT("0,6,0,0")))};
}
RUI_COMPONENT(ContextConsumerComp)

static FRuiNodeArray ContextDemoComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [bPrimary, SetPrimary] = Ctx.UseState<bool>(true);
	TFunction<void(bool)> Set = SetPrimary;
	const bool bNow = bPrimary;

	const FLinearColor Theme =
		bPrimary ? FLinearColor(0.23f, 0.65f, 0.95f, 1.0f) : FLinearColor(0.95f, 0.4f, 0.35f, 1.0f);
	Ctx.ProvideContext(GDemoThemeCtx, Theme);

	FRuiDemoLabelProps Secondary;
	Secondary.SetLabel(FString(TEXT("Secondary Panel")));

	return {Padded(RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{RUI::TextBlock(TEXT("Context Demo (toggle theme to see consumers update)")), Gap(),
		 RUI::Slate::HorizontalBox(FRuiHorizontalBoxProps(), {LabeledButton(bPrimary ? TEXT("Switch To Warm Theme")
																					 : TEXT("Switch To Cool Theme"),
																			[Set, bNow]() { Set(!bNow); })}),
		 RUI::FC(&ContextConsumerComp), RUI::FC(&ContextConsumerComp, MoveTemp(Secondary))}))};
}
RUI_COMPONENT(ContextDemoComp)

// ── factories ─────────────────────────────────────────────────────────────────────────────

namespace RuiDemo
{
	FRuiNode HelloWorldScreen()
	{
		return RUI::FC(&HelloWorldComp);
	}
	FRuiNode CounterScreen()
	{
		return RUI::FC(&SimpleCounterComp);
	}
	FRuiNode TextFieldScreen()
	{
		return RUI::FC(&SimpleTextFieldComp);
	}
	FRuiNode UseEffectScreen()
	{
		return RUI::FC(&SimpleUseEffectComp);
	}
	FRuiNode SignalCounterScreen()
	{
		return RUI::FC(&SignalCounterComp);
	}
	FRuiNode ContextScreen()
	{
		return RUI::FC(&ContextDemoComp);
	}
} // namespace RuiDemo
