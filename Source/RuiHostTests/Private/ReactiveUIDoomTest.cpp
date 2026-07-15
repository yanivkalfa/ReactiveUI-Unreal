// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Port of the Godot sibling's tests/doom_game_test.gd (the 179-check Doom regression suite)
// against the RuiDoom C++ port. Sections print via AddInfo so hangs name their culprit
// (house rule). Check-for-check faithful — dt values, seeds, and expected numbers are copied
// digit-for-digit — with these documented adaptations:
//   - GDScript called game_logic.gd's INTERNAL functions directly (update_player, try_use,
//     update_doors, fire_weapon, splash, hurt, try_give_pickup, update_pickup). The C++ port
//     keeps those `static` (internal linkage, faithful to the C# original's private methods),
//     so those checks drive the SAME code paths through the public Tick() with scripted
//     FInputCmd, and Hurt/Splash through DamageMobj() on a Barrel — a zero-distance barrel
//     explosion deals exactly (int)(DMG_BARREL * 1.0 * 0.6) = 54 to the player at Normal, so
//     the exact-health assertions stay exact (the GDScript hurt(10) numbers become 54s).
//   - Where Tick() (unlike a direct update_player/update_doors call) would let the level's
//     live monsters perturb an exact-value assertion, the monsters are despawned first
//     (Id = 0) — same assertion semantics, deterministic. Checks that tolerate live monsters
//     keep them (faithful).
//   - GDScript "x is Array" nil-guards map to "the builder runs and fills the out-param"
//     checks; "col.main != null" maps to "Main.Distance > 0" (FWallSeg is a value member —
//     every CastFrame path writes a positive distance).
//   - build_merged_floor_bands == raw count is adapted: the Godot port performed NO actual
//     horizontal merge (hence equality); this port DOES merge same-slab runs (inherited
//     improvement), so the invariant becomes 0 < merged <= raw.
//   - NOT ported (4 checks): _test_integration_tick (2) and _test_menu_and_switch (2) mount
//     the Godot reconciler + DoomInputState singleton + scene-tree physics_frame wiring —
//     Godot-host plumbing with no pure-logic equivalent; the Unreal screen-mount lives with
//     the ReactiveUI.Demos suite once the Doom screen wave lands. Replaced by 7 pure-logic
//     level-exit / boss-gate checks (ReactiveUI.Doom.Exit) the GDScript suite covered only
//     indirectly via any_boss_alive. Total here: 182 checks vs the GDScript 179.

#include "Doom/DoomTypes.h" // FIRST — #undefs the engine's MAX_PITCH macro leak

#include "Doom/DoomGameLogic.h"
#include "Doom/DoomMaps.h"
#include "Doom/DoomRaycast.h"
#include "Doom/DoomScreenGeometry.h"
#include "Doom/DoomTextures.h"

#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "TextureResource.h"

// Engine headers included after DoomTypes.h can re-leak AudioDefines.h's bare
// `#define MAX_PITCH 2.0f` through the shared PCH — drop it again so C::MAX_PITCH survives.
#ifdef MAX_PITCH
#undef MAX_PITCH
#endif

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

using namespace RuiDoom;

namespace DoomTestUtil
{
	// Despawn the level's monsters (Id = 0 is the pool's free-slot sentinel) so Tick()-driven
	// exact-value checks can't be perturbed by monster AI. bKeepBosses preserves Baron/
	// Cacodemon for the boss-gate checks.
	static void DespawnMonsters(FGameState& St, bool bKeepBosses = false)
	{
		for (int32 i = 1; i <= St.MobjCount; i++)
		{
			FMobj& M = St.Mobjs[i];
			if (M.Id != 0 && IsMonster(M.Kind) && !(bKeepBosses && IsBoss(M.Kind)))
			{
				M.Id = 0;
			}
		}
	}

	// AddMobj takes the mobj by value and assigns the next pool id — spawn and return the
	// claimed pool index (the GDScript test's "scan the pool for the instance" idiom).
	static int32 SpawnTracked(FGameState& St, const FMobj& M)
	{
		const int32 NewId = St.NextMobjId;
		AddMobj(St, M);
		for (int32 i = 1; i <= St.MobjCount; i++)
		{
			if (St.Mobjs[i].Id == NewId)
			{
				return i;
			}
		}
		return -1;
	}

	// Linear cell index of the first cell of `Kind`, or -1.
	static int32 FindCellOfKind(const FMapDef& Map, ECellKind Kind)
	{
		for (int32 i = 0; i < Map.Cells.Num(); i++)
		{
			if (Map.Cells[i].Kind == Kind)
			{
				return i;
			}
		}
		return -1;
	}
} // namespace DoomTestUtil

