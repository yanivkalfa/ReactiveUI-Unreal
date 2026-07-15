// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Doom demo — the RAYCAST module. Faithful port of the Unity ReactiveUIToolKit `DoomGame`
// sample's `Raycast.uitkx` (cross-checked against the Godot sibling's raycast.gd): the
// portal-walking sector ray/segment caster. Pure FVector2D geometry — no engine dependency
// beyond math types. Every epsilon/constant is copied digit-for-digit from the original;
// column output must match the siblings.
//
// Inherited from the Godot port (raycast.gd GO-04): Cast()'s per-hop WallHit records are
// pooled with a rewind-per-call discipline. In C++ FWallHit is a plain value struct, so the
// natural translation of Godot's static `_wallhit_pool` is the caller-reused OutHits array:
// Cast() rewinds it to 0 each call (Reset() keeps capacity, capped at MAX_RAY_HOPS entries),
// so one array reused across all 160 rays per frame never heap-allocates after warm-up.
// Safe for the same reason as Godot's pool: every caller consumes Cast()'s hits fully and
// sequentially before the next call. Godot's other GO-04 change (inlining the ray-segment
// math into cast() to dodge Dictionary allocs) is NOT needed here — C++ out-params are
// already allocation-free, exactly like the C# original's.

#pragma once

#include "CoreMinimal.h"
#include "Doom/DoomTypes.h"

namespace RuiDoom
{
	namespace Raycast
	{
		struct FWallHit
		{
			float Distance = 0.f;				   // ray parameter t at hit
			FVector2D Hit = FVector2D::ZeroVector; // world-space (x,y)
			int32 LinedefId = 0;
			int32 FromSector = 0;	 // sector we were traversing when we hit
			int32 ToSector = -1;	 // -1 if solid wall, else neighbor sector
			bool IsBackside = false; // ray hit the line from its back side
			float U = 0.f;			 // 0..1 along the linedef (V1→V2)
			float SegLength = 0.f;	 // |V2 − V1| in world units
			bool IsSky = false;		 // back sector has IsSky and ceiling above
		};

		// ──────────────────────────────────────────────────────────────────────────
		//  Geometry primitives
		// ──────────────────────────────────────────────────────────────────────────

		// Returns true if the ray (origin, dir, |dir|=1) intersects segment (a,b).
		// Out: t (ray distance, ≥ 0), u (along segment 0..1), backside (true if ray
		// hits the segment from the right of V1→V2 direction).
		RUIDEMO_API bool RaySegment(const FVector2D& Origin, const FVector2D& Dir, const FVector2D& A,
									const FVector2D& B, float& OutT, float& OutU, bool& OutBackside);

		// Polygon containment via crossing-number using a sector's linedefs.
		// The sector is convex in our generated maps, but this tolerates concave too.
		RUIDEMO_API bool PointInSector(const FMapData& Map, int32 SectorId, const FVector2D& P);

		// Best-effort sector lookup. Tries hint first, then any neighbor of hint
		// through a two-sided line, then brute-forces all sectors. Returns -1 if
		// the point is in no sector (e.g. outside the map).
		RUIDEMO_API int32 PointInSectorFromHint(const FMapData& Map, const FVector2D& P, int32 Hint);

		// ──────────────────────────────────────────────────────────────────────────
		//  Portal-walking ray cast
		// ──────────────────────────────────────────────────────────────────────────

		// Cast a ray from `Origin` (inside `OriginSector`) in direction `Dir`
		// (must be unit length). Walks through two-sided linedefs into adjacent
		// sectors. Stops at the first one-sided line, the first solid (Impassable
		// flag), or after MAX_RAY_HOPS / MAX_RAY distance.
		//
		// Result is a list of hits in order of distance along the ray. The LAST
		// hit is the terminal one. The renderer uses this list to draw upper/
		// lower portal segs at each portal crossing and the final wall on close.
		//
		// `OutHits` is rewound (Reset) each call — reuse ONE array across rays so
		// the capacity warm-up happens once (the Godot pool discipline, see above).
		RUIDEMO_API void Cast(const FMapData& Map, const FVector2D& Origin, int32 OriginSector, const FVector2D& Dir,
							  TArray<FWallHit>& OutHits);

		// ──────────────────────────────────────────────────────────────────────────
		//  Convenience helpers
		// ──────────────────────────────────────────────────────────────────────────

		// Distance from point P to segment AB (squared, plus the parameter u).
		RUIDEMO_API float DistPointToSegmentSq(const FVector2D& P, const FVector2D& A, const FVector2D& B, float& OutU);

		// Test whether a circle at center with radius collides with any solid
		// linedef of `SectorId` (or any one-sided line). Used by Phase 5+ collision.
		RUIDEMO_API bool CircleHitsSolidLine(const FMapData& Map, int32 SectorId, const FVector2D& Center,
											 float Radius);
	} // namespace Raycast
} // namespace RuiDoom
