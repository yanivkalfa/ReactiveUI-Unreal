// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// core_test.gd port, part 2: hook semantics, signals, Suspense, the error latch, fragments,
// reuse_by_slot, time slicing, diagnostics counters.

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "RuiMockHost.h"
#include "RuiContext.h"
#include "RuiSignal.h"
#include "RuiCoreElements.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace CoreTest2
{
	static int32 Renders = 0;
	static int32 MemoCalls = 0;
	static TRuiSetter<int32> IntSetter;
	static TFunction<void(FString)> Dispatch;
	static TArray<FString> Order;
	static FString Seen;
	static int32 SeenInt = 0;

	static void Reset()
	{
		Renders = 0;
		MemoCalls = 0;
		IntSetter = TRuiSetter<int32>();
		Dispatch = nullptr;
		Order.Reset();
		Seen.Empty();
		SeenInt = 0;
	}
} // namespace CoreTest2

// â”€â”€ Reducer + memo
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuiNodeArray ReducerMemoComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Value, Disp] = Ctx.UseReducer<int32, FString>(
		[](const int32& S, const FString& A) { return A == TEXT("inc") ? S + 1 : (A == TEXT("dec") ? S - 1 : S); }, 10);
	CoreTest2::Dispatch = Disp;
	const int32 V = Value;
	const int32 Doubled = Ctx.UseMemo<int32>(
		[V]()
		{
			++CoreTest2::MemoCalls;
			return V * 2;
		},
		RUI::Deps(V));
	return {RUI::TextBlock(FString::Printf(TEXT("%d/%d"), V, Doubled))};
}
RUI_COMPONENT(ReducerMemoComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreReducerMemoTest, "ReactiveUI.Core.ReducerAndMemo", RUI_TEST_FLAGS)
bool FRuiCoreReducerMemoTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&ReducerMemoComp));
	TestEqual(TEXT("initial reducer+memo"), H.TextAt(0), FString(TEXT("10/20")));
	TestEqual(TEXT("memo computed once"), CoreTest2::MemoCalls, 1);

	CoreTest2::Dispatch(TEXT("inc"));
	H.Pump();
	TestEqual(TEXT("after inc"), H.TextAt(0), FString(TEXT("11/22")));
	TestEqual(TEXT("memo recomputed on dep change"), CoreTest2::MemoCalls, 2);
	return true;
}

// â”€â”€ Layout-before-passive ordering
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuiNodeArray LayoutOrderComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	Ctx.UseLayoutEffect([]() { CoreTest2::Order.Add(TEXT("layout")); }, RUI::Deps());
	Ctx.UseEffect([]() { CoreTest2::Order.Add(TEXT("passive")); }, RUI::Deps());
	return {RUI::TextBlock(TEXT("x"))};
}
RUI_COMPONENT(LayoutOrderComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreLayoutEffectTest, "ReactiveUI.Core.LayoutEffectOrder", RUI_TEST_FLAGS)
bool FRuiCoreLayoutEffectTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&LayoutOrderComp));
	TestEqual(TEXT("layout before passive"), FString::Join(CoreTest2::Order, TEXT(",")),
			  FString(TEXT("layout,passive")));
	return true;
}

