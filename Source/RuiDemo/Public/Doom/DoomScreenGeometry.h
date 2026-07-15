// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Doom demo — SCREEN GEOMETRY builders (plans/DOOM_DEMO_PLAN.md Phase 3). Faithful port of
// the Unity ReactiveUIToolKit `DoomGame` sample's `DoomGameScreenLogic.uitkx` list builders
// PLUS the per-quad math its `DoomGameScreen.uitkx` @foreach passes computed inline
// (screen-space projection, light shading into tint, sky scroll window, sprite billboard
// placement, tracer streak geometry, weapon bob). Pure per-tick functions: FGameState in,
// flat per-pass FDoomQuad lists out — the .uetkx screen renders them as keyed <Image>
// children of ONE <Canvas> (DoomScreenTypes.h is the contract).
//
// Rendering model (the Phase-0 spike, DOOM_DEMO_PLAN §1/§1b): Slate has no per-quad z, so
// quads are emitted in PAINTER'S ORDER — pass by pass, stable build order within each pass,
// never re-sorted after emission — and occlusion is decided HERE in the builders. Wall
// strips sample the PRE-TILED 64x256 `FDoomTextures::WallTiled` texture through a per-strip
// brush UVRegion window (U = the texel-column slice, V = the fractional world-repeat range,
// both inside [0,1]); solid quads (bands/rims/flashes/tracers) share FDoomBrushPool::Solid()
// and carry their color in the widget tint.
//
// Inherited from the Godot sibling's `doom_game_screen_logic.gd` (documented non-z
// improvements only — its per-quad z_index model is deliberately NOT ported):
//   - merged ceiling/floor band runs: consecutive same-slab bands per column (Z quantized
//     to 0.2u, slab = round(Z * 5)) collapse into one tall quad — the per-frame quad-count
//     win (~1900 floor quads -> ~160 on E1M1),
//   - stable keyed naming: "w%d" walls, "fb%d_%d"/"cb%d_%d" bands (column, group index),
//     "x%d_%d" extra segs, "s%d_%d" sprite runs, "tr%d" tracers,
//   - the shared-solid-brush trick (every flat quad is the same widget type — no type
//     churn) and per-key brush pooling (maps to FDoomBrushPool::GetTextured),
//   - per-column sprite depth-CLIPPING: a billboard is cut (not squashed) at wall edges by
//     emitting one quad per contiguous visible column run — fixes "enemy through the wall",
//   - MIN_WALL_SHADE (dark sectors otherwise crush walls/risers to hard black bars),
//   - integer-pixel x snapping on band/wall quads (sub-pixel x leaves a 1px seam per column).
//
// Every projection constant, shading curve, and bob number is copied digit-for-digit from
// the Unity original — the golden-frame tests depend on it.

#pragma once

#include "Doom/DoomTypes.h"
#include "Doom/DoomScreenTypes.h"

#include "CoreMinimal.h"

namespace RuiDoom
{
	/** Minimum wall/seg brightness (GODOT ADDITION, inherited): dark sectors otherwise crush
	 *  walls, thin stair risers and mortar columns to near-black (hard black bars). Raise for
	 *  a brighter level, lower for a moodier one — a top-level readability knob applied to
	 *  every wall/seg's mono shade. (The Unity original used 0.45 on extra segs only.) */
	inline constexpr float MIN_WALL_SHADE = 0.55f;

	// ───── Intermediate entry records (the original's public builder structs) ─────

	/** One visible mobj billboard (the original's `SpriteEntry`). */
	struct FSpriteEntry
	{
		int32 Id = 0;
		int32 SpriteIdx = 0;
		float ScreenX = 0.f;
		float ScreenY = 0.f;
		float Size = 0.f;
		float Distance = 0.f;
		FColor Tint = FColor::White;
		float Light = 0.f;
	};

	/** One live hitscan tracer streak, projected + near-plane clipped (the original's
	 *  `TracerEntry`). AngleDeg rotates about the rect's LEFT-CENTER (the markup pins
	 *  RenderTransformPivot to (0, 0.5)). */
	struct FTracerEntry
	{
		int32 Slot = 0;
		float Left = 0.f;
		float Top = 0.f;
		float Length = 0.f;
		float AngleDeg = 0.f;
		float Alpha = 0.f;
		uint8 ColorIdx = 0;
	};

