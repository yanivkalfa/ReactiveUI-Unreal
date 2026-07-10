// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Bench.Core — reconciler-only microbenches over the MOCK host (no Slate, no
// widgets: pure render/diff/commit cost). NOT pass/fail (family: bench*.gd) — every run
// logs "RUIBENCH ..." rows; committed numbers live in plans/BENCH_BASELINES.md with their
// machine/engine/config context. PerfFilter keeps them out of the correctness battery:
//   Automation RunTests ReactiveUI.Bench
//
// Workload notes (context for the numbers):
// - Leaf labels are precomputed ONCE (static cache) so rows measure the reconciler, not
//   1000x FString::Printf. The update bench formats exactly ONE fresh label per frame.
// - Mount rows include component render + fiber build + host-node creation (mock nodes are
//   MakeShared structs). Churn rows include unmount (cleanups + slab release) too.

#include "Misc/AutomationTest.h"
#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiMockHost.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RuiBench
{
	static constexpr int32 LeafCount = 1000;
	static constexpr int32 KeyedCount = 500;
	static constexpr int32 ChurnLeaves = 200;
	static constexpr int32 Reps = 5;

	static int32 ChangedIndex = -1; // update_1_of_1000: which leaf gets a fresh label
	static TFunction<void(int32)> SetFrame;
	static TFunction<void(bool)> SetReversed;

	static const TArray<FString>& Labels()
	{
		static TArray<FString> Cache;
		if (Cache.IsEmpty())
		{
			Cache.Reserve(LeafCount);
			for (int32 i = 0; i < LeafCount; ++i)
			{
				Cache.Add(FString::Printf(TEXT("leaf %d"), i));
			}
		}
		return Cache;
	}

	static double ToUs(uint64 Cycles)
	{
		return FPlatformTime::ToSeconds64(Cycles) * 1e6;
	}

	static void Reset()
	{
		ChangedIndex = -1;
		SetFrame = nullptr;
		SetReversed = nullptr;
	}
} // namespace RuiBench

// ── workloads ─────────────────────────────────────────────────────────────────────────────

/** 1000 unkeyed leaf boxes under one container (the fast-leaf-list shape). */
static FRuiNodeArray BenchLeafList(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Frame, SetFrameState] = Ctx.UseState<int32>(0);
	RuiBench::SetFrame = SetFrameState;
	const TArray<FString>& Labels = RuiBench::Labels();
	TArray<FRuiNode> Leaves;
	Leaves.Reserve(RuiBench::LeafCount);
	for (int32 i = 0; i < RuiBench::LeafCount; ++i)
	{
		const bool bChanged = (i == RuiBench::ChangedIndex);
		Leaves.Add(
			RuiTest::Box(RuiTest::BoxProps(bChanged ? FString::Printf(TEXT("leaf %d f%d"), i, Frame) : Labels[i], i)));
	}
	return {RuiTest::Box(RuiTest::BoxProps(TEXT("list")), MoveTemp(Leaves))};
}
RUI_COMPONENT(BenchLeafList)

/** 500 keyed boxes whose order a state flips — the keyed mark-sweep + reorder path. */
static FRuiNodeArray BenchKeyedList(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [bReversed, SetReversedState] = Ctx.UseState<bool>(false);
	RuiBench::SetReversed = SetReversedState;
	const TArray<FString>& Labels = RuiBench::Labels();
	TArray<FRuiNode> Leaves;
	Leaves.Reserve(RuiBench::KeyedCount);
	for (int32 n = 0; n < RuiBench::KeyedCount; ++n)
	{
		const int32 i = bReversed ? (RuiBench::KeyedCount - 1 - n) : n;
		Leaves.Add(RuiTest::Box(RuiTest::BoxProps(Labels[i], i), {}, FRuiKey(i)));
	}
	return {RuiTest::Box(RuiTest::BoxProps(TEXT("list")), MoveTemp(Leaves))};
}
RUI_COMPONENT(BenchKeyedList)

/** Small nested tree (containers + leaves + a hook) for mount/unmount churn. */
static FRuiNodeArray BenchChurnTree(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [V, SetV] = Ctx.UseState<int32>(0);
	(void)SetV;
	const TArray<FString>& Labels = RuiBench::Labels();
	TArray<FRuiNode> Rows;
	Rows.Reserve(RuiBench::ChurnLeaves / 10);
	for (int32 r = 0; r < RuiBench::ChurnLeaves / 10; ++r)
	{
		TArray<FRuiNode> Cells;
		Cells.Reserve(10);
		for (int32 c = 0; c < 10; ++c)
		{
			Cells.Add(RuiTest::Box(RuiTest::BoxProps(Labels[r * 10 + c], V)));
		}
		Rows.Add(RuiTest::Box(RuiTest::BoxProps(Labels[r], r), MoveTemp(Cells)));
	}
	return {RuiTest::Box(RuiTest::BoxProps(TEXT("grid")), MoveTemp(Rows))};
}
RUI_COMPONENT(BenchChurnTree)

