// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include <limits>

// ═════════════════════════════════════════════════════════════════════════════════════════════
// Doom demo — TYPES module (plans/DOOM_DEMO_PLAN.md Phase 1).
//
// Faithful port of the Unity ReactiveUIToolKit `DoomGame` sample's `DoomTypes.uitkx`
// (the original, C# — value structs map 1:1 to these plain C++ structs), inheriting the
// documented fixes from the Godot port's `doom_types.gd`:
//   - the single deterministic LCG `Frand()` owned by the game state (same multiplier/
//     increment as both siblings so replays/tests match bit-for-bit),
//   - the GO-03 per-frame linear-allocator pools for WallSeg/FloorBand/CeilingBand on
//     FFrameData (ResetPools() rewinds the cursors; Take* hands out a default-reset record),
//   - MAX_RAY_HOPS = 64 (the Godot port's see-through-far-walls fix; the C# original's 16
//     stopped rays before distant walls at one-sector-per-tile density),
//   - the FloorBand/CeilingBand `Distance` field (joins bands into the one depth-sorted
//     paint list) and the -1 "unknown sector" defaults.
//
// No UObject/reflection — demo-module data only. Every tunable keeps its original name and
// exact value. Where C# used a null array for "none", the TArray is simply empty.
// ═════════════════════════════════════════════════════════════════════════════════════════════

// AudioMixerCore's AudioDefines.h (reachable via the Engine shared PCH) leaks a bare
// `#define MAX_PITCH 2.0f` that would macro-expand our identically named tunable — and every
// `C::MAX_PITCH` call site after it. The family naming contract keeps the original tunable
// names, so drop the engine macro here for good: the demo module never touches audio-mixer
// pitch clamps. (push/pop_macro can't work — call sites appear AFTER this header.)
#ifdef MAX_PITCH
#undef MAX_PITCH
#endif

namespace RuiDoom
{
	// Optional Phase-7 BSP renderer hook (Godot-only upgrade, ported later if at all).
	struct FDoomBsp;

	// ── Enums ─────────────────────────────────────────────────────────────────────────────────

	enum class EGameScreen : uint8
	{
		MainMenu,
		Game
	};

	enum class EDifficulty : uint8
	{
		Easy = 0,
		Normal = 1,
		Hard = 2
	};

	enum class ECellKind : uint8
	{
		Empty = 0,
		Wall = 1,
		Door = 2,
		DoorBlue = 3,
		DoorYellow = 4,
		DoorRed = 5,
		Exit = 6,
		Liquid = 7,
	};

	enum class EWeaponType : uint8
	{
		Fist = 0,
		Pistol = 1,
		Shotgun = 2,
		Chaingun = 3,
		RocketLauncher = 4,
		PlasmaRifle = 5,
		BFG9000 = 6,
	};

	enum class EAmmoType : uint8
	{
		Bullets = 0,
		Shells = 1,
		Rockets = 2,
		Cells = 3,
		None = 255,
	};

	// [Flags] in the original — combine with bitwise OR (EKeyCard::Blue | EKeyCard::Red).
	enum class EKeyCard : uint8
	{
		None = 0,
		Blue = 1,
		Yellow = 2,
		Red = 4,
	};
	ENUM_CLASS_FLAGS(EKeyCard)

	enum class EMobjKind : uint8
	{
		Player,
		Imp,
		Demon,
		Baron,
		Cacodemon,
		LostSoul,
		Zombieman,
		Shotgunner,
		ImpFireball,
		BaronBall,
		CacoBall,
		RocketProj,
		PlasmaProj,
		BfgProj,
		Explosion,
		PickupHealth,
		PickupArmor,
		PickupArmorBlue,
		PickupBullets,
		PickupShells,
		PickupRockets,
		PickupCells,
		PickupShotgun,
		PickupChaingun,
		PickupRocketLauncher,
		PickupPlasma,
		PickupBFG,
		KeyBlue,
		KeyYellow,
		KeyRed,
		Barrel,
		Light,
		Corpse,
	};

	enum class EAIState : uint8
	{
		Idle,
		Hunting,
		Attacking,
		Pain,
		Dying,
		Dead,
	};