// â”€â”€ State equality semantics (Â§5: value-equality for value types â€” DOCUMENTED divergence
//    from Godot's ref-equality on fresh-but-equal collections)
//    â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuiNodeArray EqualityComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	++CoreTest2::Renders;
	auto [Arr, SetArr] = Ctx.UseState<TArray<int32>>(TArray<int32>{1, 2, 3});
	CoreTest2::SeenInt = Arr.Num();
	// expose via a lambda the test can call
	CoreTest2::Dispatch = [SetArr](FString Cmd)
	{
		if (Cmd == TEXT("equal"))
		{
			SetArr(TArray<int32>{1, 2, 3}); // equal by VALUE -> bails (C++ semantics)
		}
		else
		{
			SetArr(TArray<int32>{1, 2, 3, 4});
		}
	};
	return {RUI::TextBlock(FString::FromInt(Arr.Num()))};
}
RUI_COMPONENT(EqualityComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreEqualityTest, "ReactiveUI.Core.StateEqualitySemantics", RUI_TEST_FLAGS)
bool FRuiCoreEqualityTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&EqualityComp));
	TestEqual(TEXT("mounted"), CoreTest2::Renders, 1);

	AddInfo(TEXT("[equality] value-equal set BAILS (documented Â§5 divergence: C++ containers are values)"));
	CoreTest2::Dispatch(TEXT("equal"));
	H.Pump();
	TestEqual(TEXT("no re-render on value-equal set"), CoreTest2::Renders, 1);

	AddInfo(TEXT("[equality] a different value re-renders"));
	CoreTest2::Dispatch(TEXT("diff"));
	H.Pump();
	TestEqual(TEXT("re-rendered"), CoreTest2::Renders, 2);
	TestEqual(TEXT("new size seen"), CoreTest2::SeenInt, 4);
	return true;
}

// â”€â”€ Signals: basic + unmount unsubscribes + keyed + selector re-bind
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static TSharedPtr<TRuiSignal<int32>> GSigInt;

static FRuiNodeArray SignalComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	++CoreTest2::Renders;
	CoreTest2::SeenInt = RUI::UseSignal<int32>(Ctx, GSigInt.ToSharedRef());
	return {RUI::TextBlock(FString::FromInt(CoreTest2::SeenInt))};
}
RUI_COMPONENT(SignalComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreSignalTest, "ReactiveUI.Core.Signal", RUI_TEST_FLAGS)
bool FRuiCoreSignalTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	GSigInt = MakeShared<TRuiSignal<int32>>(0);
	{
		FRuiTestHarness H;
		H.Mount(RUI::FC(&SignalComp));
		H.Pump(); // effect subscribes post-commit
		TestEqual(TEXT("initial value"), CoreTest2::SeenInt, 0);
		TestEqual(TEXT("one render"), CoreTest2::Renders, 1);
		TestEqual(TEXT("subscribed in effect"), GSigInt->NumSubscribers(), 1);

		GSigInt->Set(5);
		H.Pump();
		TestEqual(TEXT("signal update propagated"), CoreTest2::SeenInt, 5);
		TestEqual(TEXT("re-rendered on change"), CoreTest2::Renders, 2);

		H.Reconciler->Unmount();
	}
	TestEqual(TEXT("unmount unsubscribed"), GSigInt->NumSubscribers(), 0);
	const int32 RendersAfter = CoreTest2::Renders;
	GSigInt->Set(99);
	TestEqual(TEXT("no re-render after unmount"), CoreTest2::Renders, RendersAfter);
	GSigInt.Reset();
	return true;
}

static FRuiNodeArray SignalKeyCompA(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	++CoreTest2::Renders;
	CoreTest2::SeenInt = RUI::UseSignalKey<int32>(Ctx, FName(TEXT("counter")), 10);
	return {RUI::TextBlock(FString::FromInt(CoreTest2::SeenInt))};
}
RUI_COMPONENT(SignalKeyCompA)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreSignalKeyTest, "ReactiveUI.Core.SignalKey", RUI_TEST_FLAGS)
bool FRuiCoreSignalKeyTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	RUI::ClearSignals();
	FRuiTestHarness H1, H2;
	H1.Mount(RUI::FC(&SignalKeyCompA));
	H2.Mount(RUI::FC(&SignalKeyCompA));
	H1.Pump();
	H2.Pump();
	TestEqual(TEXT("both keyed readers mounted"), CoreTest2::Renders, 2);
	TSharedRef<TRuiSignal<int32>> Sig = RUI::GetOrCreateSignal<int32>(FName(TEXT("counter")), 10);
	TestEqual(TEXT("shared keyed signal carries initial"), Sig->Get(), 10);

	Sig->Set(20);
	H1.Pump();
	H2.Pump();
	TestEqual(TEXT("both readers re-rendered"), CoreTest2::Renders, 4);
	TestEqual(TEXT("readers see 20"), CoreTest2::SeenInt, 20);
	RUI::ClearSignals();
	return true;
}

