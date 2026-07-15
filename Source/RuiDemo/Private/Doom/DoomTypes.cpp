// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
#include "Doom/DoomTypes.h"

namespace RuiDoom
{
	// ── FMapDef ────────────────────────────────────────────────────────────────────────────────

	const FCell& FMapDef::AtSafe(int32 X, int32 Y) const
	{
		// The C# original returned `new Cell { Kind = Wall, WallTexIdx = 0, LightLevel = 200 }`
		// per out-of-bounds call; the cell is immutable in practice, so one shared instance
		// returned by const& is behavior-identical and allocation-free.
		static const FCell Boundary = []
		{
			FCell Cell;
			Cell.Kind = ECellKind::Wall;
			Cell.WallTexIdx = 0;
			Cell.LightLevel = 200;
			return Cell;
		}();
		if (X < 0 || X >= Width || Y < 0 || Y >= Height)
		{
			return Boundary;
		}
		return Cells[Y * Width + X];
	}

	bool FMapDef::BlocksMovement(int32 X, int32 Y) const
	{
		const FCell& Cell = AtSafe(X, Y);
		if (Cell.Kind == ECellKind::Wall)
		{
			return true;
		}
		if (Cell.Kind == ECellKind::Door || Cell.Kind == ECellKind::DoorBlue || Cell.Kind == ECellKind::DoorYellow ||
			Cell.Kind == ECellKind::DoorRed)
		{
			return Cell.DoorState < 200;
		}
		return false;
	}

	bool FMapDef::BlocksMovementZ(int32 X, int32 Y, float FootZ, float HeadZ, float StepHeight) const
	{
		const FCell& Cell = AtSafe(X, Y);
		if (Cell.Kind == ECellKind::Wall)
		{
			return true;
		}
		if (Cell.Kind == ECellKind::Door || Cell.Kind == ECellKind::DoorBlue || Cell.Kind == ECellKind::DoorYellow ||
			Cell.Kind == ECellKind::DoorRed)
		{
			if (Cell.DoorState < 200)
			{
				return true;
			}
		}
		// Phase 9: pick the highest standing surface at-or-below (FootZ + StepHeight): the cell
		// floor or any FExtraFloor.TopZ.
		float Floor = Cell.FloorZ;
		float Ceil = Cell.IsSky ? 1e9f : (Cell.CeilingZ <= 0.f ? 1.5f : Cell.CeilingZ);
		for (const FExtraFloor& Ef : Cell.ExtraFloors)
		{
			if (!Ef.Solid)
			{
				continue;
			}
			if (Ef.TopZ <= FootZ + StepHeight + 0.001f && Ef.TopZ > Floor)
			{
				Floor = Ef.TopZ;
			}
		}
		// Block if the step-up exceeds the actor's stride.
		if (Floor - FootZ > StepHeight + 0.001f)
		{
			return true;
		}
		// Ceiling: lowest blocking surface ABOVE the standing floor. Player body would occupy
		// [Floor, Floor + (HeadZ - FootZ)] after step-up. Any solid slab whose body intersects
		// that volume is a blocker.
		const float StandHead = Floor + (HeadZ - FootZ);
		for (const FExtraFloor& Ef : Cell.ExtraFloors)
		{
			if (!Ef.Solid)
			{
				continue;
			}
			// Slab top at-or-below standing floor is the floor itself — not a blocker. Slab
			// whose BODY overlaps [Floor + eps, StandHead - eps] is a blocker (we'd be inside
			// it).
			if (Ef.TopZ <= Floor + 0.001f)
			{
				continue;
			}
			if (Ef.BottomZ < StandHead - 0.001f && Ef.TopZ > Floor + 0.001f)
			{
				return true;
			}
			if (Ef.BottomZ < Ceil)
			{
				Ceil = Ef.BottomZ;
			}
		}
		if (Ceil < StandHead - 0.001f)
		{
			return true;
		}
		return false;
	}

	float FMapDef::FloorAt(int32 X, int32 Y, float FootZ, float StepHeight) const
	{
		const FCell& Cell = AtSafe(X, Y);
		float Best = Cell.FloorZ;
		for (const FExtraFloor& Ef : Cell.ExtraFloors)
		{
			if (!Ef.Solid)
			{
				continue;
			}
			if (Ef.TopZ <= FootZ + StepHeight + 0.001f && Ef.TopZ > Best)
			{
				Best = Ef.TopZ;
			}
		}
		return Best;
	}

	bool FMapDef::BlocksRays(int32 X, int32 Y) const
	{
		const FCell& Cell = AtSafe(X, Y);
		if (Cell.Kind == ECellKind::Wall)
		{
			return true;
		}
		if (Cell.Kind == ECellKind::Door || Cell.Kind == ECellKind::DoorBlue || Cell.Kind == ECellKind::DoorYellow ||
			Cell.Kind == ECellKind::DoorRed)
		{
			return Cell.DoorState < 250;
		}
		return false;
	}

	// ── FFrameData pools ───────────────────────────────────────────────────────────────────────

