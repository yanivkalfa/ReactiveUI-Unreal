// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Widgets.SegmentedControl — TD-012 tail: labelled tab-bar selector. Adapter-driven:
// proves segment baking from Labels, controlled SelectedIndex (skip-when-equal), and the
// construct-only Labels reconstruct-mask gate (a label-set change forces a widget replacement).

#include "Misc/AutomationTest.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiSegmentedControl.h"
#include "RuiTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiSegmentedControlTest, "ReactiveUI.Widgets.SegmentedControl",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiSegmentedControlTest::RunTest(const FString&)
{
	IRuiElementAdapter* Adapter = RUI::Slate::FindAdapter(RUI::Slate::SegmentedControlType());
	if (!TestNotNull(TEXT("SegmentedControl adapter registered"), Adapter))
	{
		return false;
	}

	FRuiSegmentedControlProps Props;
	Props.SetLabels({TEXT("Easy"), TEXT("Normal"), TEXT("Hard")});
	Props.SetSelectedIndex(1);

	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuiSegmentedControl& Seg = static_cast<SRuiSegmentedControl&>(Widget.Get());
	Adapter->ApplyDiff(Seg, nullptr, Props);

	TestEqual(TEXT("one segment per label"), Seg.NumSegments(), 3);
	TestEqual(TEXT("initial selection is index 1"), Seg.GetSelectedIndex(), 1);

	// Controlled selection: a new SelectedIndex drives SetValue.
	FRuiSegmentedControlProps Pick2 = Props;
	Pick2.SetSelectedIndex(2);
	Adapter->ApplyDiff(Seg, &Props, Pick2);
	TestEqual(TEXT("controlled prop moved selection to index 2"), Seg.GetSelectedIndex(), 2);

	// The reconstruct mask covers Labels: same labels -> no rebuild; changed labels -> rebuild.
	TestFalse(TEXT("same labels -> no reconstruct"), Adapter->ConstructOnlyChanged(Props, Pick2));

	FRuiSegmentedControlProps Relabel;
	Relabel.SetLabels({TEXT("On"), TEXT("Off")});
	Relabel.SetSelectedIndex(0);
	TestTrue(TEXT("changed labels -> reconstruct"), Adapter->ConstructOnlyChanged(Props, Relabel));

	// A fresh widget from the new labels bakes the new segment count.
	TSharedRef<SWidget> W2 = Adapter->CreateWidget(Relabel, nullptr);
	SRuiSegmentedControl& Seg2 = static_cast<SRuiSegmentedControl&>(W2.Get());
	TestEqual(TEXT("relabelled widget has two segments"), Seg2.NumSegments(), 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
