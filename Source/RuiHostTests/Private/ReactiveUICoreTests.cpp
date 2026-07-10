// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Port of tests/core_test.gd + update_test.gd against the mock host. Sections print via
// AddInfo so hangs name their culprit (house rule). Godot tests that exercise HOST features
// (react events on real controls, item models, classes/stylesheets, custom draw, node pool
// internals, tree/root-node) port with the Slate host in Phases 8/9 suites; the router suite
// is post-v1 (TD-001). C++-semantics adaptations are commented inline (§5 deviations).

#include "Misc/AutomationTest.h"
#include "RuiMockHost.h"
#include "RuiContext.h"
#include "RuiSignal.h"
#include "RuiCoreElements.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Shared test-component plumbing: statics reset per test (components must be free functions
// for registry identity — D-05).
namespace CoreTestState
{
	static int32 RenderCountA = 0;
	static int32 RenderCountB = 0;
	static int32 RenderCountC = 0;
	static TRuiSetter<int32> SetterA;
	static TRuiSetter<int32> SetterB;
	static TArray<FString> Log;

	static void ResetAll()
	{
		RenderCountA = RenderCountB = RenderCountC = 0;
		SetterA = TRuiSetter<int32>();
		SetterB = TRuiSetter<int32>();
		Log.Reset();
	}
} // namespace CoreTestState

// ─────────────────────────────────────────────────────────────────────────────────────────
// Update path (update_test.gd)
// ─────────────────────────────────────────────────────────────────────────────────────────

static FRuiNodeArray UpdateTestComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [V, SetV] = Ctx.UseState<int32>(0);
	++CoreTestState::RenderCountA;
	CoreTestState::SetterA = SetV;
	return {RUI::Text(FString::Printf(TEXT("v=%d"), V))};
}
RUI_COMPONENT(UpdateTestComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreUpdateTest, "ReactiveUI.Update.CoalescedDiff", RUI_TEST_FLAGS)
bool FRuiCoreUpdateTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	FRuiTestHarness H;

	AddInfo(TEXT("[update] 1/3 initial render"));
	H.Mount(RUI::FC(&UpdateTestComp));
	TestEqual(TEXT("initial render count == 1"), RenderCountA, 1);
	FMockNode* Label = H.ChildAt(0);
	if (!TestNotNull(TEXT("label mounted"), Label))
	{
		return false;
	}
	TestEqual(TEXT("initial text"), Label->TextOf(), FString(TEXT("v=0")));

	AddInfo(TEXT("[update] 2/3 setState -> one re-render, node REUSED"));
	SetterA(5);
	H.Pump();
	TestEqual(TEXT("one coalesced re-render"), RenderCountA, 2);
	TestTrue(TEXT("host node REUSED (diff, not recreate)"), H.ChildAt(0) == Label);
	TestEqual(TEXT("updated text"), Label->TextOf(), FString(TEXT("v=5")));

	AddInfo(TEXT("[update] 3/3 two sets in one frame coalesce"));
	SetterA(10);
	SetterA(11);
	H.Pump();
	TestEqual(TEXT("two sets coalesce to one render"), RenderCountA, 3);
	TestEqual(TEXT("final text"), Label->TextOf(), FString(TEXT("v=11")));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Effects (deps + cleanup ordering + unmount)
// ─────────────────────────────────────────────────────────────────────────────────────────

static FRuiNodeArray EffectsComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Count, SetCount] = Ctx.UseState<int32>(0);
	auto [Other, SetOther] = Ctx.UseState<int32>(0);
	CoreTestState::SetterA = SetCount;
	CoreTestState::SetterB = SetOther;
	const int32 Cur = Count;
	Ctx.UseEffect(
		[Cur]() -> FRuiEffectCleanup
		{
			CoreTestState::Log.Add(FString::Printf(TEXT("setup:%d"), Cur));
			return [Cur]() { CoreTestState::Log.Add(FString::Printf(TEXT("cleanup:%d"), Cur)); };
		},
		RUI::Deps(Count));
	return {RUI::Text(TEXT("x"))};
}
RUI_COMPONENT(EffectsComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreEffectsTest, "ReactiveUI.Core.Effects", RUI_TEST_FLAGS)
bool FRuiCoreEffectsTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	{
		FRuiTestHarness H;
		AddInfo(TEXT("[effects] mount"));
		H.Mount(RUI::FC(&EffectsComp));
		TestEqual(TEXT("effect runs on mount"), FString::Join(Log, TEXT(",")), FString(TEXT("setup:0")));

		AddInfo(TEXT("[effects] dep change -> cleanup then setup"));
		SetterA(1);
		H.Pump();
		TestEqual(TEXT("cleanup->setup on dep change"), FString::Join(Log, TEXT(",")),
				  FString(TEXT("setup:0,cleanup:0,setup:1")));

		AddInfo(TEXT("[effects] unrelated state -> effect skipped"));
		SetterB(99);
		H.Pump();
		TestEqual(TEXT("effect skipped when deps unchanged"), FString::Join(Log, TEXT(",")),
				  FString(TEXT("setup:0,cleanup:0,setup:1")));

		AddInfo(TEXT("[effects] unmount runs cleanup"));
		H.Reconciler->Unmount();
	}
	TestEqual(TEXT("cleanup on unmount"), FString::Join(Log, TEXT(",")),
			  FString(TEXT("setup:0,cleanup:0,setup:1,cleanup:1")));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Bailout + SUBTREE-SKIP (D-08.1 — the adopted React mechanism, asserted directly)
