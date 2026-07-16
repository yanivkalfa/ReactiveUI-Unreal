// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Widgets.Batch3 — WIDGET_COMPLETION_PLAN wave 1: the eight mechanical leaves,
// the fully-masked-construct-only class (ColorBlock/gradients/Hyperlink replace in place on
// any prop change — the first widgets whose WHOLE surface rides TD-011), the TD-012 riders,
// and the TD-011 adapter meta-gate over the whole registry.

#include "Misc/AutomationTest.h"
#include "RuiContext.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiRoot.h"
#include "RuiSlateElements.h"
#include "RuiSlateHost.h"
#include "Widgets/SInvalidationPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_B3_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace B3Test
{
	static TFunction<void(int32)> IntSetter;

	static TSharedPtr<SWidget> RootChild(FRuiRoot& Root)
	{
		FChildren* Children = Root.GetWidget()->GetRootPanel()->GetChildren();
		return Children->Num() > 0 ? TSharedPtr<SWidget>(Children->GetChildAt(0)) : nullptr;
	}
} // namespace B3Test

// ── mount all eight + live-setter and reconstruct behavior ─────────────────────────────────

static FRuiNodeArray B3GalleryComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Mode, SetMode] = Ctx.UseState<int32>(0);
	B3Test::IntSetter = SetMode;

	FRuiColorBlockProps ColorP;
	ColorP.SetColor(Mode == 0 ? FLinearColor::Red : FLinearColor::Green);
	ColorP.SetSize(FVector2D(24.0, 24.0));

	FRuiSimpleGradientProps GradP;
	GradP.SetStartColor(FLinearColor::Black);
	GradP.SetEndColor(FLinearColor::White);

	FRuiComplexGradientProps CGradP;
	CGradP.SetGradientColors({FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue});

	FRuiHyperlinkProps LinkP;
	LinkP.SetText(FText::FromString(TEXT("docs")));

	FRuiBackgroundBlurProps BlurP;
	BlurP.SetBlurStrength(Mode == 0 ? 2.0f : 5.0f);

	FRuiInvalidationPanelProps InvalP;
	InvalP.SetbCanCache(true);

	return {RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{RUI::Slate::ColorBlock(MoveTemp(ColorP)), RUI::Slate::SimpleGradient(MoveTemp(GradP)),
		 RUI::Slate::ComplexGradient(MoveTemp(CGradP)), RUI::Slate::Hyperlink(MoveTemp(LinkP)),
		 RUI::Slate::EnableBox(FRuiEnableBoxProps(), {RUI::TextBlock(TEXT("enabled-island"))}),
		 RUI::Slate::ScissorRectBox(FRuiScissorRectBoxProps(), {RUI::TextBlock(TEXT("clipped"))}),
		 RUI::Slate::BackgroundBlur(MoveTemp(BlurP), {RUI::TextBlock(TEXT("blurred-behind"))}),
		 RUI::Slate::InvalidationPanel(MoveTemp(InvalP), {RUI::TextBlock(TEXT("cached"))})})};
}
RUI_COMPONENT(B3GalleryComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiWidgetsBatch3Test, "ReactiveUI.Widgets.Batch3", RUI_B3_TEST_FLAGS)
bool FRuiWidgetsBatch3Test::RunTest(const FString&)
{
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&B3GalleryComp));
	TSharedPtr<SWidget> Panel = B3Test::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	FChildren* Kids = Panel->GetChildren();
	TestEqual(TEXT("eight widgets mounted"), Kids->Num(), 8);
	TestEqual(TEXT("SColorBlock"), Kids->GetChildAt(0)->GetType(), FName(TEXT("SColorBlock")));
	TestEqual(TEXT("SSimpleGradient"), Kids->GetChildAt(1)->GetType(), FName(TEXT("SSimpleGradient")));
	TestEqual(TEXT("SComplexGradient"), Kids->GetChildAt(2)->GetType(), FName(TEXT("SComplexGradient")));
	TestEqual(TEXT("SHyperlink"), Kids->GetChildAt(3)->GetType(), FName(TEXT("SHyperlink")));
	TestEqual(TEXT("SEnableBox"), Kids->GetChildAt(4)->GetType(), FName(TEXT("SEnableBox")));
	TestEqual(TEXT("SScissorRectBox"), Kids->GetChildAt(5)->GetType(), FName(TEXT("SScissorRectBox")));
	TestEqual(TEXT("SBackgroundBlur"), Kids->GetChildAt(6)->GetType(), FName(TEXT("SBackgroundBlur")));
	TestEqual(TEXT("SInvalidationPanel"), Kids->GetChildAt(7)->GetType(), FName(TEXT("SInvalidationPanel")));

	// Containers actually hold their children.
	TestEqual(TEXT("EnableBox holds content"), Kids->GetChildAt(4)->GetChildren()->Num(), 1);
	TestEqual(TEXT("BackgroundBlur holds content"), Kids->GetChildAt(6)->GetChildren()->Num(), 1);

	SWidget* ColorBefore = &Kids->GetChildAt(0).Get();
	SWidget* BlurBefore = &Kids->GetChildAt(6).Get();

	AddInfo(TEXT("[b3] construct-only prop change REPLACES the masked leaf; setter widget stays"));
	B3Test::IntSetter(1);
	Root->FlushSync();
	TestTrue(TEXT("ColorBlock REPLACED on Color change (fully-masked TD-011 class)"),
			 &Panel->GetChildren()->GetChildAt(0).Get() != ColorBefore);
	TestEqual(TEXT("replacement is still an SColorBlock"), Panel->GetChildren()->GetChildAt(0)->GetType(),
			  FName(TEXT("SColorBlock")));
	TestTrue(TEXT("BackgroundBlur KEPT on BlurStrength change (live setter)"),
			 &Panel->GetChildren()->GetChildAt(6).Get() == BlurBefore);
	return true;
}

// ── TD-011 meta-gate: the whole adapter registry honors the reconstruct-mask contract ──────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiAdapterMaskContractTest, "ReactiveUI.Contract.AdapterMasks", RUI_B3_TEST_FLAGS)
bool FRuiAdapterMaskContractTest::RunTest(const FString&)
{
	// Force full registration (Boot normally does this; keep the test order-independent).
	RUI::Slate::RegisterBuiltinAdapters();

	int32 Total = 0, Masked = 0;
	RUI::Slate::ForEachAdapter(
		[&](FRuiElementTypeId Type, IRuiElementAdapter& Adapter)
		{
			++Total;
			const uint64 Mask = Adapter.GetReconstructMask();
			if (Mask == 0)
			{
				return;
			}
			++Masked;
			// The precise gate must be side-effect-free and FALSE for identical inputs — a
			// ConstructOnlyChanged that fires on equal props would rebuild every commit
			// (state/focus loss), the exact bug class TD-011 exists to prevent. Empty props
			// carry no Has-bits, so any correct Has-gated implementation returns false.
			const FRuiEmptyProps A, B;
			TestFalse(FString::Printf(TEXT("adapter '%s': ConstructOnlyChanged(empty, empty) must be false"),
									  *RUI::GetElementTypeName(Type).ToString()),
					  Adapter.ConstructOnlyChanged(A, B));
		});
	AddInfo(FString::Printf(TEXT("[masks] %d adapters, %d with reconstruct masks"), Total, Masked));
	TestTrue(TEXT("registry populated"), Total > 30);
	TestTrue(TEXT("the masked class exists (Separator + the wave-1 construct-only leaves)"), Masked >= 5);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
