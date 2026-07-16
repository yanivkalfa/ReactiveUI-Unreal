// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.WaveG — grammar wave G end to end through the COMPILED pipeline
// (MultiReturnProof.uetkx → committed .inl → MSVC → runtime mount): early component-level
// returns (verbatim-emit + splice — C++'s own control flow branches) and short-circuit
// markup (`cond && <X/>` / `cond || <X/>` desugared to ternaries; UETKX3002 retired).

#include "Misc/AutomationTest.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace WaveGTest
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
} // namespace WaveGTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxWaveGTest, "ReactiveUI.Uetkx.WaveG",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxWaveGTest::RunTest(const FString&)
{
	TestTrue(TEXT("MultiReturnProof registered"), RUI::HasNamedFactory(FName(TEXT("MultiReturnProof"))));

	// Default Mode=0: the final return renders; `Mode > 10 && big-mode` is FALSE (renders
	// nothing), `Mode > 10 || small-mode` is FALSE -> the || arm renders.
	{
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("MultiReturnProof"))));
		Root->FlushSync();
		SWidget& W = Root->GetWidget().Get();
		TestTrue(TEXT("main branch renders"), WaveGTest::ContainsText(W, TEXT("main")));
		TestFalse(TEXT("&& arm suppressed when false"), WaveGTest::ContainsText(W, TEXT("big-mode")));
		TestTrue(TEXT("|| arm renders when false"), WaveGTest::ContainsText(W, TEXT("small-mode")));
		Root->Unmount();
	}

	// Mode=1: the FIRST early return wins — nothing from the main window mounts.
	{
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("MultiReturnProofEarly"))));
		Root->FlushSync();
		SWidget& W = Root->GetWidget().Get();
		TestTrue(TEXT("early return renders"), WaveGTest::ContainsText(W, TEXT("early-one")));
		TestFalse(TEXT("main window does not mount"), WaveGTest::ContainsText(W, TEXT("main")));
		Root->Unmount();
	}

	// Mode=20: main window with `&&` TRUE (big-mode renders) and `||` TRUE (arm suppressed).
	{
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("MultiReturnProofBig"))));
		Root->FlushSync();
		SWidget& W = Root->GetWidget().Get();
		TestTrue(TEXT("&& arm renders when true"), WaveGTest::ContainsText(W, TEXT("big-mode")));
		TestFalse(TEXT("|| arm suppressed when true"), WaveGTest::ContainsText(W, TEXT("small-mode")));
		Root->Unmount();
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