// ─────────────────────────────────────────────────────────────────────────────────────────
// _test_types (7 checks)
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomTypesTest, "ReactiveUI.Doom.Types", RUI_TEST_FLAGS)
bool FRuiDoomTypesTest::RunTest(const FString&)
{
	AddInfo(TEXT("[types] 3x3 smoke map + sector conversion + state wiring"));
	FMapDef Map;
	Map.Width = 3;
	Map.Height = 3;
	Map.Name = TEXT("smoke");
	Map.Cells.SetNum(9); // default FCell is Empty, FloorZ/CeilingZ = 0 (as the GD test sets)
	Map.Cells[0].Kind = ECellKind::Wall;

	TestTrue(TEXT("at_safe out-of-bounds is WALL"), Map.AtSafe(-1, -1).Kind == ECellKind::Wall);
	TestTrue(TEXT("blocks_movement: wall cell blocks"), Map.BlocksMovement(0, 0));
	TestFalse(TEXT("blocks_movement: empty cell doesn't block"), Map.BlocksMovement(1, 0));

	const FMapData SectorMap = FMapData::FromTiles(Map);
	TestTrue(TEXT("from_tiles: sector map is valid"), SectorMap.IsValid());
	TestEqual(TEXT("from_tiles: 8 non-wall cells -> 8 sectors"), SectorMap.Sectors.Num(), 8);
	TestTrue(TEXT("from_tiles: generated linedefs"), SectorMap.Lines.Num() > 0);

	FGameState Gs;
	Gs.Player.Health = C::START_HEALTH;
	TestEqual(TEXT("GameState/PlayerState/C constant wiring"), Gs.Player.Health, 100);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// _test_textures (11 checks)
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomTexturesTest, "ReactiveUI.Doom.Textures", RUI_TEST_FLAGS)
bool FRuiDoomTexturesTest::RunTest(const FString&)
{
	AddInfo(TEXT("[textures] EnsureBuilt + inventories"));
	FDoomTextures::EnsureBuilt();

	// The GD arrays become the per-index accessors here — "size == COUNT" is "the last index
	// resolves"; the all-non-null sweep stays its own check.
	TestNotNull(TEXT("walls has W_COUNT entries"), FDoomTextures::Wall(W_COUNT - 1));
	bool bWallsOk = true;
	for (int32 i = 0; i < W_COUNT; i++)
	{
		bWallsOk = bWallsOk && FDoomTextures::Wall(i) != nullptr;
	}
	TestTrue(TEXT("every wall texture is non-null"), bWallsOk);

	bool bFloorsOk = true;
	for (int32 i = 0; i < F_COUNT; i++)
	{
		bFloorsOk = bFloorsOk && FDoomTextures::Floor(i) != nullptr;
	}
	TestTrue(TEXT("floors has F_COUNT entries"), bFloorsOk);

	bool bSpritesOk = true;
	for (int32 i = 0; i < S_COUNT; i++)
	{
		bSpritesOk = bSpritesOk && FDoomTextures::Sprite(i) != nullptr;
	}
	TestTrue(TEXT("sprites has S_COUNT entries"), bSpritesOk);

	UTexture2D* Sky = FDoomTextures::Sky();
	TestNotNull(TEXT("sky is non-null"), Sky);
	TestTrue(TEXT("sky is 512x100"), Sky != nullptr && Sky->GetSizeX() == 512 && Sky->GetSizeY() == 100);

	bool bFacesOk = true;
	for (int32 i = 0; i < 8; i++)
	{
		bFacesOk = bFacesOk && FDoomTextures::Face(i) != nullptr;
	}
	TestTrue(TEXT("faces has 8 entries"), bFacesOk);
	bool bWeaponsOk = true;
	for (int32 i = 0; i < 8; i++)
	{
		bWeaponsOk = bWeaponsOk && FDoomTextures::Weapon(i) != nullptr;
	}
	TestTrue(TEXT("weapons has 8 entries"), bWeaponsOk);

	AddInfo(TEXT("[textures] brick pixels"));
	UTexture2D* Brick = FDoomTextures::Wall(W_BRICK_RED);
	TestTrue(TEXT("brick tex is 64x64"), Brick != nullptr && Brick->GetSizeX() == TEX_W && Brick->GetSizeY() == TEX_H);

	bool bNonZero = false;
	if (Brick != nullptr && Brick->GetPlatformData() != nullptr && Brick->GetPlatformData()->Mips.Num() > 0)
	{
		FByteBulkData& Bulk = Brick->GetPlatformData()->Mips[0].BulkData;
		if (const uint8* Data = static_cast<const uint8*>(Bulk.LockReadOnly()))
		{
			for (int32 i = 0; i < TEX_W * TEX_H && !bNonZero; i++)
			{
				bNonZero = Data[i * 4 + 3] > 0; // BGRA — alpha byte
			}
		}
		Bulk.Unlock();
	}
	TestTrue(TEXT("brick texture has non-transparent pixels"), bNonZero);

	FDoomTextures::EnsureBuilt();
	TestTrue(TEXT("ensure_built is idempotent (same texture instance)"), FDoomTextures::Wall(W_BRICK_RED) == Brick);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// _test_maps (48 checks)
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomMapsTest, "ReactiveUI.Doom.Maps", RUI_TEST_FLAGS)
bool FRuiDoomMapsTest::RunTest(const FString&)
{
	const TCHAR* ExpectedNames[] = {
		TEXT("E1M1: Hangar"),  TEXT("E1M2: Toxin Refinery"), TEXT("E1M3: Phobos Lab"),
		TEXT("E1M4: Outpost"), TEXT("E1M5: Watchtower"),	 TEXT("E1M6: Skybridge"),
	};
	for (int32 Level = 1; Level <= LEVEL_COUNT; Level++)
	{
		AddInfo(FString::Printf(TEXT("[maps] level %d"), Level));
		const FLevelStart Ls = BuildLevel(Level);
		TestTrue(FString::Printf(TEXT("level %d builds"), Level), Ls.Map.Width > 0 && Ls.Map.Cells.Num() > 0);
		TestEqual(FString::Printf(TEXT("level %d name"), Level), Ls.Map.Name, FString(ExpectedNames[Level - 1]));
		TestEqual(FString::Printf(TEXT("level %d cell count matches width*height"), Level), Ls.Map.Cells.Num(),
				  Ls.Map.Width * Ls.Map.Height);
		TestTrue(FString::Printf(TEXT("level %d has mobjs"), Level), Ls.Mobjs.Num() > 0);
		TestTrue(FString::Printf(TEXT("level %d has a player start"), Level), Ls.PlayerX > 0.f && Ls.PlayerY > 0.f);

		int32 Bad = 0;
		for (const FCell& Cell : Ls.Map.Cells)
		{
			// WallTexIdx is uint8 here so the GD `< 0` half is structurally impossible.
			if (Cell.Kind == ECellKind::Wall && Cell.WallTexIdx >= W_COUNT)
			{
				Bad++;
			}
		}
		TestEqual(FString::Printf(TEXT("level %d has no out-of-range wall texture indices"), Level), Bad, 0);

		const FMapData SectorMap = FMapData::FromTiles(Ls.Map);
		TestTrue(FString::Printf(TEXT("level %d sector map is valid"), Level), SectorMap.IsValid());
	}

	AddInfo(TEXT("[maps] boss gates + 3D floors + stat tables"));
	TestTrue(TEXT("level 1 is boss-exit-gated"), BuildLevel(1).BossExitGated);
	TestFalse(TEXT("level 2 is not boss-exit-gated"), BuildLevel(2).BossExitGated);

	const FLevelStart L6 = BuildLevel(6);
	bool bFoundExtraFloor = false;
	for (const FCell& Cell : L6.Map.Cells)
	{
		if (Cell.ExtraFloors.Num() > 0)
		{
			bFoundExtraFloor = true;
			break;
		}
	}
	TestTrue(TEXT("level 6 has a 3D floor (extra_floors)"), bFoundExtraFloor);

	TestEqual(TEXT("health_for(BARON)"), HealthFor(EMobjKind::Baron), C::HP_BARON);
	TestEqual(TEXT("radius_for(CACODEMON)"), RadiusFor(EMobjKind::Cacodemon), 0.42f);
	TestEqual(TEXT("height_for(KEY_RED)"), HeightFor(EMobjKind::KeyRed), 0.35f);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// _test_game_logic — RNG determinism + classification (9 checks)
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomDeterminismTest, "ReactiveUI.Doom.Determinism", RUI_TEST_FLAGS)
bool FRuiDoomDeterminismTest::RunTest(const FString&)
{
	AddInfo(TEXT("[determinism] shared LCG"));
	FGameState StA;
	StA.RngSeed = 777;
	FGameState StB;
	StB.RngSeed = 777;
	TestEqual(TEXT("frand is deterministic given the same seed"), StA.Frand(), StB.Frand(), 0.f);
	const float R = StA.Frand();
	TestTrue(TEXT("frand returns a value in [0,1)"), R >= 0.f && R < 1.f);

	AddInfo(TEXT("[determinism] classification helpers"));
	TestTrue(TEXT("is_monster(IMP)"), IsMonster(EMobjKind::Imp));
	TestFalse(TEXT("not is_monster(BARREL)"), IsMonster(EMobjKind::Barrel));
	TestTrue(TEXT("is_boss(BARON)"), IsBoss(EMobjKind::Baron));
	TestTrue(TEXT("is_pickup(KEY_RED)"), IsPickup(EMobjKind::KeyRed));
	TestFalse(TEXT("not is_pickup(BARREL)"), IsPickup(EMobjKind::Barrel));
	TestTrue(TEXT("is_projectile(ROCKET_PROJ)"), IsProjectile(EMobjKind::RocketProj));

	AddInfo(TEXT("[determinism] boss census after new_game"));
	FGameState L1 = NewGame(1, EDifficulty::Normal);
	TestTrue(TEXT("level 1 has a living boss right after new_game"), AnyBossAlive(L1));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// _test_game_logic per-level frame invariants (32) + _test_raycast (6) +
// _test_screen_logic (12) = 50 checks
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomFrameTest, "ReactiveUI.Doom.Frame", RUI_TEST_FLAGS)
bool FRuiDoomFrameTest::RunTest(const FString&)
{
	for (int32 Level = 1; Level <= LEVEL_COUNT; Level++)
	{
		AddInfo(FString::Printf(TEXT("[frame] level %d new_game invariants"), Level));
		FGameState St = NewGame(Level, EDifficulty::Normal);
		TestTrue(FString::Printf(TEXT("level %d new_game succeeds"), Level), St.Map.Cells.Num() > 0);
		TestTrue(FString::Printf(TEXT("level %d player has a valid sector"), Level), St.Player.SectorId >= 0);
		TestEqual(FString::Printf(TEXT("level %d frame has VIEW_W columns"), Level), St.Frame.Columns.Num(), C::VIEW_W);

		int32 NonSky = 0;
		int32 HasFloorBand = 0;
		bool bAllHaveMain = true;
		for (const FColumnInfo& Col : St.Frame.Columns)
		{
			// FWallSeg is a value member — the GD null-check becomes "every CastFrame path
			// wrote a positive distance into Main".
			if (Col.Main.Distance <= 0.f)
			{
				bAllHaveMain = false;
				continue;
			}
			if (!Col.Main.IsSky)
			{
				NonSky++;
			}
			if (Col.FloorBands.Num() > 0)
			{
				HasFloorBand++;
			}
		}
		TestTrue(FString::Printf(TEXT("level %d: every column has a Main seg"), Level), bAllHaveMain);
		// Every level has a floor band in every column (the near sector's own floor, always
		// below eye height at spawn). Wall reachability from spawn is NOT universal —
		// MAX_RAY_HOPS genuinely can't cross some levels' open areas (see the GD comment) —
		// only levels 1 and 6 have walls close enough to spawn to check positively.
		TestEqual(FString::Printf(TEXT("level %d: every column has a floor band"), Level), HasFloorBand, C::VIEW_W);
		if (Level == 1 || Level == 6)
		{
			TestTrue(FString::Printf(TEXT("level %d (walls near spawn): at least one column reaches a wall"), Level),
					 NonSky > 0);
		}
	}

	AddInfo(TEXT("[frame] raycast primitives"));
	{
		FGameState St = NewGame(1, EDifficulty::Normal);
		TArray<Raycast::FWallHit> Hits;
		Raycast::Cast(St.SectorMap, FVector2D(St.Player.X, St.Player.Y), St.Player.SectorId, FVector2D(1.f, 0.f), Hits);
		TestTrue(TEXT("Raycast.cast returns at least one hit for a level-1 ray"), Hits.Num() > 0);
		TestTrue(TEXT("last hit has a positive distance"), Hits.Num() > 0 && Hits.Last().Distance > 0.f);

		float T = 0.f, U = 0.f;
		bool bBackside = false;
		const bool bHit = Raycast::RaySegment(FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(5.f, -1.f),
											  FVector2D(5.f, 1.f), T, U, bBackside);
		TestTrue(TEXT("ray_segment finds a perpendicular crossing segment"), bHit);
		TestTrue(TEXT("ray_segment t is the correct distance"), FMath::Abs(T - 5.f) < 0.001f);
		TestTrue(TEXT("ray_segment u is the midpoint"), FMath::Abs(U - 0.5f) < 0.001f);

		float SegU = 0.f;
		const float DistSq =
			Raycast::DistPointToSegmentSq(FVector2D(0.f, 5.f), FVector2D(-1.f, 0.f), FVector2D(1.f, 0.f), SegU);
		TestTrue(TEXT("dist_point_to_segment_sq: point directly above segment midpoint"),
				 FMath::Abs(DistSq - 25.f) < 0.001f);
	}

	AddInfo(TEXT("[frame] screen geometry builders"));
	{
		FDoomTextures::EnsureBuilt();
		FGameState St = NewGame(1, EDifficulty::Normal);
		const FVector2D Viewport(C::VIEWPORT_W, C::VIEWPORT_H);

		TArray<FSpriteEntry> Sprites;
		BuildSpriteList(St, Viewport, Sprites);
		TestTrue(TEXT("build_sprite_list fills the out list (bounded by the mobj pool)"),
				 Sprites.Num() <= St.MobjCount);
		bool bSortedOk = true;
		for (int32 i = 1; i < Sprites.Num(); i++)
		{
			if (Sprites[i - 1].Distance < Sprites[i].Distance)
			{
				bSortedOk = false;
			}
		}
		TestTrue(TEXT("build_sprite_list is sorted far-to-near (descending distance)"), bSortedOk);

		FDoomBrushPool Brushes;
		FDoomFrameGeometry Geo;
		BuildFrameGeometry(St, Brushes, Viewport, Geo);
		TestTrue(TEXT("frame geometry builds an extra-seg pass"), Geo.ExtraSegs.Num() >= 0);

		int32 RawBands = 0;
		for (const FColumnInfo& Col : St.Frame.Columns)
		{
			RawBands += Col.FloorBands.Num();
		}
		TestTrue(TEXT("frame geometry finds floor bands (level 1 has floor bands every column)"),
				 Geo.FloorBands.Num() > 0);
		// ADAPTED: the Godot port did no horizontal merge (asserted merged == raw); this port
		// DOES merge same-slab runs per column, so the invariant is 0 < merged <= raw.
		TestTrue(TEXT("merged floor bands never exceed the raw band count"), Geo.FloorBands.Num() <= RawBands);
		TestTrue(TEXT("frame geometry builds a ceiling-band pass"), Geo.CeilingBands.Num() >= 0);

		AddInfo(TEXT("[frame] tracers"));
		TArray<FTracerEntry> Tracers;
		BuildTracerList(St, Viewport, Tracers);
		TestEqual(TEXT("build_tracer_list is empty with no tracers fired"), Tracers.Num(), 0);

		// Manually fire a tracer straight ahead, mirroring the real muzzle offset SpawnTracer
		// uses: forward + below eye, so the projected segment has real on-screen extent (a
		// tracer exactly AT the camera origin degenerately projects to a point and is
		// filtered, same as the original).
		const float ViewZ = St.Player.Z + St.Player.ViewHeight;
		const float FwdX = FMath::Cos(St.Player.Angle);
		const float FwdY = FMath::Sin(St.Player.Angle);
		FTracer T;
		T.Ax = St.Player.X + FwdX * C::MUZZLE_FORWARD;
		T.Ay = St.Player.Y + FwdY * C::MUZZLE_FORWARD;
		T.Az = ViewZ - C::MUZZLE_BELOW_EYE;
		T.Bx = St.Player.X + FwdX * 5.f;
		T.By = St.Player.Y + FwdY * 5.f;
		T.Bz = ViewZ;
		T.AgeMs = 0.f;
		T.ColorIdx = 0;
		St.Tracers[0] = T;
		TArray<FTracerEntry> Tracers2;
		BuildTracerList(St, Viewport, Tracers2);
		TestEqual(TEXT("a fresh, in-view tracer projects to exactly one entry"), Tracers2.Num(), 1);
		if (Tracers2.Num() == 1)
		{
			TestTrue(TEXT("projected tracer has a positive screen length"), Tracers2[0].Length > 0.f);
			TestTrue(TEXT("a fresh tracer (age=0) is fully opaque"), Tracers2[0].Alpha > 0.99f);
		}

		TestEqual(TEXT("sprite_scale(BARON)"), SpriteScale(EMobjKind::Baron), 1.6f);
		TestEqual(TEXT("sprite_vertical_anchor(CACODEMON)"), SpriteVerticalAnchor(EMobjKind::Cacodemon), 0.f);
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// _test_player_movement (17 checks) — update_player/hurt are internal here, so movement
// checks drive Tick() with the same dt/inputs, and the hurt checks use the deterministic
// zero-distance barrel explosion (54 damage at Normal) through the public DamageMobj().
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomMovementTest, "ReactiveUI.Doom.Movement", RUI_TEST_FLAGS)
bool FRuiDoomMovementTest::RunTest(const FString&)
{
	using namespace DoomTestUtil;

	AddInfo(TEXT("[movement] mouse-look"));
	FGameState St = NewGame(1, EDifficulty::Normal);
	const float StartX = St.Player.X;
	const float StartAngle = St.Player.Angle;

	FInputCmd LookInput;
	LookInput.YawDelta = 0.1f;
	LookInput.PitchDelta = 10.f;
	Tick(St, 0.016f, LookInput);
	TestEqual(TEXT("yaw_delta adds directly to player.angle"), St.Player.Angle, StartAngle + 0.1f, 1.e-4f);
	TestEqual(TEXT("pitch_delta is negated and scaled by MOUSE_PITCH_SENS"), St.Player.Pitch,
			  -10.f * C::MOUSE_PITCH_SENS, 1.e-4f);

	FInputCmd ClampInput;
	ClampInput.PitchDelta = -100000.f;
	Tick(St, 0.016f, ClampInput);
	TestTrue(TEXT("pitch is clamped to -MAX_PITCH"), St.Player.Pitch >= -C::MAX_PITCH);

	AddInfo(TEXT("[movement] forward along the level-1 corridor"));
	// Level 1 spawn faces straight down -Y at angle=-PI/2, per DoomMaps Level1's PlayerStart.
	FGameState St2 = NewGame(1, EDifficulty::Normal);
	const float Y0 = St2.Player.Y;
	FInputCmd MoveInput;
	MoveInput.Forward = true;
	for (int32 i = 0; i < 10; i++)
	{
		Tick(St2, 1.f / 60.f, MoveInput);
	}
	TestTrue(TEXT("forward movement decreases Y (facing -PI/2 toward the door)"), St2.Player.Y < Y0);
	TestTrue(TEXT("forward movement at angle=-PI/2 doesn't drift X"), FMath::Abs(St2.Player.X - StartX) < 0.05f);

	AddInfo(TEXT("[movement] sustained forward — walls keep the player in bounds"));
	FGameState St3 = NewGame(1, EDifficulty::Normal);
	DespawnMonsters(St3); // GD drove update_player alone; keep the 10-second run undisturbed
	FInputCmd LongMove;
	LongMove.Forward = true;
	for (int32 i = 0; i < 600; i++) // 10 simulated seconds
	{
		Tick(St3, 1.f / 60.f, LongMove);
	}
	TestTrue(TEXT("collision keeps player.x in map bounds after sustained forward movement"),
			 St3.Player.X >= 0.f && St3.Player.X < St3.Map.Width);
	TestTrue(TEXT("collision keeps player.y in map bounds after sustained forward movement"),
			 St3.Player.Y >= 0.f && St3.Player.Y < St3.Map.Height);

	AddInfo(TEXT("[movement] jump"));
	FGameState St4 = NewGame(1, EDifficulty::Normal);
	TestTrue(TEXT("player starts on the ground (z ~ 0)"), St4.Player.Z <= 0.001f);
	FInputCmd JumpInput;
	JumpInput.Jump = true;
	Tick(St4, 1.f / 60.f, JumpInput);
	TestTrue(TEXT("jump lifts the player off the ground on the next tick"), St4.Player.Z > 0.f);

	AddInfo(TEXT("[movement] weapon switch honors ownership"));
	FGameState St5 = NewGame(1, EDifficulty::Normal);
	FInputCmd SwitchUnowned;
	SwitchUnowned.WeaponSwitch = 3; // index 2 = Shotgun, not owned at new_game
	Tick(St5, 1.f / 60.f, SwitchUnowned);
	TestTrue(TEXT("weapon_switch to an unowned slot is ignored"), St5.Player.Weapon == EWeaponType::Pistol);
	FInputCmd SwitchOwned;
	SwitchOwned.WeaponSwitch = 1; // index 0 = Fist, owned by default
	Tick(St5, 1.f / 60.f, SwitchOwned);
	TestTrue(TEXT("weapon_switch to an owned slot switches weapon"), St5.Player.Weapon == EWeaponType::Fist);

	AddInfo(TEXT("[movement] hurt via the deterministic barrel explosion"));
	// Internal Hurt() isn't linkable; a barrel exploded AT the player's feet is the public
	// deterministic path: Splash gives the player (int)(DMG_BARREL * (1 - 0/2.2) * 0.6) = 54
	// damage, then Hurt applies it unscaled at Normal with no armor.
	FGameState St6 = NewGame(1, EDifficulty::Normal);
	DespawnMonsters(St6);
	const int32 Hp0 = St6.Player.Health;
	FMobj BarrelA;
	BarrelA.Kind = EMobjKind::Barrel;
	BarrelA.Health = C::HP_BARREL;
	BarrelA.Radius = RadiusFor(EMobjKind::Barrel);
	BarrelA.X = St6.Player.X;
	BarrelA.Y = St6.Player.Y;
	const int32 BarrelAIdx = SpawnTracked(St6, BarrelA);
	DamageMobj(St6, BarrelAIdx, 1000, 0);
	TestEqual(TEXT("hurt (Normal, no armor) reduces health by the exact splash amount (54)"), St6.Player.Health,
			  Hp0 - 54);
	TestTrue(TEXT("hurt sets hurt_flash"), St6.Player.HurtFlash > 0.f);
	St6.Player.Health = 40; // below the next 54-point blast — lethal, like the GD hurt(10000)
	FMobj BarrelB;
	BarrelB.Kind = EMobjKind::Barrel;
	BarrelB.Health = C::HP_BARREL;
	BarrelB.Radius = RadiusFor(EMobjKind::Barrel);
	BarrelB.X = St6.Player.X;
	BarrelB.Y = St6.Player.Y;
	const int32 BarrelBIdx = SpawnTracked(St6, BarrelB);
	DamageMobj(St6, BarrelBIdx, 1000, 0);
	TestTrue(TEXT("lethal damage zeroes health, kills the player, and sets game_over"),
			 St6.Player.Health == 0 && !St6.Player.Alive && St6.GameOver);

	AddInfo(TEXT("[movement] tick() advances tic/time_accum and re-casts"));
	FGameState St7 = NewGame(1, EDifficulty::Normal);
	const FInputCmd IdleInput;
	const int32 Tic0 = St7.Tic;
	for (int32 i = 0; i < 5; i++)
	{
		Tick(St7, 1.f / 60.f, IdleInput);
	}
	TestEqual(TEXT("tick() advances the tic counter once per call"), St7.Tic, Tic0 + 5);
	TestTrue(TEXT("tick() accumulates time_accum"), St7.TimeAccum > 0.f);
	TestEqual(TEXT("tick() keeps re-running cast_frame (VIEW_W columns present)"), St7.Frame.Columns.Num(), C::VIEW_W);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// _test_doors (11 checks) — try_use/update_doors are internal, so the door FSM is driven
// through Tick() (UpdatePlayer -> TryUse on Input.Use; UpdateDoors runs every tick).
// Level 1 has a plain door at tile (24,37) and a locked DOOR_RED at (9,27).
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomDoorsTest, "ReactiveUI.Doom.Doors", RUI_TEST_FLAGS)
bool FRuiDoomDoorsTest::RunTest(const FString&)
{
	using namespace DoomTestUtil;

	AddInfo(TEXT("[doors] plain door open cycle"));
	FGameState St = NewGame(1, EDifficulty::Normal);
	DespawnMonsters(St); // GD drove try_use/update_doors alone; keep the 120-tick run undisturbed
	const int32 W = St.Map.Width;
	FCell& Door = St.Map.Cells[37 * W + 24];
	TestTrue(TEXT("level1 has a plain door at (24,37)"), Door.Kind == ECellKind::Door);
	TestTrue(TEXT("door starts closed + idle"), Door.DoorState == 0 && Door.DoorTimer == 0);
	TestTrue(TEXT("closed door blocks movement"), St.Map.BlocksMovement(24, 37));

	// Face the door from just south of it and press Use.
	St.Player.X = 24.5f;
	St.Player.Y = 38.5f;
	St.Player.Angle = -UE_PI / 2.f; // facing -Y (toward the door tile)
	FInputCmd UseInput;
	UseInput.Use = true;
	Tick(St, 1.f / 60.f, UseInput);
	TestEqual(TEXT("try_use starts the door opening (timer=1)"), (int32)Door.DoorTimer, 1);

	// Animate to fully open.
	bool bOpened = false;
	const FInputCmd Idle;
	for (int32 i = 0; i < 120; i++)
	{
		Tick(St, 1.f / 60.f, Idle);
		if (Door.DoorState >= 255)
		{
			bOpened = true;
			break;
		}
	}
	TestTrue(TEXT("door fully opens then enters wait state (timer=2)"), bOpened && Door.DoorTimer == 2);
	TestFalse(TEXT("open door no longer blocks movement"), St.Map.BlocksMovement(24, 37));
	const int32 Sid = St.SectorMap.CellToSector[37 * W + 24];
	TestTrue(TEXT("open door mirrors onto sector ceiling_z"), Sid >= 0 && St.SectorMap.Sectors[Sid].CeilingZ > 1.f);

	AddInfo(TEXT("[doors] locked red door + keycard gate"));
	FCell& RedDoor = St.Map.Cells[27 * W + 9];
	TestTrue(TEXT("level1 has a red door at (9,27)"), RedDoor.Kind == ECellKind::DoorRed);
	// This door sits in a vertical wall (passable E<->W), so approach from the east.
	St.Player.Keys = EKeyCard::None;
	St.Player.X = 10.5f;
	St.Player.Y = 27.5f;
	St.Player.Angle = UE_PI; // facing -X (toward the door tile)
	St.Player.MessageTimer = 0.f;
	Tick(St, 1.f / 60.f, UseInput);
	TestEqual(TEXT("locked red door stays shut without the key"), (int32)RedDoor.DoorTimer, 0);
	TestTrue(TEXT("locked door posts a 'need keycard' message"), St.Player.MessageTimer > 0.f);

	// With the key, it opens.
	St.Player.Keys = EKeyCard::Red;
	Tick(St, 1.f / 60.f, UseInput);
	TestEqual(TEXT("red door opens once the player holds the red key"), (int32)RedDoor.DoorTimer, 1);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// _test_combat — damage_mobj / splash / fire_weapon / monster wake (15 checks)
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomCombatTest, "ReactiveUI.Doom.Combat", RUI_TEST_FLAGS)
bool FRuiDoomCombatTest::RunTest(const FString&)
{
	using namespace DoomTestUtil;

	AddInfo(TEXT("[combat] damage_mobj — pain, death, kill count"));
	FGameState St = NewGame(1, EDifficulty::Normal);
	FMobj Imp;
	Imp.Kind = EMobjKind::Imp;
	Imp.State = EAIState::Hunting;
	Imp.Health = C::HP_IMP;
	Imp.X = St.Player.X + 3.f;
	Imp.Y = St.Player.Y;
	const int32 ImpIdx = SpawnTracked(St, Imp);
	TestTrue(TEXT("spawned imp is in the mobj pool"), ImpIdx > 0);
	const int32 KillsBefore = St.KillCount;
	DamageMobj(St, ImpIdx, 5, 0);
	TestEqual(TEXT("non-lethal damage lowers monster health"), St.Mobjs[ImpIdx].Health, C::HP_IMP - 5);
	TestTrue(TEXT("hurt monster stays active (pain/hunting)"),
			 St.Mobjs[ImpIdx].State == EAIState::Pain || St.Mobjs[ImpIdx].State == EAIState::Hunting);
	DamageMobj(St, ImpIdx, 1000, 0);
	TestTrue(TEXT("lethal damage drops health to <= 0"), St.Mobjs[ImpIdx].Health <= 0);
	TestTrue(TEXT("lethal damage sets DYING"), St.Mobjs[ImpIdx].State == EAIState::Dying);
	TestEqual(TEXT("a kill increments kill_count"), St.KillCount, KillsBefore + 1);

	AddInfo(TEXT("[combat] splash via barrel explosion"));
	// Internal Splash() isn't linkable; a barrel exploded at the player's position is the
	// public path (radius 2.2, max 90): the imp 1.0 away takes 49, the player takes 54.
	FGameState St2 = NewGame(1, EDifficulty::Normal);
	DespawnMonsters(St2);
	FMobj Imp2;
	Imp2.Kind = EMobjKind::Imp;
	Imp2.State = EAIState::Hunting;
	Imp2.Health = C::HP_IMP;
	Imp2.X = St2.Player.X + 1.f;
	Imp2.Y = St2.Player.Y;
	const int32 Imp2Idx = SpawnTracked(St2, Imp2);
	St2.Player.Health = 100;
	FMobj Barrel;
	Barrel.Kind = EMobjKind::Barrel;
	Barrel.Health = C::HP_BARREL;
	Barrel.Radius = RadiusFor(EMobjKind::Barrel);
	Barrel.X = St2.Player.X;
	Barrel.Y = St2.Player.Y;
	const int32 BarrelIdx = SpawnTracked(St2, Barrel);
	DamageMobj(St2, BarrelIdx, 1000, 0);
	TestTrue(TEXT("barrel splash damages a nearby monster"), St2.Mobjs[Imp2Idx].Health < C::HP_IMP);
	TestTrue(TEXT("barrel splash also hurts the nearby player"), St2.Player.Health < 100);

	AddInfo(TEXT("[combat] fire_weapon — pistol / rocket / empty fallback via Tick(Attack)"));
	FGameState St3 = NewGame(1, EDifficulty::Normal);
	DespawnMonsters(St3); // keep the exact ammo/cooldown numbers monster-free
	FInputCmd AttackInput;
	AttackInput.Attack = true;
	const int32 BulletsBefore = St3.Player.Ammo[0];
	Tick(St3, 1.f / 60.f, AttackInput);
	TestEqual(TEXT("pistol fire consumes one bullet"), St3.Player.Ammo[0], BulletsBefore - 1);
	TestTrue(TEXT("pistol fire spawns a hitscan tracer"), St3.TracerCount > 0);
	TestTrue(TEXT("firing sets a shoot cooldown"), St3.Player.ShootCooldown > 0.f);

	St3.Player.Weapon = EWeaponType::RocketLauncher;
	St3.Player.Ammo[2] = 5;
	St3.Player.ShootCooldown = 0.f;
	Tick(St3, 1.f / 60.f, AttackInput);
	TestEqual(TEXT("rocket fire consumes one rocket"), St3.Player.Ammo[2], 4);
	bool bHasRocket = false;
	for (int32 i = 1; i <= St3.MobjCount; i++)
	{
		const FMobj& M = St3.Mobjs[i];
		if (M.Id != 0 && M.Kind == EMobjKind::RocketProj)
		{
			bHasRocket = true;
		}
	}
	TestTrue(TEXT("rocket fire spawns a ROCKET_PROJ projectile"), bHasRocket);

	St3.Player.Weapon = EWeaponType::PlasmaRifle;
	St3.Player.Ammo[3] = 0;
	St3.Player.ShootCooldown = 0.f;
	Tick(St3, 1.f / 60.f, AttackInput);
	TestTrue(TEXT("empty plasma falls back to another weapon"), St3.Player.Weapon != EWeaponType::PlasmaRifle);

	AddInfo(TEXT("[combat] idle imp in sight wakes via the tick loop"));
	FGameState St4 = NewGame(1, EDifficulty::Normal);
	FMobj Imp3;
	Imp3.Kind = EMobjKind::Imp;
	Imp3.State = EAIState::Idle;
	Imp3.Health = C::HP_IMP;
	Imp3.X = St4.Player.X + 1.5f;
	Imp3.Y = St4.Player.Y;
	const int32 Imp3Idx = SpawnTracked(St4, Imp3); // AddMobj resolves its sector like the GD set did
	const FInputCmd Cmd;
	Tick(St4, 0.016f, Cmd);
	TestTrue(TEXT("an idle imp in sight range wakes when the tick loop runs"),
			 St4.Mobjs[Imp3Idx].State == EAIState::Hunting || St4.Mobjs[Imp3Idx].State == EAIState::Pain);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// _test_combat pickups block (7 checks) — try_give_pickup/update_pickup are internal; a
// pickup mobj spawned on the player's tile and one Tick() drives the identical path
// (UpdateMobj -> UpdatePickup -> TryGivePickup).
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomPickupsTest, "ReactiveUI.Doom.Pickups", RUI_TEST_FLAGS)
bool FRuiDoomPickupsTest::RunTest(const FString&)
{
	using namespace DoomTestUtil;

	FGameState St = NewGame(1, EDifficulty::Normal);
	DespawnMonsters(St); // keep the exact health numbers monster-free
	const FInputCmd Idle;

	auto SpawnPickup = [&St](EMobjKind Kind) -> int32
	{
		FMobj M;
		M.Kind = Kind;
		M.X = St.Player.X;
		M.Y = St.Player.Y;
		return SpawnTracked(St, M);
	};

	AddInfo(TEXT("[pickups] stimpack when hurt"));
	St.Player.Health = 50;
	const int32 Med1 = SpawnPickup(EMobjKind::PickupHealth);
	Tick(St, 1.f / 60.f, Idle);
	TestTrue(TEXT("stimpack accepted when hurt"), St.Mobjs[Med1].Collected);
	TestEqual(TEXT("stimpack heals +25"), St.Player.Health, 75);

	AddInfo(TEXT("[pickups] stimpack refused at full health"));
	St.Player.Health = 100;
	const int32 Med2 = SpawnPickup(EMobjKind::PickupHealth);
	Tick(St, 1.f / 60.f, Idle);
	TestFalse(TEXT("stimpack refused at full health"), St.Mobjs[Med2].Collected);
	St.Mobjs[Med2].Id = 0; // despawn the refused pickup so later ticks can't collect it

	AddInfo(TEXT("[pickups] keycard + weapon"));
	SpawnPickup(EMobjKind::KeyBlue);
	Tick(St, 1.f / 60.f, Idle);
	TestTrue(TEXT("blue keycard sets the BLUE flag"), EnumHasAnyFlags(St.Player.Keys, EKeyCard::Blue));
	SpawnPickup(EMobjKind::PickupShotgun);
	Tick(St, 1.f / 60.f, Idle);
	TestTrue(TEXT("shotgun pickup grants the shotgun"), St.Player.OwnedWeapons[(int32)EWeaponType::Shotgun]);

	AddInfo(TEXT("[pickups] auto-collect on the player's tile"));
	St.Player.Health = 40;
	const int32 Med3 = SpawnPickup(EMobjKind::PickupHealth);
	Tick(St, 1.f / 60.f, Idle);
	TestTrue(TEXT("walking over a stimpack collects it"), St.Mobjs[Med3].Collected);
	TestEqual(TEXT("collected stimpack heals the player"), St.Player.Health, 65);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Level exit + boss gating (5 checks) — replaces the 4 unported reconciler-mount checks
// (_test_integration_tick/_test_menu_and_switch) with pure-logic coverage the GDScript
// suite only touched via any_boss_alive.
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomExitTest, "ReactiveUI.Doom.Exit", RUI_TEST_FLAGS)
bool FRuiDoomExitTest::RunTest(const FString&)
{
	using namespace DoomTestUtil;
	const FInputCmd Idle;

	AddInfo(TEXT("[exit] ungated exit (level 2) fires victory"));
	FGameState St2 = NewGame(2, EDifficulty::Normal);
	DespawnMonsters(St2);
	const int32 ExitIdx2 = FindCellOfKind(St2.Map, ECellKind::Exit);
	if (!TestTrue(TEXT("level 2 has an exit cell"), ExitIdx2 >= 0))
	{
		return false;
	}
	St2.Player.X = (ExitIdx2 % St2.Map.Width) + 0.5f;
	St2.Player.Y = (ExitIdx2 / St2.Map.Width) + 0.5f;
	Tick(St2, 1.f / 60.f, Idle);
	TestTrue(TEXT("standing on an ungated exit cell wins the level"), St2.Victory);
	const int32 TicAtVictory = St2.Tic;
	Tick(St2, 1.f / 60.f, Idle);
	TestEqual(TEXT("victory freezes the tic counter"), St2.Tic, TicAtVictory);

	AddInfo(TEXT("[exit] boss-gated exit (level 1) holds while a boss lives"));
	FGameState St1 = NewGame(1, EDifficulty::Normal);
	DespawnMonsters(St1, /*bKeepBosses*/ true);
	const int32 ExitIdx1 = FindCellOfKind(St1.Map, ECellKind::Exit);
	if (!TestTrue(TEXT("level 1 has an exit cell"), ExitIdx1 >= 0))
	{
		return false;
	}
	St1.Player.X = (ExitIdx1 % St1.Map.Width) + 0.5f;
	St1.Player.Y = (ExitIdx1 / St1.Map.Width) + 0.5f;
	// The gate message posts on Tic % 60 == 0 — run past tic 60.
	for (int32 i = 0; i < 66 && !St1.Victory; i++)
	{
		Tick(St1, 1.f / 60.f, Idle);
	}
	TestFalse(TEXT("boss-gated exit does NOT win while a boss lives"), St1.Victory);
	TestTrue(TEXT("gated exit posts the 'Kill the boss first.' message"),
			 St1.Player.MessageText == TEXT("Kill the boss first.") && St1.Player.MessageTimer > 0.f);

	AddInfo(TEXT("[exit] gate opens once every boss is dead"));
	for (int32 i = 1; i <= St1.MobjCount; i++)
	{
		if (St1.Mobjs[i].Id != 0 && IsBoss(St1.Mobjs[i].Kind))
		{
			St1.Mobjs[i].State = EAIState::Dead; // AnyBossAlive keys off State
		}
	}
	Tick(St1, 1.f / 60.f, Idle);
	TestTrue(TEXT("exit fires victory once every boss is dead"), St1.Victory);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
