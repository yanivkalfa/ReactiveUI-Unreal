// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Doom demo — the GAME LOGIC module: simulation + the sector/portal raycasting renderer
// core. Faithful port of the Unity ReactiveUIToolKit `DoomGame` sample's `GameLogic.uitkx`
// (NewGame/Tick, movement + Z-aware collision, doors-as-moving-sectors, monster AI states,
// hitscan + projectile combat, pickups, keys, damage/armor, level exit/boss gate, and the
// per-column sector-portal renderer writing FFrameData), inheriting the documented fixes
// from the Godot sibling's game_logic.gd:
//   - the single deterministic LCG everywhere — `FGameState::Frand()`; this also replaces
//     the original's stray `UnityEngine.Random.value` jitter in AdvanceFace (the one RNG
//     inconsistency in the reference), which widens AdvanceFace to take the game state,
//   - the GO-03 per-frame render-record pools: CastFrame rewinds `Frame.ResetPools()` and
//     the column builder takes pooled WallSeg/FloorBand/CeilingBand records instead of
//     allocating fresh ones per column (the original news up C# lists every column),
//   - the FloorBand/CeilingBand `Distance` field (perpendicular ray distance) so bands can
//     join the renderer's one depth-sorted paint list.
// The Godot-only 2D-BSP renderer path (USE_BSP) is NOT ported — `FGameState::Bsp` stays
// nullptr and CastFrame always uses the per-column portal ray-walker (DOOM_DEMO_PLAN
// Phase 7 decides whether the BSP upgrade ever lands here).
//
// Every tunable/number is copied digit-for-digit from the original — the determinism test
// suite depends on it. Function names and section order match the C# original 1:1 so the
// port reviews side-by-side; C# `ref`/`out` parameters become C++ references.

#pragma once

#include "Doom/DoomTypes.h"

#include "CoreMinimal.h"

namespace RuiDoom
{
	// Phase 2 (original): when true, CastFrame uses Raycast::Cast (portal walker on the
	// sector graph). When false, falls back to the original DDA grid walker. Toggle off if
	// a regression appears — the engine should still play.
	inline constexpr bool USE_SECTOR_RAYCAST = true;

	// ───── Public API ─────

	/** Build a fresh game state for `Level` (1..LEVEL_COUNT) at `Diff`, spawn the level's
	 *  mobjs, build the parallel sector model, and cast the first frame. */
	RUIDEMO_API FGameState NewGame(int32 Level, EDifficulty Diff);

	/** Advance the simulation one frame: player movement/combat, doors, sector specials,
	 *  monster/projectile/pickup updates, flash decay, tracer aging, then CastFrame. */
	RUIDEMO_API void Tick(FGameState& St, float Dt, FInputCmd Input);

	/** Rebuild Frame.Columns/DepthBuffer for the player's current view (the renderer core).
	 *  Rewinds the per-frame record pools first — pooled pointers from the previous frame
	 *  die here. */
	RUIDEMO_API void CastFrame(FGameState& St);

	/** Apply damage to mobj `Idx` (barrel explosion, monster pain/death, score/kill count). */
	RUIDEMO_API void DamageMobj(FGameState& St, int32 Idx, int32 Dmg, int32 SourceId);

	/** Claim a pool slot for `M` (assigns Id, anchors Z to the spawn sector's floor,
	 *  floats Cacodemon/LostSoul). */
	RUIDEMO_API void AddMobj(FGameState& St, FMobj M);

	// ───── Misc classification helpers ─────

	RUIDEMO_API bool IsMonster(EMobjKind K);
	RUIDEMO_API bool IsBoss(EMobjKind K);

	/** True while any boss-tier monster (Baron / Cacodemon) is still alive on the map.
	 *  Used to gate level-exit cells when FLevelStart::BossExitGated is set. */
	RUIDEMO_API bool AnyBossAlive(const FGameState& St);

	RUIDEMO_API bool IsProjectile(EMobjKind K);
	RUIDEMO_API bool IsPickup(EMobjKind K);

	/** Sprite sheet index (S_*) for a mobj kind. */
	RUIDEMO_API int32 SpriteIndexForMobj(EMobjKind K);

	/** Render tint for a mobj (pain flash, Shotgunner/Zombieman palette swaps). */
	RUIDEMO_API FColor TintForMobj(EMobjKind K, EAIState S);
} // namespace RuiDoom
