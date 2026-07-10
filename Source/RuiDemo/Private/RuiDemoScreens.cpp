// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiDemoScreens.h"

#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiSlateElements.h"
#include "RuiStyle.h"

namespace
{
	FRuiNode Padded(FRuiNode Inner, float Padding)
	{
		FRuiBorderProps P;
		P.SetPadding(FMargin(Padding));
		return RUI::Slate::Border(MoveTemp(P), {MoveTemp(Inner)});
	}

	FRuiNode StyledText(const FString& S, float FontSize, FLinearColor Color = FLinearColor::White)
	{
		FRuiNode Node = RUI::Text(S);
		TSharedRef<FRuiTextProps> Props = MakeShared<FRuiTextProps>(static_cast<const FRuiTextProps&>(*Node.Props));
		Props->Style = MakeShared<FRuiStyleDict>();
		Props->Style->Add(FName(TEXT("fontSize")), FRuiValue(FontSize));
		Props->Style->Add(FName(TEXT("color")), FRuiValue(Color));
		Node.Props = Props;
		return Node;
	}

	FRuiNode LabeledButton(const FString& Label, TFunction<void()> OnClick)
	{
		FRuiButtonProps P;
		P.SetOnClicked(FRuiCallback::Create(MoveTemp(OnClick)));
		return RUI::Slate::Button(MoveTemp(P), {RUI::Text(Label)});
	}
} // namespace

// ── Counter: hooks + events + progress ────────────────────────────────────────────────────

static FRuiNodeArray DemoCounter(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Count, SetCount] = Ctx.UseState<int32>(3);
	TFunction<void(int32)> Set = SetCount;

	FRuiProgressBarProps Bar;
	Bar.SetPercent(FMath::Clamp(Count / 10.0f, 0.0f, 1.0f));

	const int32 Current = Count;
	return {
		Padded(RUI::Slate::VBox(FRuiVerticalBoxProps(),
								{StyledText(TEXT("Counter"), 16.0f, FLinearColor(0.4f, 0.8f, 1.0f)),
								 RUI::Text(FString::Printf(TEXT("count = %d"), Count)),
								 RUI::Slate::HBox(FRuiHorizontalBoxProps(),
												  {LabeledButton(TEXT(" - "), [Set, Current]() { Set(Current - 1); }),
												   LabeledButton(TEXT(" + "), [Set, Current]() { Set(Current + 1); })}),
								 RUI::Slate::ProgressBar(MoveTemp(Bar))}),
			   8.0f)};
}
RUI_COMPONENT(DemoCounter)

// ── Keyed list: add / remove / shuffle with stable identity ──────────────────────────────

static FRuiNodeArray DemoKeyedList(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Items, SetItems] = Ctx.UseState<TArray<int32>>(TArray<int32>{1, 2, 3});
	auto [NextId, SetNextId] = Ctx.UseState<int32>(4);
	TFunction<void(TArray<int32>)> Set = SetItems;
	TFunction<void(int32)> SetNext = SetNextId;

	const TArray<int32> Current = Items;
	const int32 Id = NextId;

	TArray<FRuiNode> Rows;
	Rows.Add(StyledText(TEXT("Keyed list"), 16.0f, FLinearColor(1.0f, 0.8f, 0.4f)));
	Rows.Add(RUI::Slate::HBox(FRuiHorizontalBoxProps(), {LabeledButton(TEXT(" add "),
																	   [Set, SetNext, Current, Id]()
																	   {
																		   TArray<int32> Next = Current;
																		   Next.Add(Id);
																		   SetNext(Id + 1);
																		   Set(MoveTemp(Next));
																	   }),
														 LabeledButton(TEXT(" remove "),
																	   [Set, Current]()
																	   {
																		   if (!Current.IsEmpty())
																		   {
																			   TArray<int32> Next = Current;
																			   Next.RemoveAt(0);
																			   Set(MoveTemp(Next));
																		   }
																	   }),
														 LabeledButton(TEXT(" reverse "),
																	   [Set, Current]()
																	   {
																		   TArray<int32> Next = Current;
																		   Algo::Reverse(Next);
																		   Set(MoveTemp(Next));
																	   })}));
	for (int32 Item : Current)
	{
		FRuiNode Row = RUI::Text(FString::Printf(TEXT("item #%d"), Item));
		Row.Key = FRuiKey(Item);
		Rows.Add(MoveTemp(Row));
	}
	return {Padded(RUI::Slate::VBox(FRuiVerticalBoxProps(), MoveTemp(Rows)), 8.0f)};
}
RUI_COMPONENT(DemoKeyedList)

