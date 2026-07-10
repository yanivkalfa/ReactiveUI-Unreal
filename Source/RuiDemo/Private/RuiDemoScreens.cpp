// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiDemoScreens.h"

#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiSlateElements.h"
#include "RuiStyle.h"

namespace
{
	/** Section card: dark translucent backdrop + inner padding + a gap above (slot.*). */
	FRuiNode Padded(FRuiNode Inner, float Padding)
	{
		FRuiBorderProps P;
		P.SetPadding(FMargin(Padding + 4.0f));
		P.SetBorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f));
		FRuiNode Node = RUI::Slate::Border(MoveTemp(P), {MoveTemp(Inner)});
		TSharedRef<FRuiBorderProps> Props =
			MakeShared<FRuiBorderProps>(static_cast<const FRuiBorderProps&>(*Node.Props));
		Props->SlotProps = MakeShared<FRuiStyleDict>();
		Props->SlotProps->Add(FName(TEXT("slot.padding")), FRuiValue(TEXT("0,10,0,0")));
		Node.Props = Props;
		return Node;
	}

	/** Breathing room between rows inside a section. */
	FRuiNode Gap(float Height = 6.0f)
	{
		FRuiSpacerProps P;
		P.SetSize(FVector2D(1.0f, Height));
		return RUI::Slate::Spacer(MoveTemp(P));
	}

	FRuiNode StyledText(const FString& S, float FontSize, FLinearColor Color = FLinearColor::White)
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

	FRuiNode LabeledButton(const FString& Label, TFunction<void()> OnClick)
	{
		FRuiButtonProps P;
		P.SetOnClicked(FRuiCallback::Create(MoveTemp(OnClick)));
		P.SetContentPadding(FMargin(14.0f, 5.0f));
		FRuiNode Node = RUI::Slate::Button(MoveTemp(P), {RUI::TextBlock(Label)});
		// Right margin so button rows don't fuse together.
		TSharedRef<FRuiButtonProps> Props =
			MakeShared<FRuiButtonProps>(static_cast<const FRuiButtonProps&>(*Node.Props));
		Props->SlotProps = MakeShared<FRuiStyleDict>();
		Props->SlotProps->Add(FName(TEXT("slot.padding")), FRuiValue(TEXT("0,0,6,0")));
		Node.Props = Props;
		return Node;
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
	return {Padded(RUI::Slate::VerticalBox(
					   FRuiVerticalBoxProps(),
					   {StyledText(TEXT("Counter"), 16.0f, FLinearColor(0.4f, 0.8f, 1.0f)), Gap(),
						RUI::TextBlock(FString::Printf(TEXT("count = %d"), Count)), Gap(),
						RUI::Slate::HorizontalBox(FRuiHorizontalBoxProps(),
												  {LabeledButton(TEXT(" - "), [Set, Current]() { Set(Current - 1); }),
												   LabeledButton(TEXT(" + "), [Set, Current]() { Set(Current + 1); })}),
						Gap(), RUI::Slate::ProgressBar(MoveTemp(Bar))}),
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
	Rows.Add(Gap());
	Rows.Add(RUI::Slate::HorizontalBox(FRuiHorizontalBoxProps(), {LabeledButton(TEXT(" add "),
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
		FRuiNode Row = RUI::TextBlock(FString::Printf(TEXT("item #%d"), Item));
		Row.Key = FRuiKey(Item);
		Rows.Add(MoveTemp(Row));
	}
	return {Padded(RUI::Slate::VerticalBox(FRuiVerticalBoxProps(), MoveTemp(Rows)), 8.0f)};
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
	Check.SetOnCheckStateChanged(FRuiCallback::Create([SetDimFn](const FRuiValue& V) { SetDimFn(V.BoolValue); }));

	FRuiEditableTextBoxProps Edit;
	Edit.SetText(FText::FromString(Text));
	Edit.SetOnTextChanged(FRuiCallback::Create([SetTextFn](const FRuiValue& V) { SetTextFn(V.TextValue.ToString()); }));

	// The mirrored text dims through a style CLASS when the checkbox is on.
	FRuiNode Mirror = RUI::TextBlock(FString::Printf(TEXT("mirror: %s"), *Text));
	TSharedRef<FRuiTextBlockProps> MirrorProps =
		MakeShared<FRuiTextBlockProps>(static_cast<const FRuiTextBlockProps&>(*Mirror.Props));
	if (bDim)
	{
		MirrorProps->Classes.Add(FName(TEXT("rui-demo-dim")));
	}
	Mirror.Props = MirrorProps;

	return {
		Padded(RUI::Slate::VerticalBox(FRuiVerticalBoxProps(),
									   {StyledText(TEXT("Styled panels"), 16.0f, FLinearColor(0.6f, 1.0f, 0.6f)), Gap(),
										RUI::Slate::CheckBox(MoveTemp(Check), {RUI::TextBlock(TEXT("dim the mirror"))}),
										Gap(), RUI::Slate::EditableTextBox(MoveTemp(Edit)), Gap(), MoveTemp(Mirror)}),
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
			Dim.Add(FName(TEXT("RenderOpacity")), FRuiValue(0.35f));
			RUI::Slate::RegisterStyleClass(FName(TEXT("rui-demo-dim")), MoveTemp(Dim));
		},
		RUI::Deps());

	FRuiBoxProps Width;
	Width.SetWidthOverride(420.0f);
	Width.SetHAlign(FName(TEXT("left")));
	Width.SetVAlign(FName(TEXT("top")));
	return {RUI::Slate::Box(
		MoveTemp(Width),
		{RUI::Slate::VerticalBox(FRuiVerticalBoxProps(),
								 {StyledText(TEXT("ReactiveUI for Unreal — Phase 2 demo"), 20.0f),
								  RUI::FC(&DemoCounter), RUI::FC(&DemoKeyedList), RUI::FC(&DemoStyledPanels)})})};
}
RUI_COMPONENT(DemoGallery)

namespace RuiDemo
{
	FRuiNode GalleryRoot()
	{
		return RUI::FC(&DemoGallery);
	}
} // namespace RuiDemo
