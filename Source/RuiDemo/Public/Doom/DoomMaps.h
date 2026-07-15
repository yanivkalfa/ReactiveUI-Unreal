// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Doom demo — the MAPS module. Faithful port of the Unity ReactiveUIToolKit `DoomGame`
// sample's `DoomMaps.uitkx` (cross-checked against the Godot sibling's doom_maps.gd):
// the fluent MapBuilder DSL + the six hand-authored levels (E1M1–E1M6). Every
// coordinate/number is copied digit-for-digit from the original — gameplay depends on it.

#pragma once

#include "CoreMinimal.h"
#include "Doom/DoomTypes.h"

namespace RuiDoom
{
	// Everything a level hands to the game on load.
	struct FLevelStart
	{
		FMapDef Map;
		float PlayerX = 0.f;
		float PlayerY = 0.f;
		float PlayerAngle = 0.f;
		TArray<FMobj> Mobjs;
		// When true, this level's Exit cells will only fire victory once every
		// boss-tier monster (Baron, Cacodemon) is dead.
		bool BossExitGated = false;
	};

	// ───── Builder ─────
	// Fluent map-authoring DSL — method names 1:1 with the Unity original's MapBuilder.
	class RUIDEMO_API FMapBuilder
	{
	public:
		int32 W = 0;
		int32 H = 0;
		TArray<FCell> Cells;
		FString Name;
		TArray<FMobj> Mobjs;
		float PX = 0.f;
		float PY = 0.f;
		float PAng = 0.f;
		bool BossExitGated = false;

		FMapBuilder(int32 InW, int32 InH, const FString& InName);

		FCell& At(int32 X, int32 Y);

		FMapBuilder& Border(uint8 WallTex);
		FMapBuilder& Fill(int32 X0, int32 Y0, int32 X1, int32 Y1, uint8 FloorTex, uint8 CeilTex, uint8 Light);
		FMapBuilder& Box(int32 X0, int32 Y0, int32 X1, int32 Y1, uint8 WallTex);
		FMapBuilder& Wall(int32 X, int32 Y, uint8 WallTex);
		FMapBuilder& Door(int32 X, int32 Y, ECellKind Kind = ECellKind::Door);
		FMapBuilder& Exit(int32 X, int32 Y);
		// Walkable exit pad — same trigger semantics as Exit() but the cell renders
		// as plain floor (no EXIT-sign wall block). The blue floor texture is the cue.
		FMapBuilder& ExitPad(int32 X, int32 Y);
		FMapBuilder& Liquid(int32 X0, int32 Y0, int32 X1, int32 Y1, uint8 Tex);
		// Raised platform / step: per-cell FloorZ on an existing walkable cell.
		// Use STEP_HEIGHT (0.4) units for a single step.
		FMapBuilder& Step(int32 X0, int32 Y0, int32 X1, int32 Y1, float FloorZ);
		// Low ceiling / overhang: per-cell CeilingZ. Implicitly clears the sky flag
		// because a bounded ceiling is the opposite of sky.
		FMapBuilder& LowCeiling(int32 X0, int32 Y0, int32 X1, int32 Y1, float CeilZ);
		// Open-air ceiling. Player can jump as high as they like and looks up into sky.
		FMapBuilder& Sky(int32 X0, int32 Y0, int32 X1, int32 Y1);
		FMapBuilder& PlayerStart(float X, float Y, float Angle);
		// Stack a 3D floor slab into the rectangle [X0..X1, Y0..Y1]. The slab spans
		// world-Z [BottomZ, TopZ]; the player walks on TopZ and the underside is
		// visible as a "ceiling" below BottomZ. Multiple calls in the same footprint
		// stack: a basement under a courtyard is sector floor + slab whose top is
		// the courtyard surface; a balcony over a hall is the hall + slab whose
		// bottom is the hall ceiling.
		FMapBuilder& Floor3D(int32 X0, int32 Y0, int32 X1, int32 Y1, float BottomZ, float TopZ, uint8 SideTex,
							 uint8 TopTex, uint8 BottomTex = 0, uint8 Light = 200, bool Solid = true);
		FMapBuilder& Spawn(EMobjKind Kind, float X, float Y, float Angle = 0.f);

		FLevelStart Build() const;
	};

	RUIDEMO_API int32 HealthFor(EMobjKind K);
	RUIDEMO_API float RadiusFor(EMobjKind K);
	RUIDEMO_API float HeightFor(EMobjKind K);

	// ───── Levels ─────

	// Number of authored levels (E1M1..E1M6) — BuildLevel/LevelName take 1..LEVEL_COUNT.
	constexpr int32 LEVEL_COUNT = 6;

	/** Build level 1..6 (anything else falls back to Level1, like the original). */
	RUIDEMO_API FLevelStart BuildLevel(int32 Level);

	/** Display name for level 1..6 without building it ("E1M1: Hangar", ...). */
	RUIDEMO_API const TCHAR* LevelName(int32 Level);

	RUIDEMO_API FLevelStart Level1();
	RUIDEMO_API FLevelStart Level2();
	RUIDEMO_API FLevelStart Level3();
	RUIDEMO_API FLevelStart Level4();
	RUIDEMO_API FLevelStart Level5();
	RUIDEMO_API FLevelStart Level6();
} // namespace RuiDoom
