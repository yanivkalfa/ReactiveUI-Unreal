// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Style.* — the D-13 v1 contract: style keys map to setters (a style-only
// re-render provably does NOT reconstruct the widget — pointer identity is asserted),
// removed style keys RESET to defaults (unlike plain props), classes merge under inline.
// Plus the GO-05 pool: released leaves come back as the SAME widget pointer.

#include "Misc/AutomationTest.h"
#include "RuiContext.h"
#include "RuiRoot.h"
#include "RuiSlateElements.h"
#include "RuiSlateHost.h"
#include "RuiStyle.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_STYLE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace StyleTest
{
	static TFunction<void(int32)> IntSetter;

	static TSharedPtr<SWidget> RootChild(FRuiRoot& Root)
	{
		FChildren* Children = Root.GetWidget()->GetRootPanel()->GetChildren();
		return Children->Num() > 0 ? TSharedPtr<SWidget>(Children->GetChildAt(0)) : nullptr;
	}

	static FRuiNode StyledText(const FString& S, TSharedPtr<FRuiStyleDict> Style, TArray<FName> Classes = {})
	{
		FRuiNode Node = RUI::Text(S);
		TSharedRef<FRuiTextProps> Props = MakeShared<FRuiTextProps>(static_cast<const FRuiTextProps&>(*Node.Props));
		Props->Style = MoveTemp(Style);
		Props->Classes = MoveTemp(Classes);
		Node.Props = Props;
		return Node;
	}
} // namespace StyleTest

// ── apply / update / removal-reset + pointer identity ─────────────────────────────────────

static FRuiNodeArray StyleModesComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Mode, SetMode] = Ctx.UseState<int32>(0);
	StyleTest::IntSetter = SetMode;
	TSharedPtr<FRuiStyleDict> Style;
	if (Mode < 2)
	{
		Style = MakeShared<FRuiStyleDict>();
		Style->Add(FName(TEXT("opacity")), FRuiValue(Mode == 0 ? 0.4f : 0.8f));
		if (Mode == 0)
		{
			Style->Add(FName(TEXT("visibility")), FRuiValue(FName(TEXT("hidden"))));
			Style->Add(FName(TEXT("translation")), FRuiValue(FVector2D(5.0f, 7.0f)));
		}
	}
	return {RUI::Slate::VBox(FRuiVerticalBoxProps(), {StyleTest::StyledText(TEXT("styled"), MoveTemp(Style))})};
}
RUI_COMPONENT(StyleModesComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiStyleApplyTest, "ReactiveUI.Style.ApplyAndReset", RUI_STYLE_TEST_FLAGS)
bool FRuiStyleApplyTest::RunTest(const FString&)
{
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&StyleModesComp));
	TSharedPtr<SWidget> Panel = StyleTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> Text = Panel->GetChildren()->GetChildAt(0);
	SWidget* TextPtrBefore = &Text.Get();
	TestEqual(TEXT("opacity applied at mount"), Text->GetRenderOpacity(), 0.4f);
	TestTrue(TEXT("visibility applied"), Text->GetVisibility() == EVisibility::Hidden);
	TestTrue(TEXT("render transform applied"), Text->GetRenderTransform().IsSet());

	AddInfo(TEXT("[style] style-only change: same widget, new values, removed keys reset"));
	StyleTest::IntSetter(1);
	Root->FlushSync();
	TSharedRef<SWidget> TextAfter = Panel->GetChildren()->GetChildAt(0);
	TestTrue(TEXT("POINTER IDENTITY across style-only re-render (D-13 gate)"), &TextAfter.Get() == TextPtrBefore);
	TestEqual(TEXT("opacity updated"), TextAfter->GetRenderOpacity(), 0.8f);
	TestTrue(TEXT("removed visibility RESET to visible"), TextAfter->GetVisibility() == EVisibility::Visible);
	TestFalse(TEXT("removed translation RESET to identity"), TextAfter->GetRenderTransform().IsSet());

	AddInfo(TEXT("[style] whole dict removed -> everything resets"));
	StyleTest::IntSetter(2);
	Root->FlushSync();
	TestEqual(TEXT("opacity reset to 1"), TextAfter->GetRenderOpacity(), 1.0f);
	TestTrue(TEXT("still the same widget"), &Panel->GetChildren()->GetChildAt(0).Get() == TextPtrBefore);
	return true;
}