static TSharedPtr<TRuiSignal<TMap<FString, int32>>> GSigMap;

static FRuiNodeArray SignalRebindComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Key, SetKey] = Ctx.UseState<FString>(FString(TEXT("a")));
	CoreTest2::Dispatch = [SetKey](FString K) { SetKey(K); };
	const FString K = Key;
	CoreTest2::SeenInt = RUI::UseSignal<TMap<FString, int32>, int32>(
		Ctx, GSigMap.ToSharedRef(), [K](const TMap<FString, int32>& M) { return M.FindRef(K); });
	return {RUI::TextBlock(FString::FromInt(CoreTest2::SeenInt))};
}
RUI_COMPONENT(SignalRebindComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreSignalRebindTest, "ReactiveUI.Core.SignalSelectorRebind", RUI_TEST_FLAGS)
bool FRuiCoreSignalRebindTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	TMap<FString, int32> Init;
	Init.Add(TEXT("a"), 10);
	Init.Add(TEXT("b"), 20);
	GSigMap = MakeShared<TRuiSignal<TMap<FString, int32>>>(Init);
	FRuiTestHarness H;
	H.Mount(RUI::FC(&SignalRebindComp));
	H.Pump();
	TestEqual(TEXT("selects 'a' = 10"), CoreTest2::SeenInt, 10);

	CoreTest2::Dispatch(TEXT("b")); // selector key changes -> hook re-selects on re-render
	H.Pump();
	TestEqual(TEXT("re-bound selector reads 'b' = 20"), CoreTest2::SeenInt, 20);
	GSigMap.Reset();
	return true;
}

// â”€â”€ MemoEquals (V.memo parity): custom comparer forces bail despite changed props â”€â”€â”€â”€â”€â”€â”€â”€â”€

struct FLabelPropsFwd final : public FRuiPropsBase
{
	RUI_PROP(int32, V, 0)
	RUI_PROPS_BODY(FLabelPropsFwd, RUI_EQ(V))
};

static FRuiNodeArray MemoEqChildComp(FRuiContext&, const FLabelPropsFwd&, const TArray<FRuiNode>&)
{
	++CoreTest2::Renders;
	return {RUI::TextBlock(TEXT("x"))};
}
RUI_COMPONENT(MemoEqChildComp)

static FRuiNodeArray MemoEqParentComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [S, SetS] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetS;
	FLabelPropsFwd P;
	P.SetV(S);
	P.MemoEquals = [](const FRuiPropsBase&, const FRuiPropsBase&) { return true; }; // "always equal"
	return {RUI::FC(&MemoEqChildComp, MoveTemp(P))};
}
RUI_COMPONENT(MemoEqParentComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreMemoEqTest, "ReactiveUI.Core.MemoEquals", RUI_TEST_FLAGS)
bool FRuiCoreMemoEqTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&MemoEqParentComp));
	TestEqual(TEXT("memo child rendered once"), CoreTest2::Renders, 1);
	CoreTest2::IntSetter(1);
	H.Pump();
	TestEqual(TEXT("memo child did NOT re-render (custom comparer)"), CoreTest2::Renders, 1);
	return true;
}

// â”€â”€ Suspense: fallback until IsReady flips
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static TSharedPtr<bool> GReadyFlag;

static FRuiNodeArray SuspenseHostComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	TSharedPtr<bool> Flag = GReadyFlag;
	return {RUI::Suspense([Flag]() { return Flag.IsValid() && *Flag; }, RUI::TextBlock(TEXT("loading")),
						  {RUI::TextBlock(TEXT("loaded"))})};
}
RUI_COMPONENT(SuspenseHostComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreSuspenseTest, "ReactiveUI.Core.Suspense", RUI_TEST_FLAGS)
bool FRuiCoreSuspenseTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	GReadyFlag = MakeShared<bool>(false);
	FRuiTestHarness H;
	H.Mount(RUI::FC(&SuspenseHostComp));
	H.Pump(); // effect arms the poll driver
	TestEqual(TEXT("fallback initially"), H.TextAt(0), FString(TEXT("loading")));

	*GReadyFlag = true;
	H.Pump(4); // poll fires -> SetReady -> re-render
	TestEqual(TEXT("children after ready"), H.TextAt(0), FString(TEXT("loaded")));
	GReadyFlag.Reset();
	return true;
}

