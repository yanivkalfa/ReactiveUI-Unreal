// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Doom/DoomRaycast.h"

#include <limits>

namespace RuiDoom
{
	namespace Raycast
	{
		// ──────────────────────────────────────────────────────────────────────────
		//  Geometry primitives
		// ──────────────────────────────────────────────────────────────────────────

		bool RaySegment(const FVector2D& Origin, const FVector2D& Dir, const FVector2D& A, const FVector2D& B,
						float& OutT, float& OutU, bool& OutBackside)
		{
			OutT = 0.f;
			OutU = 0.f;
			OutBackside = false;
			const FVector2D Sd = B - A;
			const float Denom = static_cast<float>(Dir.X * Sd.Y - Dir.Y * Sd.X);
			if (FMath::Abs(Denom) < 1e-7f)
			{
				return false; // parallel
			}
			const FVector2D Oa = Origin - A;
			// t along ray: (oa × sd) / (dir × sd)
			const float Tt = static_cast<float>(Oa.X * Sd.Y - Oa.Y * Sd.X) / -Denom;
			// u along segment: (oa × dir) / (dir × sd)
			const float Uu = static_cast<float>(Oa.X * Dir.Y - Oa.Y * Dir.X) / -Denom;
			if (Tt < 0.f)
			{
				return false;
			}
			if (Uu < 0.f || Uu > 1.f)
			{
				return false;
			}
			OutT = Tt;
			OutU = Uu;
			// The "front" of a linedef is to the RIGHT of V1→V2 (Doom convention).
			// Right-of(V1→V2) means cross(sd, oa) > 0. If our origin is to the left,
			// we are hitting the back side.
			OutBackside = Denom > 0.f;
			return true;
		}

		bool PointInSector(const FMapData& Map, int32 SectorId, const FVector2D& P)
		{
			if (SectorId < 0 || SectorId >= Map.Sectors.Num())
			{
				return false;
			}
			const FSector& S = Map.Sectors[SectorId];
			int32 Crossings = 0;
			for (int32 i = 0; i < S.LineIds.Num(); i++)
			{
				const FLinedef& Ln = Map.Lines[S.LineIds[i]];
				const FVector2D A = Map.Vertices[Ln.V1].P;
				const FVector2D B = Map.Vertices[Ln.V2].P;
				if ((A.Y > P.Y) != (B.Y > P.Y))
				{
					const float XCross = static_cast<float>(A.X + (P.Y - A.Y) * (B.X - A.X) / (B.Y - A.Y));
					if (P.X < XCross)
					{
						Crossings++;
					}
				}
			}
			return (Crossings & 1) == 1;
		}

		int32 PointInSectorFromHint(const FMapData& Map, const FVector2D& P, int32 Hint)
		{
			if (Hint >= 0 && PointInSector(Map, Hint, P))
			{
				return Hint;
			}
			if (Hint >= 0)
			{
				const FSector& S = Map.Sectors[Hint];
				for (int32 i = 0; i < S.LineIds.Num(); i++)
				{
					const FLinedef& Ln = Map.Lines[S.LineIds[i]];
					const int32 Neighbor = (Ln.FrontSector == Hint) ? Ln.BackSector : Ln.FrontSector;
					if (Neighbor >= 0 && PointInSector(Map, Neighbor, P))
					{
						return Neighbor;
					}
				}
			}
			for (int32 i = 0; i < Map.Sectors.Num(); i++)
			{
				if (PointInSector(Map, i, P))
				{
					return i;
				}
			}
			return -1;
		}

		// ──────────────────────────────────────────────────────────────────────────
		//  Portal-walking ray cast
		// ──────────────────────────────────────────────────────────────────────────