	// [Flags] in the original.
	enum class ELinedefFlags : uint8
	{
		None = 0,
		Impassable = 1, // even two-sided lines can be flagged solid (rails)
		BlockMonsters = 2,
		TwoSided = 4,
		UpperUnpegged = 8,
		LowerUnpegged = 16,
		DontDraw = 32, // automap hint
	};
	ENUM_CLASS_FLAGS(ELinedefFlags)

	enum class ELineSpecial : uint8
	{
		None = 0,
		UseDoorOpen = 1, // E-key opens BackSector ceiling
		UseDoorBlue = 2,
		UseDoorYellow = 3,
		UseDoorRed = 4,
		UseLift = 5,	   // E-key triggers lift on BackSector floor
		CrossExit = 6,	   // touching this linedef wins the level
		CrossTeleport = 7, // touching teleports player to Tag-marked spot
		UseSwitchExit = 8,
	};

	enum class ESectorSpecial : uint8
	{
		None = 0,
		LightFlicker = 1,
		LightBlink = 2,
		LightGlow = 3,
		DamageNukage5 = 4, // -5 hp/2s while standing
		DamageLava10 = 5,
		Secret = 6,
		DoorClose30 = 7, // door auto-close after 30s
	};

	enum class ESegKind : uint8
	{
		Mid = 0,	   // single solid wall (one-sided line)
		Upper = 1,	   // step-down ceiling neighbor
		Lower = 2,	   // step-up floor neighbor
		ExtraTop = 3,  // top plane of a 3D floor
		ExtraBot = 4,  // bottom plane of a 3D floor
		ExtraSide = 5, // side wall of a 3D floor
	};

	// ── Tunables (the original's `static class C`). Values copied exactly. ────────────────────

	namespace C
	{
		inline constexpr int32 VIEW_W = 160;
		inline constexpr int32 VIEW_H = 200;
		inline constexpr int32 VIEWPORT_W = 800;
		inline constexpr int32 VIEWPORT_H = 500;
		inline constexpr int32 HUD_HEIGHT = 90;
		inline constexpr float STRIP_W = 5.f; // VIEWPORT_W / VIEW_W

		inline constexpr float FOV = 1.0472f;
		inline constexpr float HALF_FOV = FOV / 2.f;

		inline constexpr float MOVE_SPEED = 4.0f;
		inline constexpr float RUN_MULT = 1.6f;
		inline constexpr float STRAFE_SPEED = 3.5f;
		inline constexpr float TURN_SPEED = 2.6f;
		inline constexpr float MAX_PITCH = 200.f;
		inline constexpr float MOUSE_YAW_SENS = 0.008f;
		inline constexpr float MOUSE_PITCH_SENS = 0.5f;
		inline constexpr float PLAYER_RADIUS = 0.28f;

		inline constexpr float COOLDOWN_FIST = 0.35f;
		inline constexpr float COOLDOWN_PISTOL = 0.32f;
		inline constexpr float COOLDOWN_SHOTGUN = 0.85f;
		inline constexpr float COOLDOWN_CHAIN = 0.10f;
		inline constexpr float COOLDOWN_ROCKET = 1.0f;
		inline constexpr float COOLDOWN_PLASMA = 0.10f;
		inline constexpr float COOLDOWN_BFG = 1.5f;

		inline constexpr int32 DMG_FIST = 8;
		inline constexpr int32 DMG_PISTOL = 14;
		inline constexpr int32 DMG_PELLET = 9;
		inline constexpr int32 DMG_CHAIN = 11;
		inline constexpr int32 DMG_ROCKET = 100;
		inline constexpr int32 DMG_PLASMA = 18;
		inline constexpr int32 DMG_BFG = 350;
		inline constexpr int32 DMG_BARREL = 90;

		inline constexpr int32 MAX_BULLETS = 200;
		inline constexpr int32 MAX_SHELLS = 50;
		inline constexpr int32 MAX_ROCKETS = 50;
		inline constexpr int32 MAX_CELLS = 300;

		inline constexpr int32 START_HEALTH = 100;