	void FFrameData::ResetPools()
	{
		WallSegUsed = 0;
		FloorBandUsed = 0;
		CeilBandUsed = 0;
	}

	FWallSeg& FFrameData::TakeWallSeg()
	{
		if (!WallSegPool.IsValidIndex(WallSegUsed))
		{
			WallSegPool.Add(MakeUnique<FWallSeg>());
		}
		FWallSeg& Out = *WallSegPool[WallSegUsed++];
		Out.Reset();
		return Out;
	}

	FFloorBand& FFrameData::TakeFloorBand()
	{
		if (!FloorBandPool.IsValidIndex(FloorBandUsed))
		{
			FloorBandPool.Add(MakeUnique<FFloorBand>());
		}
		FFloorBand& Out = *FloorBandPool[FloorBandUsed++];
		Out.Reset();
		return Out;
	}

	FCeilingBand& FFrameData::TakeCeilBand()
	{
		if (!CeilBandPool.IsValidIndex(CeilBandUsed))
		{
			CeilBandPool.Add(MakeUnique<FCeilingBand>());
		}
		FCeilingBand& Out = *CeilBandPool[CeilBandUsed++];
		Out.Reset();
		return Out;
	}

	// ── FMapData ───────────────────────────────────────────────────────────────────────────────

	FMapData FMapData::FromTiles(const FMapDef& Map)
	{
		TArray<FVertex> Verts;
		TMap<int64, int32> VIdx;
		const int32 W = Map.Width;
		const int32 H = Map.Height;

		auto GetVertex = [&Verts, &VIdx](int32 X, int32 Y) -> int32
		{
			const int64 Key = (static_cast<int64>(X) << 32) | static_cast<uint32>(Y);
			if (const int32* Found = VIdx.Find(Key))
			{
				return *Found;
			}
			const int32 Id = Verts.Num();
			Verts.Add(FVertex(static_cast<float>(X), static_cast<float>(Y)));
			VIdx.Add(Key, Id);
			return Id;
		};

		// 1) Allocate one sector per non-Wall cell, indexed by (Y*W + X).
		TArray<int32> CellSector;
		CellSector.Init(-1, W * H);
		TArray<FSector> Sectors;

		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const FCell& Cell = Map.Cells[Y * W + X];
				if (Cell.Kind == ECellKind::Wall)
				{
					continue;
				}
				FSector NewSector;
				NewSector.FloorZ = Cell.FloorZ;
				NewSector.CeilingZ = Cell.IsSky ? 64.f : (Cell.CeilingZ <= 0.f ? 1.5f : Cell.CeilingZ);
				NewSector.Light = Cell.LightLevel == 0 ? uint8(200) : Cell.LightLevel;
				NewSector.FloorTex = Cell.FloorTexIdx;
				NewSector.CeilingTex = Cell.CeilingTexIdx;
				NewSector.Special = Cell.Special;
				NewSector.Tag = Cell.Tag;
				NewSector.IsSky = Cell.IsSky;
				NewSector.LineIds.Reserve(4);
				NewSector.ExtraFloors = Cell.ExtraFloors;
				NewSector.TargetCeilingZ = Cell.IsSky ? 64.f : (Cell.CeilingZ <= 0.f ? 1.5f : Cell.CeilingZ);
				NewSector.TargetFloorZ = Cell.FloorZ;
				NewSector.CeilingSpeed = 0.f;
				NewSector.FloorSpeed = 0.f;
				NewSector.DoorWaitTimer = 0.f;
				// Doors map to closed sectors with CeilingZ=FloorZ (closed) until opened.
				if (Cell.Kind == ECellKind::Door || Cell.Kind == ECellKind::DoorBlue ||
					Cell.Kind == ECellKind::DoorYellow || Cell.Kind == ECellKind::DoorRed)
				{
					NewSector.CeilingZ = 0.f; // closed
					NewSector.TargetCeilingZ = 1.5f;
				}
				CellSector[Y * W + X] = Sectors.Num();
				Sectors.Add(MoveTemp(NewSector));
			}
		}

		// 2) Walk all 4-edges of every cell. Each edge becomes either a one-sided line (cell vs
		//    Wall) or a two-sided line (cell vs cell).
		TArray<FLinedef> Lines;
		auto AddLine = [&Lines, &Sectors, &GetVertex](int32 X1, int32 Y1, int32 X2, int32 Y2, int32 FrontSec,
													  int32 BackSec, uint8 MidTex)
		{
			const int32 V1 = GetVertex(X1, Y1);
			const int32 V2 = GetVertex(X2, Y2);
			FLinedef Line;
			Line.V1 = V1;
			Line.V2 = V2;
			Line.FrontSector = FrontSec;
			Line.BackSector = BackSec;
			Line.Flags = BackSec >= 0 ? ELinedefFlags::TwoSided : ELinedefFlags::None;
			Line.Special = ELineSpecial::None;
			Line.Tag = 0;
			Line.MidTex = MidTex;
			Line.UpperTex = MidTex;
			Line.LowerTex = MidTex;
			const int32 Li = Lines.Num();
			Lines.Add(Line);
			if (FrontSec >= 0)
			{
				Sectors[FrontSec].LineIds.Add(Li);
			}
			if (BackSec >= 0)
			{
				Sectors[BackSec].LineIds.Add(Li);
			}
		};

		// Helper: pick wall texture for an edge between cell (X,Y) and neighbor (Nx,Ny). If
		// neighbor is wall -> use neighbor's WallTexIdx. Otherwise if neighbor is a door cell,
		// use the door's WallTexIdx (so the door renders when closed).
		auto EdgeTex = [&Map, W, H](int32 X, int32 Y, int32 Nx, int32 Ny) -> uint8
		{
			const FCell& SelfCell = Map.Cells[Y * W + X];
			if (Nx < 0 || Nx >= W || Ny < 0 || Ny >= H)
			{
				return SelfCell.WallTexIdx;
			}
			const FCell& Nb = Map.Cells[Ny * W + Nx];
			if (Nb.Kind == ECellKind::Wall || (Nb.Kind == ECellKind::Exit && Nb.WallTexIdx != 0))
			{
				return Nb.WallTexIdx;
			}
			if (Nb.Kind == ECellKind::Door || Nb.Kind == ECellKind::DoorBlue || Nb.Kind == ECellKind::DoorYellow ||
				Nb.Kind == ECellKind::DoorRed)
			{
				return Nb.WallTexIdx;
			}
			return SelfCell.WallTexIdx;
		};

		// For each non-wall cell, emit its right and bottom edges. Right edge: from (X+1,Y) to
		// (X+1,Y+1). Bottom: from (X,Y+1) to (X+1,Y+1). Top edge handled by neighbor above; left
		// edge by neighbor to the left. Cells on the outer boundary get an explicit edge to
		// "wall" (back=-1).
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const int32 S = CellSector[Y * W + X];
				if (S < 0)
				{
					continue;
				}
				// Right edge (between this and (X+1, Y))
				const int32 NeighborR = (X + 1 < W) ? CellSector[Y * W + (X + 1)] : -1;
				const uint8 TexR = EdgeTex(X, Y, X + 1, Y);
				if (X + 1 >= W || Map.Cells[Y * W + (X + 1)].Kind == ECellKind::Wall)
				{
					AddLine(X + 1, Y, X + 1, Y + 1, S, -1, TexR);
				}
				else
				{
					AddLine(X + 1, Y, X + 1, Y + 1, S, NeighborR, TexR);
				}
				// Bottom edge (between this and (X, Y+1))
				const int32 NeighborB = (Y + 1 < H) ? CellSector[(Y + 1) * W + X] : -1;
				const uint8 TexB = EdgeTex(X, Y, X, Y + 1);
				if (Y + 1 >= H || Map.Cells[(Y + 1) * W + X].Kind == ECellKind::Wall)
				{
					AddLine(X, Y + 1, X + 1, Y + 1, S, -1, TexB);
				}
				else
				{
					AddLine(X, Y + 1, X + 1, Y + 1, S, NeighborB, TexB);
				}
				// Top edge (only if topmost row or neighbor above is wall — otherwise already
				// handled by neighbor's bottom edge)
				if (Y == 0 || Map.Cells[(Y - 1) * W + X].Kind == ECellKind::Wall)
				{
					const uint8 TexT = EdgeTex(X, Y, X, Y - 1);
					AddLine(X, Y, X + 1, Y, S, -1, TexT);
				}
				// Left edge (only if leftmost col or neighbor left is wall)
				if (X == 0 || Map.Cells[Y * W + (X - 1)].Kind == ECellKind::Wall)
				{
					const uint8 TexL = EdgeTex(X, Y, X - 1, Y);
					AddLine(X, Y, X, Y + 1, S, -1, TexL);
				}
			}
		}

		// 3) Player start sector is computed from CellToSector by the caller once it knows the
		//    tile coordinate; PlayerStart itself is filled in later too.
		FMapData Result;
		Result.Vertices = MoveTemp(Verts);
		Result.Lines = MoveTemp(Lines);
		Result.Sectors = MoveTemp(Sectors);
		Result.Name = Map.Name;
		Result.PlayerStart = FVector2D::ZeroVector;
		Result.PlayerStartAngle = 0.f;
		Result.PlayerStartSector = -1;
		Result.CellToSector = MoveTemp(CellSector);
		Result.CellWidth = W;
		Result.CellHeight = H;
		return Result;
	}

	int32 FMapData::PointInSector(float X, float Y, int32 /*Width*/) const
	{
		// Phase 1 fallback: use original tile coordinates. Intentionally a hint-only API in the
		// original (grid-generated maps use CellToSector for O(1) lookup instead) — ported
		// verbatim, always -1.
		const int32 Ix = static_cast<int32>(X);
		const int32 Iy = static_cast<int32>(Y);
		if (Ix < 0 || Iy < 0)
		{
			return -1;
		}
		return -1;
	}
} // namespace RuiDoom
