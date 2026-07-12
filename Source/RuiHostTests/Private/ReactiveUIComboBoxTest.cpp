// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Widgets.ComboBox — TD-012 tail: the dropdown selector. Verifies the render-prop
// selected-display sub-root (read back through the widget tree), controlled selection, AND the real
// dropdown: opens the menu and ticks its list via the interaction harness so the option-row sub-roots
// generate — the menu-stack path that made this widget "interactive-only" before the harness.

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "RuiComboBox.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiListView.h" // MakeItemRenderer
#include "RuiCoreElements.h"
#include "RuiSlateTestHarness.h"
#include "RuiTypes.h"
#include "Layout/Geometry.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ComboBoxTest
{
	static FString SelectedText(SRuiComboBox& Combo)
	{
		TSharedPtr<SWidget> Content = Combo.GetSelectedContent();
		if (!Content.IsValid())
		{
			return FString();
		}
		SWidget* Text = RuiTest::FindDescendantByType(*Content, FName(TEXT("STextBlock")));
		return Text != nullptr ? static_cast<STextBlock*>(Text)->GetText().ToString() : FString();
	}
} // namespace ComboBoxTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiComboBoxTest, "ReactiveUI.Widgets.ComboBox",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiComboBoxTest::RunTest(const FString&)
{
	using namespace RUI::Slate;
	using namespace ComboBoxTest;

	IRuiElementAdapter* Adapter = FindAdapter(ComboBoxType());
	if (!TestNotNull(TEXT("ComboBox adapter registered"), Adapter))
	{
		return false;
	}

	TArray<TSharedPtr<FRuiValue>> Options;
	Options.Add(MakeShared<FRuiValue>(10));
	Options.Add(MakeShared<FRuiValue>(20));
	Options.Add(MakeShared<FRuiValue>(30));

	auto Renderer =
		MakeItemRenderer([](const FRuiValue& V, int32) -> FRuiNode
						 { return RUI::TextBlock(FString::Printf(TEXT("opt-%d"), static_cast<int32>(V.IntValue))); });

	FRuiComboBoxProps Props;
	Props.SetOptions(Options);
	Props.SetRenderOption(Renderer);
	Props.SetSelectedIndex(1);

	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuiComboBox& Combo = static_cast<SRuiComboBox&>(Widget.Get());
	Adapter->ApplyDiff(Combo, nullptr, Props);

	// ── the selected-display sub-root shows RenderOption(options[1]) ─────────────────────────────
	TestEqual(TEXT("selected index is 1"), Combo.GetSelectedIndex(), 1);
	TestEqual(TEXT("selected display renders option 1"), SelectedText(Combo), FString(TEXT("opt-20")));

	// Controlled selection moves the display.
	FRuiComboBoxProps Pick2 = Props;
	Pick2.SetSelectedIndex(2);
	Adapter->ApplyDiff(Combo, &Props, Pick2);
	TestEqual(TEXT("controlled selection moved display to option 2"), SelectedText(Combo), FString(TEXT("opt-30")));

	// ── the real dropdown: open the menu and tick its list so the row sub-roots generate ────────
	if (FSlateApplication::IsInitialized())
	{
		RuiTest::FTestWindow Win(Widget);
		if (Win.IsValid())
		{
			Win.PumpGeometry();
			Combo.OpenMenu();
			FSlateApplication::Get().Tick(); // let the menu-stack push realize
			Win.PumpGeometry();

			// SComboBox's dropdown list is an SComboListType (an SListView subclass) living in the
			// pushed menu window; tick it with geometry so the visible option rows generate.
			const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(200, 600), FSlateLayoutTransform());
			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				if (SWidget* List = RuiTest::FindInAllWindowsByType(FName(TEXT("SComboListType"))))
				{
					List->SlatePrepass(1.0f);
					List->Tick(Geometry, 0.0, 0.0f);
				}
			}
			TestEqual(TEXT("all three option rows generated in the dropdown"), Combo.NumGeneratedRows(), 3);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
