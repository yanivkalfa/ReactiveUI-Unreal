// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Bench.Doom — the Doom-demo workload (the family's marquee stress test): a real
// game frame's cost split the way the Godot F3 overlay splits it — sim tick, renderer cast,
// geometry build, and the reconcile+Slate-apply of the full framebuffer (several hundred
// keyed widgets churning per frame). NOT pass/fail; rows log as "RUIBENCH ..." and committed
// numbers live in plans/BENCH_BASELINES.md with machine/config context.
//   Automation RunTests ReactiveUI.Bench

#include "Doom/DoomGameLogic.h"
#include "Doom/DoomScreenGeometry.h"
#include "Doom/DoomScreenTypes.h"
#include "Doom/DoomTypes.h"
#include "Misc/AutomationTest.h"
#include "RuiContext.h"
#include "RuiRoot.h"
#include "RuiSlateElements.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DoomBench
{
	// The reconcile bench's shared frame data (the component reads these; the driver mutates).
	static const RuiDoom::FDoomFrameGeometry* GGeometry = nullptr;
	static TFunction<void(int32)> GSetVersion;

	static double ToUs(uint64 Cycles)
	{
		return FPlatformTime::ToSeconds64(Cycles) * 1e6;
	}

	/** A walking+turning input so the view (and thus the quad set) actually churns. */
	static RuiDoom::FInputCmd WalkCmd(int32 TicIndex)
	{
		RuiDoom::FInputCmd Cmd;
		Cmd.Forward = true;
		Cmd.YawDelta = (TicIndex % 2 == 0) ? 0.015f : -0.011f;
		return Cmd;
	}

	static FRuiNode NodeForQuad(const RuiDoom::FDoomQuad& Q)
	{
		FRuiImageProps P;
		P.SetImage(Q.Brush);
		P.SetColorAndOpacity(Q.Tint);
		FRuiNode Node = RUI::Slate::Image(MoveTemp(P), FRuiKey(Q.Key));
		TSharedRef<FRuiStyleDict> Slot = MakeShared<FRuiStyleDict>();
		Slot->Add(FName(TEXT("slot.position")), FRuiValue(Q.Position));
		Slot->Add(FName(TEXT("slot.size")), FRuiValue(Q.Size));
		const_cast<FRuiPropsBase&>(*Node.Props).SlotProps = Slot;
		return Node;
	}

	/** The framebuffer component — the same 11 passes DoomGameScreen.uetkx renders, driven by
	 *  a version state exactly like the real hook drives it. */
	static FRuiNodeArray BenchFrameComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		auto [Version, SetVersion] = Ctx.UseState<int32>(0);
		GSetVersion = SetVersion;
		(void)Version;

		TArray<FRuiNode> Quads;
		if (GGeometry != nullptr)
		{
			const RuiDoom::FDoomFrameGeometry& G = *GGeometry;
			const TArray<RuiDoom::FDoomQuad>* Passes[] = {
				&G.Sky,		  &G.FloorBackstop, &G.CeilingBands, &G.FloorBands, &G.FloorRims, &G.ExtraSegs,
				&G.RiserRims, &G.Walls,			&G.Sprites,		 &G.Tracers,	&G.Overlay};
			int32 Total = 0;
			for (const TArray<RuiDoom::FDoomQuad>* Pass : Passes)
			{
				Total += Pass->Num();
			}
			Quads.Reserve(Total);
			for (const TArray<RuiDoom::FDoomQuad>* Pass : Passes)
			{
				for (const RuiDoom::FDoomQuad& Q : *Pass)
				{
					Quads.Add(NodeForQuad(Q));
				}
			}
		}
		return {RUI::Slate::Canvas(FRuiCanvasPanelProps(), MoveTemp(Quads))};
	}
	RUI_COMPONENT(BenchFrameComp)
} // namespace DoomBench

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBenchDoomTest, "ReactiveUI.Bench.Doom",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)
bool FRuiBenchDoomTest::RunTest(const FString&)
{
	using namespace RuiDoom;

	auto Report = [this](const TCHAR* Name, TArray<double>& Us, const TCHAR* Workload)
	{
		Us.Sort();
		AddInfo(FString::Printf(TEXT("RUIBENCH %s: min=%.0fus med=%.0fus max=%.0fus (n=%d, %s)"), Name, Us[0],
								Us[Us.Num() / 2], Us.Last(), Us.Num(), Workload));
	};
	const FVector2D Viewport(C::VIEWPORT_W, C::VIEWPORT_H);
	constexpr float Dt = 1.0f / 60.0f;

	// newgame_e1m1: level build + sector model + first cast.
	{
		TArray<double> Us;
		for (int32 r = 0; r < 5; ++r)
		{
			const uint64 T0 = FPlatformTime::Cycles64();
			FGameState St = NewGame(1, EDifficulty::Normal);
			Us.Add(DoomBench::ToUs(FPlatformTime::Cycles64() - T0));
			(void)St;
		}
		Report(TEXT("doom_newgame_e1m1"), Us, TEXT("E1M1 build + spawn + first CastFrame"));
	}

	FGameState St = NewGame(1, EDifficulty::Normal);
	FDoomBrushPool Brushes;
	FDoomFrameGeometry Geometry;
	BuildFrameGeometry(St, Brushes, Viewport, Geometry);

	// sim_tick: 300 walking ticks (includes CastFrame — the real per-frame sim cost).
	{
		TArray<double> Us;
		for (int32 t = 0; t < 300; ++t)
		{
			const uint64 T0 = FPlatformTime::Cycles64();
			Tick(St, Dt, DoomBench::WalkCmd(t));
			Us.Add(DoomBench::ToUs(FPlatformTime::Cycles64() - T0));
		}
		Report(TEXT("doom_sim_tick"), Us, TEXT("Tick incl. CastFrame, E1M1, walking+turning, 300 tics"));
	}

	// build_geometry: quad-list building over the live state (warm brush pool).
	{
		TArray<double> Us;
		for (int32 t = 0; t < 100; ++t)
		{
			Tick(St, Dt, DoomBench::WalkCmd(t));
			const uint64 T0 = FPlatformTime::Cycles64();
			BuildFrameGeometry(St, Brushes, Viewport, Geometry);
			Us.Add(DoomBench::ToUs(FPlatformTime::Cycles64() - T0));
		}
		int32 Total = Geometry.Sky.Num() + Geometry.FloorBackstop.Num() + Geometry.CeilingBands.Num() +
					  Geometry.FloorBands.Num() + Geometry.FloorRims.Num() + Geometry.ExtraSegs.Num() +
					  Geometry.RiserRims.Num() + Geometry.Walls.Num() + Geometry.Sprites.Num() +
					  Geometry.Tracers.Num() + Geometry.Overlay.Num();
		Report(TEXT("doom_build_geometry"), Us,
			   *FString::Printf(TEXT("BuildFrameGeometry, ~%d quads, warm brush pool"), Total));
	}

	// reconcile_frame: the headline — sim + geometry + reconcile + REAL Slate apply for the
	// whole framebuffer, per frame, exactly as the game screen's version-bump drives it.
	{
		DoomBench::GGeometry = &Geometry;
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&DoomBench::BenchFrameComp));
		Root->FlushSync();

		TArray<double> Us;
		for (int32 t = 0; t < 120; ++t)
		{
			const uint64 T0 = FPlatformTime::Cycles64();
			Tick(St, Dt, DoomBench::WalkCmd(t));
			BuildFrameGeometry(St, Brushes, Viewport, Geometry);
			DoomBench::GSetVersion(t + 1);
			Root->FlushSync();
			Us.Add(DoomBench::ToUs(FPlatformTime::Cycles64() - T0));
		}
		Report(TEXT("doom_reconcile_frame"), Us,
			   TEXT("tick + geometry + reconcile + Slate apply, full framebuffer, 120 frames"));

		Root->Unmount();
		DoomBench::GGeometry = nullptr;
		DoomBench::GSetVersion = nullptr;
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