// ── classes merge: class applies, inline wins ─────────────────────────────────────────────

static FRuiNodeArray StyleClassesComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Mode, SetMode] = Ctx.UseState<int32>(0);
	StyleTest::IntSetter = SetMode;
	TSharedPtr<FRuiStyleDict> Inline;
	if (Mode == 1)
	{
		Inline = MakeShared<FRuiStyleDict>();
		Inline->Add(FName(TEXT("opacity")), FRuiValue(0.9f)); // inline beats the class
	}
	return {RUI::Slate::VBox(FRuiVerticalBoxProps(),
							 {StyleTest::StyledText(TEXT("classy"), Inline, {FName(TEXT("rui-test-dim"))})})};
}
RUI_COMPONENT(StyleClassesComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiStyleClassesTest, "ReactiveUI.Style.Classes", RUI_STYLE_TEST_FLAGS)
bool FRuiStyleClassesTest::RunTest(const FString&)
{
	FRuiStyleDict Dim;
	Dim.Add(FName(TEXT("opacity")), FRuiValue(0.25f));
	RUI::Slate::RegisterStyleClass(FName(TEXT("rui-test-dim")), MoveTemp(Dim));

	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&StyleClassesComp));
	TSharedPtr<SWidget> Panel = StyleTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> Text = Panel->GetChildren()->GetChildAt(0);
	TestEqual(TEXT("class opacity applied"), Text->GetRenderOpacity(), 0.25f);

	StyleTest::IntSetter(1);
	Root->FlushSync();
	TestEqual(TEXT("inline style wins over the class"), Text->GetRenderOpacity(), 0.9f);
	return true;
}

// ── GO-05 pool: released leaves come back as the same widget ──────────────────────────────

static FRuiNodeArray StylePoolComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Count, SetCount] = Ctx.UseState<int32>(3);
	StyleTest::IntSetter = SetCount;
	TArray<FRuiNode> Rows;
	for (int32 i = 0; i < Count; ++i)
	{
		FRuiNode Row = RUI::Text(FString::Printf(TEXT("row %d"), i));
		Row.Key = FRuiKey(i);
		Rows.Add(MoveTemp(Row));
	}
	return {RUI::Slate::VBox(FRuiVerticalBoxProps(), MoveTemp(Rows))};
}
RUI_COMPONENT(StylePoolComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiStylePoolTest, "ReactiveUI.Style.NodePool", RUI_STYLE_TEST_FLAGS)
bool FRuiStylePoolTest::RunTest(const FString&)
{
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&StylePoolComp));
	TSharedPtr<SWidget> Panel = StyleTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSet<SWidget*> OriginalPtrs;
	for (int32 i = 0; i < 3; ++i)
	{
		OriginalPtrs.Add(&Panel->GetChildren()->GetChildAt(i).Get());
	}

	AddInfo(TEXT("[pool] shrink stashes the released leaves"));
	StyleTest::IntSetter(1);
	Root->FlushSync();
	TestEqual(TEXT("one row left"), Panel->GetChildren()->Num(), 1);
	TestEqual(TEXT("two texts pooled"), Root->GetHost().NumPooled(RUI::TextElementType()), 2);

	AddInfo(TEXT("[pool] regrow reuses the SAME widgets (diff-on-reuse)"));
	StyleTest::IntSetter(3);
	Root->FlushSync();
	TestEqual(TEXT("three rows again"), Panel->GetChildren()->Num(), 3);
	TestEqual(TEXT("pool drained"), Root->GetHost().NumPooled(RUI::TextElementType()), 0);
	int32 Reused = 0;
	for (int32 i = 0; i < 3; ++i)
	{
		if (OriginalPtrs.Contains(&Panel->GetChildren()->GetChildAt(i).Get()))
		{
			++Reused;
		}
	}
	TestEqual(TEXT("all three widgets are the original pointers (1 kept + 2 pooled)"), Reused, 3);
	// And the reused widgets carry the RIGHT text (diff against stashed props applied it).
	for (int32 i = 0; i < 3; ++i)
	{
		TSharedRef<SWidget> W = Panel->GetChildren()->GetChildAt(i);
		TestEqual(TEXT("reused row text"), StaticCastSharedRef<STextBlock>(W)->GetText().ToString(),
				  FString::Printf(TEXT("row %d"), i));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