// â”€â”€ Error boundary + the cooperative latch (D-10)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static TSharedPtr<bool> GShouldFail;
static TSharedPtr<FString> GCaught;

static FRuiNodeArray FailingComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	if (GShouldFail.IsValid() && *GShouldFail)
	{
		RUI_RENDER_FAIL(TEXT("boom"));
		return {};
	}
	return {RUI::TextBlock(TEXT("fine"))};
}
RUI_COMPONENT(FailingComp)

static FRuiNodeArray BoundaryHostComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [ResetKey, SetResetKey] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetResetKey;
	return {RUI::ErrorBoundary(RUI::TextBlock(TEXT("fallback")), {RUI::FC(&FailingComp)}, FRuiKey(ResetKey),
							   [](const FString& Reason)
							   {
								   if (GCaught.IsValid())
								   {
									   *GCaught = Reason;
								   }
							   })};
}
RUI_COMPONENT(BoundaryHostComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreErrorBoundaryTest, "ReactiveUI.Core.ErrorBoundary", RUI_TEST_FLAGS)
bool FRuiCoreErrorBoundaryTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	GShouldFail = MakeShared<bool>(true);
	GCaught = MakeShared<FString>();
	AddExpectedError(TEXT("render failed"), EAutomationExpectedErrorFlags::Contains, 0);

	FRuiTestHarness H;
	H.Mount(RUI::FC(&BoundaryHostComp));
	H.Pump(4); // failure -> restart -> boundary renders fallback
	TestEqual(TEXT("boundary shows fallback"), H.TextAt(0), FString(TEXT("fallback")));
	TestTrue(TEXT("OnError received the reason"), GCaught->Contains(TEXT("boom")));

	AddInfo(TEXT("[boundary] reset-key change recovers"));
	*GShouldFail = false;
	CoreTest2::IntSetter(1); // new reset key -> boundary clears and re-renders children
	H.Pump(4);
	TestEqual(TEXT("children after reset"), H.TextAt(0), FString(TEXT("fine")));
	GShouldFail.Reset();
	GCaught.Reset();
	return true;
}

// â”€â”€ Fragment flattening order
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuiNodeArray FragmentComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	return {RuiTest::Box(RuiTest::BoxProps(TEXT("list")),
						 {
							 RUI::TextBlock(TEXT("a")),
							 RUI::Fragment({RUI::TextBlock(TEXT("b")), RUI::TextBlock(TEXT("c"))}),
							 RUI::TextBlock(TEXT("d")),
						 })};
}
RUI_COMPONENT(FragmentComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreFragmentTest, "ReactiveUI.Core.Fragment", RUI_TEST_FLAGS)
bool FRuiCoreFragmentTest::RunTest(const FString&)
{
	FRuiTestHarness H;
	H.Mount(RUI::FC(&FragmentComp));
	FMockNode* List = H.ChildAt(0);
	if (!TestEqual(TEXT("fragment flattens to 4 children"), List->Children.Num(), 4))
	{
		return false;
	}
	TArray<FString> Texts;
	for (const TSharedPtr<FMockNode>& C : List->Children)
	{
		Texts.Add(C->TextOf());
	}
	TestEqual(TEXT("order a,b,c,d"), FString::Join(Texts, TEXT(",")), FString(TEXT("a,b,c,d")));
	return true;
}