// ── Styled panels: classes, style toggles, controlled input ──────────────────────────────

static FRuiNodeArray DemoStyledPanels(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [bDim, SetDim] = Ctx.UseState<bool>(false);
	auto [Text, SetText] = Ctx.UseState<FString>(FString(TEXT("type here")));
	TFunction<void(bool)> SetDimFn = SetDim;
	TFunction<void(FString)> SetTextFn = SetText;

	FRuiCheckBoxProps Check;
	Check.SetbIsChecked(bDim);
	Check.SetOnCheckedChanged(FRuiCallback::Create([SetDimFn](const FRuiValue& V) { SetDimFn(V.BoolValue); }));

	FRuiEditableTextProps Edit;
	Edit.SetText(FText::FromString(Text));
	Edit.SetOnTextChanged(FRuiCallback::Create([SetTextFn](const FRuiValue& V) { SetTextFn(V.TextValue.ToString()); }));

	// The mirrored text dims through a style CLASS when the checkbox is on.
	FRuiNode Mirror = RUI::Text(FString::Printf(TEXT("mirror: %s"), *Text));
	TSharedRef<FRuiTextProps> MirrorProps = MakeShared<FRuiTextProps>(static_cast<const FRuiTextProps&>(*Mirror.Props));
	if (bDim)
	{
		MirrorProps->Classes.Add(FName(TEXT("rui-demo-dim")));
	}
	Mirror.Props = MirrorProps;

	return {Padded(RUI::Slate::VBox(FRuiVerticalBoxProps(),
									{StyledText(TEXT("Styled panels"), 16.0f, FLinearColor(0.6f, 1.0f, 0.6f)),
									 RUI::Slate::CheckBox(MoveTemp(Check), {RUI::Text(TEXT("dim the mirror"))}),
									 RUI::Slate::EditableText(MoveTemp(Edit)), MoveTemp(Mirror)}),
				   8.0f)};
}
RUI_COMPONENT(DemoStyledPanels)

// ── gallery root ──────────────────────────────────────────────────────────────────────────

static FRuiNodeArray DemoGallery(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	Ctx.UseEffect(
		[]()
		{
			FRuiStyleDict Dim;
			Dim.Add(FName(TEXT("opacity")), FRuiValue(0.35f));
			RUI::Slate::RegisterStyleClass(FName(TEXT("rui-demo-dim")), MoveTemp(Dim));
		},
		RUI::Deps());

	FRuiBoxProps Width;
	Width.SetWidth(420.0f);
	Width.SetHAlign(FName(TEXT("left")));
	Width.SetVAlign(FName(TEXT("top")));
	return {RUI::Slate::Box(
		MoveTemp(Width),
		{RUI::Slate::VBox(FRuiVerticalBoxProps(),
						  {StyledText(TEXT("ReactiveUI for Unreal — Phase 2 demo"), 20.0f), RUI::FC(&DemoCounter),
						   RUI::FC(&DemoKeyedList), RUI::FC(&DemoStyledPanels)})})};
}
RUI_COMPONENT(DemoGallery)

namespace RuiDemo
{
	FRuiNode GalleryRoot()
	{
		return RUI::FC(&DemoGallery);
	}
} // namespace RuiDemo
