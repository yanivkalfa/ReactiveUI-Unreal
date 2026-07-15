// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Doom/DoomMaps.h"

#include "Doom/DoomTextures.h"

namespace RuiDoom
{
	// ───── Builder ─────

	FMapBuilder::FMapBuilder(int32 InW, int32 InH, const FString& InName) : W(InW), H(InH), Name(InName)
	{
		Cells.SetNum(W * H);
		for (FCell& Cell : Cells)
		{
			Cell.Kind = ECellKind::Empty;
			Cell.WallTexIdx = (uint8)W_BRICK_GREY;
			Cell.FloorTexIdx = (uint8)F_CONCRETE;
			Cell.CeilingTexIdx = (uint8)F_CONCRETE;
			Cell.LightLevel = 200;
		}
	}

	FCell& FMapBuilder::At(int32 X, int32 Y)
	{
		return Cells[Y * W + X];
	}

	FMapBuilder& FMapBuilder::Border(uint8 WallTex)
	{
		for (int32 X = 0; X < W; ++X)
		{
			FCell& Top = At(X, 0);
			Top.Kind = ECellKind::Wall;
			Top.WallTexIdx = WallTex;
			FCell& Bot = At(X, H - 1);
			Bot.Kind = ECellKind::Wall;
			Bot.WallTexIdx = WallTex;
		}
		for (int32 Y = 0; Y < H; ++Y)
		{
			FCell& L = At(0, Y);
			L.Kind = ECellKind::Wall;
			L.WallTexIdx = WallTex;
			FCell& R = At(W - 1, Y);
			R.Kind = ECellKind::Wall;
			R.WallTexIdx = WallTex;
		}
		return *this;
	}