		inline constexpr int32 HP_IMP = 60;
		inline constexpr int32 HP_DEMON = 150;
		inline constexpr int32 HP_BARON = 200;
		inline constexpr int32 HP_CACO = 120;
		inline constexpr int32 HP_LOST = 100;
		inline constexpr int32 HP_ZOMBIE = 20;
		inline constexpr int32 HP_SHOTG = 30;
		inline constexpr int32 HP_BARREL = 20;

		inline constexpr int32 DMG_IMP_MELEE = 6;
		inline constexpr int32 DMG_IMP_BALL = 8;
		inline constexpr int32 DMG_DEMON_BITE = 10;
		inline constexpr int32 DMG_BARON_CLAW = 14;
		inline constexpr int32 DMG_BARON_BALL = 24;
		inline constexpr int32 DMG_CACO_BALL = 12;
		inline constexpr int32 DMG_LOST_RAM = 7;
		inline constexpr int32 DMG_ZOMBIE = 4;
		inline constexpr int32 DMG_SHOTG = 5;

		inline constexpr float SPEED_IMP = 1.7f;
		inline constexpr float SPEED_DEMON = 2.6f;
		inline constexpr float SPEED_BARON = 1.5f;
		inline constexpr float SPEED_CACO = 1.4f;
		inline constexpr float SPEED_LOST = 3.5f;
		inline constexpr float SPEED_ZOMBIE = 1.4f;
		inline constexpr float SPEED_SHOTG = 1.5f;

		inline constexpr float SPEED_IMPBALL = 9.f;
		inline constexpr float SPEED_BARONBALL = 11.f;
		inline constexpr float SPEED_CACOBALL = 9.f;
		inline constexpr float SPEED_ROCKET = 14.f;
		inline constexpr float SPEED_PLASMA = 22.f;
		inline constexpr float SPEED_BFG = 20.f;

		inline constexpr int32 SCORE_IMP = 100;
		inline constexpr int32 SCORE_DEMON = 200;
		inline constexpr int32 SCORE_BARON = 1500;
		inline constexpr int32 SCORE_CACO = 600;
		inline constexpr int32 SCORE_LOST = 80;
		inline constexpr int32 SCORE_ZOMBIE = 60;
		inline constexpr int32 SCORE_SHOTG = 90;
		inline constexpr int32 SCORE_BARREL = 10;

		inline constexpr int32 MAX_MOBJS = 256;
		inline constexpr float SIGHT_RANGE = 18.f;
		inline constexpr float MELEE_RANGE = 1.4f;

		inline constexpr float MAX_RAY = 32.f;

		// ── Phase 1+ Sector engine constants ──
		// Portal traversal cap. GODOT FIX (inherited): the C# original's 16 was a bug at
		// one-sector-per-tile density — a 32-unit diagonal ray crosses ~45 cell portals, so at
		// 16 the ray stopped BEFORE reaching walls >16 cells away; those far walls never
		// rendered and monsters/geometry behind them leaked through ("see-through" until you
		// walked close). Must cover MAX_RAY at the map's sector density.
		inline constexpr int32 MAX_RAY_HOPS = 64;
		inline constexpr float STEP_HEIGHT = 0.4f;		  // max floor step the player can walk up
		inline constexpr float PLAYER_HEIGHT = 0.9f;	  // feet→head
		inline constexpr float PLAYER_VIEW_HEIGHT = 0.6f; // feet→eye
		inline constexpr float GRAVITY = 9.f;			  // units/s² for jump/fall
		inline constexpr float JUMP_VELOCITY = 7.0f;	  // initial Z velocity on Jump
		inline constexpr float CROUCH_HEIGHT = 0.45f;
		inline constexpr float DOOR_OPEN_SPEED = 2.0f;		// units/sec ceiling lerp
		inline constexpr float DOOR_AUTO_CLOSE_DELAY = 4.f; // seconds
		inline constexpr float LIFT_SPEED = 1.5f;

		// Phase 8: hitscan tracer.
		inline constexpr int32 MAX_TRACERS = 32;
		inline constexpr float TRACER_LIFE_MS = 90.f;
		inline constexpr float TRACER_THICKNESS_PX = 2.f;
		inline constexpr float MUZZLE_FORWARD = 0.35f;	 // offset from player along view
		inline constexpr float MUZZLE_BELOW_EYE = 0.12f; // drop tracer below crosshair
	} // namespace C

