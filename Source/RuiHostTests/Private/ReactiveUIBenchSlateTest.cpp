// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Bench.SlateReorder — the Phase 2 step 1 reorder-strategy spike, decided on
// numbers, not vibes (MASTER_PLAN: detach/attach minimal-move vs full rebuild; the
// invalidation-boundary wrap claim was downgraded by the critique and is measured only if
// these two disagree badly). Rows land in plans/BENCH_BASELINES.md; the strategy decision
// in plans/TECH_DEBT.md. PerfFilter — run: Automation RunTests ReactiveUI.Bench
//
// Workloads on a raw SVerticalBox of N=200 STextBlocks (no reconciler — pure panel cost):
//   move_one:     one widget end->front. minimal-move = 1 RemoveSlot + 1 InsertSlot;
//                 rebuild = ClearChildren + 200 AddSlot.
//   full_reverse: worst case. minimal-move = ~N remove+insert pairs; rebuild = same clear+add.

#include "Misc/AutomationTest.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RuiSlateBench
{
	static constexpr int32 N = 200;
	static constexpr int32 Reps = 5;

	static double ToUs(uint64 Cycles)
	{
		return FPlatformTime::ToSeconds64(Cycles) * 1e6;
	}

	static TArray<TSharedRef<SWidget>> MakeWidgets()
	{
		TArray<TSharedRef<SWidget>> Out;
		Out.Reserve(N);
		for (int32 i = 0; i < N; ++i)
		{
			Out.Add(SNew(STextBlock).Text(FText::AsNumber(i)));
		}
		return Out;
	}

	static TSharedRef<SVerticalBox> BuildBox(const TArray<TSharedRef<SWidget>>& Widgets)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (const TSharedRef<SWidget>& W : Widgets)
		{
			SVerticalBox::FScopedWidgetSlotArguments Slot = Box->InsertSlot(INDEX_NONE);
			Slot.AttachWidget(W);
		}
		return Box;
	}

	/** Enforce Target order by moving only out-of-place widgets (the shipped strategy). */
	static int32 MinimalMove(SVerticalBox& Box, const TArray<TSharedRef<SWidget>>& Target)
	{
		int32 Moves = 0;
		FChildren* Children = Box.GetChildren();
		for (int32 i = 0; i < Target.Num(); ++i)
		{
			if (i < Children->Num() && &Children->GetChildAt(i).Get() == &Target[i].Get())
			{
				continue;
			}
			Box.RemoveSlot(Target[i]);
			SVerticalBox::FScopedWidgetSlotArguments Slot = Box.InsertSlot(i);
			Slot.AttachWidget(Target[i]);
			++Moves;
		}
		return Moves;
	}

	static void Rebuild(SVerticalBox& Box, const TArray<TSharedRef<SWidget>>& Target)
	{
		Box.ClearChildren();
		for (const TSharedRef<SWidget>& W : Target)
		{
			SVerticalBox::FScopedWidgetSlotArguments Slot = Box.InsertSlot(INDEX_NONE);
			Slot.AttachWidget(W);
		}
	}
} // namespace RuiSlateBench

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBenchSlateReorderTest, "ReactiveUI.Bench.SlateReorder",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)
bool FRuiBenchSlateReorderTest::RunTest(const FString&)
{
	using namespace RuiSlateBench;

	auto Report = [this](const TCHAR* Name, TArray<double>& Us, const TCHAR* Workload)
	{
		Us.Sort();
		AddInfo(FString::Printf(TEXT("RUIBENCH %s: min=%.0fus med=%.0fus max=%.0fus (n=%d, %s)"), Name, Us[0],
								Us[Us.Num() / 2], Us.Last(), Us.Num(), Workload));
	};

	// move_one: end -> front
	{
		TArray<double> MinimalUs, RebuildUs;
		for (int32 r = 0; r < Reps; ++r)
		{
			TArray<TSharedRef<SWidget>> Widgets = MakeWidgets();
			TArray<TSharedRef<SWidget>> Target = Widgets;
			const TSharedRef<SWidget> Last = Target.Pop();
			Target.Insert(Last, 0);

			TSharedRef<SVerticalBox> BoxA = BuildBox(Widgets);
			uint64 T0 = FPlatformTime::Cycles64();
			MinimalMove(*BoxA, Target);
			MinimalUs.Add(ToUs(FPlatformTime::Cycles64() - T0));

			TSharedRef<SVerticalBox> BoxB = BuildBox(Widgets);
			T0 = FPlatformTime::Cycles64();
			Rebuild(*BoxB, Target);
			RebuildUs.Add(ToUs(FPlatformTime::Cycles64() - T0));
		}
		Report(TEXT("reorder_move_one_minimal"), MinimalUs, TEXT("200 texts, 1 moved end->front, minimal-move"));
		Report(TEXT("reorder_move_one_rebuild"), RebuildUs, TEXT("200 texts, 1 moved end->front, clear+rebuild"));
	}

	// full_reverse
	{
		TArray<double> MinimalUs, RebuildUs;
		for (int32 r = 0; r < Reps; ++r)
		{
			TArray<TSharedRef<SWidget>> Widgets = MakeWidgets();
			TArray<TSharedRef<SWidget>> Target = Widgets;
			Algo::Reverse(Target);

			TSharedRef<SVerticalBox> BoxA = BuildBox(Widgets);
			uint64 T0 = FPlatformTime::Cycles64();
			MinimalMove(*BoxA, Target);
			MinimalUs.Add(ToUs(FPlatformTime::Cycles64() - T0));

			TSharedRef<SVerticalBox> BoxB = BuildBox(Widgets);
			T0 = FPlatformTime::Cycles64();
			Rebuild(*BoxB, Target);
			RebuildUs.Add(ToUs(FPlatformTime::Cycles64() - T0));
		}
		Report(TEXT("reorder_reverse_minimal"), MinimalUs, TEXT("200 texts, full reverse, minimal-move"));
		Report(TEXT("reorder_reverse_rebuild"), RebuildUs, TEXT("200 texts, full reverse, clear+rebuild"));
	}

	return true; // benches never fail
}

#endif // WITH_DEV_AUTOMATION_TESTS
