// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Slate.SlotInPlace — TD-010(a): a slot.* prop UPDATE mutates the LIVE FSlot in place
// (no detach/reinsert churn). Proven by the FSlot object staying the SAME address across the
// update while its padding value changes.

#include "Misc/AutomationTest.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiTypes.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiSlotInPlaceTest, "ReactiveUI.Slate.SlotInPlace",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiSlotInPlaceTest::RunTest(const FString&)
{
	IRuiElementAdapter* VB = RUI::Slate::FindAdapter(RUI::InternElementType(FName(TEXT("VerticalBox"))));
	if (!TestNotNull(TEXT("VerticalBox adapter registered"), VB))
	{
		return false;
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	TSharedRef<SWidget> Child = SNew(SSpacer);

	FRuiStyleDict Slot10;
	Slot10.Add(FName(TEXT("slot.padding")), FRuiValue(FString(TEXT("10"))));
	VB->InsertChild(*Box, Child, -1, &Slot10);

	const SVerticalBox::FSlot& S0 = static_cast<const SVerticalBox::FSlot&>(Box->GetChildren()->GetSlotAt(0));
	TestEqual(TEXT("initial padding 10"), S0.GetPadding(), FMargin(10.0f));
	const void* SlotAddr = &S0;

	// Update the slot props — this routes through UpdateChildSlotProps.
	FRuiStyleDict Slot20;
	Slot20.Add(FName(TEXT("slot.padding")), FRuiValue(FString(TEXT("20"))));
	VB->UpdateChildSlotProps(*Box, Child, &Slot20);

	const SVerticalBox::FSlot& S1 = static_cast<const SVerticalBox::FSlot&>(Box->GetChildren()->GetSlotAt(0));
	TestEqual(TEXT("padding updated to 20"), S1.GetPadding(), FMargin(20.0f));
	TestEqual(TEXT("SAME FSlot object — mutated in place, not reinserted"), (const void*)&S1, SlotAddr);
	TestEqual(TEXT("still exactly one child"), Box->GetChildren()->Num(), 1);
	TestEqual(TEXT("child identity preserved"), (void*)&Box->GetChildren()->GetChildAt(0).Get(), (void*)&Child.Get());

	// Removing the padding key RESETS to the slot default (0) — mirrors the fresh-reinsert result.
	FRuiStyleDict SlotEmpty;
	VB->UpdateChildSlotProps(*Box, Child, &SlotEmpty);
	const SVerticalBox::FSlot& S2 = static_cast<const SVerticalBox::FSlot&>(Box->GetChildren()->GetSlotAt(0));
	TestEqual(TEXT("removed padding resets to 0"), S2.GetPadding(), FMargin(0.0f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