	// ── World data ─────────────────────────────────────────────────────────────────────────────

	// 3D-floor (Phase 7): an extra walkable plane inside a sector. Multiple ExtraFloors per
	// sector enable basements, balconies, mezzanines.
	struct FExtraFloor
	{
		float BottomZ = 0.f; // bottom of the slab (player walks ON TopZ)
		float TopZ = 0.f;	 // top of the slab
		uint8 SideTex = 0;	 // texture on the slab's vertical sides
		uint8 TopTex = 0;	 // texture on top (what the player walks on)
		uint8 BottomTex = 0; // texture on the underside (ceiling-of-below)
		uint8 Light = 0;	 // light level for this slab's surfaces
		bool Solid = false;	 // false = swimmable / shootable through
	};

	struct FCell
	{
		ECellKind Kind = ECellKind::Empty;
		uint8 WallTexIdx = 0;
		uint8 FloorTexIdx = 0;
		uint8 CeilingTexIdx = 0;
		uint8 LightLevel = 0;
		uint8 DoorState = 0; // 0=closed, 255=open
		uint8 DoorTimer = 0;
		uint8 Tag = 0;
		// Phase 3: per-cell floor/ceiling height (world units). When 0 they default to 0 and 1.5
		// respectively in FromTiles (flat rooms). Use to make stepped rooms, raised platforms,
		// low ceilings.
		float FloorZ = 0.f;
		float CeilingZ = 0.f;
		// Phase 4: sector special (lighting/damage). None by default.
		ESectorSpecial Special = ESectorSpecial::None;
		// Phase 7: open-air ceiling. When true, the ceiling renders as sky and the player is not
		// bonked; the cell behaves as if CeilingZ = +infinity for vertical movement.
		bool IsSky = false;
		// Phase 9: stacked 3D-floor slabs occupying this cell. Each slab is a walkable platform
		// with a BottomZ underside and TopZ walkable surface. Empty = no extra floors (C# null).
		// Sorted by BottomZ ascending.
		TArray<FExtraFloor> ExtraFloors;
	};

	struct RUIDEMO_API FMapDef
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<FCell> Cells;
		FString Name;

		// Out-of-bounds returns the shared boundary cell (Kind=Wall, WallTexIdx=0, LightLevel=200
		// — the exact cell the C# original constructed per call; returned by const& here).
		const FCell& AtSafe(int32 X, int32 Y) const;

		bool BlocksMovement(int32 X, int32 Y) const;

		// Phase 7: Z-aware blocking. The cell blocks the actor if either its floor is more than
		// StepHeight above the actor's feet, or its ceiling is below the actor's head. Walls
		// remain infinite blockers. Sky cells ignore the ceiling check entirely.
		bool BlocksMovementZ(int32 X, int32 Y, float FootZ, float HeadZ, float StepHeight) const;

		// Phase 7: floor height the actor would stand on at this cell.
		// Phase 9: also considers ExtraFloor tops at-or-below (FootZ + StepHeight), picking the
		// highest reachable surface.
		float FloorAt(int32 X, int32 Y, float FootZ = 0.f, float StepHeight = 1e6f) const;