	/**
	 * One frame's render geometry: one quad list PER PASS, passes in PAINTER'S ORDER (the
	 * Unity markup's element order top to bottom). The markup renders the arrays in this
	 * exact order as keyed children of one Canvas; within each array the build order is
	 * stable and already occlusion-resolved — never sort after emission.
	 *
	 * Pass inventory (array -> key scheme):
	 *   1. Sky           "sky0"/"sky1"      scrolling sky window; splits in two at the
	 *                                       horizontal texture wrap point
	 *   2. FloorBackstop "floorbk"          solid ground color from horizon to bottom (seals
	 *                                       seams between merged floor bands)
	 *   3. CeilingBands  "cb<col>_<group>"  merged same-slab runs, far->near per column
	 *   4. FloorBands    "fb<col>_<group>"  merged same-slab runs, far->near per column
	 *   5. FloorRims     "frim<col>_<group>" step-down silhouette chalk line (RimAtFar)
	 *   6. ExtraSegs     "x<col>_<seg>"     portal upper/lower wall segs, pre-tiled UVRegion
	 *   7. RiserRims     "rim<col>_<seg>"   chalk line at the top edge of step risers
	 *   8. Walls         "w<col>"           the 160 terminal wall strips, pre-tiled UVRegion
	 *   9. Sprites       "s<id>_<runCol>"   billboard slices, one per visible column run,
	 *                                       far->near across sprites
	 *  10. Tracers       "tr<slot>"         rotated streaks (FDoomQuad::Angle degrees)
	 *  11. Overlay       "wpn"/"muzzle"/"xhair"/"hurt"/"pickup"
	 *                                       weapon bob, muzzle flash, crosshair, hurt +
	 *                                       pickup full-screen flashes (alpha-driven)
	 */
	struct RUIDEMO_API FDoomFrameGeometry
	{
		TArray<FDoomQuad> Sky;
		TArray<FDoomQuad> FloorBackstop;
		TArray<FDoomQuad> CeilingBands;
		TArray<FDoomQuad> FloorBands;
		TArray<FDoomQuad> FloorRims;
		TArray<FDoomQuad> ExtraSegs;
		TArray<FDoomQuad> RiserRims;
		TArray<FDoomQuad> Walls;
		TArray<FDoomQuad> Sprites;
		TArray<FDoomQuad> Tracers;
		TArray<FDoomQuad> Overlay;

		/** Reset() every pass array (keeps capacity — per-frame reuse, no reallocation). */
		void Reset();

		// ── Internal per-frame scratch (reused; not render output) ──

		/** One merged same-slab band run within a column (GODOT ADDITION, inherited). */
		struct FBandGroup
		{
			float Top = 0.f;
			float Bot = 0.f;
			float Z = 0.f;
			uint8 Light = 0;
			float Distance = 0.f;
			bool bRimAtFar = false;
		};
		TArray<FSpriteEntry> SpriteScratch;
		TArray<FTracerEntry> TracerScratch;
		TArray<FBandGroup> BandScratch;
	};

	// ───── The pure per-tick builders ─────

	/** Billboard size multiplier per mobj kind (the original's `SpriteScale`). */
	RUIDEMO_API float SpriteScale(EMobjKind K);

	/** World-Z anchor offset added to a mobj's feet for billboard placement (the original's
	 *  `SpriteVerticalAnchor`). */
	RUIDEMO_API float SpriteVerticalAnchor(EMobjKind K);

	/** Project every live mobj to a screen-space billboard entry, applying the per-column
	 *  depth test plus the floor-step / ceiling-slab / step-down-silhouette occlusion culls,
	 *  then sort far->near (the ONE pre-emission sort the original performs). `Out` is
	 *  Reset(), not reallocated. Pass (C::VIEWPORT_W, C::VIEWPORT_H) for the reference frame. */
	RUIDEMO_API void BuildSpriteList(const FGameState& State, const FVector2D& ViewportSize, TArray<FSpriteEntry>& Out);

	/** Project the live tracer ring to screen-space rotated rects (camera-space rotation +
	 *  perspective + near-plane clip). `Out` is Reset(), not reallocated. */
	RUIDEMO_API void BuildTracerList(const FGameState& State, const FVector2D& ViewportSize, TArray<FTracerEntry>& Out);

	/** Build the whole frame: every pass array in `Out` is Reset() and refilled in painter's
	 *  order from `State`'s cast frame. `Brushes` pools the per-key textured brushes (walls/
	 *  sprites/sky/weapon); solid quads share FDoomBrushPool::Solid(). `ViewportSize` is the
	 *  canvas size in px — pass (C::VIEWPORT_W, C::VIEWPORT_H) for the reference 800x500
	 *  frame the sibling ports hardcode (all math scales from it). */
	RUIDEMO_API void BuildFrameGeometry(const FGameState& State, FDoomBrushPool& Brushes, const FVector2D& ViewportSize,
										FDoomFrameGeometry& Out);
} // namespace RuiDoom
