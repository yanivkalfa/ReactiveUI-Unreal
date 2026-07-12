// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Children — TD-015: `{children}` forwards a component's children. ChildParent
// renders <ChildHost> wrapping two TextBlocks; ChildHost splices `{children}` between its own
// header/footer. Mounting ChildParent must show all of HOST-HEADER, the two forwarded rows, and
// HOST-FOOTER — proving the compiled Ch.Append(children) splice works end to end.

#include "Misc/AutomationTest.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ChildrenTest
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
} // namespace ChildrenTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiChildrenTest, "ReactiveUI.Uetkx.Children",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiChildrenTest::RunTest(const FString&)
{
	TestTrue(TEXT("ChildHost registered"), RUI::HasNamedFactory(FName(TEXT("ChildHost"))));
	TestTrue(TEXT("ChildParent registered"), RUI::HasNamedFactory(FName(TEXT("ChildParent"))));

	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("ChildParent"))));
	Root->FlushSync();
	SWidget& W = Root->GetWidget().Get();

	TestTrue(TEXT("host header renders"), ChildrenTest::ContainsText(W, TEXT("HOST-HEADER")));
	TestTrue(TEXT("forwarded child A renders (spliced via {children})"),
			 ChildrenTest::ContainsText(W, TEXT("FORWARDED-A")));
	TestTrue(TEXT("forwarded child B renders (spliced via {children})"),
			 ChildrenTest::ContainsText(W, TEXT("FORWARDED-B")));
	TestTrue(TEXT("host footer renders after the spliced children"),
			 ChildrenTest::ContainsText(W, TEXT("HOST-FOOTER")));

	Root->Unmount();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
