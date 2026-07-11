// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Style.Builder — TD-013: the typed fluent authoring API (RUI::Style() / RUI::Slot())
// produces FRuiStyleDicts that apply IDENTICALLY to the .uetkx markup path — proven by applying a
// built style dict to a real widget and a built slot dict through the box adapter.

#include "Misc/AutomationTest.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiStyleBuilderTest, "ReactiveUI.Style.Builder",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiStyleBuilderTest::RunTest(const FString&)
{
	// ── style builder: generic keys apply through ApplyStyleDiff (null adapter) ───────────────
	{
		TSharedRef<SBox> W = SNew(SBox);
		TSharedPtr<FRuiStyleDict> Dict = RUI::Style().RenderOpacity(0.5f).Enabled(false);
		TestEqual(TEXT("built dict has 2 keys"), Dict->Num(), 2);
		RUI::Slate::ApplyStyleDiff(*W, nullptr, nullptr, Dict.Get());
		TestEqual(TEXT("RenderOpacity applied"), W->GetRenderOpacity(), 0.5f);
		TestFalse(TEXT("Enabled(false) applied"), W->IsEnabled());

		// Removing a key (diff to an empty dict) RESETS it (style keys reset on removal, D-13).
		FRuiStyleDict Empty;
		RUI::Slate::ApplyStyleDiff(*W, nullptr, Dict.Get(), &Empty);
		TestEqual(TEXT("RenderOpacity reset to 1"), W->GetRenderOpacity(), 1.0f);
		TestTrue(TEXT("Enabled reset to true"), W->IsEnabled());
	}

	// ── slot builder: padding + alignment flow through the box adapter ────────────────────────
	{
		IRuiElementAdapter* VB = RUI::Slate::FindAdapter(RUI::InternElementType(FName(TEXT("VerticalBox"))));
		if (TestNotNull(TEXT("VerticalBox adapter"), VB))
		{
			TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
			TSharedRef<SWidget> Child = SNew(SSpacer);
			TSharedPtr<FRuiStyleDict> Slot = RUI::Slot().Padding(8.0f).HAlign(HAlign_Center);
			VB->InsertChild(*Box, Child, -1, Slot.Get());

			const SVerticalBox::FSlot& S = static_cast<const SVerticalBox::FSlot&>(Box->GetChildren()->GetSlotAt(0));
			TestEqual(TEXT("slot padding 8"), S.GetPadding(), FMargin(8.0f));
			TestEqual(TEXT("slot HAlign center"), (int32)S.GetHorizontalAlignment(), (int32)HAlign_Center);
		}
	}

	// ── the convertible builder can be passed where a TSharedPtr<FRuiStyleDict> is expected ───
	{
		TSharedPtr<FRuiStyleDict> Dict = RUI::Style().FontSize(16.0f).ColorAndOpacity(FLinearColor::Red);
		TestEqual(TEXT("Font.Size stored as float"), Dict->FindChecked(FName(TEXT("Font.Size"))).FloatValue, 16.0);
		const FRuiValue& Col = Dict->FindChecked(FName(TEXT("ColorAndOpacity")));
		TestEqual(TEXT("ColorAndOpacity stored as color kind"), (int32)Col.Kind, (int32)FRuiValue::EKind::Color);
		TestEqual(TEXT("color value red"), Col.ColorValue.R, 1.0f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