		void Cast(const FMapData& Map, const FVector2D& Origin, int32 OriginSector, const FVector2D& Dir,
				  TArray<FWallHit>& OutHits)
		{
			// Rewind-per-call (the Godot _wallhit_pool discipline): Reset keeps the
			// allocation, Reserve caps the one warm-up growth at MAX_RAY_HOPS records.
			OutHits.Reset();
			OutHits.Reserve(C::MAX_RAY_HOPS);
			if (!Map.IsValid() || OriginSector < 0)
			{
				return;
			}

			int32 CurrentSector = OriginSector;
			FVector2D Cursor = Origin;
			float AccumulatedT = 0.f;

			for (int32 Hop = 0; Hop < C::MAX_RAY_HOPS; Hop++)
			{
				const FSector& Sec = Map.Sectors[CurrentSector];

				// Find the closest linedef of the current sector that the ray hits,
				// in front of the cursor (t > epsilon to avoid re-hitting the entry).
				int32 BestLineLocal = -1;
				float BestT = std::numeric_limits<float>::max();
				float BestU = 0.f;
				bool BestBack = false;

				for (int32 Li = 0; Li < Sec.LineIds.Num(); Li++)
				{
					const int32 LineId = Sec.LineIds[Li];
					const FLinedef& Ln = Map.Lines[LineId];
					const FVector2D A = Map.Vertices[Ln.V1].P;
					const FVector2D B = Map.Vertices[Ln.V2].P;
					float T = 0.f;
					float U = 0.f;
					bool Bk = false;
					// The C# original calls RaySegment here with out-params — already
					// allocation-free in C++, so Godot's GO-04 inlining is unnecessary.
					if (RaySegment(Cursor, Dir, A, B, T, U, Bk))
					{
						if (T > 1e-4f && T < BestT)
						{
							BestT = T;
							BestLineLocal = LineId;
							BestU = U;
							BestBack = Bk;
						}
					}
				}

				if (BestLineLocal < 0)
				{
					break; // ray escaped (shouldn't happen on closed sector)
				}
				if (AccumulatedT + BestT > C::MAX_RAY)
				{
					break;
				}

				const FLinedef& HitLine = Map.Lines[BestLineLocal];
				const FVector2D HitPos = Cursor + Dir * BestT;
				const FVector2D Va = Map.Vertices[HitLine.V1].P;
				const FVector2D Vb = Map.Vertices[HitLine.V2].P;
				const float SegLen = static_cast<float>((Vb - Va).Size());

				int32 Neighbor = -1;
				const bool IsPortal = EnumHasAnyFlags(HitLine.Flags, ELinedefFlags::TwoSided) &&
									  !EnumHasAnyFlags(HitLine.Flags, ELinedefFlags::Impassable);
				if (IsPortal)
				{
					Neighbor = (HitLine.FrontSector == CurrentSector) ? HitLine.BackSector : HitLine.FrontSector;
				}

				bool IsSky = false;
				if (Neighbor >= 0)
				{
					IsSky = Map.Sectors[Neighbor].IsSky;
				}

				FWallHit& Wh = OutHits.AddDefaulted_GetRef();
				Wh.Distance = AccumulatedT + BestT;
				Wh.Hit = HitPos;
				Wh.LinedefId = BestLineLocal;
				Wh.FromSector = CurrentSector;
				Wh.ToSector = Neighbor;
				Wh.IsBackside = BestBack;
				Wh.U = BestU;
				Wh.SegLength = SegLen;
				Wh.IsSky = IsSky;

				if (Neighbor < 0)
				{
					break; // solid wall — stop
				}
				// Step cursor slightly past the hit into the neighbor sector.
				Cursor = HitPos + Dir * 1e-3f;
				AccumulatedT += BestT;
				CurrentSector = Neighbor;
			}
		}

		// ──────────────────────────────────────────────────────────────────────────
		//  Convenience helpers
		// ──────────────────────────────────────────────────────────────────────────

		float DistPointToSegmentSq(const FVector2D& P, const FVector2D& A, const FVector2D& B, float& OutU)
		{
			const FVector2D Ab = B - A;
			const float LenSq = static_cast<float>(Ab.SizeSquared());
			if (LenSq < 1e-9f)
			{
				OutU = 0.f;
				return static_cast<float>((P - A).SizeSquared());
			}
			float U = static_cast<float>(FVector2D::DotProduct(P - A, Ab)) / LenSq;
			U = FMath::Clamp(U, 0.f, 1.f);
			const FVector2D Closest = A + Ab * U;
			OutU = U;
			return static_cast<float>((P - Closest).SizeSquared());
		}

		bool CircleHitsSolidLine(const FMapData& Map, int32 SectorId, const FVector2D& Center, float Radius)
		{
			if (SectorId < 0)
			{
				return true;
			}
			const FSector& S = Map.Sectors[SectorId];
			const float R2 = Radius * Radius;
			for (int32 i = 0; i < S.LineIds.Num(); i++)
			{
				const FLinedef& Ln = Map.Lines[S.LineIds[i]];
				const bool IsPortal = EnumHasAnyFlags(Ln.Flags, ELinedefFlags::TwoSided) &&
									  !EnumHasAnyFlags(Ln.Flags, ELinedefFlags::Impassable);
				if (IsPortal)
				{
					continue;
				}
				const FVector2D A = Map.Vertices[Ln.V1].P;
				const FVector2D B = Map.Vertices[Ln.V2].P;
				float U = 0.f;
				const float DSq = DistPointToSegmentSq(Center, A, B, U);
				if (DSq < R2)
				{
					return true;
				}
			}
			return false;
		}
	} // namespace Raycast
} // namespace RuiDoom
