// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Widgets.NumericEntryBox — TD-012 tail: the controlled numeric field. Uses the headless
// interaction harness to mount in a live window, drive the controlled value, and READ IT BACK from
// the inner editable-text widget (a real display round-trip, not just the wrapper's own member).

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiNumericEntryBox.h"
#include "RuiSlateTestHarness.h"
#include "RuiTypes.h"
#include "Widgets/Input/SEditableText.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiNumericEntryBoxTest, "ReactiveUI.Widgets.NumericEntryBox",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiNumericEntryBoxTest::RunTest(const FString&)
{
	IRuiElementAdapter* Adapter = RUI::Slate::FindAdapter(RUI::Slate::NumericEntryBoxType());
	if (!TestNotNull(TEXT("NumericEntryBox adapter registered"), Adapter))
	{
		return false;
	}

	// ── controlled value + event forwarding (no window needed) ──────────────────────────────────
	FRuiNumericEntryBoxProps Props;
	Props.SetValue(42.5f);
	Props.SetMinValue(0.0f);
	Props.SetMaxValue(100.0f);
	int32 Changed = 0;
	float LastChanged = -1.0f;
	Props.SetOnValueChanged(FRuiCallback::Create(
		[&Changed, &LastChanged](const FRuiValue& V)
		{
			++Changed;
			LastChanged = static_cast<float>(V.FloatValue);
		}));

	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuiNumericEntryBox& Entry = static_cast<SRuiNumericEntryBox&>(Widget.Get());
	Adapter->ApplyDiff(Entry, nullptr, Props);
	TestTrue(TEXT("controlled value stored"), FMath::IsNearlyEqual(Entry.GetValue(), 42.5f));

	// ── the DISPLAY round-trip through the real widget: mount in a window, read the inner text ──
	if (FSlateApplication::IsInitialized())
	{
		RuiTest::FTestWindow Win(Widget);
		if (Win.IsValid())
		{
			Win.PumpGeometry();
			SWidget* Inner = RuiTest::FindDescendantByType(Widget.Get(), FName(TEXT("SEditableText")));
			if (TestNotNull(TEXT("inner editable text found"), Inner))
			{
				// SEditableText::GetText reflects the numeric field's formatted value.
				const FString Shown = static_cast<SEditableText*>(Inner)->GetText().ToString();
				const float Parsed = FCString::Atof(*Shown);
				TestTrue(FString::Printf(TEXT("displays 42.5 (shown '%s')"), *Shown),
						 FMath::IsNearlyEqual(Parsed, 42.5f, 0.05f));

				// Re-render with a new controlled value: the display follows.
				FRuiNumericEntryBoxProps Next = Props;
				Next.SetValue(7.0f);
				Adapter->ApplyDiff(Entry, &Props, Next);
				Win.PumpGeometry();
				const FString Shown2 = static_cast<SEditableText*>(Inner)->GetText().ToString();
				TestTrue(FString::Printf(TEXT("display follows to 7 (shown '%s')"), *Shown2),
						 FMath::IsNearlyEqual(FCString::Atof(*Shown2), 7.0f, 0.05f));
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
