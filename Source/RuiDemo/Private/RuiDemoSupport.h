// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Support code for the .uetkx gallery screens — the family's logic-file/markup-file split
// (doom_game_screen_logic.gd next to doom_game_screen.guitkx). .uetkx bodies are function
// bodies and preambles are includes, so file-level constructs (context objects, signal names,
// draw functions, sim structs) live here and the screens' setup blocks consume them.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Rendering/DrawElements.h"
#include "RuiContext.h"
#include "RuiSignalViewModel.h"
#include "RuiSlateElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UWorld;

namespace RuiDemo
{
	// ── shared identities (ONE instance process-wide — inline variables) ───────────────────
	inline TRuiContext<FLinearColor> GDemoThemeCtx(FLinearColor(0.23f, 0.65f, 0.95f), FName(TEXT("DemoTheme")));
	inline const FName GDemoCounterSignal(TEXT("demo.counter"));

	// ── the demo world (set by the game mode; the interop screens' UMG embeds need one) ─────
	// Screens read this instead of GWorld: explicit ownership, null headless → fallback branch.
	inline TWeakObjectPtr<UWorld> GDemoWorld;
	inline void SetDemoWorld(UWorld* World)
	{
		GDemoWorld = World;
	}
	inline UWorld* GetDemoWorld()
	{
		return GDemoWorld.Get();
	}

	// ── the shared reverse-MVVM viewmodel (ours; THEIR widgets bind it — TD-021 reverse) ────
	// One process-wide URuiSignalViewModel, GC-rooted by the strong ptr: the UmgHostDemo button
	// writes it, our UseField reads it, and UDemoVmBoundWidget (a plain UMG widget) subscribes
	// to the same FieldNotify broadcast — one value, both worlds.
	inline URuiSignalViewModel* GetSharedVm()
	{
		static TStrongObjectPtr<URuiSignalViewModel> Vm;
		if (!Vm.IsValid())
		{
			Vm.Reset(NewObject<URuiSignalViewModel>());
		}
		return Vm.Get();
	}

	// ── CustomDraw paint functions ─────────────────────────────────────────────────────────
	inline int32 DrawPolygon(const FGeometry& Geo, FSlateWindowElementList& Out, int32 LayerId, int32 Sides)
	{
		const FVector2D Center = FVector2D(Geo.GetLocalSize()) * 0.5;
		const double Radius = FMath::Min(Center.X, Center.Y) - 8.0;
		TArray<FVector2D> Points;
		for (int32 i = 0; i <= Sides; ++i)
		{
			const double Angle = 2.0 * PI * i / Sides - PI / 2.0;
			Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(Out, LayerId + 1, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None,
									 FLinearColor(0.4f, 0.8f, 1.0f), true, 2.0f);
		return LayerId + 1;
	}

	inline int32 DrawQuad(const FGeometry& Geo, FSlateWindowElementList& Out, int32 LayerId, bool bBlue)
	{
		FSlateDrawElement::MakeBox(Out, LayerId + 1, Geo.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"),
								   ESlateDrawEffect::None,
								   bBlue ? FLinearColor(0.2f, 0.4f, 0.9f) : FLinearColor(0.95f, 0.55f, 0.15f));
		return LayerId + 1;
	}

	inline int32 DrawScatter(const FGeometry& Geo, FSlateWindowElementList& Out, int32 LayerId)
	{
		// Re-seeds per PAINT — the whole point of the RedrawKey demo (same fn, new pixels).
		FRandomStream Rng(static_cast<int32>(FPlatformTime::Cycles()));
		const FVector2D Size = FVector2D(Geo.GetLocalSize());
		for (int32 i = 0; i < 60; ++i)
		{
			const FVector2D Pos(Rng.FRandRange(0.0f, Size.X - 3.0f), Rng.FRandRange(0.0f, Size.Y - 3.0f));
			FSlateDrawElement::MakeBox(Out, LayerId + 1,
									   Geo.ToPaintGeometry(FVector2D(3.0, 3.0), FSlateLayoutTransform(Pos)),
									   FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None,
									   FLinearColor(0.6f, 1.0f, 0.6f, Rng.FRandRange(0.4f, 1.0f)));
		}
		return LayerId + 1;
	}

	// ── StressTest simulation types ────────────────────────────────────────────────────────
	inline constexpr float StressAreaW = 620.0f;
	inline constexpr float StressAreaH = 320.0f;

	struct FStressBox
	{
		int32 Id = 0;
		FVector2D Pos = FVector2D::ZeroVector;
		FVector2D Vel = FVector2D::ZeroVector;
		float Size = 10.0f;
		FLinearColor Color = FLinearColor::White;
	};

	struct FStressStats
	{
		float AvgFps = 0.0f;
		float Elapsed = 0.0f;
		int32 Frames = 0;
		bool bFinished = false;
		bool operator==(const FStressStats& O) const
		{
			return AvgFps == O.AvgFps && Elapsed == O.Elapsed && Frames == O.Frames && bFinished == O.bFinished;
		}
	};
} // namespace RuiDemo
