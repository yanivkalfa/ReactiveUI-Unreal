// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Core.Presence — TD-003 exit-animation (delayed-unmount) protocol. A <Presence>
// boundary keeps a removed keyed child MOUNTED so it can animate out, flips bPresent=false into
// it via context, and unmounts for real only on NotifyDone() (or the MaxExitSeconds timeout).
// Covered: deferred deletion, NotifyDone-driven real unmount, the timeout fence for a child that
// never notifies, and re-entry cancelling an in-flight exit (fiber + tween state preserved).

#include "Misc/AutomationTest.h"
#include "RuiCoreElements.h"
#include "RuiMockHost.h"
#include "RuiNode.h"
#include "RuiPresence.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PresenceTest
{
	// Which keyed children the boundary's parent renders this pass.
	static bool GIncludeA = false;
	static bool GIncludeB = false;
	static bool GIncludeC = false;

	struct FItemProps final : public FRuiPropsBase
	{
		RUI_PROP(FString, Label, 0)
		RUI_PROPS_BODY(FItemProps, RUI_EQ(Label))
	};

	static FItemProps ItemProps(const TCHAR* Label)
	{
		FItemProps P;
		P.SetLabel(Label);
		return P;
	}

	// A child that animates out and reports completion: when exiting (bPresent=false) and its
	// UseAnimate driver has settled at 0, it calls NotifyDone so the boundary can unmount it.
	static FRuiNodeArray ExitItem(FRuiContext& Ctx, const FItemProps& Props, const TArray<FRuiNode>&)
	{
		const FRuiPresenceState P = UsePresence(Ctx);
		const float T = Ctx.UseAnimate(P.bPresent, 1.0f, ERuiEase::Linear);
		const bool bPresent = P.bPresent;
		const FRuiCallback Done = P.NotifyDone;
		Ctx.UseEffect(
			[bPresent, T, Done]()
			{
				if (!bPresent && T <= 0.0f)
				{
					Done.Execute();
				}
			},
			RUI::EveryCommit());
		return {RuiTest::Box(RuiTest::BoxProps(Props.Label))};
	}
	RUI_COMPONENT(ExitItem)

	// A child that reads the signal but NEVER notifies — proves the timeout fence unmounts it.
	static FRuiNodeArray SilentItem(FRuiContext& Ctx, const FItemProps& Props, const TArray<FRuiNode>&)
	{
		(void)UsePresence(Ctx);
		return {RuiTest::Box(RuiTest::BoxProps(Props.Label))};
	}
	RUI_COMPONENT(SilentItem)

	static FRuiNodeArray PresenceHost(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		TArray<FRuiNode> Kids;
		if (GIncludeA)
		{
			Kids.Add(RUI::FC<FItemProps>(&ExitItem, ItemProps(TEXT("A")), {}, FRuiKey(TEXT("A"))));
		}
		if (GIncludeB)
		{
			Kids.Add(RUI::FC<FItemProps>(&ExitItem, ItemProps(TEXT("B")), {}, FRuiKey(TEXT("B"))));
		}
		if (GIncludeC)
		{
			Kids.Add(RUI::FC<FItemProps>(&SilentItem, ItemProps(TEXT("C")), {}, FRuiKey(TEXT("C"))));
		}
		return {RUI::Presence(MoveTemp(Kids), /*MaxExitSeconds*/ 2.0f)};
	}
	RUI_COMPONENT(PresenceHost)
} // namespace PresenceTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiPresenceTest, "ReactiveUI.Core.Presence",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiPresenceTest::RunTest(const FString&)
{
	using namespace PresenceTest;

	FRuiTestHarness H;
	H.Host.MockTimeSeconds = 100.0;

	// The live (un-released) host box labels, in tree order.
	auto Labels = [&H]() -> TArray<FString>
	{
		TArray<FString> Out;
		for (const TSharedPtr<FMockNode>& C : H.Host.Root->Children)
		{
			if (C.IsValid() && !C->bReleased)
			{
				Out.Add(C->PropsAs<FTestBoxProps>()->Label);
			}
		}
		return Out;
	};
	auto Has = [&Labels](const TCHAR* L) { return Labels().Contains(FString(L)); };
	// Advance the clock and drive the frame-chain + state flushes to quiescence.
	auto Drain = [&H](double AdvanceTo)
	{
		H.Host.MockTimeSeconds = AdvanceTo;
		for (int32 i = 0; i < 16; ++i)
		{
			H.Host.PumpFrame();
			H.Reconciler->FlushSync();
		}
	};
	auto Rerender = [&H]()
	{
		H.Reconciler->HmrRefreshAll();
		H.Reconciler->FlushSync();
	};

	// ── mount with A + B ───────────────────────────────────────────────────────────────────
	GIncludeA = true;
	GIncludeB = true;
	GIncludeC = false;
	H.Mount(RUI::FC(&PresenceHost));
	TestEqual(TEXT("mounted A + B"), Labels().Num(), 2);
	TestTrue(TEXT("A present"), Has(TEXT("A")));
	TestTrue(TEXT("B present"), Has(TEXT("B")));
	const int32 ReleasedAfterMount = H.Host.ReleasedCount;

	// ── remove B: it must stay mounted (exiting), not vanish ────────────────────────────────
	GIncludeB = false;
	Rerender();
	TestEqual(TEXT("B deferred: still 2 boxes"), Labels().Num(), 2);
	TestTrue(TEXT("B still mounted while exiting"), Has(TEXT("B")));
	TestEqual(TEXT("nothing released yet"), H.Host.ReleasedCount, ReleasedAfterMount);

	// ── let B's 1s exit animation settle -> NotifyDone -> real unmount ───────────────────────
	Drain(102.0);
	TestEqual(TEXT("B unmounted after it animated out"), Labels().Num(), 1);
	TestTrue(TEXT("A survived"), Has(TEXT("A")));
	TestFalse(TEXT("B gone"), Has(TEXT("B")));
	TestTrue(TEXT("B was released for real"), H.Host.ReleasedCount > ReleasedAfterMount);

	// ── timeout fence: a child that never notifies is force-unmounted after MaxExitSeconds ───
	GIncludeC = true; // SilentItem
	Rerender();
	TestTrue(TEXT("C mounted"), Has(TEXT("C")));
	const int32 ReleasedBeforeTimeout = H.Host.ReleasedCount;

	GIncludeC = false;
	Rerender();
	TestTrue(TEXT("C kept during its (silent) exit"), Has(TEXT("C")));
	TestEqual(TEXT("C not released before the timeout"), H.Host.ReleasedCount, ReleasedBeforeTimeout);

	Drain(H.Host.MockTimeSeconds + 3.0); // > MaxExitSeconds (2s)
	TestFalse(TEXT("C force-unmounted by the timeout fence"), Has(TEXT("C")));
	TestTrue(TEXT("only A remains"), Labels().Num() == 1 && Has(TEXT("A")));

	// ── re-entry cancels an in-flight exit: same fiber, never released ───────────────────────
	const int32 ReleasedBeforeReentry = H.Host.ReleasedCount;
	GIncludeA = false;
	Rerender();											   // A starts exiting
	H.Host.MockTimeSeconds = H.Host.MockTimeSeconds + 0.3; // partway through the 1s exit
	H.Host.PumpFrame();
	H.Reconciler->FlushSync();
	TestTrue(TEXT("A still mid-exit"), Has(TEXT("A")));

	GIncludeA = true;
	Rerender(); // A re-added before it settled -> exit cancelled
	Drain(H.Host.MockTimeSeconds + 5.0);
	TestTrue(TEXT("A survived the cancelled exit"), Has(TEXT("A")));
	TestEqual(TEXT("re-entry never triggered a real unmount"), H.Host.ReleasedCount, ReleasedBeforeReentry);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