	FMapBuilder& FMapBuilder::Fill(int32 X0, int32 Y0, int32 X1, int32 Y1, uint8 FloorTex, uint8 CeilTex, uint8 Light)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				if (X < 0 || X >= W || Y < 0 || Y >= H)
				{
					continue;
				}
				FCell& Cell = At(X, Y);
				Cell.Kind = ECellKind::Empty;
				Cell.FloorTexIdx = FloorTex;
				Cell.CeilingTexIdx = CeilTex;
				Cell.LightLevel = Light;
				Cell.IsSky = false; // explicit fill = not outdoors
			}
		}
		return *this;
	}

	FMapBuilder& FMapBuilder::Box(int32 X0, int32 Y0, int32 X1, int32 Y1, uint8 WallTex)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			if (X >= 0 && X < W)
			{
				if (Y0 >= 0 && Y0 < H)
				{
					FCell& Cell = At(X, Y0);
					Cell.Kind = ECellKind::Wall;
					Cell.WallTexIdx = WallTex;
				}
				if (Y1 >= 0 && Y1 < H)
				{
					FCell& Cell = At(X, Y1);
					Cell.Kind = ECellKind::Wall;
					Cell.WallTexIdx = WallTex;
				}
			}
		}
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			if (Y >= 0 && Y < H)
			{
				if (X0 >= 0 && X0 < W)
				{
					FCell& Cell = At(X0, Y);
					Cell.Kind = ECellKind::Wall;
					Cell.WallTexIdx = WallTex;
				}
				if (X1 >= 0 && X1 < W)
				{
					FCell& Cell = At(X1, Y);
					Cell.Kind = ECellKind::Wall;
					Cell.WallTexIdx = WallTex;
				}
			}
		}
		return *this;
	}

	FMapBuilder& FMapBuilder::Wall(int32 X, int32 Y, uint8 WallTex)
	{
		if (X < 0 || X >= W || Y < 0 || Y >= H)
		{
			return *this;
		}
		FCell& Cell = At(X, Y);
		Cell.Kind = ECellKind::Wall;
		Cell.WallTexIdx = WallTex;
		return *this;
	}

	FMapBuilder& FMapBuilder::Door(int32 X, int32 Y, ECellKind Kind)
	{
		if (X < 0 || X >= W || Y < 0 || Y >= H)
		{
			return *this;
		}
		FCell& Cell = At(X, Y);
		Cell.Kind = Kind;
		Cell.WallTexIdx = Kind == ECellKind::DoorBlue	  ? (uint8)W_DOOR_BLUE
						  : Kind == ECellKind::DoorYellow ? (uint8)W_DOOR_YELLOW
						  : Kind == ECellKind::DoorRed	  ? (uint8)W_DOOR_RED
														  : (uint8)W_DOOR;
		Cell.DoorState = 0;
		Cell.DoorTimer = 0;
		return *this;
	}

	FMapBuilder& FMapBuilder::Exit(int32 X, int32 Y)
	{
		if (X < 0 || X >= W || Y < 0 || Y >= H)
		{
			return *this;
		}
		FCell& Cell = At(X, Y);
		Cell.Kind = ECellKind::Exit;
		Cell.WallTexIdx = (uint8)W_EXIT;
		return *this;
	}

	// Walkable exit pad — same trigger semantics as Exit() but the cell renders as
	// plain floor (no EXIT-sign wall block). The blue floor texture is the visual cue.
	FMapBuilder& FMapBuilder::ExitPad(int32 X, int32 Y)
	{
		if (X < 0 || X >= W || Y < 0 || Y >= H)
		{
			return *this;
		}
		FCell& Cell = At(X, Y);
		Cell.Kind = ECellKind::Exit;
		Cell.WallTexIdx = 0;
		Cell.FloorTexIdx = (uint8)F_BLUE;
		return *this;
	}

	FMapBuilder& FMapBuilder::Liquid(int32 X0, int32 Y0, int32 X1, int32 Y1, uint8 Tex)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				if (X < 0 || X >= W || Y < 0 || Y >= H)
				{
					continue;
				}
				FCell& Cell = At(X, Y);
				Cell.Kind = ECellKind::Liquid;
				Cell.FloorTexIdx = Tex;
			}
		}
		return *this;
	}

	// Phase 7: raised platform / step. Sets per-cell FloorZ on an existing walkable
	// cell. Use STEP_HEIGHT (0.4) units for a single step.
	FMapBuilder& FMapBuilder::Step(int32 X0, int32 Y0, int32 X1, int32 Y1, float FloorZ)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				if (X < 0 || X >= W || Y < 0 || Y >= H)
				{
					continue;
				}
				At(X, Y).FloorZ = FloorZ;
			}
		}
		return *this;
	}

	// Phase 7: low ceiling / overhang. Sets per-cell CeilingZ. Implicitly clears the
	// sky flag because a bounded ceiling is the opposite of sky.
	FMapBuilder& FMapBuilder::LowCeiling(int32 X0, int32 Y0, int32 X1, int32 Y1, float CeilZ)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				if (X < 0 || X >= W || Y < 0 || Y >= H)
				{
					continue;
				}
				FCell& Cell = At(X, Y);
				Cell.CeilingZ = CeilZ;
				Cell.IsSky = false;
			}
		}
		return *this;
	}

	// Phase 7: open-air ceiling. Player can jump as high as they like and looks up
	// into sky.
	FMapBuilder& FMapBuilder::Sky(int32 X0, int32 Y0, int32 X1, int32 Y1)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				if (X < 0 || X >= W || Y < 0 || Y >= H)
				{
					continue;
				}
				At(X, Y).IsSky = true;
			}
		}
		return *this;
	}

	FMapBuilder& FMapBuilder::PlayerStart(float X, float Y, float Angle)
	{
		PX = X;
		PY = Y;
		PAng = Angle;
		return *this;
	}

	// Phase 9: stack a 3D floor slab into the rectangle [X0..X1, Y0..Y1]. The slab
	// spans world-Z [BottomZ, TopZ]; the player walks on TopZ and the underside is
	// visible as a "ceiling" below BottomZ. Multiple calls in the same footprint
	// stack: a basement under a courtyard is sector floor + slab whose top is the
	// courtyard surface; a balcony over a hall is the hall + slab whose bottom is
	// the hall ceiling.
	FMapBuilder& FMapBuilder::Floor3D(int32 X0, int32 Y0, int32 X1, int32 Y1, float BottomZ, float TopZ, uint8 SideTex,
									  uint8 TopTex, uint8 BottomTex, uint8 Light, bool Solid)
	{
		if (BottomTex == 0)
		{
			BottomTex = TopTex;
		}
		FExtraFloor Ef;
		Ef.BottomZ = BottomZ;
		Ef.TopZ = TopZ;
		Ef.SideTex = SideTex;
		Ef.TopTex = TopTex;
		Ef.BottomTex = BottomTex;
		Ef.Light = Light;
		Ef.Solid = Solid;
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				if (X < 0 || X >= W || Y < 0 || Y >= H)
				{
					continue;
				}
				FCell& Cell = At(X, Y);
				if (Cell.Kind == ECellKind::Wall)
				{
					continue;
				}
				// Insert keeping ExtraFloors sorted by BottomZ ascending; renderer
				// and Z-resolver both rely on this invariant.
				int32 Ins = 0;
				while (Ins < Cell.ExtraFloors.Num() && Cell.ExtraFloors[Ins].BottomZ <= BottomZ)
				{
					++Ins;
				}
				Cell.ExtraFloors.Insert(Ef, Ins);
			}
		}
		return *this;
	}

	FMapBuilder& FMapBuilder::Spawn(EMobjKind Kind, float X, float Y, float Angle)
	{
		FMobj M;
		M.Id = 0; // assigned later
		M.Kind = Kind;
		M.State = EAIState::Idle; // the original's ternary here was tautological (both branches Idle)
		M.X = X;
		M.Y = Y;
		M.Angle = Angle;
		M.Health = HealthFor(Kind);
		M.Radius = RadiusFor(Kind);
		M.Height = HeightFor(Kind);
		M.AnimFrame = 0;
		Mobjs.Add(M);
		return *this;
	}

	FLevelStart FMapBuilder::Build() const
	{
		FLevelStart Out;
		Out.Map.Width = W;
		Out.Map.Height = H;
		Out.Map.Cells = Cells;
		Out.Map.Name = Name;
		Out.PlayerX = PX;
		Out.PlayerY = PY;
		Out.PlayerAngle = PAng;
		Out.Mobjs = Mobjs;
		Out.BossExitGated = BossExitGated;
		return Out;
	}

	int32 HealthFor(EMobjKind K)
	{
		switch (K)
		{
		case EMobjKind::Imp:
			return C::HP_IMP;
		case EMobjKind::Demon:
			return C::HP_DEMON;
		case EMobjKind::Baron:
			return C::HP_BARON;
		case EMobjKind::Cacodemon:
			return C::HP_CACO;
		case EMobjKind::LostSoul:
			return C::HP_LOST;
		case EMobjKind::Zombieman:
			return C::HP_ZOMBIE;
		case EMobjKind::Shotgunner:
			return C::HP_SHOTG;
		case EMobjKind::Barrel:
			return C::HP_BARREL;
		default:
			return 1;
		}
	}

	float RadiusFor(EMobjKind K)
	{
		switch (K)
		{
		case EMobjKind::Cacodemon:
		case EMobjKind::Baron:
			return 0.42f;
		case EMobjKind::Demon:
			return 0.36f;
		case EMobjKind::Barrel:
			return 0.25f;
		case EMobjKind::ImpFireball:
		case EMobjKind::BaronBall:
		case EMobjKind::CacoBall:
		case EMobjKind::PlasmaProj:
		case EMobjKind::RocketProj:
		case EMobjKind::BfgProj:
			return 0.12f;
		default:
			return 0.3f;
		}
	}

	float HeightFor(EMobjKind K)
	{
		switch (K)
		{
		case EMobjKind::Baron:
			return 1.1f;
		case EMobjKind::Cacodemon:
			return 0.9f;
		case EMobjKind::Imp:
		case EMobjKind::Zombieman:
		case EMobjKind::Shotgunner:
			return 0.85f;
		case EMobjKind::Demon:
			return 0.7f;
		case EMobjKind::LostSoul:
			return 0.5f;
		case EMobjKind::Barrel:
			return 0.55f;
		case EMobjKind::PickupHealth:
		case EMobjKind::PickupArmor:
		case EMobjKind::PickupArmorBlue:
		case EMobjKind::PickupBullets:
		case EMobjKind::PickupShells:
		case EMobjKind::PickupRockets:
		case EMobjKind::PickupCells:
		case EMobjKind::PickupShotgun:
		case EMobjKind::PickupChaingun:
		case EMobjKind::PickupRocketLauncher:
		case EMobjKind::PickupPlasma:
		case EMobjKind::PickupBFG:
		case EMobjKind::KeyBlue:
		case EMobjKind::KeyYellow:
		case EMobjKind::KeyRed:
			return 0.35f;
		default:
			return 0.7f;
		}
	}

	// ───── Levels ─────

	FLevelStart BuildLevel(int32 Level)
	{
		switch (Level)
		{
		case 1:
			return Level1();
		case 2:
			return Level2();
		case 3:
			return Level3();
		case 4:
			return Level4();
		case 5:
			return Level5();
		case 6:
			return Level6();
		default:
			return Level1();
		}
	}

	const TCHAR* LevelName(int32 Level)
	{
		switch (Level)
		{
		case 1:
			return TEXT("E1M1: Hangar");
		case 2:
			return TEXT("E1M2: Toxin Refinery");
		case 3:
			return TEXT("E1M3: Phobos Lab");
		case 4:
			return TEXT("E1M4: Outpost");
		case 5:
			return TEXT("E1M5: Watchtower");
		case 6:
			return TEXT("E1M6: Skybridge");
		default:
			return TEXT("E1M1: Hangar");
		}
	}

	// E1M1 — "Hangar" — key-chain progression:
	//   entrance corridor → hub (yellow key here)
	//   yellow door east → red key
	//   red door west → blue key
	//   blue door north → boss room → walk onto blue exit pad to finish
	// Boss-gated: Victory only fires once Baron + Cacodemon are dead.
	FLevelStart Level1()
	{
		FMapBuilder B(48, 48, TEXT("E1M1: Hangar"));
		// outer arena
		B.Fill(1, 1, 46, 46, (uint8)F_CONCRETE, (uint8)F_CONCRETE, 200);
		B.Border((uint8)W_TECH_PANEL);

		// starting corridor (bottom)
		B.Fill(20, 38, 28, 46, (uint8)F_TILE, (uint8)F_CONCRETE, 220);
		B.Box(19, 37, 29, 47, (uint8)W_TECH_PANEL);
		B.Fill(20, 38, 28, 46, (uint8)F_TILE, (uint8)F_CONCRETE, 220);

		// hub (center)
		B.Fill(10, 18, 38, 36, (uint8)F_TILE, (uint8)F_CONCRETE, 200);
		B.Box(9, 17, 39, 37, (uint8)W_TECH_PANEL);
		B.Fill(10, 18, 38, 36, (uint8)F_TILE, (uint8)F_CONCRETE, 200);

		// door from corridor → hub
		B.Door(24, 37);

		// pillars in hub
		B.Wall(16, 24, (uint8)W_BRICK_RED);
		B.Wall(16, 30, (uint8)W_BRICK_RED);
		B.Wall(32, 24, (uint8)W_BRICK_RED);
		B.Wall(32, 30, (uint8)W_BRICK_RED);

		// west wing (left from player view) — locked behind RED door, holds blue key
		B.Fill(2, 20, 8, 34, (uint8)F_TILE, (uint8)F_CONCRETE, 180);
		B.Box(1, 19, 9, 35, (uint8)W_BRICK_GREY);
		B.Fill(2, 20, 8, 34, (uint8)F_TILE, (uint8)F_CONCRETE, 180);
		B.Door(9, 27, ECellKind::DoorRed);

		// east wing (right from player view) — locked behind YELLOW door, holds red key
		B.Fill(40, 20, 45, 34, (uint8)F_TILE, (uint8)F_CONCRETE, 160);
		B.Box(39, 19, 46, 35, (uint8)W_HELL_STONE);
		B.Fill(40, 20, 45, 34, (uint8)F_TILE, (uint8)F_CONCRETE, 160);
		B.Door(39, 27, ECellKind::DoorYellow);

		// north wing — BOSS ROOM, locked behind BLUE door.
		// Back wall (y=4..5) is paved with blue exit-pad tiles; stepping onto
		// any of them ends the level once the boss + helper are dead.
		B.Fill(18, 4, 30, 16, (uint8)F_TILE, (uint8)F_CONCRETE, 230);
		B.Box(17, 3, 31, 17, (uint8)W_BRICK_RED);
		B.Fill(18, 4, 30, 16, (uint8)F_TILE, (uint8)F_CONCRETE, 230);
		B.Door(24, 17, ECellKind::DoorBlue);
		for (int32 Ex = 20; Ex <= 28; ++Ex)
		{
			B.ExitPad(Ex, 4);
			B.ExitPad(Ex, 5);
		}
		// Mark the back wall behind the exit pads in blue brick so the player
		// knows where the level ends.
		for (int32 Wx = 19; Wx <= 29; ++Wx)
		{
			B.Wall(Wx, 3, (uint8)W_BRICK_BLUE);
		}

		// player start (corridor, facing north toward the hub door)
		B.PlayerStart(24.5f, 44.5f, -UE_PI / 2.f);

		// ── Pickups & keys ──
		// Hub welcome stash + the YELLOW key sits openly in the middle so the
		// player picks it up the moment they enter.
		B.Spawn(EMobjKind::PickupBullets, 24.5f, 41.5f);
		B.Spawn(EMobjKind::PickupHealth, 21.5f, 41.5f);
		B.Spawn(EMobjKind::PickupHealth, 27.5f, 41.5f);
		B.Spawn(EMobjKind::PickupArmor, 24.5f, 33.5f);
		B.Spawn(EMobjKind::KeyYellow, 24.5f, 27.5f);

		// East wing — RED key + ammo for the next leg.
		B.Spawn(EMobjKind::KeyRed, 43.5f, 27.5f);
		B.Spawn(EMobjKind::PickupBullets, 42.5f, 24.5f);
		B.Spawn(EMobjKind::PickupShells, 42.5f, 30.5f);

		// West wing — BLUE key + the shotgun (you'll want it for the boss).
		B.Spawn(EMobjKind::KeyBlue, 5.5f, 27.5f);
		B.Spawn(EMobjKind::PickupShotgun, 5.5f, 24.5f);
		B.Spawn(EMobjKind::PickupShells, 6.5f, 30.5f);

		// Hub barrels for fun chain explosions.
		B.Spawn(EMobjKind::Barrel, 20.5f, 26.5f);
		B.Spawn(EMobjKind::Barrel, 28.5f, 26.5f);
		B.Spawn(EMobjKind::Barrel, 24.5f, 22.5f);

		// ── Monsters ──
		// Hub patrol.
		B.Spawn(EMobjKind::Zombieman, 14.5f, 24.5f, 0.f);
		B.Spawn(EMobjKind::Zombieman, 34.5f, 30.5f, UE_PI);
		B.Spawn(EMobjKind::Zombieman, 24.5f, 22.5f, UE_PI / 2.f);
		// Wing dwellers.
		B.Spawn(EMobjKind::Imp, 4.5f, 24.5f, 0.f);
		B.Spawn(EMobjKind::Imp, 4.5f, 31.5f, 0.f);
		B.Spawn(EMobjKind::Imp, 43.5f, 22.5f, UE_PI);
		B.Spawn(EMobjKind::Imp, 43.5f, 32.5f, UE_PI);
		// Boss room — Baron (boss) + Cacodemon (helper). Both must die before
		// the exit pad becomes active. Two health pickups inside as a bonus.
		B.Spawn(EMobjKind::Baron, 24.5f, 10.5f, UE_PI / 2.f);
		B.Spawn(EMobjKind::Cacodemon, 20.5f, 8.5f, UE_PI / 2.f);
		B.Spawn(EMobjKind::PickupHealth, 19.5f, 14.5f);
		B.Spawn(EMobjKind::PickupArmor, 28.5f, 14.5f);

		B.BossExitGated = true;
		return B.Build();
	}

	// E1M2 — "Toxin Refinery" — bigger maze with nukage and demons
	FLevelStart Level2()
	{
		FMapBuilder B(48, 48, TEXT("E1M2: Toxin Refinery"));
		B.Fill(1, 1, 46, 46, (uint8)F_TILE, (uint8)F_CONCRETE, 180);
		B.Border((uint8)W_TECH_PANEL);

		// central courtyard
		B.Fill(16, 16, 32, 32, (uint8)F_TILE, (uint8)F_CONCRETE, 200);

		// nukage moat surrounding center
		B.Liquid(14, 22, 15, 26, (uint8)F_NUKAGE);
		B.Liquid(33, 22, 34, 26, (uint8)F_NUKAGE);
		B.Liquid(22, 14, 26, 15, (uint8)F_NUKAGE);
		B.Liquid(22, 33, 26, 34, (uint8)F_NUKAGE);

		// 4 corner rooms connected via doors
		B.Box(2, 2, 14, 14, (uint8)W_BRICK_GREY);
		B.Fill(3, 3, 13, 13, (uint8)F_TILE, (uint8)F_CONCRETE, 170);
		B.At(14, 8).Kind = ECellKind::Empty;
		B.Door(14, 8);

		B.Box(34, 2, 46, 14, (uint8)W_HELL_STONE);
		B.Fill(35, 3, 45, 13, (uint8)F_TILE, (uint8)F_CONCRETE, 150);
		B.At(34, 8).Kind = ECellKind::Empty;
		B.Door(34, 8, ECellKind::DoorRed);

		B.Box(2, 34, 14, 46, (uint8)W_WOOD);
		B.Fill(3, 35, 13, 45, (uint8)F_TILE, (uint8)F_CONCRETE, 200);
		B.At(14, 40).Kind = ECellKind::Empty;
		B.Door(14, 40);

		B.Box(34, 34, 46, 46, (uint8)W_MARBLE);
		B.Fill(35, 35, 45, 45, (uint8)F_TILE, (uint8)F_CONCRETE, 230);
		B.At(34, 40).Kind = ECellKind::Empty;
		B.Door(34, 40, ECellKind::DoorYellow);
		B.Exit(45, 40);

		// player start
		B.PlayerStart(24.5f, 24.5f, 0.f);

		// weapons
		B.Spawn(EMobjKind::PickupChaingun, 24.5f, 18.5f);
		B.Spawn(EMobjKind::PickupShotgun, 24.5f, 30.5f);

		// keys
		B.Spawn(EMobjKind::KeyRed, 8.5f, 8.5f);		// SW corner
		B.Spawn(EMobjKind::KeyYellow, 8.5f, 40.5f); // NW corner — wait actually y axis: top=0
		// ammo + health
		B.Spawn(EMobjKind::PickupBullets, 24.5f, 22.5f);
		B.Spawn(EMobjKind::PickupShells, 22.5f, 24.5f);
		B.Spawn(EMobjKind::PickupShells, 26.5f, 24.5f);
		B.Spawn(EMobjKind::PickupHealth, 20.5f, 28.5f);
		B.Spawn(EMobjKind::PickupHealth, 28.5f, 28.5f);
		B.Spawn(EMobjKind::PickupArmorBlue, 40.5f, 40.5f);

		// monsters
		B.Spawn(EMobjKind::Imp, 18.5f, 18.5f, 0.f);
		B.Spawn(EMobjKind::Imp, 30.5f, 18.5f, UE_PI);
		B.Spawn(EMobjKind::Imp, 18.5f, 30.5f, 0.f);
		B.Spawn(EMobjKind::Imp, 30.5f, 30.5f, UE_PI);
		B.Spawn(EMobjKind::Demon, 7.5f, 7.5f);
		B.Spawn(EMobjKind::Demon, 7.5f, 11.5f);
		B.Spawn(EMobjKind::Shotgunner, 41.5f, 7.5f, UE_PI);
		B.Spawn(EMobjKind::Shotgunner, 41.5f, 11.5f, UE_PI);
		B.Spawn(EMobjKind::Imp, 7.5f, 41.5f, 0.f);
		B.Spawn(EMobjKind::Demon, 41.5f, 41.5f, UE_PI);
		B.Spawn(EMobjKind::Cacodemon, 37.5f, 37.5f);
		B.Spawn(EMobjKind::Zombieman, 24.5f, 5.5f);
		B.Spawn(EMobjKind::Zombieman, 24.5f, 43.5f);
		B.Spawn(EMobjKind::Zombieman, 5.5f, 24.5f);
		B.Spawn(EMobjKind::Zombieman, 43.5f, 24.5f);
		B.Spawn(EMobjKind::Barrel, 22.5f, 22.5f);
		B.Spawn(EMobjKind::Barrel, 26.5f, 22.5f);
		B.Spawn(EMobjKind::Barrel, 22.5f, 26.5f);
		B.Spawn(EMobjKind::Barrel, 26.5f, 26.5f);

		return B.Build();
	}

	// E1M3 — "Phobos Lab" — open arena with high-tier weapons + Baron
	FLevelStart Level3()
	{
		FMapBuilder B(56, 56, TEXT("E1M3: Phobos Lab"));
		B.Fill(1, 1, 54, 54, (uint8)F_TILE, (uint8)F_CONCRETE, 200);
		B.Border((uint8)W_HELL_STONE);

		// center pit of blood
		B.Liquid(24, 24, 32, 32, (uint8)F_BLOOD);

		// 4 pillars of fire
		B.Wall(20, 20, (uint8)W_HELL_STONE);
		B.Wall(36, 20, (uint8)W_HELL_STONE);
		B.Wall(20, 36, (uint8)W_HELL_STONE);
		B.Wall(36, 36, (uint8)W_HELL_STONE);

		// weapon bunkers in 4 corners
		B.Box(4, 4, 12, 12, (uint8)W_TECH_PANEL);
		B.Fill(5, 5, 11, 11, (uint8)F_TILE, (uint8)F_CONCRETE, 220);
		B.At(12, 8).Kind = ECellKind::Empty;
		B.Box(44, 4, 52, 12, (uint8)W_TECH_PANEL);
		B.Fill(45, 5, 51, 11, (uint8)F_TILE, (uint8)F_CONCRETE, 220);
		B.At(44, 8).Kind = ECellKind::Empty;
		B.Box(4, 44, 12, 52, (uint8)W_TECH_PANEL);
		B.Fill(5, 45, 11, 51, (uint8)F_TILE, (uint8)F_CONCRETE, 220);
		B.At(12, 48).Kind = ECellKind::Empty;
		B.Box(44, 44, 52, 52, (uint8)W_TECH_PANEL);
		B.Fill(45, 45, 51, 51, (uint8)F_TILE, (uint8)F_CONCRETE, 220);
		B.At(44, 48).Kind = ECellKind::Empty;

		// exit teleporter pad
		B.Exit(28, 4);

		B.PlayerStart(28.5f, 50.5f, -UE_PI / 2.f);

		// weapons
		B.Spawn(EMobjKind::PickupShotgun, 8.5f, 8.5f);
		B.Spawn(EMobjKind::PickupChaingun, 48.5f, 8.5f);
		B.Spawn(EMobjKind::PickupRocketLauncher, 8.5f, 48.5f);
		B.Spawn(EMobjKind::PickupPlasma, 48.5f, 48.5f);
		B.Spawn(EMobjKind::PickupBFG, 28.5f, 28.5f);

		// ammo galore
		B.Spawn(EMobjKind::PickupShells, 9.5f, 7.5f);
		B.Spawn(EMobjKind::PickupShells, 9.5f, 9.5f);
		B.Spawn(EMobjKind::PickupBullets, 47.5f, 7.5f);
		B.Spawn(EMobjKind::PickupBullets, 47.5f, 9.5f);
		B.Spawn(EMobjKind::PickupRockets, 9.5f, 47.5f);
		B.Spawn(EMobjKind::PickupRockets, 9.5f, 49.5f);
		B.Spawn(EMobjKind::PickupCells, 47.5f, 47.5f);
		B.Spawn(EMobjKind::PickupCells, 47.5f, 49.5f);

		B.Spawn(EMobjKind::PickupArmorBlue, 28.5f, 36.5f);
		B.Spawn(EMobjKind::PickupHealth, 26.5f, 50.5f);
		B.Spawn(EMobjKind::PickupHealth, 30.5f, 50.5f);

		// demonic horde
		B.Spawn(EMobjKind::Imp, 14.5f, 14.5f);
		B.Spawn(EMobjKind::Imp, 42.5f, 14.5f);
		B.Spawn(EMobjKind::Imp, 14.5f, 42.5f);
		B.Spawn(EMobjKind::Imp, 42.5f, 42.5f);
		B.Spawn(EMobjKind::Demon, 22.5f, 16.5f);
		B.Spawn(EMobjKind::Demon, 34.5f, 16.5f);
		B.Spawn(EMobjKind::Demon, 22.5f, 40.5f);
		B.Spawn(EMobjKind::Demon, 34.5f, 40.5f);
		B.Spawn(EMobjKind::Cacodemon, 16.5f, 28.5f);
		B.Spawn(EMobjKind::Cacodemon, 40.5f, 28.5f);
		B.Spawn(EMobjKind::LostSoul, 28.5f, 14.5f);
		B.Spawn(EMobjKind::LostSoul, 28.5f, 42.5f);
		B.Spawn(EMobjKind::Shotgunner, 18.5f, 18.5f);
		B.Spawn(EMobjKind::Shotgunner, 38.5f, 18.5f);
		B.Spawn(EMobjKind::Baron, 28.5f, 8.5f, UE_PI / 2.f);

		return B.Build();
	}

	// E1M4 — "Outpost" — Phase 7 showcase: open courtyard with sky, stair
	// staircase, raised platform you must jump or climb onto, and a low tunnel
	// you must crouch through.
	FLevelStart Level4()
	{
		FMapBuilder B(40, 40, TEXT("E1M4: Outpost"));

		// Outdoor courtyard floor (grass under sky)
		B.Fill(2, 2, 37, 37, (uint8)F_GRASS, (uint8)F_GRASS, 230);
		B.Border((uint8)W_BRICK_GREY);
		B.Sky(2, 2, 37, 37);

		// Player starts in the south, looking north.
		B.PlayerStart(8.5f, 32.5f, -UE_PI / 2.f);

		// Stairs going up north toward a raised plateau (smaller rises so you can
		// walk up without jumping; total ~1.0 over 5 steps).
		B.Step(6, 24, 14, 24, 0.0f);
		B.Step(6, 23, 14, 23, 0.2f);
		B.Step(6, 22, 14, 22, 0.4f);
		B.Step(6, 21, 14, 21, 0.6f);
		B.Step(6, 20, 14, 20, 0.8f);
		// Plateau on top
		B.Step(4, 14, 16, 19, 1.0f);

		// Raised pillar of brick to the east (must jump to climb)
		B.Step(22, 30, 26, 32, 0.7f);

		// Low tunnel west (height 0.7, must crouch). Opens into a small room.
		B.Box(20, 16, 28, 22, (uint8)W_BRICK_GREY);
		B.Fill(21, 17, 27, 21, (uint8)F_TILE, (uint8)F_CONCRETE, 160);
		// Tunnel mouth (open the south wall)
		B.At(24, 22).Kind = ECellKind::Empty;
		B.At(24, 22).FloorTexIdx = (uint8)F_TILE;
		B.LowCeiling(24, 22, 24, 22, 0.7f);
		B.LowCeiling(21, 17, 27, 21, 1.4f);

		// A taller "lookout tower" in the NE — sky room with high parapet.
		B.Box(28, 4, 36, 12, (uint8)W_BRICK_RED);
		B.Fill(29, 5, 35, 11, (uint8)F_TILE, (uint8)F_TILE, 220);
		B.Sky(29, 5, 35, 11);
		// Doorway in
		B.At(32, 12).Kind = ECellKind::Empty;
		B.At(32, 12).FloorTexIdx = (uint8)F_TILE;

		// Exit on the plateau
		B.Exit(10, 16);

		// Some enemies and pickups
		// (enemies disabled for Phase 7 traversal testing)
		// B.Spawn(EMobjKind::Imp, 10.5f, 18.5f);
		// B.Spawn(EMobjKind::Demon, 24.5f, 31.5f);
		// B.Spawn(EMobjKind::Cacodemon, 32.5f, 8.5f);
		// B.Spawn(EMobjKind::Zombieman, 24.5f, 19.5f);
		// B.Spawn(EMobjKind::Shotgunner, 6.5f, 26.5f);

		B.Spawn(EMobjKind::PickupHealth, 8.5f, 28.5f);
		B.Spawn(EMobjKind::PickupShells, 12.5f, 16.5f);
		B.Spawn(EMobjKind::PickupShotgun, 24.5f, 31.5f);
		B.Spawn(EMobjKind::PickupArmor, 32.5f, 8.5f);
		B.Spawn(EMobjKind::PickupBullets, 24.5f, 19.5f);

		return B.Build();
	}

	// E1M5 — "Watchtower" — Phase 8 showcase: small ground-floor courtyard,
	// staircase up to a second-story balcony, and a guard room on top with a
	// small squad. Tight 28×28 layout so the action stays close.
	FLevelStart Level5()
	{
		FMapBuilder B(28, 28, TEXT("E1M5: Watchtower"));

		// Ground level: outdoor courtyard with sky.
		B.Fill(2, 2, 25, 25, (uint8)F_GRASS, (uint8)F_GRASS, 220);
		B.Border((uint8)W_BRICK_GREY);
		B.Sky(2, 2, 25, 25);

		// Player starts in the south, looking north toward the tower.
		B.PlayerStart(14.5f, 22.5f, -UE_PI / 2.f);

		// Guard room (second story): 8×5 cells at Z=2.0, real ceiling at Z=3.6.
		B.Fill(4, 3, 11, 7, (uint8)F_TILE, (uint8)F_TILE, 200);
		B.Step(4, 3, 11, 7, 2.0f);
		B.LowCeiling(4, 3, 11, 7, 3.6f);
		B.Box(3, 2, 12, 8, (uint8)W_BRICK_RED);

		// Open-sky balcony connecting stairs to the guard room.
		B.Fill(9, 8, 12, 11, (uint8)F_TILE, (uint8)F_TILE, 220);
		B.Step(9, 8, 12, 11, 2.0f);
		B.Sky(9, 8, 12, 11);

		// Doorway from balcony into guard room.
		B.At(10, 8).Kind = ECellKind::Empty;
		B.At(10, 8).FloorTexIdx = (uint8)F_TILE;
		B.At(11, 8).Kind = ECellKind::Empty;
		B.At(11, 8).FloorTexIdx = (uint8)F_TILE;
		B.Step(10, 8, 11, 8, 2.0f);
		B.LowCeiling(10, 8, 11, 8, 3.6f);

		// Staircase: 10 steps of 0.2u rising from south to north along x=10..12.
		B.Step(10, 21, 12, 21, 0.2f);
		B.Step(10, 20, 12, 20, 0.4f);
		B.Step(10, 19, 12, 19, 0.6f);
		B.Step(10, 18, 12, 18, 0.8f);
		B.Step(10, 17, 12, 17, 1.0f);
		B.Step(10, 16, 12, 16, 1.2f);
		B.Step(10, 15, 12, 15, 1.4f);
		B.Step(10, 14, 12, 14, 1.6f);
		B.Step(10, 13, 12, 13, 1.8f);
		B.Step(10, 12, 12, 12, 2.0f);

		// Cover pillar in the courtyard.
		B.Wall(20, 18, (uint8)W_BRICK_RED);

		// Exit pad inside the guard room.
		B.Exit(6, 4);

		// Pickups along the route.
		B.Spawn(EMobjKind::PickupShotgun, 14.5f, 17.5f);
		B.Spawn(EMobjKind::PickupShells, 11.5f, 14.5f);
		B.Spawn(EMobjKind::PickupHealth, 18.5f, 20.5f);
		B.Spawn(EMobjKind::PickupArmor, 8.5f, 5.5f);

		// Enemies — small, focused encounter.
		// Courtyard: 1 sniper.
		B.Spawn(EMobjKind::Zombieman, 22.5f, 12.5f, UE_PI);
		// Guard room squad: 1 shotgunner, 2 imps.
		B.Spawn(EMobjKind::Shotgunner, 6.5f, 5.5f, UE_PI / 2.f);
		B.Spawn(EMobjKind::Imp, 9.5f, 4.5f, UE_PI / 2.f);
		B.Spawn(EMobjKind::Imp, 5.5f, 6.5f, UE_PI / 2.f);

		return B.Build();
	}

	// E1M6 — "Skybridge" — Phase 9 showcase: a TRUE three-story building with
	// walkable floors stacked at the same XY footprint. Each story has the
	// same plan — back of the building hosts the upper slabs, front opens
	// through an atrium so the player can look up/down between floors.
	//   Story 1 (Z=0)        : ground hall, accessed via south doorway
	//   Story 2 (Z=3.2..3.5) : slab covering the back ⅔ of the footprint
	//   Story 3 (Z=6.4..6.7) : smaller slab covering the back ⅓
	// Access:
	//   Ground → 2nd  : exterior staircase on the EAST side (per-cell Step)
	//   2nd   → 3rd  : INTERNAL staircase made of stacked ExtraFloor slabs
	//                  rising from slab#1 top up to slab#2 top
	//   Plus: a basement pit at Z=-1.5 in the courtyard (no way out — death pit).
	// No exit trigger — this is the final level, fight your way through.
	FLevelStart Level6()
	{
		FMapBuilder B(32, 32, TEXT("E1M6: Skybridge"));

		// Outdoor courtyard with sky.
		B.Fill(1, 1, 30, 30, (uint8)F_GRASS, (uint8)F_GRASS, 220);
		B.Border((uint8)W_BRICK_GREY);
		B.Sky(1, 1, 30, 30);

		// Player starts in the south courtyard, looking north.
		B.PlayerStart(16.5f, 26.5f, -UE_PI / 2.f);

		// ── Building 12×12 at (10..21, 6..17) ─────────────────────────────────
		B.Fill(10, 6, 21, 17, (uint8)F_TILE, (uint8)F_TILE, 200);
		// Tall sector ceiling — the visible "ceilings" inside are the slab
		// bottoms at Z=3.2 (between floors 1 & 2) and Z=6.4 (between 2 & 3).
		B.LowCeiling(10, 6, 21, 17, 10.0f);
		B.Box(9, 5, 22, 18, (uint8)W_BRICK_RED);

		// South doorway into the ground hall.
		B.At(15, 18).Kind = ECellKind::Empty;
		B.At(15, 18).FloorTexIdx = (uint8)F_TILE;
		B.At(16, 18).Kind = ECellKind::Empty;
		B.At(16, 18).FloorTexIdx = (uint8)F_TILE;

		// 2nd-floor slab — back ⅔ of the footprint (y=6..14). Atrium open
		// above the south strip (y=15..17) so you can look up/down.
		B.Floor3D(10, 6, 21, 14, 3.2f, 3.5f,
				  /*SideTex=*/(uint8)W_BRICK_RED, /*TopTex=*/(uint8)F_TILE,
				  /*BottomTex=*/(uint8)F_TILE, /*Light=*/200);

		// 3rd-floor slab — WEST HALF only (x=10..13). The internal staircase
		// climbs through cells x=14..15, which must be CLEAR of slab#2 —
		// otherwise the slab body (Z=6.4..6.7) overlaps the player's head as
		// they climb past Z=5.5, and collision blocks them out.
		B.Floor3D(10, 6, 13, 10, 6.4f, 6.7f,
				  /*SideTex=*/(uint8)W_BRICK_RED, /*TopTex=*/(uint8)F_TILE,
				  /*BottomTex=*/(uint8)F_TILE, /*Light=*/200);

		// ── Exterior EAST staircase: ground → 2nd floor ───────────────────────
		// 2-cell-wide (x=23..24) so player has room to walk straight up. 8
		// treads at 0.4u (= STEP_HEIGHT) so player walks up without jumping.
		B.Step(23, 14, 24, 14, 0.4f);
		B.Step(23, 13, 24, 13, 0.8f);
		B.Step(23, 12, 24, 12, 1.2f);
		B.Step(23, 11, 24, 11, 1.6f);
		B.Step(23, 10, 24, 10, 2.0f);
		B.Step(23, 9, 24, 9, 2.4f);
		B.Step(23, 8, 24, 8, 2.8f);
		B.Step(23, 7, 24, 7, 3.2f);

		// East-wall opening at (22, 7) acts as a landing onto slab#1.
		// Cell sits at Z=3.2 — slab#1 top is Z=3.5, a 0.3u step westward.
		B.At(22, 7).Kind = ECellKind::Empty;
		B.At(22, 7).FloorTexIdx = (uint8)F_TILE;
		B.At(22, 7).FloorZ = 3.2f;
		B.At(22, 7).CeilingZ = 10.0f;

		// ── Internal staircase: 2nd → 3rd floor ──────────────────────────────
		// 2-cell-wide stacked ExtraFloor slabs at columns x=14..15, going north
		// (y=13 → y=7). Each slab sits on top of slab#1 (BottomZ=3.5) with
		// rising TopZ in 0.4u increments. Tread 7 (top Z=6.3) lets the player
		// make a final 0.4u climb west onto slab#2 (top Z=6.7).
		B.Floor3D(14, 13, 15, 13, 3.5f, 3.9f, (uint8)W_BRICK_RED, (uint8)F_TILE);
		B.Floor3D(14, 12, 15, 12, 3.5f, 4.3f, (uint8)W_BRICK_RED, (uint8)F_TILE);
		B.Floor3D(14, 11, 15, 11, 3.5f, 4.7f, (uint8)W_BRICK_RED, (uint8)F_TILE);
		B.Floor3D(14, 10, 15, 10, 3.5f, 5.1f, (uint8)W_BRICK_RED, (uint8)F_TILE);
		B.Floor3D(14, 9, 15, 9, 3.5f, 5.5f, (uint8)W_BRICK_RED, (uint8)F_TILE);
		B.Floor3D(14, 8, 15, 8, 3.5f, 5.9f, (uint8)W_BRICK_RED, (uint8)F_TILE);
		B.Floor3D(14, 7, 15, 7, 3.5f, 6.3f, (uint8)W_BRICK_RED, (uint8)F_TILE);

		// ── Basement pit in the courtyard (death pit — no way out) ────────────
		B.Fill(3, 13, 6, 16, (uint8)F_LAVA, (uint8)F_LAVA, 160);
		B.Step(3, 13, 6, 16, -1.5f);

		// ── Pickups ───────────────────────────────────────────────────────────
		B.Spawn(EMobjKind::PickupShotgun, 16.5f, 21.5f); // courtyard
		B.Spawn(EMobjKind::PickupArmor, 16.5f, 11.5f);	 // ground hall
		B.Spawn(EMobjKind::PickupHealth, 12.5f, 10.5f);	 // 2nd floor
		B.Spawn(EMobjKind::PickupShells, 18.5f, 12.5f);	 // 2nd floor
		B.Spawn(EMobjKind::PickupRockets, 11.5f, 7.5f);	 // 3rd floor (west)
		B.Spawn(EMobjKind::PickupCells, 12.5f, 8.5f);	 // 3rd floor (west)

		// ── Enemies, distributed across all three stories ─────────────────────
		B.Spawn(EMobjKind::Zombieman, 27.5f, 15.5f, UE_PI);
		B.Spawn(EMobjKind::Imp, 12.5f, 13.5f, -UE_PI / 2.f);
		B.Spawn(EMobjKind::Imp, 19.5f, 13.5f, -UE_PI / 2.f);
		B.Spawn(EMobjKind::Shotgunner, 16.5f, 16.5f, -UE_PI / 2.f);
		B.Spawn(EMobjKind::Imp, 12.5f, 9.5f);
		B.Spawn(EMobjKind::Shotgunner, 19.5f, 9.5f);
		B.Spawn(EMobjKind::Imp, 11.5f, 8.5f);

		// No exit trigger — final level.

		return B.Build();
	}
} // namespace RuiDoom
