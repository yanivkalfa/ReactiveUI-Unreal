// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.IncludeRetirement — INCLUDE_RETIREMENT_PLAN.md §B end to end through the
// COMPILED pipeline (HostImportProof.uetkx -> committed .inl -> MSVC -> runtime mount): the
// `import "@Misc/DateTime.h"` host include actually emits the #include the setup code needs
// (FDateTime is NOT on the auto-included prelude list, so this proves the real thing, not a
// header that would have compiled anyway).

#include "Misc/AutomationTest.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace IncludeRetirementTest
{
	static bool ContainsText(SWidget& W, const FString& Needle)
	{
		if (W.GetType() == FName(TEXT("STextBlock")) &&
			static_cast<STextBlock&>(W).GetText().ToString().Contains(Needle))
		{
			return true;
		}
		FChildren* Kids = W.GetChildren();
		for (int32 i = 0; i < Kids->Num(); ++i)
		{
			if (ContainsText(Kids->GetChildAt(i).Get(), Needle))
			{
				return true;
			}
		}
		return false;
	}
} // namespace IncludeRetirementTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxIncludeRetirementTest, "ReactiveUI.Uetkx.IncludeRetirement",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxIncludeRetirementTest::RunTest(const FString&)
{
	TestTrue(TEXT("HostImportProof registered"), RUI::HasNamedFactory(FName(TEXT("HostImportProof"))));

	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("HostImportProof"))));
	Root->FlushSync();
	SWidget& W = Root->GetWidget().Get();
	TestTrue(TEXT("FDateTime from the @-imported header rendered"),
			 IncludeRetirementTest::ContainsText(W, TEXT("year=2026")));
	Root->Unmount();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