		bool BlocksRays(int32 X, int32 Y) const;
	};

	// ── Actors ─────────────────────────────────────────────────────────────────────────────────

	struct FPlayerState
	{
		float X = 0.f;
		float Y = 0.f;
		float Angle = 0.f;
		float Pitch = 0.f; // pixels of Y-shear
		int32 Health = 0;
		int32 Armor = 0;
		uint8 ArmorClass = 0; // 0=none 1=green 2=blue
		EWeaponType Weapon = EWeaponType::Fist;
		TArray<int32> Ammo;		   // indexed by EAmmoType (Bullets..Cells)
		TArray<bool> OwnedWeapons; // indexed by EWeaponType
		EKeyCard Keys = EKeyCard::None;
		float ShootCooldown = 0.f;
		float MuzzleFlash = 0.f;
		float BobT = 0.f;
		bool Alive = false;
		int32 FaceState = 0; // 0=god,1..5=hp buckets,6=hurt,7=dead
		float FaceTimer = 0.f;
		uint8 LastDamageDir = 0; // 0=front 1=right 2=behind 3=left
		float HurtFlash = 0.f;
		float PickupFlash = 0.f;
		float MessageTimer = 0.f;
		FString MessageText;
		// ── Phase 1 (Sector-engine) additions. Ignored by pre-sector renderers. ──
		float Z = 0.f;			// feet height (0 = ground)
		float ZVel = 0.f;		// for jump/gravity (Phase 7)
		float ViewHeight = 0.f; // eye offset from feet (NewGame sets 0.6)
		int32 SectorId = -1;	// current sector (-1 = unknown / fallback to grid)
		// Phase 7: pixel offset added to the horizon line (sky/floor/wall) so jump/crouch
		// animation stays in sync across all renderers. Computed by CastFrame from Z + ViewHeight.
		float ViewShiftPx = 0.f;
		bool JumpHeldPrev = false; // for edge-detection on Jump key
	};

	// One slot of GameState's fixed MAX_MOBJS pool. Id == 0 is the free-slot sentinel: a
	// default-constructed FMobj IS a free slot, and killing/despawning writes Id = 0.
	struct FMobj
	{
		int32 Id = 0;
		EMobjKind Kind = EMobjKind::Player;
		EAIState State = EAIState::Idle;
		float X = 0.f;
		float Y = 0.f;
		float MomX = 0.f;
		float MomY = 0.f;
		float Angle = 0.f;
		int32 Health = 0;
		float StateTimer = 0.f;
		float AttackCooldown = 0.f;
		int32 OwnerId = 0;
		int32 Damage = 0;
		float Radius = 0.f;
		bool Collected = false;
		uint8 AnimFrame = 0;
		// ── Phase 1 (Sector-engine) additions. Ignored by pre-sector renderers. ──
		float Z = 0.f;		 // feet height in world units
		float ZVel = 0.f;	 // for projectiles + future jump/gravity
		float Height = 0.f;	 // from feet to head
		int32 SectorId = -1; // current sector (-1 = unknown / fallback to grid)
	};

	// ── Per-frame render records (pooled — see FFrameData) ────────────────────────────────────

	struct FWallSeg
	{
		float TopPx = 0.f;
		float BotPx = 0.f;
		float Distance = 0.f;
		uint8 WallTexIdx = 0;
		float TexU = 0.f;
		uint8 LightLevel = 0;
		bool HitVertical = false;
		bool IsSky = false;
		bool IsRiser = false; // true if this extra represents the front-face of a step-up
		// Plan C: when TopPx was clipped UP by an occlusion window, this stores the screen-pixel
		// delta (negative) that the renderer must apply to the texture's Y offset so texel row 0
		// stays anchored to the UNCLIPPED top (= the wall's true world ceiling). Without this,
		// adjacent columns clipped by different amounts produce diagonal/staircase texture rows
		// on flat walls.
		float TexOffsetPx = 0.f;

		// GO-03 pooling: restore every field to what a fresh FWallSeg has, so a recycled record
		// behaves identically to a new one (some cast sites don't set IsRiser/TexOffsetPx).
		void Reset() { *this = FWallSeg(); }
	};

	struct FFloorBand
	{
		float TopPx = 0.f;	// far edge of this floor slab on screen
		float BotPx = 0.f;	// near edge (toward bottom of screen)
		float FloorZ = 0.f; // world Z (for color)
		uint8 Light = 0;	// sector light, attenuated by distance
		uint8 FloorTex = 0; // sector floor texture index
		// FloorZ of the slab IMMEDIATELY behind this one in the same ray (NaN if none).
		float BehindFloorZ = std::numeric_limits<float>::quiet_NaN();
		bool RimAtFar = false; // true if the far edge of this band is a visible step-down rim
		// GODOT ADDITION (inherited): perpendicular ray distance — lets bands join the one
		// depth-sorted paint list.
		float Distance = std::numeric_limits<float>::infinity();

		// GO-03 pooling: reproduce the default-constructed field values for recycled records.
		void Reset() { *this = FFloorBand(); }
	};

	// Phase 8: ceiling band — mirror of FFloorBand. Painted only for non-sky sectors; sky
	// sectors leave the sky backdrop showing through. Within a column, ceiling bands and floor
	// bands occupy disjoint sub-ranges of the ray's vertical occlusion window so they never
	// overlap in Y.
	struct FCeilingBand
	{
		float TopPx = 0.f;	  // upper edge in screen px (smaller = higher)
		float BotPx = 0.f;	  // lower edge in screen px (= projected ceiling line)
		float CeilingZ = 0.f; // world Z (for shading)
		uint8 Light = 0;
		uint8 CeilingTex = 0;
		// GODOT ADDITION (inherited): perpendicular ray distance — lets bands join the one
		// depth-sorted paint list.
		float Distance = std::numeric_limits<float>::infinity();

		// GO-03 pooling: reproduce the default-constructed field values for recycled records.
		void Reset() { *this = FCeilingBand(); }
	};

	struct FColumnInfo
	{
		FWallSeg Main;
		// Phase 3+: extra segs above/below Main from portal upper/lower at varying floor/ceiling
		// heights. Renderer iterates Extras then draws Main on top (Main is the closest/most-
		// clipping seg). Pointers into FFrameData's WallSeg pool — valid until the next
		// ResetPools() (i.e. for exactly one cast frame).
		TArray<FWallSeg*> Extras;
		// Phase 7: world Z of the floor at the terminal wall hit, used as a fallback when there
		// are no per-portal floor bands.
		float FrontFloorZ = 0.f;
		// Phase 7: per-sector floor bands along this ray, ordered near→far. Each band paints
		// from TopPx down to BotPx, colored by FloorZ. Pooled — same lifetime as Extras.
		TArray<FFloorBand*> FloorBands;
		// Phase 8: per-sector ceiling bands along this ray, near→far. Skipped for sky sectors
		// (sky backdrop shows through). Pooled — same lifetime as Extras.
		TArray<FCeilingBand*> CeilingBands;
		// Phase 8: floor-step occlusion for sprite culling. When the ray crosses a step-up
		// portal (back.FloorZ > front.FloorZ), the riser blocks any monster standing on the
		// lower side. Sprites with perp > FloorOccluderDist AND anchorZ < FloorOccluderZ - 0.05
		// are culled. Default values mean "no occluder".
		float FloorOccluderDist = 0.f;
		float FloorOccluderZ = 0.f;
		// Phase 9: ceiling-slab occlusion for sprite culling. When the ray crosses an ExtraFloor
		// BOTTOM (slab underside, viewer below), the slab hides anything behind it whose anchor
		// Z sits at-or-above the slab body. Sprites with perp > CeilingOccluderDist AND
		// anchorZ >= CeilingOccluderZ are culled. Default = no occluder.
		float CeilingOccluderDist = 0.f;
		float CeilingOccluderZ = 0.f;
	};

	// Phase 8: hitscan tracer streak. Lives ~TRACER_LIFE_MS, fades alpha out. Stored in a
	// fixed-size ring on FGameState; AgeMs >= TRACER_LIFE_MS = dead.
	struct FTracer
	{
		float Ax = 0.f, Ay = 0.f, Az = 0.f; // muzzle (start)
		float Bx = 0.f, By = 0.f, Bz = 0.f; // impact / max-range (end)
		float AgeMs = 0.f;
		uint8 ColorIdx = 0; // 0=yellow pistol/chaingun, 1=red shotgun pellet
	};

	struct RUIDEMO_API FFrameData
	{
		// Move-only: the pool slots below are uniquely owned, and neither sibling ever copies a
		// FrameData (C# shares the arrays by reference, Godot the RefCounted object). This makes
		// FGameState move-only too — pass game state by reference/pointer, as the siblings do.
		FFrameData() = default;
		FFrameData(const FFrameData&) = delete;
		FFrameData& operator=(const FFrameData&) = delete;
		FFrameData(FFrameData&&) = default;
		FFrameData& operator=(FFrameData&&) = default;

		TArray<FColumnInfo> Columns; // sized VIEW_W by NewGame; reused every cast frame
		TArray<float> DepthBuffer;	 // sized VIEW_W by NewGame

		// GO-03 pooling (inherited from the Godot port): a per-frame linear allocator for the
		// WallSeg/FloorBand/CeilingBand records the column caster emits ~13x per column, every
		// column, every tick. ResetPools() rewinds the used-cursors at the top of CastFrame;
		// Take* hand out a Reset() record, growing the backing pool only when a frame needs more
		// records than any prior frame did. Records live in individually heap-owned slots so the
		// pointers handed out (and stored in FColumnInfo) stay stable while the pool grows.
		// Safe because a frame's records are fully consumed by the render before the next tick
		// reuses them (time-slicing, which could park a render past the next tick, is off by
		// default and unused by the demo).
		void ResetPools();
		FWallSeg& TakeWallSeg();
		FFloorBand& TakeFloorBand();
		FCeilingBand& TakeCeilBand();

	private:
		TArray<TUniquePtr<FWallSeg>> WallSegPool;
		int32 WallSegUsed = 0;
		TArray<TUniquePtr<FFloorBand>> FloorBandPool;
		int32 FloorBandUsed = 0;
		TArray<TUniquePtr<FCeilingBand>> CeilBandPool;
		int32 CeilBandUsed = 0;
	};

	// ── Phase 1: Sector-engine data model (parallel to the FMapDef tile grid) ─────────────────
	// These types describe the world as Doom-style sectors + linedefs.

	struct FVertex
	{
		FVector2D P = FVector2D::ZeroVector;

		FVertex() = default;
		explicit FVertex(const FVector2D& InP) : P(InP) {}
		FVertex(float X, float Y) : P(X, Y) {}
	};

	struct FLinedef
	{
		int32 V1 = 0; // vertex indices
		int32 V2 = 0;
		int32 FrontSector = 0; // sector on the right side of V1→V2
		int32 BackSector = -1; // -1 if one-sided (solid wall)
		ELinedefFlags Flags = ELinedefFlags::None;
		ELineSpecial Special = ELineSpecial::None;
		uint8 Tag = 0;		// links to sector with same Tag for triggers
		uint8 MidTex = 0;	// wall texture for one-sided / window mid
		uint8 UpperTex = 0; // shown when neighbor ceiling is lower
		uint8 LowerTex = 0; // shown when neighbor floor is higher
	};

	struct FSector
	{
		float FloorZ = 0.f;
		float CeilingZ = 0.f;
		uint8 Light = 0;
		uint8 FloorTex = 0;
		uint8 CeilingTex = 0;
		ESectorSpecial Special = ESectorSpecial::None;
		uint8 Tag = 0;
		bool IsSky = false;				 // ceiling renders as sky if true
		TArray<int32> LineIds;			 // linedefs that touch this sector
		TArray<FExtraFloor> ExtraFloors; // empty if no 3D floors (C# null)
		// Door/lift animation state (Phase 5)
		float TargetCeilingZ = 0.f;
		float TargetFloorZ = 0.f;
		float CeilingSpeed = 0.f; // units/sec; 0 = idle
		float FloorSpeed = 0.f;
		float DoorWaitTimer = 0.f; // counts down after open before auto-close
	};

	struct RUIDEMO_API FMapData
	{
		TArray<FVertex> Vertices;
		TArray<FLinedef> Lines;
		TArray<FSector> Sectors;
		FString Name;
		FVector2D PlayerStart = FVector2D::ZeroVector;
		float PlayerStartAngle = 0.f;
		int32 PlayerStartSector = -1;
		// Phase 2: tile-cell to sector lookup (W*H, -1 if cell is Wall). Built by FromTiles;
		// lets the legacy door cell animation also drive the corresponding sector's CeilingZ so
		// the sector renderer sees doors open/close.
		TArray<int32> CellToSector;
		int32 CellWidth = 0;
		int32 CellHeight = 0;

		bool IsValid() const { return Sectors.Num() > 0; }

		// Build a flat tile-grid map into a sector model. Each non-Wall cell becomes its own
		// sector; tile-to-tile boundaries become linedefs. Wall cells contribute solid edges to
		// neighbor sectors. Baseline conversion that keeps the game running unchanged while the
		// sector pipeline comes online.
		static FMapData FromTiles(const FMapDef& Map);

		// Find sector containing point (X,Y). Hint-only API in the original (always returns -1;
		// callers use CellToSector directly) — ported verbatim.
		int32 PointInSector(float X, float Y, int32 Width) const;
	};

	// ── Game state ─────────────────────────────────────────────────────────────────────────────

	struct FGameState
	{
		FPlayerState Player;
		// The fixed Mobj pool: NewGame sizes this to C::MAX_MOBJS once; slots with Id == 0 are
		// free (default-constructed = free). Spawning claims the slot AFTER MobjCount when room
		// remains, else scans slots 1..N-1 for Id == 0; despawning writes Id = 0 and shrinks
		// MobjCount past trailing free slots.
		TArray<FMobj> Mobjs;
		int32 MobjCount = 0;
		int32 NextMobjId = 0;
		FMapDef Map;
		FFrameData Frame;
		int32 Level = 0;
		int32 Score = 0;
		int32 KillCount = 0;
		int32 KillTotal = 0;
		int32 Tic = 0;
		bool GameOver = false;
		bool Victory = false;
		// When true, stepping on an Exit cell only triggers Victory once every boss-tier monster
		// (Baron, Cacodemon) on the map is dead.
		bool BossExitGated = false;
		EDifficulty Difficulty = EDifficulty::Easy;
		int32 RngSeed = 0;
		float TimeAccum = 0.f;
		// ── Phase 1+: sector-engine map (parallel to Map). Built by NewGame. ──
		FMapData SectorMap;
		// Optional 2D BSP over the sector map (a Godot-only upgrade; DOOM_DEMO_PLAN Phase 7).
		// nullptr = fall back to the per-column portal ray-walker.
		TSharedPtr<FDoomBsp> Bsp;
		// Phase 8: hitscan tracer ring. NewGame sizes to C::MAX_TRACERS; index =
		// TracerCount % MAX_TRACERS. Slots with AgeMs >= TRACER_LIFE_MS are skipped by the
		// renderer.
		TArray<FTracer> Tracers;
		int32 TracerCount = 0;

		// The ONE deterministic RNG (family fix, ported bit-exactly from the Godot `frand`):
		// a tiny LCG stepped with int32 wraparound — seed via RngSeed (NewGame uses
		// 12345 + Level * 17). Same multiplier/increment as both siblings so replays and the
		// determinism test suite match across engines.
		float Frand()
		{
			RngSeed = static_cast<int32>(static_cast<uint32>(RngSeed) * 1103515245u + 12345u);
			return static_cast<float>((static_cast<uint32>(RngSeed) >> 16) & 0x7fff) / static_cast<float>(0x7fff);
		}
	};

	struct FInputCmd
	{
		bool Forward = false, Back = false, StrafeLeft = false, StrafeRight = false;
		bool TurnLeft = false, TurnRight = false;
		bool Attack = false, Use = false, Run = false;
		uint8 WeaponSwitch = 0;
		float YawDelta = 0.f;
		float PitchDelta = 0.f;
		// Phase 7: vertical control
		bool Jump = false;
		bool Crouch = false;
	};

	// ── Per-frame render data extensions (Phase 2+) ────────────────────────────────────────────
	// Declared but vestigial in the current renderer path in BOTH siblings — ported for fidelity.

	struct FWallSegV2
	{
		float TopPx = 0.f;
		float BotPx = 0.f;
		float Distance = 0.f;
		uint8 WallTexIdx = 0;
		float TexU = 0.f;
		float TexVStart = 0.f, TexVEnd = 0.f; // for variable-height segs
		uint8 LightLevel = 0;
		bool HitVertical = false;
		bool IsSky = false;
		ESegKind Kind = ESegKind::Mid;
		int32 SectorId = 0; // sector this seg belongs to (for sprite ordering)
	};

	struct FFloorSpan
	{
		int32 Y = 0; // screen Y
		int32 LeftCol = 0;
		int32 RightCol = 0; // inclusive
		int32 SectorId = 0;
		bool IsCeiling = false;
		uint8 Light = 0;
		uint8 TexIdx = 0;
		float DistanceAtCenter = 0.f; // for shading
	};
} // namespace RuiDoom
