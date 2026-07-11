// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Hooks.Tween + ReactiveUI.Hooks.Sfx — the Phase-7 animation/media hooks over the
// mock host's deterministic clock: prime-at-target (no mount animation), frame-chain
// progression, retarget-from-current (no snap), settle-and-stop, UseAnimate's 0..1 driver,
// and the UseSfx sink dispatch.

#include "Misc/AutomationTest.h"
#include "RuiCoreElements.h"
#include "RuiMockHost.h"
#include "RuiNode.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TweenTest
{
	static float GTarget = 0.0f;
	static float GLastValue = -1.0f;
	static bool GAnimateIn = false;
	static float GAnimateValue = -1.0f;

	static FRuiNodeArray TweenComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		GLastValue = Ctx.UseTween(GTarget, /*DurationSec*/ 1.0f, ERuiEase::Linear);
		return {RUI::TextBlock(FString::SanitizeFloat(GLastValue))};
	}
	RUI_COMPONENT(TweenComp)

	static FRuiNodeArray AnimateComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		GAnimateValue = Ctx.UseAnimate(GAnimateIn, 1.0f, ERuiEase::Linear);
		return {RUI::TextBlock(FString::SanitizeFloat(GAnimateValue))};
	}
	RUI_COMPONENT(AnimateComp)
} // namespace TweenTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiTweenTest, "ReactiveUI.Hooks.Tween",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiTweenTest::RunTest(const FString&)
{
	using namespace TweenTest;
	FRuiTestHarness H;
	H.Host.MockTimeSeconds = 100.0;

	GTarget = 10.0f;
	H.Mount(RUI::FC(&TweenComp));
	TestEqual(TEXT("primes AT target (no mount animation)"), GLastValue, 10.0f);
	TestTrue(TEXT("settled: no frame chain armed"), !H.Host.HasQueuedFrames());

	// retarget: animate 10 -> 20 over 1s (linear)
	GTarget = 20.0f;
	H.Reconciler->HmrRefreshAll(); // new target arrives via a re-render
	H.Reconciler->FlushSync();
	TestEqual(TEXT("t=0 still at start"), GLastValue, 10.0f);
	TestTrue(TEXT("frame chain armed"), H.Host.HasQueuedFrames());

	H.Host.MockTimeSeconds = 100.5; // halfway
	H.Host.PumpFrame();
	H.Reconciler->FlushSync();
	TestEqual(TEXT("t=0.5 halfway"), GLastValue, 15.0f, 0.01f);

	H.Host.MockTimeSeconds = 101.25; // past the end
	H.Host.PumpFrame();
	H.Reconciler->FlushSync();
	TestEqual(TEXT("t>=1 settles exactly at target"), GLastValue, 20.0f);

	// retarget mid-flight continues from the CURRENT value (no snap)
	GTarget = 0.0f;
	H.Reconciler->HmrRefreshAll();
	H.Reconciler->FlushSync();
	H.Host.MockTimeSeconds = 101.75; // half of the new 1s run: 20 -> 10
	H.Host.PumpFrame();
	H.Reconciler->FlushSync();
	TestEqual(TEXT("mid-flight retarget lerps from current"), GLastValue, 10.0f, 0.01f);

	// drain to settle
	H.Host.MockTimeSeconds = 105.0;
	TestTrue(TEXT("chain drains once settled"), H.Host.PumpUntilIdle());
	H.Reconciler->FlushSync();
	TestEqual(TEXT("settled at 0"), GLastValue, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiAnimateSfxTest, "ReactiveUI.Hooks.Sfx",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiAnimateSfxTest::RunTest(const FString&)
{
	using namespace TweenTest;

	// UseAnimate: the 0..1 enter/exit driver
	{
		FRuiTestHarness H;
		H.Host.MockTimeSeconds = 10.0;
		GAnimateIn = false;
		H.Mount(RUI::FC(&AnimateComp));
		TestEqual(TEXT("out state primes at 0"), GAnimateValue, 0.0f);

		GAnimateIn = true;
		H.Reconciler->HmrRefreshAll();
		H.Reconciler->FlushSync();
		H.Host.MockTimeSeconds = 10.5;
		H.Host.PumpFrame();
		H.Reconciler->FlushSync();
		TestEqual(TEXT("entering: halfway"), GAnimateValue, 0.5f, 0.01f);
		H.Host.MockTimeSeconds = 11.5;
		TestTrue(TEXT("drains"), H.Host.PumpUntilIdle());
		H.Reconciler->FlushSync();
		TestEqual(TEXT("entered: 1"), GAnimateValue, 1.0f);
	}

	// UseSfx: dispatch goes through the registered sink with bus + payload
	{
		static int32 SinkCalls = 0;
		static FName SinkBus;
		SinkCalls = 0;
		RUI::SetSfxSink(
			[](FName Bus, const FRuiValue&)
			{
				++SinkCalls;
				SinkBus = Bus;
			});

		FRuiTestHarness H;
		static FRuiCallback GPlay;
		struct FLocal
		{
			static FRuiNodeArray Comp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
			{
				GPlay = Ctx.UseSfx(FName(TEXT("ui.click")));
				return {RUI::TextBlock(TEXT("sfx"))};
			}
		};
		H.Mount(RUI::FC(&FLocal::Comp));
		TestTrue(TEXT("callback minted"), GPlay.IsBound());
		GPlay.Execute(FRuiValue(1));
		TestEqual(TEXT("sink invoked"), SinkCalls, 1);
		TestEqual(TEXT("bus carried"), SinkBus, FName(TEXT("ui.click")));

		RUI::SetSfxSink(nullptr);
		GPlay.Execute(FRuiValue(1)); // unset sink: quiet no-op
		TestEqual(TEXT("no sink, no crash"), SinkCalls, 1);
		GPlay = FRuiCallback();
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