// ─────────────────────────────────────────────────────────────────────────────────────────

struct FLabelProps final : public FRuiPropsBase
{
	RUI_PROP(FString, LabelText, 0)
	RUI_PROPS_BODY(FLabelProps, RUI_EQ(LabelText))
};

static FRuiNodeArray BailChildComp(FRuiContext&, const FLabelProps& Props, const TArray<FRuiNode>&)
{
	++CoreTestState::RenderCountB;
	return {RUI::Text(Props.LabelText)};
}
RUI_COMPONENT(BailChildComp)

static FRuiNodeArray BailParentComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	++CoreTestState::RenderCountA;
	auto [S, SetS] = Ctx.UseState<int32>(0);
	CoreTestState::SetterA = SetS;
	FLabelProps ChildProps;
	ChildProps.SetLabelText(TEXT("static"));
	return {
		RUI::Text(FString::Printf(TEXT("count %d"), S)),
		RUI::FC(&BailChildComp, MoveTemp(ChildProps)),
	};
}
RUI_COMPONENT(BailParentComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreBailoutTest, "ReactiveUI.Core.Bailout", RUI_TEST_FLAGS)
bool FRuiCoreBailoutTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&BailParentComp));
	TestEqual(TEXT("initial parent renders"), RenderCountA, 1);
	TestEqual(TEXT("initial child renders"), RenderCountB, 1);

	SetterA(1);
	H.Pump();
	TestEqual(TEXT("parent re-rendered"), RenderCountA, 2);
	TestEqual(TEXT("child BAILED (equal props)"), RenderCountB, 1);
	return true;
}

// A → B(static passthrough) → C(leaf): bumping A re-renders A; B bails on Equals (fresh but
// equal props); B's cached output hands C IDENTICAL vnodes -> C takes the SUBTREE-SKIP path.
static FRuiNodeArray SkipLeafComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	++CoreTestState::RenderCountC;
	return {RUI::Text(TEXT("leaf"))};
}
RUI_COMPONENT(SkipLeafComp)

static FRuiNodeArray SkipMidComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	++CoreTestState::RenderCountB;
	return {RUI::FC(&SkipLeafComp)};
}
RUI_COMPONENT(SkipMidComp)

static FRuiNodeArray SkipTopComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	++CoreTestState::RenderCountA;
	auto [S, SetS] = Ctx.UseState<int32>(0);
	CoreTestState::SetterA = SetS;
	return {
		RUI::Text(FString::Printf(TEXT("top %d"), S)),
		RUI::FC(&SkipMidComp),
	};
}
RUI_COMPONENT(SkipTopComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreSubtreeSkipTest, "ReactiveUI.Core.SubtreeSkip", RUI_TEST_FLAGS)
bool FRuiCoreSubtreeSkipTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&SkipTopComp));
	TestEqual(TEXT("mount: top"), RenderCountA, 1);
	TestEqual(TEXT("mount: mid"), RenderCountB, 1);
	TestEqual(TEXT("mount: leaf"), RenderCountC, 1);
	FMockNode* LeafText = H.ChildAt(1); // [topText, leafText] flattened under root

	SetterA(1);
	H.Pump();
	TestEqual(TEXT("top re-rendered"), RenderCountA, 2);
	TestEqual(TEXT("mid bailed"), RenderCountB, 1);
	TestEqual(TEXT("leaf skipped entirely (subtree-skip)"), RenderCountC, 1);
	TestTrue(TEXT("leaf host node untouched"), H.ChildAt(1) == LeafText && LeafText->UpdateCount == 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Keyed reorder + identity + removal
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace CoreTestState
{
	static TRuiSetter<TArray<FString>> KeySetter;
}

static FRuiNodeArray KeyedComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Ids, SetIds] = Ctx.UseState<TArray<FString>>(TArray<FString>{TEXT("a"), TEXT("b"), TEXT("c")});
	CoreTestState::KeySetter = SetIds;
	TArray<FRuiNode> Items;
	for (const FString& Id : Ids)
	{
		Items.Add(RuiTest::Box(RuiTest::BoxProps(Id), {}, FRuiKey(Id)));
	}
	return {RuiTest::Box(RuiTest::BoxProps(TEXT("list")), MoveTemp(Items))};
}
RUI_COMPONENT(KeyedComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreKeyedTest, "ReactiveUI.Core.KeyedReorder", RUI_TEST_FLAGS)
bool FRuiCoreKeyedTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	FRuiTestHarness H;
	H.Mount(RUI::FC(&KeyedComp));
	FMockNode* List = H.ChildAt(0);
	if (!TestNotNull(TEXT("list mounted"), List) || !TestEqual(TEXT("3 children"), List->Children.Num(), 3))
	{
		return false;
	}
	FMockNode* A = List->Children[0].Get();
	FMockNode* B = List->Children[1].Get();
	FMockNode* C = List->Children[2].Get();

	AddInfo(TEXT("[keyed] reorder c,a,b preserves identity"));
	KeySetter(TArray<FString>{TEXT("c"), TEXT("a"), TEXT("b")});
	H.Pump();
	TestTrue(TEXT("c first"), List->Children[0].Get() == C);
	TestTrue(TEXT("a second"), List->Children[1].Get() == A);
	TestTrue(TEXT("b third"), List->Children[2].Get() == B);

	AddInfo(TEXT("[keyed] removal keeps survivors' identity"));
	KeySetter(TArray<FString>{TEXT("c"), TEXT("b")});
	H.Pump();
	TestEqual(TEXT("2 children after removal"), List->Children.Num(), 2);
	TestTrue(TEXT("c,b remain with identity"), List->Children[0].Get() == C && List->Children[1].Get() == B);
	TestTrue(TEXT("a released"), A->bReleased);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Context: typed handles, defaults, distinct identity, change propagation, survives bailout