// â”€â”€ GO-09 reuse_by_slot: full key churn, zero node churn
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuiNodeArray ReuseBySlotComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Frame, SetFrame] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetFrame;
	TArray<FRuiNode> Items;
	for (int32 i = 0; i < 5; ++i)
	{
		// EVERY key changes every render â€” the keyed path would delete+recreate all 5.
		Items.Add(RuiTest::Box(RuiTest::BoxProps(FString::Printf(TEXT("v%d"), i + Frame), i + Frame), {},
							   FRuiKey(FString::Printf(TEXT("k%d_%d"), i, Frame))));
	}
	FTestBoxProps ContainerProps = RuiTest::BoxProps(TEXT("container"));
	ContainerProps.bReuseBySlot = true;
	return {RuiTest::Box(MoveTemp(ContainerProps), MoveTemp(Items))};
}
RUI_COMPONENT(ReuseBySlotComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreReuseBySlotTest, "ReactiveUI.Core.ReuseBySlot", RUI_TEST_FLAGS)
bool FRuiCoreReuseBySlotTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&ReuseBySlotComp));
	FMockNode* Container = H.ChildAt(0);
	if (!TestEqual(TEXT("5 slots"), Container->Children.Num(), 5))
	{
		return false;
	}
	FMockNode* N0 = Container->Children[0].Get();
	FMockNode* N4 = Container->Children[4].Get();

	FRuiDiagnostics::bEnabled = true;
	FRuiDiagnostics::Reset();
	CoreTest2::IntSetter(1);
	H.Pump();
	TestTrue(TEXT("nodes REUSED across full key change"),
			 Container->Children[0].Get() == N0 && Container->Children[4].Get() == N4);
	TestTrue(TEXT("ZERO mount/unmount churn"), FRuiDiagnostics::Placements == 0 && FRuiDiagnostics::Deletions == 0);
	TestEqual(TEXT("reused node's props updated in place"), N0->PropsAs<FTestBoxProps>()->Value, 1);
	FRuiDiagnostics::bEnabled = false;
	return true;
}

// â”€â”€ Time slicing: budget 0 parks per unit; the sliced update still completes
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuiNodeArray SlicedComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Frame, SetFrame] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetFrame;
	TArray<FRuiNode> Items;
	for (int32 i = 0; i < 8; ++i)
	{
		Items.Add(RuiTest::Box(RuiTest::BoxProps(FString::Printf(TEXT("item %d-%d"), i, Frame)), {}, FRuiKey(i)));
	}
	return {RuiTest::Box(RuiTest::BoxProps(TEXT("list")), MoveTemp(Items))};
}
RUI_COMPONENT(SlicedComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreTimeSlicingTest, "ReactiveUI.Core.TimeSlicing", RUI_TEST_FLAGS)
bool FRuiCoreTimeSlicingTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	IConsoleVariable* Slicing = IConsoleManager::Get().FindConsoleVariable(TEXT("rui.TimeSlicing"));
	IConsoleVariable* Budget = IConsoleManager::Get().FindConsoleVariable(TEXT("rui.FrameBudgetMs"));
	Slicing->Set(true);
	Budget->Set(0.0f);

	FRuiTestHarness H;
	H.Mount(RUI::FC(&SlicedComp)); // mount is always synchronous
	FMockNode* List = H.ChildAt(0);
	TestEqual(TEXT("sliced: initial 8 items"), List->Children.Num(), 8);
	TestEqual(TEXT("sliced: initial label"), List->Children[0]->PropsAs<FTestBoxProps>()->Label,
			  FString(TEXT("item 0-0")));

	CoreTest2::IntSetter(1);
	TestTrue(TEXT("sliced update completes"), H.Host.PumpUntilIdle(200));
	TestEqual(TEXT("first item updated"), List->Children[0]->PropsAs<FTestBoxProps>()->Label,
			  FString(TEXT("item 0-1")));
	TestEqual(TEXT("last item updated"), List->Children[7]->PropsAs<FTestBoxProps>()->Label, FString(TEXT("item 7-1")));

	Slicing->Set(false);
	Budget->Set(8.0f);
	return true;
}

// â”€â”€ Refs: attach on placement, null on removal (React lifecycle, D-08.4)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static TSharedPtr<FRuiHostHandle> GCapturedRef;