// ── driver ────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBenchCoreTest, "ReactiveUI.Bench.Core",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)
bool FRuiBenchCoreTest::RunTest(const FString&)
{
	auto Report = [this](const TCHAR* Name, TArray<double>& Us, const TCHAR* Workload)
	{
		Us.Sort();
		AddInfo(FString::Printf(TEXT("RUIBENCH %s: min=%.0fus med=%.0fus max=%.0fus (n=%d, %s)"), Name, Us[0],
								Us[Us.Num() / 2], Us.Last(), Us.Num(), Workload));
	};

	// mount_1000_leaves: full cold mount (render + fibers + host nodes), teardown untimed.
	{
		RuiBench::Reset();
		TArray<double> Us;
		for (int32 r = 0; r < RuiBench::Reps; ++r)
		{
			FRuiTestHarness H;
			const uint64 T0 = FPlatformTime::Cycles64();
			H.Mount(RUI::FC(&BenchLeafList));
			Us.Add(RuiBench::ToUs(FPlatformTime::Cycles64() - T0));
		}
		Report(TEXT("mount_1000_leaves"), Us, TEXT("1 container + 1000 unkeyed leaf boxes"));
	}

	// noop_rerender_1000: RequestUpdate with nothing changed — bailout + fast-list walk.
	{
		RuiBench::Reset();
		FRuiTestHarness H;
		H.Mount(RUI::FC(&BenchLeafList));
		TArray<double> Us;
		for (int32 r = 0; r < RuiBench::Reps; ++r)
		{
			const uint64 T0 = FPlatformTime::Cycles64();
			H.Reconciler->RequestUpdate();
			H.Host.PumpFrame();
			Us.Add(RuiBench::ToUs(FPlatformTime::Cycles64() - T0));
		}
		Report(TEXT("noop_rerender_1000"), Us, TEXT("root RequestUpdate, zero prop changes"));
	}

	// update_1_of_1000: one leaf's label changes per frame — render + diff + 1 CommitUpdate.
	{
		RuiBench::Reset();
		FRuiTestHarness H;
		H.Mount(RUI::FC(&BenchLeafList));
		TArray<double> Us;
		for (int32 r = 0; r < RuiBench::Reps; ++r)
		{
			RuiBench::ChangedIndex = (r * 137) % RuiBench::LeafCount;
			const uint64 T0 = FPlatformTime::Cycles64();
			RuiBench::SetFrame(r + 1);
			H.Host.PumpFrame();
			Us.Add(RuiBench::ToUs(FPlatformTime::Cycles64() - T0));
		}
		Report(TEXT("update_1_of_1000"), Us, TEXT("setState -> 1 changed label among 1000"));
	}

	// keyed_reverse_500: full order flip of a keyed list — mark-sweep + reorder enforce.
	{
		RuiBench::Reset();
		FRuiTestHarness H;
		H.Mount(RUI::FC(&BenchKeyedList));
		TArray<double> Us;
		for (int32 r = 0; r < RuiBench::Reps; ++r)
		{
			const uint64 T0 = FPlatformTime::Cycles64();
			RuiBench::SetReversed((r % 2) == 0);
			H.Host.PumpFrame();
			Us.Add(RuiBench::ToUs(FPlatformTime::Cycles64() - T0));
		}
		Report(TEXT("keyed_reverse_500"), Us, TEXT("500 keyed boxes, order fully reversed"));
	}

	// mount_unmount_churn_200: cold mount + full teardown of a nested 200-leaf grid.
	{
		RuiBench::Reset();
		TArray<double> Us;
		for (int32 r = 0; r < RuiBench::Reps; ++r)
		{
			const uint64 T0 = FPlatformTime::Cycles64();
			{
				FRuiTestHarness H;
				H.Mount(RUI::FC(&BenchChurnTree));
			}
			Us.Add(RuiBench::ToUs(FPlatformTime::Cycles64() - T0));
		}
		Report(TEXT("mount_unmount_churn_200"), Us, TEXT("20 rows x 10 cells, mount + unmount"));
	}

	RuiBench::Reset();
	return true; // benches never fail — numbers go to plans/BENCH_BASELINES.md
}

#endif // WITH_DEV_AUTOMATION_TESTS
