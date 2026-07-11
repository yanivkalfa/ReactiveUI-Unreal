// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Cycle — cross-file COMPONENT-cycle parity (M6). CycleA and CycleB (under
// Source/RuiHostTests/CycleProof/) import and render each other. That the RuiHostTests aggregator
// TU compiled at all is the compile-time proof the two-phase (RUI_UETKX_DECL_PHASE) fwd-decl pass
// legalizes the cycle (the old UETKX2107 error retired). This test adds the runtime half: both
// compiled cyclic components register a named factory and mount/render.

#include "Misc/AutomationTest.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UetkxCycleTest
{
	static bool ContainsText(SWidget& RootWidget, const FString& Needle)
	{
		if (RootWidget.GetType() == FName(TEXT("STextBlock")) &&
			static_cast<STextBlock&>(RootWidget).GetText().ToString().Contains(Needle))
		{
			return true;
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (ContainsText(Children->GetChildAt(i).Get(), Needle))
			{
				return true;
			}
		}
		return false;
	}
} // namespace UetkxCycleTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxCycleTest, "ReactiveUI.Uetkx.Cycle",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxCycleTest::RunTest(const FString&)
{
	// Both halves of the mutual cycle compiled into this module and self-registered.
	TestTrue(TEXT("CycleA is registered"), RUI::HasNamedFactory(FName(TEXT("CycleA"))));
	TestTrue(TEXT("CycleB is registered"), RUI::HasNamedFactory(FName(TEXT("CycleB"))));

	TSharedRef<FRuiRoot> RootA = FRuiRoot::Create(RUI::Named(FName(TEXT("CycleA"))));
	RootA->FlushSync();
	TestTrue(TEXT("CycleA mounts and renders"), UetkxCycleTest::ContainsText(RootA->GetWidget().Get(), TEXT("A")));
	RootA->Unmount();

	TSharedRef<FRuiRoot> RootB = FRuiRoot::Create(RUI::Named(FName(TEXT("CycleB"))));
	RootB->FlushSync();
	TestTrue(TEXT("CycleB mounts and renders"), UetkxCycleTest::ContainsText(RootB->GetWidget().Get(), TEXT("B")));
	RootB->Unmount();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