static FRuiNodeArray RefComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [bShow, SetShow] = Ctx.UseState<bool>(true);
	CoreTest2::Dispatch = [SetShow](FString Cmd) { SetShow(Cmd == TEXT("show")); };
	if (bShow)
	{
		FTestBoxProps P = RuiTest::BoxProps(TEXT("target"));
		P.Ref = [](const FRuiHostHandle& H)
		{
			if (GCapturedRef.IsValid())
			{
				*GCapturedRef = H;
			}
		};
		return {RuiTest::Box(MoveTemp(P))};
	}
	return {RUI::TextBlock(TEXT("gone"))};
}
RUI_COMPONENT(RefComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreRefTest, "ReactiveUI.Core.RefLifecycle", RUI_TEST_FLAGS)
bool FRuiCoreRefTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	GCapturedRef = MakeShared<FRuiHostHandle>();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&RefComp));
	TestTrue(TEXT("ref populated while mounted"), GCapturedRef->IsValid());

	CoreTest2::Dispatch(TEXT("hide"));
	H.Pump();
	TestFalse(TEXT("ref nulled when node removed"), GCapturedRef->IsValid());
	GCapturedRef.Reset();
	return true;
}

// â”€â”€ Deferred value: lags one render, then catches up
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuiNodeArray DeferredComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Now, SetNow] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetNow;
	const int32 Deferred = Ctx.UseDeferredValue<int32>(Now);
	CoreTest2::SeenInt = Deferred;
	CoreTest2::Seen = FString::Printf(TEXT("%d/%d"), Now, Deferred);
	return {RUI::TextBlock(CoreTest2::Seen)};
}
RUI_COMPONENT(DeferredComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreDeferredTest, "ReactiveUI.Core.DeferredValue", RUI_TEST_FLAGS)
bool FRuiCoreDeferredTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&DeferredComp));
	TestEqual(TEXT("initial 0/0"), CoreTest2::Seen, FString(TEXT("0/0")));

	CoreTest2::IntSetter(5);
	H.Host.PumpFrame(); // one frame: urgent renders with stale deferred
	TestEqual(TEXT("urgent updates, deferred lags (5/0)"), CoreTest2::Seen, FString(TEXT("5/0")));

	TestTrue(TEXT("deferred settles"), H.Host.PumpUntilIdle(16));
	TestEqual(TEXT("deferred catches up"), CoreTest2::SeenInt, 5);
	return true;
}

// â”€â”€ SafeArea comes from the host (mock returns 1,2,3,4)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuiNodeArray SafeAreaComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	FRuiSafeArea SA = Ctx.UseSafeArea();
	CoreTest2::Seen = FString::Printf(TEXT("%.0f,%.0f,%.0f,%.0f"), SA.Left, SA.Top, SA.Right, SA.Bottom);
	return {RUI::TextBlock(CoreTest2::Seen)};
}
RUI_COMPONENT(SafeAreaComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreSafeAreaTest, "ReactiveUI.Core.SafeArea", RUI_TEST_FLAGS)
bool FRuiCoreSafeAreaTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&SafeAreaComp));
	TestEqual(TEXT("host-supplied safe area"), CoreTest2::Seen, FString(TEXT("1,2,3,4")));
	return true;
}

// â”€â”€ Diagnostics counters
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuiNodeArray DiagComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [V, SetV] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetV;
	return {RUI::TextBlock(FString::FromInt(V))};
}
RUI_COMPONENT(DiagComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreDiagnosticsTest, "ReactiveUI.Core.Diagnostics", RUI_TEST_FLAGS)
bool FRuiCoreDiagnosticsTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuiDiagnostics::bEnabled = true;
	FRuiDiagnostics::Reset();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&DiagComp));
	TestTrue(TEXT("counted initial render"), FRuiDiagnostics::Renders >= 1);
	TestTrue(TEXT("counted placements"), FRuiDiagnostics::Placements >= 1);
	const int32 R0 = FRuiDiagnostics::Renders;
	CoreTest2::IntSetter(1);
	H.Pump();
	TestTrue(TEXT("counted update render"), FRuiDiagnostics::Renders > R0);
	TestTrue(TEXT("counted prop update"), FRuiDiagnostics::Updates >= 1);
	FRuiDiagnostics::bEnabled = false;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