// ─────────────────────────────────────────────────────────────────────────────────────────

static TRuiContext<FString> GThemeCtx(FString(TEXT("fallback")), FName(TEXT("Theme")));

namespace CoreTestState
{
	static FString SeenContext;
	static TRuiSetter<FString> ThemeSetter;
} // namespace CoreTestState

static FRuiNodeArray CtxConsumerComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	++CoreTestState::RenderCountB;
	auto [S, SetS] = Ctx.UseState<int32>(0); // gives the test a handle to force re-render
	CoreTestState::SetterB = SetS;
	CoreTestState::SeenContext = Ctx.UseContext(GThemeCtx);
	return {RUI::Text(CoreTestState::SeenContext)};
}
RUI_COMPONENT(CtxConsumerComp)

static FRuiNodeArray CtxProviderComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Theme, SetTheme] = Ctx.UseState<FString>(FString(TEXT("dark")));
	CoreTestState::ThemeSetter = SetTheme;
	Ctx.ProvideContext(GThemeCtx, Theme);
	return {RUI::FC(&CtxConsumerComp)};
}
RUI_COMPONENT(CtxProviderComp)

static FRuiNodeArray CtxGrandparentComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [S, SetS] = Ctx.UseState<int32>(0);
	CoreTestState::SetterA = SetS;
	return {
		RUI::Text(FString::Printf(TEXT("gp %d"), S)),
		RUI::FC(&CtxProviderComp),
	};
}
RUI_COMPONENT(CtxGrandparentComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCoreContextTest, "ReactiveUI.Core.Context", RUI_TEST_FLAGS)
bool FRuiCoreContextTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	SeenContext.Empty();
	{
		AddInfo(TEXT("[context] provider -> consumer + change propagation through bailouts"));
		FRuiTestHarness H;
		H.Mount(RUI::FC(&CtxProviderComp));
		TestEqual(TEXT("consumer sees provided value"), SeenContext, FString(TEXT("dark")));
		TestEqual(TEXT("consumer rendered once"), RenderCountB, 1);

		ThemeSetter(FString(TEXT("light")));
		H.Pump();
		TestEqual(TEXT("consumer sees updated value"), SeenContext, FString(TEXT("light")));
		TestEqual(TEXT("consumer re-rendered on context change"), RenderCountB, 2);
	}
	{
		AddInfo(TEXT("[context] no provider -> handle default"));
		ResetAll();
		SeenContext.Empty();
		FRuiTestHarness H;
		H.Mount(RUI::FC(&CtxConsumerComp));
		TestEqual(TEXT("unprovided handle returns default"), SeenContext, FString(TEXT("fallback")));
	}
	{
		AddInfo(TEXT("[context] distinct handles have distinct identity"));
		TRuiContext<int32> A(1);
		TRuiContext<int32> B(1);
		TestTrue(TEXT("identity differs despite equal defaults"), A.Key() != B.Key());
	}
	{
		AddInfo(TEXT("[context] survives provider bailout (carried provided map pushes on descend)"));
		ResetAll();
		SeenContext.Empty();
		FRuiTestHarness H;
		H.Mount(RUI::FC(&CtxGrandparentComp));
		TestEqual(TEXT("initial context"), SeenContext, FString(TEXT("dark")));
		SetterA(1); // grandparent re-renders; provider bails (fresh-but-equal props)
		H.Pump();
		SetterB(1); // force the consumer to re-render and re-read
		H.Pump();
		TestEqual(TEXT("context SURVIVES provider bailout"), SeenContext, FString(TEXT("dark")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
