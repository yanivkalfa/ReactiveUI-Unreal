// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Doom demo — GAME LOGIC implementation. See DoomGameLogic.h for the port pedigree.
// Section banners and function order mirror the C# original (GameLogic.uitkx) 1:1 so the
// port can be reviewed side-by-side. Private helpers are `static` (internal linkage) with
// prototypes up front; C# `ref`/`out` parameters are C++ references; C# structs passed by
// value stay by value where they are cheap (FMobj, FInputCmd) and become const& where a C#
// struct copy was only ever read through (FPlayerState, FSector, FCell — behaviorally
// identical, avoids deep-copying the contained arrays).
//
// RNG: every random draw goes through `St.Frand()` — the ONE deterministic LCG the family
// shares (inherited Godot fix; the C# original's AdvanceFace used UnityEngine.Random.value,
// which is why AdvanceFace here takes the game state like the Godot port does).

#include "Doom/DoomGameLogic.h"

#include "Doom/DoomMaps.h"
#include "Doom/DoomRaycast.h"
#include "Doom/DoomTextures.h"

#include <limits>

namespace RuiDoom
{
	static const float RUI_DOOM_INF = std::numeric_limits<float>::infinity();

	// ───── Private structs (C# nested structs; F-prefixed per the naming contract) ─────

	// Result of the DDA grid walk (CastRay).
	struct FRayHit
	{
		float Distance = 0.f;
		uint8 WallTexIdx = 0;
		float TexU = 0.f;
		uint8 Light = 0;
		bool HitVertical = false;
		bool IsSky = false;
	};

	// Result of a hitscan trace (TraceShot): whether it hit a monster before a wall,
	// which mobj, and the world-space impact point (for the tracer streak endpoint).
	struct FShotHit
	{
		bool HitMobj = false;
		int32 MobjIdx = 0;
		float X = 0.f, Y = 0.f;
	};

	// ───── Private prototypes (definitions follow in the C# original's order) ─────

	static void UpdatePlayer(FGameState& St, FInputCmd Input, float Dt);
	static void TryUse(FGameState& St);
	static void UpdateDoors(FGameState& St, float Dt);
	static bool ActorInCell(const FGameState& St, int32 Cx, int32 Cy);
	static void FireWeapon(FGameState& St);
	static void FireMelee(FGameState& St, int32 Dmg, float Range);
	static void FireHitscan(FGameState& St, int32 Dmg, float Spread);
	static void SpawnTracer(FGameState& St, const FPlayerState& P, float Ang, const FShotHit& Hit, float Spread);
	static void SpawnProjectile(FGameState& St, EMobjKind Kind, int32 OwnerId, int32 Damage, float Speed);
	static EAmmoType WeaponAmmoType(EWeaponType W);
	static float WeaponCooldown(EWeaponType W);
	static void UpdateMobj(FGameState& St, int32 Idx, float Dt);
	static void UpdateProjectile(FGameState& St, int32 Idx, float Dt);
	static void OnProjectileHit(FGameState& St, int32 Idx, float X, float Y, int32 HitMobjIdx);
	static void Splash(FGameState& St, float X, float Y, float Radius, int32 MaxDmg, int32 OwnerId);
	static void UpdateMonster(FGameState& St, int32 Idx, float Dt);
	static void DoAttack(FGameState& St, int32 Idx, float Dist, bool bMelee);
	static void SpawnEnemyProjectile(FGameState& St, int32 SrcIdx, EMobjKind Kind, int32 Damage, float Speed);
	static float MonsterSpeed(EMobjKind K);
	static void UpdatePickup(FGameState& St, int32 Idx);
	static bool TryGivePickup(FGameState& St, EMobjKind K);
	static int32 KillScore(EMobjKind K);
	static void Hurt(FGameState& St, FPlayerState& P, int32 Dmg, uint8 DirHint);
	static float StandingFloorIn(const FSector& Sec, float FootZ, float StepUp);
	static float CeilingAboveIn(const FSector& Sec, float FootZ, float ActorHeight);
	static void UpdatePlayerVertical(FGameState& St, FPlayerState& P, FInputCmd Input, float Dt);
	static void UpdateSectors(FGameState& St, float Dt);
	static void BuildColumnSector(FGameState& St, FColumnInfo& Col, TArray<Raycast::FWallHit>& Hits, float Ox, float Oy,
								  float Rdx, float Rdy, float AngCos, float ViewZ, float Horizon, float& DepthOut);
	static FRayHit CastRay(const FGameState& St, float Ox, float Oy, float Dx, float Dy);
	static uint8 LightFromDist(float Dist, uint8 CellLight);
	static bool HasLineOfSight(const FGameState& St, float Ax, float Ay, float Bx, float By);
	static bool HasLineOfSight3D(const FGameState& St, float Ax, float Ay, float Az, float Bx, float By, float Bz,
								 int32 SrcSector);
	static int32 FindMobjInCone(const FGameState& St, float Ox, float Oy, float Ang, float Range, float HalfAngle);
	static FShotHit TraceShot(const FGameState& St, float Ox, float Oy, float Ang);
	static void MoveActor(FGameState& St, float& X, float& Y, float Dx, float Dy, float Radius, bool bIsPlayer,
						  int32 SelfIdx = -1);
	static bool CollidesAt(const FGameState& St, float X, float Y, float R, bool bIsPlayer, int32 SelfIdx);
	static void CompactMobjs(FGameState& St);
	static void AdvanceFace(FGameState& St, float Dt);
	static void Message(FPlayerState& P, const TCHAR* Text, float Dur);

	// ───── Public API ─────

	FGameState NewGame(int32 Level, EDifficulty Diff)
	{
		FLevelStart Start = BuildLevel(Level);
		FGameState St;
		St.Level = Level;
		St.Difficulty = Diff;
		St.Map = MoveTemp(Start.Map);
		St.Mobjs.SetNum(C::MAX_MOBJS); // slots default to Id == 0 (free)
		St.MobjCount = 0;
		St.NextMobjId = 1;
		St.Frame.Columns.SetNum(C::VIEW_W);
		St.Frame.DepthBuffer.SetNum(C::VIEW_W);

		FPlayerState& Player = St.Player;
		Player.X = Start.PlayerX;
		Player.Y = Start.PlayerY;
		Player.Angle = Start.PlayerAngle;
		Player.Pitch = 0.f;
		Player.Health = C::START_HEALTH;
		Player.Armor = 0;
		Player.ArmorClass = 0;
		Player.Weapon = EWeaponType::Pistol;
		Player.Ammo = {50, 0, 0, 0};
		Player.OwnedWeapons = {true, true, false, false, false, false, false};
		Player.Keys = EKeyCard::None;
		Player.Alive = true;
		Player.FaceState = 1;
		Player.FaceTimer = 0.5f;
		Player.MessageText = TEXT("");

		St.RngSeed = 12345 + Level * 17;
		St.Tic = 0;
		St.Tracers.SetNum(C::MAX_TRACERS);
		// Types-contract init (DoomTypes.h): a zeroed ring slot would render as a live
		// streak at the origin for the first TRACER_LIFE_MS. Mark every slot dead.
		for (FTracer& T : St.Tracers)
		{
			T.AgeMs = C::TRACER_LIFE_MS;
		}
		St.TracerCount = 0;
		St.BossExitGated = Start.BossExitGated;

		// Phase 1: build the parallel sector model (no rendering use yet).
		St.SectorMap = FMapData::FromTiles(St.Map);
		St.SectorMap.PlayerStart = FVector2D(Start.PlayerX, Start.PlayerY);
		St.SectorMap.PlayerStartAngle = Start.PlayerAngle;
		St.SectorMap.PlayerStartSector = Raycast::PointInSectorFromHint(St.SectorMap, St.SectorMap.PlayerStart, -1);
		St.Player.ViewHeight = C::PLAYER_VIEW_HEIGHT;
		St.Player.SectorId = St.SectorMap.PlayerStartSector;
		for (const FMobj& M : Start.Mobjs)
		{
			AddMobj(St, M);
		}
		int32 KillTotal = 0;
		for (int32 i = 1; i < St.MobjCount + 1; i++)
		{
			if (IsMonster(St.Mobjs[i].Kind))
			{
				KillTotal++;
			}
		}
		St.KillTotal = KillTotal;
		CastFrame(St);
		return St;
	}

	void Tick(FGameState& St, float Dt, FInputCmd Input)
	{
		if (St.GameOver || St.Victory)
		{
			St.TimeAccum += Dt;
			// still advance face timer for fun
			AdvanceFace(St, Dt);
			CastFrame(St);
			return;
		}
		St.Tic++;
		St.TimeAccum += Dt;

		UpdatePlayer(St, Input, Dt);
		UpdateDoors(St, Dt);
		UpdateSectors(St, Dt);

		for (int32 i = 1; i <= St.MobjCount; i++)
		{
			if (St.Mobjs[i].Id == 0)
			{
				continue;
			}
			UpdateMobj(St, i, Dt);
		}

		CompactMobjs(St);
		AdvanceFace(St, Dt);

		// decay flashes
		if (St.Player.HurtFlash > 0)
		{
			St.Player.HurtFlash = FMath::Max(0.f, St.Player.HurtFlash - Dt * 2.f);
		}
		if (St.Player.PickupFlash > 0)
		{
			St.Player.PickupFlash = FMath::Max(0.f, St.Player.PickupFlash - Dt * 3.f);
		}
		if (St.Player.MuzzleFlash > 0)
		{
			St.Player.MuzzleFlash = FMath::Max(0.f, St.Player.MuzzleFlash - Dt);
		}
		if (St.Player.MessageTimer > 0)
		{
			St.Player.MessageTimer = FMath::Max(0.f, St.Player.MessageTimer - Dt);
		}

		// Phase 8: age hitscan tracers (ms units; capped at LIFE so dead slots
		// are skipped at render time and naturally overwritten by ring index).
		if (St.Tracers.Num() > 0)
		{
			float DtMs = Dt * 1000.f;
			for (int32 i = 0; i < St.Tracers.Num(); i++)
			{
				if (St.Tracers[i].AgeMs < C::TRACER_LIFE_MS)
				{
					St.Tracers[i].AgeMs = FMath::Min(C::TRACER_LIFE_MS, St.Tracers[i].AgeMs + DtMs);
				}
			}
		}

		CastFrame(St);
	}

	// ───── Player ─────

	static void UpdatePlayer(FGameState& St, FInputCmd Input, float Dt)
	{
		FPlayerState& P = St.Player;
		if (!P.Alive)
		{
			return;
		}

		// Mouse-look (ALWAYS ON)
		P.Angle += Input.YawDelta;
		P.Pitch -= Input.PitchDelta * C::MOUSE_PITCH_SENS;
		P.Pitch = FMath::Clamp(P.Pitch, -C::MAX_PITCH, C::MAX_PITCH);

		// Keyboard turn
		if (Input.TurnLeft)
		{
			P.Angle -= C::TURN_SPEED * Dt;
		}
		if (Input.TurnRight)
		{
			P.Angle += C::TURN_SPEED * Dt;
		}

		float Speed = C::MOVE_SPEED * (Input.Run ? C::RUN_MULT : 1.f);
		float Strafe = C::STRAFE_SPEED * (Input.Run ? C::RUN_MULT : 1.f);
		float FwdX = FMath::Cos(P.Angle), FwdY = FMath::Sin(P.Angle);
		float RgtX = -FwdY, RgtY = FwdX;

		float Dx = 0, Dy = 0;
		if (Input.Forward)
		{
			Dx += FwdX * Speed * Dt;
			Dy += FwdY * Speed * Dt;
		}
		if (Input.Back)
		{
			Dx -= FwdX * Speed * Dt;
			Dy -= FwdY * Speed * Dt;
		}
		if (Input.StrafeRight)
		{
			Dx += RgtX * Strafe * Dt;
			Dy += RgtY * Strafe * Dt;
		}
		if (Input.StrafeLeft)
		{
			Dx -= RgtX * Strafe * Dt;
			Dy -= RgtY * Strafe * Dt;
		}

		MoveActor(St, P.X, P.Y, Dx, Dy, C::PLAYER_RADIUS, /*bIsPlayer*/ true);

		// Phase 2: keep player.SectorId tracking the current sector for the
		// sector-based renderer/collision. Cheap hint-walk; falls back to brute
		// force if hint fails.
		if (St.SectorMap.IsValid())
		{
			P.SectorId = Raycast::PointInSectorFromHint(St.SectorMap, FVector2D(P.X, P.Y), P.SectorId);
		}

		// Phase 7: vertical movement — gravity, jump, crouch. Player feet (Z)
		// are anchored to the current sector floor unless airborne.
		UpdatePlayerVertical(St, P, Input, Dt);

		if (FMath::Abs(Dx) > 0.001f || FMath::Abs(Dy) > 0.001f)
		{
			P.BobT += Dt * 8.f;
		}

		// exit?
		int32 Gx = (int32)P.X, Gy = (int32)P.Y;
		const FCell& Cell = St.Map.AtSafe(Gx, Gy);
		if (Cell.Kind == ECellKind::Exit)
		{
			if (St.BossExitGated && AnyBossAlive(St))
			{
				if ((St.Tic % 60) == 0)
				{
					P.MessageText = TEXT("Kill the boss first.");
					P.MessageTimer = 1.5f;
				}
			}
			else
			{
				St.Victory = true;
				return;
			}
		}
		if (Cell.Kind == ECellKind::Liquid)
		{
			// damage 1hp/sec for nukage etc
			if ((St.Tic % 30) == 0)
			{
				Hurt(St, P, 1, 0);
			}
		}

		// weapon switch
		if (Input.WeaponSwitch != 0)
		{
			int32 Idx = Input.WeaponSwitch - 1;
			if (Idx >= 0 && Idx < P.OwnedWeapons.Num() && P.OwnedWeapons[Idx])
			{
				P.Weapon = (EWeaponType)Idx;
			}
		}

		// shooting
		if (P.ShootCooldown > 0)
		{
			P.ShootCooldown -= Dt;
		}
		if (Input.Attack && P.ShootCooldown <= 0)
		{
			FireWeapon(St);
		}

		// use key (open doors)
		if (Input.Use)
		{
			TryUse(St);
		}
	}

	static void TryUse(FGameState& St)
	{
		FPlayerState& P = St.Player;
		float FwdX = FMath::Cos(P.Angle), FwdY = FMath::Sin(P.Angle);
		for (float D = 0.4f; D <= 1.4f; D += 0.2f)
		{
			int32 Gx = (int32)(P.X + FwdX * D);
			int32 Gy = (int32)(P.Y + FwdY * D);
			if (Gx < 0 || Gx >= St.Map.Width || Gy < 0 || Gy >= St.Map.Height)
			{
				continue;
			}
			FCell& Cell = St.Map.Cells[Gy * St.Map.Width + Gx];
			if (Cell.Kind == ECellKind::Door)
			{
				if (Cell.DoorState < 200)
				{
					Cell.DoorTimer = 1;
				}
				return;
			}
			if (Cell.Kind == ECellKind::DoorBlue)
			{
				if (EnumHasAnyFlags(P.Keys, EKeyCard::Blue))
				{
					if (Cell.DoorState < 200)
					{
						Cell.DoorTimer = 1;
					}
				}
				else
				{
					Message(P, TEXT("You need a BLUE keycard!"), 2.f);
				}
				return;
			}
			if (Cell.Kind == ECellKind::DoorYellow)
			{
				if (EnumHasAnyFlags(P.Keys, EKeyCard::Yellow))
				{
					if (Cell.DoorState < 200)
					{
						Cell.DoorTimer = 1;
					}
				}
				else
				{
					Message(P, TEXT("You need a YELLOW keycard!"), 2.f);
				}
				return;
			}
			if (Cell.Kind == ECellKind::DoorRed)
			{
				if (EnumHasAnyFlags(P.Keys, EKeyCard::Red))
				{
					if (Cell.DoorState < 200)
					{
						Cell.DoorTimer = 1;
					}
				}
				else
				{
					Message(P, TEXT("You need a RED keycard!"), 2.f);
				}
				return;
			}
			if (Cell.Kind == ECellKind::Wall)
			{
				return;
			}
		}
	}

	static void UpdateDoors(FGameState& St, float Dt)
	{
		int32 N = St.Map.Cells.Num();
		int32 Delta = (int32)FMath::Clamp(Dt * 220.f, 1.f, 30.f);
		bool bHasSectorMap = St.SectorMap.IsValid() && St.SectorMap.CellToSector.Num() > 0;
		for (int32 i = 0; i < N; i++)
		{
			FCell& Cell = St.Map.Cells[i];
			if (Cell.Kind != ECellKind::Door && Cell.Kind != ECellKind::DoorBlue &&
				Cell.Kind != ECellKind::DoorYellow && Cell.Kind != ECellKind::DoorRed)
			{
				continue;
			}
			// DoorTimer == 1: opening. DoorTimer == 2: open & waiting. DoorTimer == 3: closing.
			if (Cell.DoorTimer == 1)
			{
				int32 S = Cell.DoorState + Delta;
				if (S >= 255)
				{
					Cell.DoorState = 255;
					Cell.DoorTimer = 2;
					// store wait countdown in the sector
					if (bHasSectorMap)
					{
						int32 Sid = St.SectorMap.CellToSector[i];
						if (Sid >= 0)
						{
							St.SectorMap.Sectors[Sid].DoorWaitTimer = C::DOOR_AUTO_CLOSE_DELAY;
						}
					}
				}
				else
				{
					Cell.DoorState = (uint8)S;
				}
			}
			else if (Cell.DoorTimer == 2)
			{
				// waiting open
				if (bHasSectorMap)
				{
					int32 Sid = St.SectorMap.CellToSector[i];
					if (Sid >= 0)
					{
						FSector& Sec = St.SectorMap.Sectors[Sid];
						Sec.DoorWaitTimer -= Dt;
						// Don't close on top of player or monsters: check the cell.
						int32 Cx = i % St.Map.Width, Cy = i / St.Map.Width;
						bool bBlocked = ActorInCell(St, Cx, Cy);
						if (Sec.DoorWaitTimer <= 0.f && !bBlocked)
						{
							Cell.DoorTimer = 3;
						}
						else if (bBlocked)
						{
							Sec.DoorWaitTimer = 1.f; // keep checking
						}
					}
				}
			}
			else if (Cell.DoorTimer == 3)
			{
				// closing
				int32 S = Cell.DoorState - Delta;
				if (S <= 0)
				{
					Cell.DoorState = 0;
					Cell.DoorTimer = 0;
				}
				else
				{
					Cell.DoorState = (uint8)S;
				}
				// Re-open immediately if blocked
				int32 Cx2 = i % St.Map.Width, Cy2 = i / St.Map.Width;
				if (ActorInCell(St, Cx2, Cy2))
				{
					Cell.DoorTimer = 1;
				}
			}
			// Phase 2/3: mirror cell DoorState onto the matching sector's CeilingZ.
			if (bHasSectorMap)
			{
				int32 SecId = St.SectorMap.CellToSector[i];
				if (SecId >= 0)
				{
					St.SectorMap.Sectors[SecId].CeilingZ = (Cell.DoorState / 255.f) * 1.5f;
				}
			}
		}
	}

	// True if the player or any live mobj's footprint overlaps tile (cx,cy).
	static bool ActorInCell(const FGameState& St, int32 Cx, int32 Cy)
	{
		const FPlayerState& P = St.Player;
		if (P.Alive && (int32)P.X == Cx && (int32)P.Y == Cy)
		{
			return true;
		}
		for (int32 j = 1; j <= St.MobjCount; j++)
		{
			const FMobj& M = St.Mobjs[j];
			if (M.Id == 0 || M.Health <= 0)
			{
				continue;
			}
			if (!IsMonster(M.Kind) && M.Kind != EMobjKind::Barrel)
			{
				continue;
			}
			if ((int32)M.X == Cx && (int32)M.Y == Cy)
			{
				return true;
			}
		}
		return false;
	}

	// ───── Weapons ─────

	static void FireWeapon(FGameState& St)
	{
		FPlayerState& P = St.Player;
		EWeaponType W = P.Weapon;
		EAmmoType Need = WeaponAmmoType(W);
		int32 AmmoIdx = (int32)Need;
		if (Need != EAmmoType::None && P.Ammo[AmmoIdx] <= 0)
		{
			// try fallback to pistol/fist
			if (P.OwnedWeapons[(int32)EWeaponType::Pistol] && P.Ammo[(int32)EAmmoType::Bullets] > 0)
			{
				P.Weapon = EWeaponType::Pistol;
			}
			else
			{
				P.Weapon = EWeaponType::Fist;
			}
			P.ShootCooldown = 0.2f;
			return;
		}
		if (Need != EAmmoType::None)
		{
			P.Ammo[AmmoIdx]--;
		}

		P.MuzzleFlash = 0.1f;
		P.ShootCooldown = WeaponCooldown(W);

		switch (W)
		{
		case EWeaponType::Fist:
			FireMelee(St, C::DMG_FIST, C::MELEE_RANGE);
			break;
		case EWeaponType::Pistol:
			FireHitscan(St, C::DMG_PISTOL, 0.04f);
			break;
		case EWeaponType::Shotgun:
			for (int32 i = 0; i < 7; i++)
			{
				FireHitscan(St, C::DMG_PELLET, 0.18f);
			}
			break;
		case EWeaponType::Chaingun:
			FireHitscan(St, C::DMG_CHAIN, 0.07f);
			break;
		case EWeaponType::RocketLauncher:
			SpawnProjectile(St, EMobjKind::RocketProj, -1, C::DMG_ROCKET, C::SPEED_ROCKET);
			break;
		case EWeaponType::PlasmaRifle:
			SpawnProjectile(St, EMobjKind::PlasmaProj, -1, C::DMG_PLASMA, C::SPEED_PLASMA);
			break;
		case EWeaponType::BFG9000:
			SpawnProjectile(St, EMobjKind::BfgProj, -1, C::DMG_BFG, C::SPEED_BFG);
			break;
		}
	}

	static void FireMelee(FGameState& St, int32 Dmg, float Range)
	{
		FPlayerState& P = St.Player;
		int32 HitIdx = FindMobjInCone(St, P.X, P.Y, P.Angle, Range, 0.4f);
		if (HitIdx > 0)
		{
			DamageMobj(St, HitIdx, Dmg, 0);
		}
	}

	static void FireHitscan(FGameState& St, int32 Dmg, float Spread)
	{
		FPlayerState& P = St.Player;
		float Ang = P.Angle + (St.Frand() - 0.5f) * Spread * 2.f;
		FShotHit Hit = TraceShot(St, P.X, P.Y, Ang);
		if (Hit.HitMobj && Hit.MobjIdx > 0)
		{
			DamageMobj(St, Hit.MobjIdx, Dmg, 0);
		}
		SpawnTracer(St, P, Ang, Hit, Spread);
	}

	// Phase 8: emit a hitscan tracer streak from muzzle to impact. Endpoints
	// are projected/clipped at render time. ColorIdx 1 = shotgun pellet (red
	// tinted), 0 = pistol/chaingun (yellow).
	static void SpawnTracer(FGameState& St, const FPlayerState& P, float Ang, const FShotHit& Hit, float Spread)
	{
		if (St.Tracers.Num() == 0)
		{
			return;
		}
		float PlaneScale = (C::VIEWPORT_H * 0.5f) / FMath::Tan(C::HALF_FOV);
		float PitchSlope = P.Pitch / PlaneScale;
		float ViewZ = P.Z + (P.ViewHeight <= 0.f ? 0.6f : P.ViewHeight);
		float FwdX = FMath::Cos(Ang), FwdY = FMath::Sin(Ang);
		float Ax = P.X + FwdX * C::MUZZLE_FORWARD;
		float Ay = P.Y + FwdY * C::MUZZLE_FORWARD;
		float Az = ViewZ - C::MUZZLE_BELOW_EYE;
		float Bx = Hit.X, By = Hit.Y;
		float Dist = FMath::Sqrt((Bx - P.X) * (Bx - P.X) + (By - P.Y) * (By - P.Y));
		float Bz = ViewZ + PitchSlope * Dist;
		int32 Slot = St.TracerCount % C::MAX_TRACERS;
		FTracer& T = St.Tracers[Slot];
		T.Ax = Ax;
		T.Ay = Ay;
		T.Az = Az;
		T.Bx = Bx;
		T.By = By;
		T.Bz = Bz;
		T.AgeMs = 0.f;
		T.ColorIdx = (uint8)(Spread > 0.10f ? 1 : 0);
		St.TracerCount++;
	}

	static void SpawnProjectile(FGameState& St, EMobjKind Kind, int32 OwnerId, int32 Damage, float Speed)
	{
		FPlayerState& P = St.Player;
		float FwdX = FMath::Cos(P.Angle), FwdY = FMath::Sin(P.Angle);
		// Phase 7: vertical aim — apply pitch slope to projectile motion.
		float PlaneScale = (C::VIEWPORT_H * 0.5f) / FMath::Tan(C::HALF_FOV);
		float PitchSlope = P.Pitch / PlaneScale;
		float ViewZ = P.Z + (P.ViewHeight <= 0.f ? 0.6f : P.ViewHeight);
		FMobj M;
		M.Kind = Kind;
		M.State = EAIState::Hunting;
		M.X = P.X + FwdX * 0.5f;
		M.Y = P.Y + FwdY * 0.5f;
		M.Z = ViewZ;
		M.MomX = FwdX * Speed;
		M.MomY = FwdY * Speed;
		M.ZVel = PitchSlope * Speed;
		M.Angle = P.Angle;
		M.Height = HeightFor(Kind);
		M.Health = 1;
		M.Damage = Damage;
		M.OwnerId = OwnerId;
		M.Radius = RadiusFor(Kind);
		AddMobj(St, M);
	}

	static EAmmoType WeaponAmmoType(EWeaponType W)
	{
		switch (W)
		{
		case EWeaponType::Pistol:
		case EWeaponType::Chaingun:
			return EAmmoType::Bullets;
		case EWeaponType::Shotgun:
			return EAmmoType::Shells;
		case EWeaponType::RocketLauncher:
			return EAmmoType::Rockets;
		case EWeaponType::PlasmaRifle:
		case EWeaponType::BFG9000:
			return EAmmoType::Cells;
		default:
			return EAmmoType::None;
		}
	}

	static float WeaponCooldown(EWeaponType W)
	{
		switch (W)
		{
		case EWeaponType::Fist:
			return C::COOLDOWN_FIST;
		case EWeaponType::Pistol:
			return C::COOLDOWN_PISTOL;
		case EWeaponType::Shotgun:
			return C::COOLDOWN_SHOTGUN;
		case EWeaponType::Chaingun:
			return C::COOLDOWN_CHAIN;
		case EWeaponType::RocketLauncher:
			return C::COOLDOWN_ROCKET;
		case EWeaponType::PlasmaRifle:
			return C::COOLDOWN_PLASMA;
		case EWeaponType::BFG9000:
			return C::COOLDOWN_BFG;
		default:
			return 0.3f;
		}
	}

	// ───── Mobj update ─────

	static void UpdateMobj(FGameState& St, int32 Idx, float Dt)
	{
		FMobj& M = St.Mobjs[Idx];
		M.StateTimer += Dt;
		if (M.AttackCooldown > 0)
		{
			M.AttackCooldown -= Dt;
		}

		// animation frame tick
		if (M.StateTimer > 0.15f)
		{
			M.AnimFrame++;
			M.StateTimer = 0;
		}

		if (IsProjectile(M.Kind))
		{
			UpdateProjectile(St, Idx, Dt);
			return;
		}
		if (IsMonster(M.Kind))
		{
			UpdateMonster(St, Idx, Dt);
			return;
		}
		if (IsPickup(M.Kind))
		{
			UpdatePickup(St, Idx);
			return;
		}

		if (M.Kind == EMobjKind::Explosion)
		{
			if (M.StateTimer > 0.4f || M.AnimFrame > 4)
			{
				M.Id = 0;
			}
			return;
		}
		// barrels just sit there
	}

	static void UpdateProjectile(FGameState& St, int32 Idx, float Dt)
	{
		FMobj& M = St.Mobjs[Idx];
		float Nx = M.X + M.MomX * Dt;
		float Ny = M.Y + M.MomY * Dt;
		float Nz = M.Z + M.ZVel * Dt;

		// Phase 9: Z-aware wall + slab collision. The old `BlocksMovement` was
		// a 2D-only check that let fireballs pass through stair risers, raised
		// floors, low ceilings, and ExtraFloor slabs. Treat the projectile as
		// an infinitesimal point: if any solid surface sits between (M.X,M.Y,M.Z)
		// and (Nx,Ny,Nz), it explodes.
		int32 Gx = (int32)Nx, Gy = (int32)Ny;
		const FCell& Cell = St.Map.AtSafe(Gx, Gy);
		if (Cell.Kind == ECellKind::Wall)
		{
			OnProjectileHit(St, Idx, M.X, M.Y, /*HitMobjIdx*/ 0);
			return;
		}
		if ((Cell.Kind == ECellKind::Door || Cell.Kind == ECellKind::DoorBlue || Cell.Kind == ECellKind::DoorYellow ||
			 Cell.Kind == ECellKind::DoorRed) &&
			Cell.DoorState < 200)
		{
			OnProjectileHit(St, Idx, M.X, M.Y, /*HitMobjIdx*/ 0);
			return;
		}
		float CellFloor = Cell.FloorZ;
		float CellCeil = Cell.IsSky ? 1e9f : (Cell.CeilingZ <= 0.f ? 1.5f : Cell.CeilingZ);
		if (Nz < CellFloor || Nz > CellCeil)
		{
			OnProjectileHit(St, Idx, Nx, Ny, /*HitMobjIdx*/ 0);
			return;
		}
		for (int32 k = 0; k < Cell.ExtraFloors.Num(); k++)
		{
			const FExtraFloor& Ef = Cell.ExtraFloors[k];
			if (!Ef.Solid)
			{
				continue;
			}
			if (Nz >= Ef.BottomZ - 0.001f && Nz <= Ef.TopZ + 0.001f)
			{
				OnProjectileHit(St, Idx, Nx, Ny, /*HitMobjIdx*/ 0);
				return;
			}
		}

		// mobj hit? (not owner, not other projectiles)
		int32 HitMobj = 0;
		for (int32 j = 1; j <= St.MobjCount; j++)
		{
			if (j == Idx)
			{
				continue;
			}
			FMobj& T = St.Mobjs[j];
			if (T.Id == 0 || T.Id == M.OwnerId)
			{
				continue;
			}
			if (IsProjectile(T.Kind) || T.Kind == EMobjKind::Explosion)
			{
				continue;
			}
			if (IsPickup(T.Kind))
			{
				continue;
			}
			if (T.Health <= 0 && T.Kind != EMobjKind::Barrel)
			{
				continue;
			}
			float Dx = T.X - Nx, Dy = T.Y - Ny;
			float R = M.Radius + T.Radius;
			if (Dx * Dx + Dy * Dy >= R * R)
			{
				continue;
			}
			// Vertical: projectile Z must overlap target [Z, Z+Height].
			float TBot = T.Z - 0.05f;
			float TTop = T.Z + (T.Height > 0.f ? T.Height : 0.7f) + 0.05f;
			if (Nz < TBot || Nz > TTop)
			{
				continue;
			}
			HitMobj = j;
			break;
		}
		// player hit (only enemy projectiles)
		if (M.OwnerId != -1 && HitMobj == 0)
		{
			float Dx = St.Player.X - Nx, Dy = St.Player.Y - Ny;
			float R = M.Radius + C::PLAYER_RADIUS;
			if (Dx * Dx + Dy * Dy < R * R)
			{
				float PBot = St.Player.Z - 0.05f;
				float PTop = St.Player.Z + (St.Player.ViewHeight <= 0.f ? 0.6f : St.Player.ViewHeight) + 0.4f;
				if (Nz >= PBot && Nz <= PTop)
				{
					Hurt(St, St.Player, M.Damage, 0);
					OnProjectileHit(St, Idx, Nx, Ny, 0);
					return;
				}
			}
		}
		if (HitMobj != 0)
		{
			OnProjectileHit(St, Idx, Nx, Ny, HitMobj);
			return;
		}

		M.X = Nx;
		M.Y = Ny;
		M.Z = Nz;
		if ((M.X < 0 || M.X >= St.Map.Width || M.Y < 0 || M.Y >= St.Map.Height))
		{
			M.Id = 0;
		}
	}

	static void OnProjectileHit(FGameState& St, int32 Idx, float X, float Y, int32 HitMobjIdx)
	{
		FMobj& M = St.Mobjs[Idx];
		if (HitMobjIdx > 0)
		{
			DamageMobj(St, HitMobjIdx, M.Damage, M.OwnerId);
		}
		if (M.Kind == EMobjKind::RocketProj || M.Kind == EMobjKind::BfgProj)
		{
			// splash
			float SplashRadius = M.Kind == EMobjKind::BfgProj ? 4.f : 2.5f;
			int32 SplashDmg = M.Kind == EMobjKind::BfgProj ? 200 : 60;
			Splash(St, X, Y, SplashRadius, SplashDmg, M.OwnerId);
			// explosion sprite
			FMobj Ex;
			Ex.Kind = EMobjKind::Explosion;
			Ex.X = X;
			Ex.Y = Y;
			Ex.Radius = 0.5f;
			Ex.Health = 1;
			AddMobj(St, Ex);
		}
		M.Id = 0;
	}

	static void Splash(FGameState& St, float X, float Y, float Radius, int32 MaxDmg, int32 OwnerId)
	{
		for (int32 j = 1; j <= St.MobjCount; j++)
		{
			FMobj& T = St.Mobjs[j];
			if (T.Id == 0 || IsProjectile(T.Kind) || IsPickup(T.Kind) || T.Kind == EMobjKind::Explosion)
			{
				continue;
			}
			float Dx = T.X - X, Dy = T.Y - Y;
			float D = FMath::Sqrt(Dx * Dx + Dy * Dy);
			if (D < Radius)
			{
				int32 Dmg = (int32)(MaxDmg * (1.f - D / Radius));
				DamageMobj(St, j, Dmg, OwnerId);
			}
		}
		float Pdx = St.Player.X - X, Pdy = St.Player.Y - Y;
		float Pd = FMath::Sqrt(Pdx * Pdx + Pdy * Pdy);
		if (Pd < Radius)
		{
			int32 Dmg = (int32)(MaxDmg * (1.f - Pd / Radius) * 0.6f);
			Hurt(St, St.Player, Dmg, 0);
		}
	}

	static void UpdateMonster(FGameState& St, int32 Idx, float Dt)
	{
		FMobj& M = St.Mobjs[Idx];
		if (M.State == EAIState::Dead)
		{
			return;
		}
		if (M.State == EAIState::Dying)
		{
			if (M.StateTimer > 1.0f || M.AnimFrame > 3)
			{
				M.State = EAIState::Dead;
				M.Kind = EMobjKind::Corpse;
			}
			return;
		}

		float Dx = St.Player.X - M.X, Dy = St.Player.Y - M.Y;
		float Dist = FMath::Sqrt(Dx * Dx + Dy * Dy);
		M.Angle = FMath::Atan2(Dy, Dx);

		// Phase 8: 3D LOS so risers, low ceilings, and second-story plateaus
		// actually block the monster's view (was tile-grid only before).
		float MonEyeZ = M.Z + (M.Height > 0.f ? M.Height * 0.65f : 0.5f);
		float PlyEyeZ = St.Player.Z + (St.Player.ViewHeight <= 0.f ? 0.6f : St.Player.ViewHeight);
		bool bSees = HasLineOfSight3D(St, M.X, M.Y, MonEyeZ, St.Player.X, St.Player.Y, PlyEyeZ, M.SectorId);

		if (M.State == EAIState::Idle)
		{
			if (Dist < C::SIGHT_RANGE && bSees)
			{
				M.State = EAIState::Hunting;
			}
			else
			{
				return;
			}
		}
		if (M.State == EAIState::Pain)
		{
			if (M.StateTimer > 0.3f)
			{
				M.State = EAIState::Hunting;
			}
			return;
		}

		// Hunting / Attacking
		float Speed = MonsterSpeed(M.Kind);
		float MeleeRange = (M.Kind == EMobjKind::Demon || M.Kind == EMobjKind::LostSoul) ? 1.6f : 1.2f;
		bool bCanMelee = Dist < MeleeRange;
		bool bCanRanged = Dist < 12.f && bSees;
		bool bRanged = M.Kind == EMobjKind::Imp || M.Kind == EMobjKind::Baron || M.Kind == EMobjKind::Cacodemon ||
					   M.Kind == EMobjKind::Shotgunner || M.Kind == EMobjKind::Zombieman;

		if (M.AttackCooldown <= 0 && (bCanMelee || (bRanged && bCanRanged)))
		{
			DoAttack(St, Idx, Dist, bCanMelee);
		}
		else
		{
			// chase
			float Mx = FMath::Cos(M.Angle) * Speed * Dt;
			float My = FMath::Sin(M.Angle) * Speed * Dt;
			MoveActor(St, M.X, M.Y, Mx, My, M.Radius, /*bIsPlayer*/ false, /*SelfIdx*/ Idx);
			// Phase 8: keep mob's sector + floor anchor in sync after movement
			// so 3D LOS and sprite occlusion stay correct on stairs/plateaus.
			if (St.SectorMap.IsValid())
			{
				int32 Sec = Raycast::PointInSectorFromHint(St.SectorMap, FVector2D(M.X, M.Y), M.SectorId);
				if (Sec >= 0)
				{
					M.SectorId = Sec;
					float Fz = StandingFloorIn(St.SectorMap.Sectors[Sec], M.Z, C::STEP_HEIGHT);
					// Snap to floor unless mob is intentionally floating.
					if (M.Kind != EMobjKind::Cacodemon && M.Kind != EMobjKind::LostSoul)
					{
						M.Z = Fz;
					}
				}
			}
		}
	}

	static void DoAttack(FGameState& St, int32 Idx, float Dist, bool bMelee)
	{
		FMobj& M = St.Mobjs[Idx];
		M.AttackCooldown = 1.4f + St.Frand() * 1.0f;

		switch (M.Kind)
		{
		case EMobjKind::Imp:
			if (bMelee)
			{
				Hurt(St, St.Player, C::DMG_IMP_MELEE, 0);
			}
			else
			{
				SpawnEnemyProjectile(St, Idx, EMobjKind::ImpFireball, C::DMG_IMP_BALL, C::SPEED_IMPBALL);
			}
			break;
		case EMobjKind::Demon:
			if (bMelee)
			{
				Hurt(St, St.Player, C::DMG_DEMON_BITE, 0);
			}
			else
			{
				M.AttackCooldown = 0.2f; // chase faster
			}
			break;
		case EMobjKind::Baron:
			if (bMelee)
			{
				Hurt(St, St.Player, C::DMG_BARON_CLAW, 0);
			}
			else
			{
				SpawnEnemyProjectile(St, Idx, EMobjKind::BaronBall, C::DMG_BARON_BALL, C::SPEED_BARONBALL);
			}
			break;
		case EMobjKind::Cacodemon:
			SpawnEnemyProjectile(St, Idx, EMobjKind::CacoBall, C::DMG_CACO_BALL, C::SPEED_CACOBALL);
			break;
		case EMobjKind::LostSoul:
			if (bMelee)
			{
				Hurt(St, St.Player, C::DMG_LOST_RAM, 0);
			}
			else
			{
				M.AttackCooldown = 0.2f;
			}
			break;
		case EMobjKind::Zombieman:
			Hurt(St, St.Player, C::DMG_ZOMBIE, 0);
			break;
		case EMobjKind::Shotgunner:
			for (int32 i = 0; i < 3; i++)
			{
				if (St.Frand() < 0.6f)
				{
					Hurt(St, St.Player, C::DMG_SHOTG, 0);
				}
			}
			break;
		default:
			break;
		}
	}

	static void SpawnEnemyProjectile(FGameState& St, int32 SrcIdx, EMobjKind Kind, int32 Damage, float Speed)
	{
		FMobj& Src = St.Mobjs[SrcIdx];
		float Ang = Src.Angle;
		// Phase 7: enemy aims at the player's torso. Compute Z slope from src eye
		// (3/4 of mob height) toward player center (Z + ViewHeight ≈ feet+0.6).
		float SrcEyeZ = Src.Z + (Src.Height > 0.f ? Src.Height * 0.75f : 0.6f);
		float PlyCenterZ = St.Player.Z + (St.Player.ViewHeight <= 0.f ? 0.6f : St.Player.ViewHeight) - 0.1f;
		float Dx = St.Player.X - Src.X, Dy = St.Player.Y - Src.Y;
		float Horiz = FMath::Sqrt(Dx * Dx + Dy * Dy);
		float ZSlope = Horiz > 0.01f ? (PlyCenterZ - SrcEyeZ) / Horiz : 0.f;
		FMobj M;
		M.Kind = Kind;
		M.X = Src.X + FMath::Cos(Ang) * 0.5f;
		M.Y = Src.Y + FMath::Sin(Ang) * 0.5f;
		M.Z = SrcEyeZ;
		M.MomX = FMath::Cos(Ang) * Speed;
		M.MomY = FMath::Sin(Ang) * Speed;
		M.ZVel = ZSlope * Speed;
		M.Angle = Ang;
		M.Health = 1;
		M.Height = HeightFor(Kind);
		M.Damage = Damage;
		M.OwnerId = Src.Id; // monster id, not -1
		M.Radius = RadiusFor(Kind);
		AddMobj(St, M);
	}

	static float MonsterSpeed(EMobjKind K)
	{
		switch (K)
		{
		case EMobjKind::Imp:
			return C::SPEED_IMP;
		case EMobjKind::Demon:
			return C::SPEED_DEMON;
		case EMobjKind::Baron:
			return C::SPEED_BARON;
		case EMobjKind::Cacodemon:
			return C::SPEED_CACO;
		case EMobjKind::LostSoul:
			return C::SPEED_LOST;
		case EMobjKind::Zombieman:
			return C::SPEED_ZOMBIE;
		case EMobjKind::Shotgunner:
			return C::SPEED_SHOTG;
		default:
			return 1.0f;
		}
	}

	static void UpdatePickup(FGameState& St, int32 Idx)
	{
		FMobj& M = St.Mobjs[Idx];
		if (M.Collected)
		{
			M.Id = 0;
			return;
		}
		float Dx = St.Player.X - M.X, Dy = St.Player.Y - M.Y;
		if (Dx * Dx + Dy * Dy > 0.5f * 0.5f)
		{
			return;
		}
		if (TryGivePickup(St, M.Kind))
		{
			M.Collected = true;
			St.Player.PickupFlash = 0.6f;
		}
	}

	static bool TryGivePickup(FGameState& St, EMobjKind K)
	{
		FPlayerState& P = St.Player;
		switch (K)
		{
		case EMobjKind::PickupHealth:
			if (P.Health >= 100)
			{
				return false;
			}
			P.Health = FMath::Min(100, P.Health + 25);
			Message(P, TEXT("Picked up a stimpack."), 1.5f);
			return true;
		case EMobjKind::PickupArmor:
			P.Armor = FMath::Max(P.Armor, 100);
			P.ArmorClass = (uint8)FMath::Max<int32>(P.ArmorClass, 1);
			Message(P, TEXT("Picked up green armor."), 1.5f);
			return true;
		case EMobjKind::PickupArmorBlue:
			P.Armor = FMath::Max(P.Armor, 200);
			P.ArmorClass = 2;
			Message(P, TEXT("Picked up MEGA ARMOR!"), 2.f);
			return true;
		case EMobjKind::PickupBullets:
			if (P.Ammo[0] >= C::MAX_BULLETS)
			{
				return false;
			}
			P.Ammo[0] = FMath::Min(C::MAX_BULLETS, P.Ammo[0] + 20);
			Message(P, TEXT("Picked up a clip."), 1.5f);
			return true;
		case EMobjKind::PickupShells:
			if (P.Ammo[1] >= C::MAX_SHELLS)
			{
				return false;
			}
			P.Ammo[1] = FMath::Min(C::MAX_SHELLS, P.Ammo[1] + 8);
			Message(P, TEXT("Picked up shotgun shells."), 1.5f);
			return true;
		case EMobjKind::PickupRockets:
			if (P.Ammo[2] >= C::MAX_ROCKETS)
			{
				return false;
			}
			P.Ammo[2] = FMath::Min(C::MAX_ROCKETS, P.Ammo[2] + 5);
			Message(P, TEXT("Picked up rockets."), 1.5f);
			return true;
		case EMobjKind::PickupCells:
			if (P.Ammo[3] >= C::MAX_CELLS)
			{
				return false;
			}
			P.Ammo[3] = FMath::Min(C::MAX_CELLS, P.Ammo[3] + 40);
			Message(P, TEXT("Picked up an energy cell."), 1.5f);
			return true;
		case EMobjKind::PickupShotgun:
		{
			bool bHadShotgun = P.OwnedWeapons[(int32)EWeaponType::Shotgun];
			P.OwnedWeapons[(int32)EWeaponType::Shotgun] = true;
			P.Ammo[1] = FMath::Min(C::MAX_SHELLS, P.Ammo[1] + 8);
			if (!bHadShotgun)
			{
				P.Weapon = EWeaponType::Shotgun;
			}
			Message(P, TEXT("You got the SHOTGUN!"), 2.f);
			return true;
		}
		case EMobjKind::PickupChaingun:
		{
			bool bHadChain = P.OwnedWeapons[(int32)EWeaponType::Chaingun];
			P.OwnedWeapons[(int32)EWeaponType::Chaingun] = true;
			P.Ammo[0] = FMath::Min(C::MAX_BULLETS, P.Ammo[0] + 20);
			if (!bHadChain)
			{
				P.Weapon = EWeaponType::Chaingun;
			}
			Message(P, TEXT("You got the CHAINGUN!"), 2.f);
			return true;
		}
		case EMobjKind::PickupRocketLauncher:
		{
			bool bHadRL = P.OwnedWeapons[(int32)EWeaponType::RocketLauncher];
			P.OwnedWeapons[(int32)EWeaponType::RocketLauncher] = true;
			P.Ammo[2] = FMath::Min(C::MAX_ROCKETS, P.Ammo[2] + 5);
			if (!bHadRL)
			{
				P.Weapon = EWeaponType::RocketLauncher;
			}
			Message(P, TEXT("You got the ROCKET LAUNCHER!"), 2.f);
			return true;
		}
		case EMobjKind::PickupPlasma:
		{
			bool bHadPL = P.OwnedWeapons[(int32)EWeaponType::PlasmaRifle];
			P.OwnedWeapons[(int32)EWeaponType::PlasmaRifle] = true;
			P.Ammo[3] = FMath::Min(C::MAX_CELLS, P.Ammo[3] + 40);
			if (!bHadPL)
			{
				P.Weapon = EWeaponType::PlasmaRifle;
			}
			Message(P, TEXT("You got the PLASMA RIFLE!"), 2.f);
			return true;
		}
		case EMobjKind::PickupBFG:
		{
			bool bHadBFG = P.OwnedWeapons[(int32)EWeaponType::BFG9000];
			P.OwnedWeapons[(int32)EWeaponType::BFG9000] = true;
			P.Ammo[3] = FMath::Min(C::MAX_CELLS, P.Ammo[3] + 40);
			if (!bHadBFG)
			{
				P.Weapon = EWeaponType::BFG9000;
			}
			Message(P, TEXT("YOU GOT THE BFG9000!  OH YES!"), 3.f);
			return true;
		}
		case EMobjKind::KeyBlue:
			P.Keys |= EKeyCard::Blue;
			Message(P, TEXT("Picked up a BLUE keycard."), 2.f);
			return true;
		case EMobjKind::KeyYellow:
			P.Keys |= EKeyCard::Yellow;
			Message(P, TEXT("Picked up a YELLOW keycard."), 2.f);
			return true;
		case EMobjKind::KeyRed:
			P.Keys |= EKeyCard::Red;
			Message(P, TEXT("Picked up a RED keycard."), 2.f);
			return true;
		default:
			break;
		}
		return false;
	}

	// ───── Damage ─────

	void DamageMobj(FGameState& St, int32 Idx, int32 Dmg, int32 SourceId)
	{
		FMobj& M = St.Mobjs[Idx];
		if (M.Health <= 0)
		{
			return;
		}
		M.Health -= Dmg;
		if (M.Kind == EMobjKind::Barrel && M.Health <= 0)
		{
			// explode
			Splash(St, M.X, M.Y, 2.2f, C::DMG_BARREL, M.OwnerId);
			FMobj Ex;
			Ex.Kind = EMobjKind::Explosion;
			Ex.X = M.X;
			Ex.Y = M.Y;
			Ex.Radius = 0.5f;
			Ex.Health = 1;
			AddMobj(St, Ex);
			M.Id = 0;
			St.Score += C::SCORE_BARREL;
			return;
		}
		if (IsMonster(M.Kind))
		{
			if (M.Health <= 0)
			{
				M.State = EAIState::Dying;
				M.StateTimer = 0;
				M.AnimFrame = 0;
				St.KillCount++;
				St.Score += KillScore(M.Kind);
				if (St.KillCount >= St.KillTotal && St.KillTotal > 0)
				{
					// optional: flag clearance, but no-op
				}
			}
			else
			{
				if (St.Frand() < 0.3f)
				{
					M.State = EAIState::Pain;
					M.StateTimer = 0;
				}
				M.State = M.State == EAIState::Pain ? EAIState::Pain : EAIState::Hunting;
			}
		}
	}

	static int32 KillScore(EMobjKind K)
	{
		switch (K)
		{
		case EMobjKind::Imp:
			return C::SCORE_IMP;
		case EMobjKind::Demon:
			return C::SCORE_DEMON;
		case EMobjKind::Baron:
			return C::SCORE_BARON;
		case EMobjKind::Cacodemon:
			return C::SCORE_CACO;
		case EMobjKind::LostSoul:
			return C::SCORE_LOST;
		case EMobjKind::Zombieman:
			return C::SCORE_ZOMBIE;
		case EMobjKind::Shotgunner:
			return C::SCORE_SHOTG;
		default:
			return 0;
		}
	}

	static void Hurt(FGameState& St, FPlayerState& P, int32 Dmg, uint8 DirHint)
	{
		if (!P.Alive)
		{
			return;
		}
		int32 Diff = (int32)St.Difficulty;
		if (Diff == 0)
		{
			Dmg = (int32)(Dmg * 0.6f);
		}
		else if (Diff == 2)
		{
			Dmg = (int32)(Dmg * 1.5f);
		}
		if (Dmg < 1)
		{
			Dmg = 1;
		}

		if (P.ArmorClass > 0 && P.Armor > 0)
		{
			float Frac = P.ArmorClass == 2 ? 0.5f : 0.33f;
			int32 Absorb = FMath::Min(P.Armor, (int32)(Dmg * Frac));
			P.Armor -= Absorb;
			Dmg -= Absorb;
			if (P.Armor <= 0)
			{
				P.ArmorClass = 0;
			}
		}
		P.Health -= Dmg;
		P.HurtFlash = FMath::Min(1.f, P.HurtFlash + Dmg / 60.f);
		P.LastDamageDir = DirHint;
		P.FaceState = 6;
		P.FaceTimer = 0.5f; // hurt face
		if (P.Health <= 0)
		{
			P.Health = 0;
			P.Alive = false;
			P.FaceState = 7;
			St.GameOver = true;
		}
	}

	// Phase 9: highest standable surface within `StepUp` of the actor's feet,
	// considering the sector floor and any solid ExtraFloor.TopZ. The actor
	// can stand on whichever is highest yet still reachable by a single step.
	static float StandingFloorIn(const FSector& Sec, float FootZ, float StepUp)
	{
		float Best = Sec.FloorZ;
		for (int32 i = 0; i < Sec.ExtraFloors.Num(); i++)
		{
			const FExtraFloor& Ef = Sec.ExtraFloors[i];
			if (!Ef.Solid)
			{
				continue;
			}
			if (Ef.TopZ <= FootZ + StepUp + 0.001f && Ef.TopZ > Best)
			{
				Best = Ef.TopZ;
			}
		}
		return Best;
	}

	// Phase 9: lowest blocking surface above the actor's head. Sector ceiling
	// by default, or the bottom of any ExtraFloor that sits above the head
	// (so a basement-dweller hits the courtyard slab as a low ceiling).
	static float CeilingAboveIn(const FSector& Sec, float FootZ, float ActorHeight)
	{
		float HeadZ = FootZ + ActorHeight;
		float Ceil = Sec.IsSky ? 1e9f : Sec.CeilingZ;
		for (int32 i = 0; i < Sec.ExtraFloors.Num(); i++)
		{
			const FExtraFloor& Ef = Sec.ExtraFloors[i];
			if (!Ef.Solid)
			{
				continue;
			}
			if (Ef.BottomZ >= HeadZ - 0.001f && Ef.BottomZ < Ceil)
			{
				Ceil = Ef.BottomZ;
			}
		}
		return Ceil;
	}

	// Phase 7: vertical physics for the player. Tracks Z (feet height) against
	// the floor of the player's current sector. Jump applies upward velocity
	// when on the ground; crouch lowers the view height temporarily.
	static void UpdatePlayerVertical(FGameState& St, FPlayerState& P, FInputCmd Input, float Dt)
	{
		float FloorZ = 0.f;
		float CeilZ = 1.f;
		if (St.SectorMap.IsValid() && P.SectorId >= 0)
		{
			const FSector& Sec = St.SectorMap.Sectors[P.SectorId];
			FloorZ = StandingFloorIn(Sec, P.Z, C::STEP_HEIGHT);
			CeilZ = CeilingAboveIn(Sec, P.Z, C::PLAYER_HEIGHT);
		}
		bool bOnGround = P.Z <= FloorZ + 0.001f && P.ZVel <= 0.f;
		// Edge-detect: only jump on the rising edge of the Jump key, not while held.
		bool bJumpEdge = Input.Jump && !P.JumpHeldPrev;
		P.JumpHeldPrev = Input.Jump;
		if (bOnGround && bJumpEdge)
		{
			P.ZVel = C::JUMP_VELOCITY;
			bOnGround = false;
		}
		if (!bOnGround)
		{
			P.ZVel -= C::GRAVITY * Dt;
			P.Z += P.ZVel * Dt;
			if (P.Z <= FloorZ)
			{
				P.Z = FloorZ;
				P.ZVel = 0.f;
			}
			// Bonk into ceiling
			if (P.Z + C::PLAYER_HEIGHT > CeilZ)
			{
				P.Z = CeilZ - C::PLAYER_HEIGHT;
				if (P.ZVel > 0)
				{
					P.ZVel = 0;
				}
			}
		}
		else
		{
			P.Z = FloorZ;
		}
		// Crouch lowers view height (smooth)
		float TargetView = Input.Crouch ? C::CROUCH_HEIGHT : C::PLAYER_VIEW_HEIGHT;
		P.ViewHeight = FMath::Lerp(P.ViewHeight, TargetView, FMath::Min(1.f, Dt * 8.f));
	}

	// Phase 4: animate sector light levels per SectorSpecial. Each sector keeps
	// its base Light from FromTiles; we modulate it into a runtime override
	// stored back in Sector.Light so the renderer picks it up automatically.
	// The original notes it can't recover a true per-sector base, so it clamps
	// to a known base of 200 (the FromTiles default) — faithfully preserved.
	static void UpdateSectors(FGameState& St, float Dt)
	{
		if (!St.SectorMap.IsValid())
		{
			return;
		}
		TArray<FSector>& Sectors = St.SectorMap.Sectors;
		int32 Tic = St.Tic;
		for (int32 i = 0; i < Sectors.Num(); i++)
		{
			ESectorSpecial Sp = Sectors[i].Special;
			if (Sp == ESectorSpecial::None)
			{
				continue;
			}
			// (The original reads `byte baseLight = sectors[i].Light;` here and never
			// uses it — omitted so warnings-as-errors builds stay clean.)
			uint8 BaseL = 200;
			switch (Sp)
			{
			case ESectorSpecial::LightFlicker:
			{
				int32 Seed = (Tic + i * 7) & 31;
				// 7/32 frames dim; rest bright
				Sectors[i].Light = Seed < 7 ? (uint8)(BaseL / 3) : BaseL;
				break;
			}
			case ESectorSpecial::LightBlink:
			{
				int32 Phase = (Tic + i * 11) & 63;
				Sectors[i].Light = Phase < 8 ? (uint8)(BaseL / 3) : BaseL;
				break;
			}
			case ESectorSpecial::LightGlow:
			{
				// smooth sin wave 0.5..1.0 of base
				float T = (Tic + i * 13) * 0.05f;
				float W = 0.5f + 0.5f * FMath::Sin(T);
				Sectors[i].Light = (uint8)FMath::Clamp(BaseL * (0.5f + 0.5f * W), 30.f, 255.f);
				break;
			}
			default:
				break;
			}
		}
	}

	// (USE_SECTOR_RAYCAST lives in the header — public in the original.)

	void CastFrame(FGameState& St)
	{
		const FPlayerState& P = St.Player;
		TArray<FColumnInfo>& Cols = St.Frame.Columns;
		TArray<float>& Depth = St.Frame.DepthBuffer;
		bool bUseSector = USE_SECTOR_RAYCAST && St.SectorMap.IsValid() && P.SectorId >= 0;
		// Phase 7: real eye height. Walls reproject against this so jump/crouch
		// actually CHANGES geometry (walls get shorter, you see more floor) rather
		// than simply tilting the horizon. Sky/floor bands stay anchored to the
		// pitch-only horizon.
		float ViewZ = P.Z + (P.ViewHeight <= 0.f ? 0.6f : P.ViewHeight);
		float Horizon = C::VIEWPORT_H * 0.5f + P.Pitch;
		St.Player.ViewShiftPx = 0.f;

		// GO-03 pooling (inherited from the Godot port): rewind the per-frame
		// WallSeg/FloorBand/CeilingBand allocator before rebuilding this frame's
		// columns — every Take* below reuses last frame's records instead of
		// heap-allocating fresh ones. Pointers stored in the columns die here.
		St.Frame.ResetPools();

		// One hits array reused across all 160 rays (Raycast::Cast rewinds it each
		// call) — the GO-04 pool discipline from DoomRaycast.h.
		TArray<Raycast::FWallHit> Hits;

		for (int32 i = 0; i < C::VIEW_W; i++)
		{
			float CameraX = 2.f * i / (float)C::VIEW_W - 1.f; // -1..1
			float RayAng = P.Angle + FMath::Atan(CameraX * FMath::Tan(C::HALF_FOV));
			float Rdx = FMath::Cos(RayAng), Rdy = FMath::Sin(RayAng);
			float AngCos = FMath::Cos(RayAng - P.Angle);

			if (bUseSector)
			{
				float PerpOut;
				BuildColumnSector(St, Cols[i], Hits, P.X, P.Y, Rdx, Rdy, AngCos, ViewZ, Horizon, PerpOut);
				Depth[i] = PerpOut;
			}
			else
			{
				FRayHit Hit = CastRay(St, P.X, P.Y, Rdx, Rdy);
				float Perp = Hit.Distance * AngCos;
				if (Perp < 0.001f)
				{
					Perp = 0.001f;
				}
				Depth[i] = Perp;
				float WallH = (C::VIEWPORT_H / Perp);
				float Top = Horizon - WallH * 0.5f;
				float Bot = Horizon + WallH * 0.5f;
				uint8 Light = LightFromDist(Perp, Hit.Light);
				// In-place equivalent of the original's fresh `new ColumnInfo { Main = … }`:
				// reset every field, then fill Main.
				FColumnInfo& Col = Cols[i];
				Col.Extras.Reset();
				Col.FloorBands.Reset();
				Col.CeilingBands.Reset();
				Col.Main = FWallSeg();
				Col.Main.TopPx = Top;
				Col.Main.BotPx = Bot;
				Col.Main.Distance = Perp;
				Col.Main.WallTexIdx = Hit.WallTexIdx;
				Col.Main.TexU = Hit.TexU;
				Col.Main.LightLevel = Light;
				Col.Main.HitVertical = Hit.HitVertical;
				Col.Main.IsSky = Hit.IsSky;
				Col.FrontFloorZ = 0.f;
				Col.FloorOccluderDist = 0.f;
				Col.FloorOccluderZ = 0.f;
				Col.CeilingOccluderDist = 0.f;
				Col.CeilingOccluderZ = 0.f;
			}
		}
	}

	// Phase 3 + Plan C: portal-walking column build with Doom-style vertical
	// occlusion clipping. Maintains a per-ray (WinTop, WinBot) screen-Y window
	// tightened by every front floor / front ceiling / portal wall surface
	// crossed. All emitted geometry is pre-clipped to that window — so within
	// a single column nothing ever overlaps in Y, and the renderer's paint
	// order becomes correctness-irrelevant.
	//
	// Why this beats clamp-and-sort hacks:
	//  - Stairs DOWN: the lower step's floor band is naturally clipped to the
	//    upper step's silhouette (WinBot = upperFloor.yFar). No phantom
	//    cliff wall has to be invented — the cliff face is geometrically
	//    occluded by the upper floor surface from the player's POV, so we
	//    don't emit it.
	//  - Step UP: emits a real lower wall (riser) clipped to the window, then
	//    pushes WinBot up. Subsequent floor bands beyond the riser only paint
	//    in the still-visible top portion.
	//  - Sky / open ceilings: don't push WinTop; sky shows through.
	//  - Closed door / terminal solid wall: emits Main clipped, ray ends.
	//  - Window collapses → ray bails early (perf win).
	//
	// C# `out float depthOut` → `float& DepthOut`; the returned ColumnInfo becomes the
	// in-place `Col` out-param (pool discipline: Reset() keeps the arrays' capacity);
	// `Hits` is the caller's reused scratch array (GO-04).
	static void BuildColumnSector(FGameState& St, FColumnInfo& Col, TArray<Raycast::FWallHit>& Hits, float Ox, float Oy,
								  float Rdx, float Rdy, float AngCos, float ViewZ, float Horizon, float& DepthOut)
	{
		Raycast::Cast(St.SectorMap, FVector2D(Ox, Oy), St.Player.SectorId, FVector2D(Rdx, Rdy), Hits);
		Col.Extras.Reset();
		Col.FloorBands.Reset();
		Col.CeilingBands.Reset();
		DepthOut = C::MAX_RAY;
		// Phase 8: first step-up riser captured for sprite occlusion.
		float FloorOccDist = RUI_DOOM_INF;
		float FloorOccZ = -RUI_DOOM_INF;
		// Phase 9: closest ceiling-slab underside encountered along the ray.
		// Sprites past this distance whose anchor sits at-or-above this Z are
		// hidden by the slab.
		float CeilOccDist = RUI_DOOM_INF;
		float CeilOccZ = RUI_DOOM_INF;

		// Vertical occlusion window. Stuff farther than the current hit can
		// only paint inside [WinTop, WinBot]. Tightens monotonically as we
		// walk the ray, never widens.
		float WinTop = 0.f;
		float WinBot = C::VIEWPORT_H;
		float PrevPerp = 0.001f;

		for (int32 i = 0; i < Hits.Num(); i++)
		{
			const Raycast::FWallHit& H = Hits[i];
			bool bTerminal = H.ToSector < 0;
			const FSector& Front = St.SectorMap.Sectors[H.FromSector];
			const FSector* Back = nullptr;
			if (H.ToSector >= 0)
			{
				Back = &St.SectorMap.Sectors[H.ToSector];
				// Closed door: treat as terminal full-height wall.
				if (Back->CeilingZ <= Back->FloorZ + 0.001f)
				{
					bTerminal = true;
				}
			}
			const FLinedef& Line = St.SectorMap.Lines[H.LinedefId];

			float Perp = H.Distance * AngCos;
			if (Perp < 0.001f)
			{
				Perp = 0.001f;
			}
			float Scale = C::VIEWPORT_H / Perp;
			float ScaleNear = C::VIEWPORT_H / FMath::Max(PrevPerp, 0.05f);
			uint8 Light = LightFromDist(Perp, Front.Light);

			float Fz = Front.FloorZ;
			float Cz = Front.CeilingZ;
			// Floor projection at this hit (far edge of the band) and at the
			// previous hit (near edge of the band).
			float YFloorFar = Horizon + (ViewZ - Fz) * Scale;
			float YFloorNear = Horizon + (ViewZ - Fz) * ScaleNear;
			// Ceiling projection (only meaningful when cz > viewZ).
			float YCeilFar = Horizon - (Cz - ViewZ) * Scale;

			// ── 1. Emit floor band for the segment we just crossed, clipped to
			//    the current visibility window. Skip floors at/above the eye —
			//    those don't render as a floor surface (they look like ceiling).
			if (Fz < ViewZ - 0.001f)
			{
				float BTop = YFloorFar;
				float BBot = YFloorNear;
				if (BTop < WinTop)
				{
					BTop = WinTop;
				}
				if (BBot > WinBot)
				{
					BBot = WinBot;
				}
				if (BBot > BTop + 0.5f)
				{
					// Rim chalk-line is drawn at the far edge of the upper-step's
					// band when we're about to step down to a lower sector. Drawn
					// for any meaningful drop (≥ 0.15u so 0.2u stair treads each
					// get their own line) and only when the band's top edge is the
					// true unclipped silhouette (not chopped by a higher occluder).
					bool bRimAtFar =
						H.ToSector >= 0 && !bTerminal && (Fz - Back->FloorZ) >= 0.15f && BTop <= YFloorFar + 0.5f;
					FFloorBand& Fb = St.Frame.TakeFloorBand();
					Fb.TopPx = BTop;
					Fb.BotPx = BBot;
					Fb.FloorZ = Fz;
					Fb.Light = Light;
					Fb.FloorTex = Front.FloorTex;
					Fb.BehindFloorZ = -RUI_DOOM_INF;
					Fb.RimAtFar = bRimAtFar;
					Fb.Distance = Perp; // GODOT ADDITION: depth-sorted paint list
					Col.FloorBands.Add(&Fb);
				}
			}

			// ── 2. Tighten window from the front-sector's own occluders.
			//    Floor below eye occludes anything beyond at screenY > yFloorFar
			//    (since farther floor pixels project closer to horizon = smaller
			//    screenY). So winBot = min(winBot, yFloorFar).
			//    Ceiling above eye (non-sky) occludes anything beyond above
			//    yCeilFar. Sky doesn't occlude — it shows through.
			if (Fz < ViewZ - 0.001f)
			{
				if (YFloorFar < WinBot)
				{
					WinBot = YFloorFar;
				}
			}
			// Phase 8: emit ceiling band for the segment we just crossed,
			// mirroring the floor-band logic. Only for non-sky sectors with a
			// ceiling above the eye — sky leaves the backdrop showing through and
			// ceilings at/below eye look like floors (handled elsewhere).
			if (!Front.IsSky && Cz > ViewZ + 0.001f)
			{
				float CTopBand = WinTop;
				float CBotBand = YCeilFar;
				if (CBotBand > WinBot)
				{
					CBotBand = WinBot;
				}
				if (CBotBand > CTopBand + 0.5f)
				{
					FCeilingBand& Cb = St.Frame.TakeCeilBand();
					Cb.TopPx = CTopBand;
					Cb.BotPx = CBotBand;
					Cb.CeilingZ = Cz;
					Cb.Light = Light;
					Cb.CeilingTex = Front.CeilingTex;
					Cb.Distance = Perp; // GODOT ADDITION
					Col.CeilingBands.Add(&Cb);
				}
			}
			if (!Front.IsSky && Cz > ViewZ + 0.001f)
			{
				if (YCeilFar > WinTop)
				{
					WinTop = YCeilFar;
				}
			}

			// ── 2b. Phase 9: per-ExtraFloor slab emission. Each slab contributes
			//    up to three surfaces in the front sector: TOP plane (visible if
			//    viewer is above it, acts like a raised floor), BOTTOM plane
			//    (visible if viewer is below it, acts like a low ceiling), and a
			//    SIDE wall on the boundary where the back sector lacks the same
			//    slab. Iterating in BottomZ-ascending order keeps the cliprange
			//    window tightening monotonic.
			bool bSlabTerminated = false;
			FWallSeg TerminatorWall;
			for (int32 k = 0; k < Front.ExtraFloors.Num(); k++)
			{
				const FExtraFloor& Ef = Front.ExtraFloors[k];
				bool bSharedWithBack = false;
				if (H.ToSector >= 0)
				{
					for (int32 j = 0; j < Back->ExtraFloors.Num(); j++)
					{
						const FExtraFloor& Bef = Back->ExtraFloors[j];
						if (FMath::Abs(Bef.BottomZ - Ef.BottomZ) < 0.01f && FMath::Abs(Bef.TopZ - Ef.TopZ) < 0.01f)
						{
							bSharedWithBack = true;
							break;
						}
					}
				}
				bool bAboveSlab = ViewZ > Ef.TopZ + 0.001f;
				bool bBelowSlab = ViewZ < Ef.BottomZ - 0.001f;
				uint8 SlabLight = LightFromDist(Perp, Ef.Light == 0 ? Front.Light : Ef.Light);

				// Slab TOP — visible when viewer above, acts like a raised floor.
				if (bAboveSlab)
				{
					float YTopFar = Horizon + (ViewZ - Ef.TopZ) * Scale;
					float YTopNear = Horizon + (ViewZ - Ef.TopZ) * ScaleNear;
					float BTop = YTopFar < WinTop ? WinTop : YTopFar;
					float BBot = YTopNear > WinBot ? WinBot : YTopNear;
					if (BBot > BTop + 0.5f)
					{
						FFloorBand& Fb = St.Frame.TakeFloorBand();
						Fb.TopPx = BTop;
						Fb.BotPx = BBot;
						Fb.FloorZ = Ef.TopZ;
						Fb.Light = SlabLight;
						Fb.FloorTex = Ef.TopTex;
						Fb.BehindFloorZ = -RUI_DOOM_INF;
						Fb.RimAtFar = !bSharedWithBack;
						Fb.Distance = Perp; // GODOT ADDITION
						Col.FloorBands.Add(&Fb);
					}
					if (YTopFar < WinBot)
					{
						WinBot = YTopFar;
					}
					// Sprite occluder hint — closest slab top wins.
					if (FloorOccDist > Perp)
					{
						FloorOccDist = Perp;
						FloorOccZ = Ef.TopZ;
					}
				}

				// Slab BOTTOM — visible when viewer below, acts like a low ceiling.
				if (bBelowSlab)
				{
					float YBotFar = Horizon - (Ef.BottomZ - ViewZ) * Scale;
					float CTopBand = WinTop;
					float CBotBand = YBotFar > WinBot ? WinBot : YBotFar;
					if (CBotBand > CTopBand + 0.5f)
					{
						FCeilingBand& Cb = St.Frame.TakeCeilBand();
						Cb.TopPx = CTopBand;
						Cb.BotPx = CBotBand;
						Cb.CeilingZ = Ef.BottomZ;
						Cb.Light = SlabLight;
						Cb.CeilingTex = Ef.BottomTex;
						Cb.Distance = Perp; // GODOT ADDITION
						Col.CeilingBands.Add(&Cb);
					}
					if (YBotFar > WinTop)
					{
						WinTop = YBotFar;
					}
					// Sprite occluder hint — closest slab bottom wins.
					if (CeilOccDist > Perp)
					{
						CeilOccDist = Perp;
						CeilOccZ = Ef.BottomZ;
					}
				}

				// Slab SIDE — exposed when back sector lacks the same slab. If
				// the viewer is INSIDE the slab body and the side is exposed,
				// the slab is opaque and we terminate the column at this hit.
				if (!bSharedWithBack && Ef.Solid)
				{
					float WTop = Horizon - (Ef.TopZ - ViewZ) * Scale;
					float WBot = Horizon - (Ef.BottomZ - ViewZ) * Scale;
					float CTop = WTop < WinTop ? WinTop : WTop;
					float CBot = WBot > WinBot ? WinBot : WBot;
					if (CBot > CTop + 0.5f)
					{
						FWallSeg Seg;
						Seg.TopPx = CTop;
						Seg.BotPx = CBot;
						Seg.Distance = Perp;
						Seg.WallTexIdx = Ef.SideTex;
						Seg.TexU = H.U;
						Seg.LightLevel = SlabLight;
						Seg.HitVertical = false;
						Seg.IsSky = false;
						Seg.TexOffsetPx = WTop - CTop;
						if (!bAboveSlab && !bBelowSlab)
						{
							// Inside slab body → opaque terminal.
							bSlabTerminated = true;
							TerminatorWall = Seg;
						}
						else
						{
							FWallSeg& Pooled = St.Frame.TakeWallSeg();
							Pooled = Seg;
							Col.Extras.Add(&Pooled);
						}
					}
				}
			}
			// Phase 9: ALSO emit BACK-only ExtraFloor slabs (those not present in
			// the front sector) — same surfaces + cliprange tightening as the
			// front loop:
			//   • SIDE wall at the portal (otherwise the slab pillar is see-through
			//     from cells without ExtraFloors of their own).
			//   • TOP plane as a FloorBand when viewer is above the slab — without
			//     this the next stair tread shows no visible top, the staircase
			//     looks like a smooth wall going up instead of discrete treads.
			//   • BOTTOM plane as a CeilingBand when viewer is below the slab.
			//   • Tighten (WinTop, WinBot) by the slab's screen-Y body so things
			//     past the staircase don't bleed THROUGH the slab side. Without
			//     this the inside floor leaks through the staircase column from
			//     the side view.
			//   • Terminate if viewer is INSIDE the slab body (ray hits opaque
			//     side wall edge-on, fully occluded past this hit).
			if (H.ToSector >= 0)
			{
				for (int32 k = 0; k < Back->ExtraFloors.Num(); k++)
				{
					const FExtraFloor& Ef = Back->ExtraFloors[k];
					if (!Ef.Solid)
					{
						continue;
					}
					bool bInFront = false;
					for (int32 j = 0; j < Front.ExtraFloors.Num(); j++)
					{
						const FExtraFloor& Fef = Front.ExtraFloors[j];
						if (FMath::Abs(Fef.BottomZ - Ef.BottomZ) < 0.01f && FMath::Abs(Fef.TopZ - Ef.TopZ) < 0.01f)
						{
							bInFront = true;
							break;
						}
					}
					if (bInFront)
					{
						continue;
					}
					uint8 SlabLight = LightFromDist(Perp, Ef.Light == 0 ? Back->Light : Ef.Light);
					bool bAboveSlab = ViewZ > Ef.TopZ + 0.001f;
					bool bBelowSlab = ViewZ < Ef.BottomZ - 0.001f;

					// Slab side endpoints in screen Y (positive = down).
					float YTopFar = Horizon - (Ef.TopZ - ViewZ) * Scale;
					float YBotFar = Horizon - (Ef.BottomZ - ViewZ) * Scale;

					// SIDE wall, clipped to current window. Never a terminator:
					// back-only slabs only occlude the screen-Y band [yTopFar..yBotFar];
					// above and below that band the column stays open so taller slabs
					// further along the ray (e.g. higher stair treads) can still emit.
					float CTop = YTopFar < WinTop ? WinTop : YTopFar;
					float CBot = YBotFar > WinBot ? WinBot : YBotFar;
					if (CBot > CTop + 0.5f)
					{
						FWallSeg& Seg = St.Frame.TakeWallSeg();
						Seg.TopPx = CTop;
						Seg.BotPx = CBot;
						Seg.Distance = Perp;
						Seg.WallTexIdx = Ef.SideTex;
						Seg.TexU = H.U;
						Seg.LightLevel = SlabLight;
						Seg.HitVertical = false;
						Seg.IsSky = false;
						Seg.TexOffsetPx = YTopFar - CTop;
						Col.Extras.Add(&Seg);
					}

					// TOP plane — FloorBand visible above the slab.
					if (bAboveSlab)
					{
						float YTopNear = Horizon + (ViewZ - Ef.TopZ) * ScaleNear;
						float YTopFarFlip = Horizon + (ViewZ - Ef.TopZ) * Scale;
						float BTop = YTopFarFlip < WinTop ? WinTop : YTopFarFlip;
						float BBot = YTopNear > WinBot ? WinBot : YTopNear;
						if (BBot > BTop + 0.5f)
						{
							FFloorBand& Fb = St.Frame.TakeFloorBand();
							Fb.TopPx = BTop;
							Fb.BotPx = BBot;
							Fb.FloorZ = Ef.TopZ;
							Fb.Light = SlabLight;
							Fb.FloorTex = Ef.TopTex;
							Fb.BehindFloorZ = -RUI_DOOM_INF;
							Fb.RimAtFar = true;
							Fb.Distance = Perp; // GODOT ADDITION
							Col.FloorBands.Add(&Fb);
						}
					}

					// BOTTOM plane — CeilingBand visible below the slab.
					if (bBelowSlab)
					{
						float CTopBand = WinTop;
						float CBotBand = YBotFar > WinBot ? WinBot : YBotFar;
						if (CBotBand > CTopBand + 0.5f)
						{
							FCeilingBand& Cb = St.Frame.TakeCeilBand();
							Cb.TopPx = CTopBand;
							Cb.BotPx = CBotBand;
							Cb.CeilingZ = Ef.BottomZ;
							Cb.Light = SlabLight;
							Cb.CeilingTex = Ef.BottomTex;
							Cb.Distance = Perp; // GODOT ADDITION
							Col.CeilingBands.Add(&Cb);
						}
					}

					// Cliprange occlusion. We can only tighten one boundary cleanly
					// (single-window cliprange), so:
					//   • viewer above slab → occlude below slab top (winBot = yTopFar).
					//     This is the stair-walking case: keeps the upper window open
					//     so taller slabs further along still emit their tops.
					//   • viewer below slab → occlude above slab bottom (winTop = yBotFar).
					//   • viewer inside slab body → same as above (winBot = yTopFar).
					//     Sacrifices visibility of stuff below the slab bottom past
					//     this hit (usually floor anyway), but keeps stuff ABOVE the
					//     slab top visible — critical for stair treads going up.
					if (!bBelowSlab)
					{
						if (YTopFar < WinBot)
						{
							WinBot = YTopFar;
						}
					}
					else
					{
						if (YBotFar > WinTop)
						{
							WinTop = YBotFar;
						}
					}
				}
			}
			if (bSlabTerminated)
			{
				DepthOut = Perp;
				Col.Main = TerminatorWall;
				Col.FrontFloorZ = Fz;
				Col.FloorOccluderDist = FloorOccDist;
				Col.FloorOccluderZ = FloorOccZ;
				Col.CeilingOccluderDist = CeilOccDist;
				Col.CeilingOccluderZ = CeilOccZ;
				return;
			}

			// ── 3a. TERMINAL hit (solid wall or closed door). Emit Main clipped
			//    to current window.
			if (bTerminal)
			{
				float CeilForWall = Front.IsSky ? FMath::Min(Cz, Fz + 2.5f) : Cz;
				float WTop = Horizon - (CeilForWall - ViewZ) * Scale;
				float WBot = Horizon - (Fz - ViewZ) * Scale;
				float UnclippedTop = WTop;
				if (WTop < WinTop)
				{
					WTop = WinTop;
				}
				if (WBot > WinBot)
				{
					WBot = WinBot;
				}
				DepthOut = Perp;
				Col.Main = FWallSeg();
				Col.Main.TopPx = WTop;
				Col.Main.BotPx = WBot;
				Col.Main.Distance = Perp;
				Col.Main.WallTexIdx = Line.MidTex;
				Col.Main.TexU = H.U;
				Col.Main.LightLevel = Light;
				Col.Main.HitVertical = false;
				Col.Main.IsSky = H.ToSector >= 0 && Back->IsSky;
				Col.Main.TexOffsetPx = UnclippedTop - WTop;
				Col.FrontFloorZ = Fz;
				Col.FloorOccluderDist = FloorOccDist;
				Col.FloorOccluderZ = FloorOccZ;
				Col.CeilingOccluderDist = CeilOccDist;
				Col.CeilingOccluderZ = CeilOccZ;
				return;
			}

			// ── 3b. PORTAL UPPER: back ceiling LOWER than front ceiling. Emit
			//    upper wall clipped, then push winTop down. Skip for sky-back
			//    (sky portals show sky above, not a wall).
			if (Back->CeilingZ < Front.CeilingZ - 0.001f && !Back->IsSky)
			{
				float WTop = Horizon - (Front.CeilingZ - ViewZ) * Scale;
				float WBot = Horizon - (Back->CeilingZ - ViewZ) * Scale;
				float CTop = WTop, CBot = WBot;
				if (CTop < WinTop)
				{
					CTop = WinTop;
				}
				if (CBot > WinBot)
				{
					CBot = WinBot;
				}
				if (CBot > CTop + 0.5f)
				{
					FWallSeg& Seg = St.Frame.TakeWallSeg();
					Seg.TopPx = CTop;
					Seg.BotPx = CBot;
					Seg.Distance = Perp;
					Seg.WallTexIdx = Line.UpperTex;
					Seg.TexU = H.U;
					Seg.LightLevel = Light;
					Seg.HitVertical = false;
					Seg.IsSky = false;
					Seg.TexOffsetPx = WTop - CTop;
					Col.Extras.Add(&Seg);
				}
				if (WBot > WinTop)
				{
					WinTop = WBot;
				}
			}

			// ── 3c. PORTAL LOWER (step-up): back floor HIGHER than front floor.
			//    Emit lower wall clipped, then push winBot up.
			if (Back->FloorZ > Front.FloorZ + 0.001f)
			{
				// Sprite occlusion: first step-up wins (closest occluder along ray).
				if (FloorOccDist > Perp)
				{
					FloorOccDist = Perp;
					FloorOccZ = Back->FloorZ;
				}
				float WTop = Horizon - (Back->FloorZ - ViewZ) * Scale;
				float WBot = Horizon - (Front.FloorZ - ViewZ) * Scale;
				float CTop = WTop, CBot = WBot;
				if (CTop < WinTop)
				{
					CTop = WinTop;
				}
				if (CBot > WinBot)
				{
					CBot = WinBot;
				}
				if (CBot > CTop + 0.5f)
				{
					// Riser flag → chalk-line rim, only for tall steps.
					bool bTallStep = (Back->FloorZ - Front.FloorZ) >= 0.5f;
					FWallSeg& Seg = St.Frame.TakeWallSeg();
					Seg.TopPx = CTop;
					Seg.BotPx = CBot;
					Seg.Distance = Perp;
					Seg.WallTexIdx = Line.LowerTex;
					Seg.TexU = H.U;
					Seg.LightLevel = Light;
					Seg.HitVertical = false;
					Seg.IsSky = false;
					Seg.IsRiser = bTallStep;
					Seg.TexOffsetPx = WTop - CTop;
					Col.Extras.Add(&Seg);
				}
				if (WTop < WinBot)
				{
					WinBot = WTop;
				}
			}

			// ── 3d. PORTAL STEP-DOWN (back floor LOWER than front floor):
			//    INTENTIONALLY no wall emitted. The cliff face from above is
			//    geometrically occluded by the upper floor surface — emitting it
			//    produces "phantom brick stripes" on stairs. The visual "rim" is
			//    the silhouette = upper band's far edge, already drawn above.
			//    Beyond this hit, the lower floor's band paints clipped to the
			//    already-tightened winBot=yFloorFar.

			PrevPerp = Perp;

			// Window collapsed → nothing past this hit can be visible.
			if (WinBot - WinTop < 1.f)
			{
				DepthOut = Perp;
				Col.Main = FWallSeg();
				Col.Main.TopPx = 0;
				Col.Main.BotPx = 0;
				Col.Main.Distance = Perp;
				Col.Main.WallTexIdx = 0;
				Col.Main.TexU = 0;
				Col.Main.LightLevel = Light;
				Col.Main.HitVertical = false;
				Col.Main.IsSky = false;
				Col.FrontFloorZ = Fz;
				Col.FloorOccluderDist = FloorOccDist;
				Col.FloorOccluderZ = FloorOccZ;
				Col.CeilingOccluderDist = CeilOccDist;
				Col.CeilingOccluderZ = CeilOccZ;
				return;
			}
		}

		// Ray escaped — sky column.
		Col.Main = FWallSeg();
		Col.Main.TopPx = 0;
		Col.Main.BotPx = 0;
		Col.Main.Distance = C::MAX_RAY;
		Col.Main.WallTexIdx = 0;
		Col.Main.TexU = 0;
		Col.Main.LightLevel = 200;
		Col.Main.HitVertical = false;
		Col.Main.IsSky = true;
		Col.FrontFloorZ = 0.f;
		Col.FloorOccluderDist = FloorOccDist;
		Col.FloorOccluderZ = FloorOccZ;
		Col.CeilingOccluderDist = CeilOccDist;
		Col.CeilingOccluderZ = CeilOccZ;
	}

	static FRayHit CastRay(const FGameState& St, float Ox, float Oy, float Dx, float Dy)
	{
		int32 MapX = (int32)Ox, MapY = (int32)Oy;
		float DeltaX = Dx == 0 ? 1e30f : FMath::Abs(1.f / Dx);
		float DeltaY = Dy == 0 ? 1e30f : FMath::Abs(1.f / Dy);
		int32 StepX, StepY;
		float SideX, SideY;
		if (Dx < 0)
		{
			StepX = -1;
			SideX = (Ox - MapX) * DeltaX;
		}
		else
		{
			StepX = 1;
			SideX = (MapX + 1.f - Ox) * DeltaX;
		}
		if (Dy < 0)
		{
			StepY = -1;
			SideY = (Oy - MapY) * DeltaY;
		}
		else
		{
			StepY = 1;
			SideY = (MapY + 1.f - Oy) * DeltaY;
		}

		bool bVertical = false;
		int32 Safety = 0;
		while (Safety++ < 96)
		{
			if (SideX < SideY)
			{
				SideX += DeltaX;
				MapX += StepX;
				bVertical = true;
			}
			else
			{
				SideY += DeltaY;
				MapY += StepY;
				bVertical = false;
			}
			if (MapX < 0 || MapX >= St.Map.Width || MapY < 0 || MapY >= St.Map.Height)
			{
				FRayHit Esc;
				Esc.Distance = C::MAX_RAY;
				Esc.IsSky = true;
				Esc.Light = 200;
				return Esc;
			}
			const FCell& Cell = St.Map.AtSafe(MapX, MapY);
			if (Cell.Kind == ECellKind::Wall ||
				(Cell.Kind >= ECellKind::Door && Cell.Kind <= ECellKind::DoorRed && Cell.DoorState < 250))
			{
				float Dist;
				float U;
				if (bVertical)
				{
					Dist = SideX - DeltaX;
					float HitY = Oy + Dist * Dy;
					U = HitY - FMath::Floor(HitY);
					if (Dx > 0)
					{
						U = 1.f - U;
					}
				}
				else
				{
					Dist = SideY - DeltaY;
					float HitX = Ox + Dist * Dx;
					U = HitX - FMath::Floor(HitX);
					if (Dy < 0)
					{
						U = 1.f - U;
					}
				}
				// for doors with partial open, account by shifting the texture vertically — just clip distance min
				if (Dist < 0.05f)
				{
					Dist = 0.05f;
				}
				FRayHit Rh;
				Rh.Distance = Dist;
				Rh.WallTexIdx = Cell.WallTexIdx;
				Rh.TexU = U;
				Rh.Light = Cell.LightLevel;
				Rh.HitVertical = bVertical;
				Rh.IsSky = false;
				return Rh;
			}
			if (Cell.Kind == ECellKind::Exit && Cell.WallTexIdx != 0)
			{
				float Dist;
				float U;
				if (bVertical)
				{
					Dist = SideX - DeltaX;
					float HitY = Oy + Dist * Dy;
					U = HitY - FMath::Floor(HitY);
					if (Dx > 0)
					{
						U = 1.f - U;
					}
				}
				else
				{
					Dist = SideY - DeltaY;
					float HitX = Ox + Dist * Dx;
					U = HitX - FMath::Floor(HitX);
					if (Dy < 0)
					{
						U = 1.f - U;
					}
				}
				FRayHit Rh;
				Rh.Distance = Dist;
				Rh.WallTexIdx = (uint8)W_EXIT;
				Rh.TexU = U;
				Rh.Light = 240;
				Rh.HitVertical = bVertical;
				return Rh;
			}
		}
		FRayHit Esc;
		Esc.Distance = C::MAX_RAY;
		Esc.IsSky = true;
		Esc.Light = 200;
		return Esc;
	}

	static uint8 LightFromDist(float Dist, uint8 CellLight)
	{
		float Fade = FMath::Clamp(1.f - Dist / 14.f, 0.f, 1.f);
		int32 V = (int32)(CellLight * (0.35f + 0.65f * Fade));
		if (V < 30)
		{
			V = 30;
		}
		if (V > 255)
		{
			V = 255;
		}
		return (uint8)V;
	}

	// ───── Helpers ─────

	static bool HasLineOfSight(const FGameState& St, float Ax, float Ay, float Bx, float By)
	{
		float Dx = Bx - Ax, Dy = By - Ay;
		float Dist = FMath::Sqrt(Dx * Dx + Dy * Dy);
		if (Dist < 0.01f)
		{
			return true;
		}
		int32 Steps = (int32)(Dist * 6.f);
		if (Steps < 1)
		{
			Steps = 1;
		}
		for (int32 i = 1; i < Steps; i++)
		{
			float T = i / (float)Steps;
			int32 Gx = (int32)(Ax + Dx * T);
			int32 Gy = (int32)(Ay + Dy * T);
			if (St.Map.BlocksRays(Gx, Gy))
			{
				return false;
			}
		}
		return true;
	}

	// Phase 8: 3D line-of-sight using the sector graph. Walks portals from the
	// source sector, checking that the bullet/eye Z stays inside every
	// traversed sector's [FloorZ, CeilingZ] (sky ignores ceiling). Stops
	// monsters from shooting through risers, low ceilings, or solid walls
	// that the tile-only check missed (a step-up cell is CellKind.Empty so
	// the old grid check let shots pass).
	static bool HasLineOfSight3D(const FGameState& St, float Ax, float Ay, float Az, float Bx, float By, float Bz,
								 int32 SrcSector)
	{
		if (!St.SectorMap.IsValid() || SrcSector < 0)
		{
			return HasLineOfSight(St, Ax, Ay, Bx, By);
		}
		float Dx = Bx - Ax, Dy = By - Ay;
		float D = FMath::Sqrt(Dx * Dx + Dy * Dy);
		if (D < 0.01f)
		{
			return true;
		}
		TArray<Raycast::FWallHit> Hits;
		Raycast::Cast(St.SectorMap, FVector2D(Ax, Ay), SrcSector, FVector2D(Dx / D, Dy / D), Hits);
		// Phase 9: same-sector case. If the ray never crosses a portal
		// (target inside source sector) the loop below runs 0 times and we'd
		// return true unconditionally, even when a slab in srcSector sits
		// between the two endpoints (e.g. monster on slab top shoots at
		// player on slab bottom in the same cell).
		if (Hits.Num() == 0)
		{
			const FSector& S = St.SectorMap.Sectors[SrcSector];
			float SegMin0 = FMath::Min(Az, Bz);
			float SegMax0 = FMath::Max(Az, Bz);
			if (SegMin0 <= S.FloorZ + 0.001f)
			{
				return false;
			}
			if (!S.IsSky && SegMax0 >= S.CeilingZ - 0.001f)
			{
				return false;
			}
			for (int32 k = 0; k < S.ExtraFloors.Num(); k++)
			{
				const FExtraFloor& Ef = S.ExtraFloors[k];
				if (!Ef.Solid)
				{
					continue;
				}
				if (SegMax0 >= Ef.BottomZ + 0.001f && SegMin0 <= Ef.TopZ - 0.001f)
				{
					return false;
				}
			}
			return true;
		}
		float PrevT = 0.f;
		for (int32 i = 0; i < Hits.Num(); i++)
		{
			const Raycast::FWallHit& H = Hits[i];
			const FSector& Front = St.SectorMap.Sectors[H.FromSector];
			// Clip the hit distance against the actual segment end.
			bool bReachedTarget = H.Distance >= D;
			float EndT = bReachedTarget ? D : H.Distance;
			float ZStart = Az + (Bz - Az) * (PrevT / D);
			float ZEnd = Az + (Bz - Az) * (EndT / D);
			float SegMin = FMath::Min(ZStart, ZEnd);
			float SegMax = FMath::Max(ZStart, ZEnd);
			if (SegMin <= Front.FloorZ + 0.001f)
			{
				return false;
			}
			if (!Front.IsSky && SegMax >= Front.CeilingZ - 0.001f)
			{
				return false;
			}
			// Phase 9: also clip against ExtraFloor slabs in this sector. The ray
			// segment from prevT..endT (Z from zStart..zEnd) is blocked if it
			// passes through the body of any solid slab.
			for (int32 k = 0; k < Front.ExtraFloors.Num(); k++)
			{
				const FExtraFloor& Ef = Front.ExtraFloors[k];
				if (!Ef.Solid)
				{
					continue;
				}
				// Block if Z range overlaps slab body. Lateral overlap is implied
				// because the ray IS in this sector for the segment we're checking.
				if (SegMax >= Ef.BottomZ + 0.001f && SegMin <= Ef.TopZ - 0.001f)
				{
					return false;
				}
			}
			if (bReachedTarget)
			{
				return true;
			}
			if (H.ToSector < 0)
			{
				return false; // solid wall blocks
			}
			const FSector& Back = St.SectorMap.Sectors[H.ToSector];
			// Sliver at the portal must pass through the back sector's window too.
			if (ZEnd <= Back.FloorZ + 0.001f)
			{
				return false;
			}
			if (!Back.IsSky && ZEnd >= Back.CeilingZ - 0.001f)
			{
				return false;
			}
			PrevT = H.Distance;
		}
		return true;
	}

	static int32 FindMobjInCone(const FGameState& St, float Ox, float Oy, float Ang, float Range, float HalfAngle)
	{
		int32 Best = -1;
		float BestDist = Range;
		for (int32 i = 1; i <= St.MobjCount; i++)
		{
			const FMobj& M = St.Mobjs[i];
			if (M.Id == 0 || !IsMonster(M.Kind) || M.Health <= 0)
			{
				continue;
			}
			float Dx = M.X - Ox, Dy = M.Y - Oy;
			float D = FMath::Sqrt(Dx * Dx + Dy * Dy);
			if (D > BestDist)
			{
				continue;
			}
			float Ma = FMath::Atan2(Dy, Dx);
			// C# routes through Mathf.DeltaAngle in degrees; this is the same shortest
			// signed angle computed directly in radians (the Godot port does likewise).
			float Diff = FMath::FindDeltaAngleRadians(Ma, Ang);
			if (FMath::Abs(Diff) > HalfAngle)
			{
				continue;
			}
			if (!HasLineOfSight(St, Ox, Oy, M.X, M.Y))
			{
				continue;
			}
			Best = i;
			BestDist = D;
		}
		return Best;
	}

	static FShotHit TraceShot(const FGameState& St, float Ox, float Oy, float Ang)
	{
		float Rdx = FMath::Cos(Ang), Rdy = FMath::Sin(Ang);
		FRayHit Rh = CastRay(St, Ox, Oy, Rdx, Rdy);
		float WallDist = Rh.Distance;

		// Phase 7: vertical aim. Pitch is in pixels of Y-shear; convert to a
		// vertical slope (dz / horizontal-distance) by dividing by the projection
		// plane scale used in CastFrame.
		const FPlayerState& P = St.Player;
		float PlaneScale = (C::VIEWPORT_H * 0.5f) / FMath::Tan(C::HALF_FOV);
		// The camera's forward axis projects to screen center (VIEWPORT_H/2).
		// Horizon visually shifts by p.Pitch px; that same px / planeScale is the
		// tangent of the pitch angle, which is the bullet's vertical slope.
		float PitchSlope = P.Pitch / PlaneScale; // looking up (Pitch>0) -> bullet rises
		float ViewZ = P.Z + (P.ViewHeight <= 0.f ? 0.6f : P.ViewHeight);

		// any mobj closer along the ray? Must also satisfy vertical hit window.
		int32 BestIdx = -1;
		float BestT = WallDist;
		for (int32 i = 1; i <= St.MobjCount; i++)
		{
			const FMobj& M = St.Mobjs[i];
			if (M.Id == 0 || !IsMonster(M.Kind) || M.Health <= 0)
			{
				continue;
			}
			float Dx = M.X - Ox, Dy = M.Y - Oy;
			float T = Dx * Rdx + Dy * Rdy; // projection along ray
			if (T <= 0 || T > BestT)
			{
				continue;
			}
			float PerpX = Dx - Rdx * T, PerpY = Dy - Rdy * T;
			float Perp2 = PerpX * PerpX + PerpY * PerpY;
			if (Perp2 > M.Radius * M.Radius)
			{
				continue;
			}
			// Vertical: bullet Z at distance t. Slight padding so close encounters
			// and tall mobjs are forgiving when looking at their belly/feet.
			float BulletZ = ViewZ + PitchSlope * T;
			float MobjBot = M.Z - 0.05f;
			float MobjTop = M.Z + (M.Height > 0.f ? M.Height : 0.7f) + 0.05f;
			if (BulletZ < MobjBot || BulletZ > MobjTop)
			{
				continue;
			}
			BestIdx = i;
			BestT = T;
		}
		FShotHit Out;
		if (BestIdx > 0)
		{
			Out.HitMobj = true;
			Out.MobjIdx = BestIdx;
			Out.X = Ox + Rdx * BestT;
			Out.Y = Oy + Rdy * BestT;
			return Out;
		}
		Out.HitMobj = false;
		Out.X = Ox + Rdx * WallDist;
		Out.Y = Oy + Rdy * WallDist;
		return Out;
	}

	static void MoveActor(FGameState& St, float& X, float& Y, float Dx, float Dy, float Radius, bool bIsPlayer,
						  int32 SelfIdx)
	{
		float Nx = X + Dx, Ny = Y + Dy;
		// X axis
		if (!CollidesAt(St, Nx, Y, Radius, bIsPlayer, SelfIdx))
		{
			X = Nx;
		}
		if (!CollidesAt(St, X, Ny, Radius, bIsPlayer, SelfIdx))
		{
			Y = Ny;
		}

		// Phase 7: step-up. After horizontal motion, snap player Z up to the
		// tallest cell footprint they're standing on (within STEP_HEIGHT).
		// Phase 9: step-up uses the actor's CURRENT footZ as the lower bound,
		// not 0. Otherwise dropping into a pit at Z<0 trampolines the player
		// back to Z=0 because every neighbor cell's floor (Z=0) is greater
		// than the trivially-initialized bestFloor.
		if (bIsPlayer)
		{
			float FootZ = St.Player.Z;
			int32 Gx0 = (int32)(X - Radius), Gx1 = (int32)(X + Radius);
			int32 Gy0 = (int32)(Y - Radius), Gy1 = (int32)(Y + Radius);
			float BestFloor = FootZ;
			for (int32 Gy = Gy0; Gy <= Gy1; Gy++)
			{
				for (int32 Gx = Gx0; Gx <= Gx1; Gx++)
				{
					float Fz = St.Map.FloorAt(Gx, Gy, FootZ, C::STEP_HEIGHT);
					if (Fz > BestFloor && Fz - FootZ <= C::STEP_HEIGHT + 0.001f)
					{
						BestFloor = Fz;
					}
				}
			}
			// Only auto-snap up while on or near the ground; jumping/falling is
			// handled by gravity in UpdatePlayerVertical.
			if (BestFloor > FootZ && FMath::Abs(St.Player.ZVel) < 0.01f)
			{
				St.Player.Z = BestFloor;
				St.Player.ZVel = 0.f;
			}
		}
	}

	static bool CollidesAt(const FGameState& St, float X, float Y, float R, bool bIsPlayer, int32 SelfIdx)
	{
		// wall collision with circle vs cell. For the player we use Z-aware
		// blocking so they can step over short ledges and crouch under low
		// ceilings; monsters retain the old 2D blocker semantics.
		int32 Gx0 = (int32)(X - R), Gx1 = (int32)(X + R);
		int32 Gy0 = (int32)(Y - R), Gy1 = (int32)(Y + R);
		float FootZ = bIsPlayer ? St.Player.Z : 0.f;
		float HeadZ = bIsPlayer ? St.Player.Z + (St.Player.ViewHeight < C::PLAYER_VIEW_HEIGHT ? C::CROUCH_HEIGHT + 0.15f
																							  : C::PLAYER_HEIGHT)
								: 0.7f;
		for (int32 Gy = Gy0; Gy <= Gy1; Gy++)
		{
			for (int32 Gx = Gx0; Gx <= Gx1; Gx++)
			{
				bool bBlocks = bIsPlayer ? St.Map.BlocksMovementZ(Gx, Gy, FootZ, HeadZ, C::STEP_HEIGHT)
										 : St.Map.BlocksMovement(Gx, Gy);
				if (bBlocks)
				{
					float Cx = FMath::Clamp<float>(X, Gx, Gx + 1);
					float Cy = FMath::Clamp<float>(Y, Gy, Gy + 1);
					float Ddx = X - Cx, Ddy = Y - Cy;
					if (Ddx * Ddx + Ddy * Ddy < R * R)
					{
						return true;
					}
				}
			}
		}
		// mobj-vs-mobj (only for monsters)
		if (!bIsPlayer && SelfIdx > 0)
		{
			// don't push other monsters too aggressively, but block on big ones
			for (int32 j = 1; j <= St.MobjCount; j++)
			{
				if (j == SelfIdx)
				{
					continue;
				}
				const FMobj& M = St.Mobjs[j];
				if (M.Id == 0 || (!IsMonster(M.Kind) && M.Kind != EMobjKind::Barrel))
				{
					continue;
				}
				if (M.Health <= 0)
				{
					continue;
				}
				float Dx = X - M.X, Dy = Y - M.Y;
				float R2 = R + M.Radius;
				if (Dx * Dx + Dy * Dy < R2 * R2 * 0.7f)
				{
					return true;
				}
			}
			// collide with player
			float Pdx = X - St.Player.X, Pdy = Y - St.Player.Y;
			float Pr = R + C::PLAYER_RADIUS;
			if (Pdx * Pdx + Pdy * Pdy < Pr * Pr)
			{
				return true;
			}
		}
		return false;
	}

	// ───── Mobj pool ─────

	void AddMobj(FGameState& St, FMobj M)
	{
		if (M.Id == 0)
		{
			M.Id = St.NextMobjId++;
		}
		// Phase 8: anchor newly-spawned mobj to the actual sector floor at its
		// spawn (X,Y). Without this, monsters spawned on a raised floor (e.g.
		// second-story plateau) sit at Z=0 — sprites render through the floor
		// and 3D LOS uses the wrong height. Projectiles already set their own Z.
		if (St.SectorMap.IsValid() && !IsProjectile(M.Kind) && M.Z == 0.f)
		{
			int32 Sec = Raycast::PointInSectorFromHint(St.SectorMap, FVector2D(M.X, M.Y), -1);
			if (Sec >= 0)
			{
				M.SectorId = Sec;
				// Phase 9: pick the highest reachable surface so monsters spawned
				// inside an ExtraFloor footprint land on the slab top, not the
				// basement floor below it.
				M.Z = StandingFloorIn(St.SectorMap.Sectors[Sec], 1e6f, 1e6f);
			}
		}
		// Phase 7: floating mobjs hover above the floor.
		if ((M.Kind == EMobjKind::Cacodemon || M.Kind == EMobjKind::LostSoul))
		{
			M.Z += 0.4f;
		}
		if (St.MobjCount + 1 < St.Mobjs.Num())
		{
			St.MobjCount++;
			St.Mobjs[St.MobjCount] = M;
		}
		else
		{
			// find a free slot
			for (int32 i = 1; i < St.Mobjs.Num(); i++)
			{
				if (St.Mobjs[i].Id == 0)
				{
					St.Mobjs[i] = M;
					return;
				}
			}
		}
	}

	static void CompactMobjs(FGameState& St)
	{
		// periodic compaction: shrink MobjCount by trailing empties
		while (St.MobjCount > 0 && St.Mobjs[St.MobjCount].Id == 0)
		{
			St.MobjCount--;
		}
	}

	// ───── Face animation ─────

	// Widened from the original's `AdvanceFace(ref PlayerState p, dt)` to take the game
	// state — needed for the single consolidated `Frand()` RNG (inherited Godot fix,
	// replacing the original's stray `UnityEngine.Random.value` jitter here with the
	// same deterministic LCG used everywhere else).
	static void AdvanceFace(FGameState& St, float Dt)
	{
		FPlayerState& P = St.Player;
		P.FaceTimer -= Dt;
		if (P.FaceTimer > 0)
		{
			return;
		}
		if (!P.Alive)
		{
			P.FaceState = 7;
			P.FaceTimer = 1.f;
			return;
		}
		if (P.HurtFlash > 0.3f)
		{
			P.FaceState = 6;
			P.FaceTimer = 0.4f;
			return;
		}
		int32 Hp = P.Health;
		int32 Bucket;
		if (Hp >= 80)
		{
			Bucket = 1;
		}
		else if (Hp >= 60)
		{
			Bucket = 2;
		}
		else if (Hp >= 40)
		{
			Bucket = 3;
		}
		else if (Hp >= 20)
		{
			Bucket = 4;
		}
		else
		{
			Bucket = 5;
		}
		P.FaceState = Bucket;
		P.FaceTimer = 0.45f + St.Frand() * 0.4f;
	}

	// ───── Misc ─────

	bool IsMonster(EMobjKind K)
	{
		return K == EMobjKind::Imp || K == EMobjKind::Demon || K == EMobjKind::Baron || K == EMobjKind::Cacodemon ||
			   K == EMobjKind::LostSoul || K == EMobjKind::Zombieman || K == EMobjKind::Shotgunner;
	}

	bool IsBoss(EMobjKind K)
	{
		return K == EMobjKind::Baron || K == EMobjKind::Cacodemon;
	}

	// True while any boss-tier monster (Baron / Cacodemon) is still alive on
	// the map. Used to gate level-exit cells when LevelStart.BossExitGated is
	// set.
	bool AnyBossAlive(const FGameState& St)
	{
		for (int32 i = 1; i <= St.MobjCount; i++)
		{
			const FMobj& M = St.Mobjs[i];
			if (!IsBoss(M.Kind))
			{
				continue;
			}
			if (M.State == EAIState::Dead)
			{
				continue;
			}
			return true;
		}
		return false;
	}

	bool IsProjectile(EMobjKind K)
	{
		return K == EMobjKind::ImpFireball || K == EMobjKind::BaronBall || K == EMobjKind::CacoBall ||
			   K == EMobjKind::RocketProj || K == EMobjKind::PlasmaProj || K == EMobjKind::BfgProj;
	}

	bool IsPickup(EMobjKind K)
	{
		return K >= EMobjKind::PickupHealth && K <= EMobjKind::KeyRed;
	}

	int32 SpriteIndexForMobj(EMobjKind K)
	{
		switch (K)
		{
		case EMobjKind::Imp:
			return S_IMP;
		case EMobjKind::Demon:
			return S_DEMON;
		case EMobjKind::Baron:
			return S_BARON;
		case EMobjKind::Cacodemon:
			return S_CACO;
		case EMobjKind::LostSoul:
			return S_LOSTSOUL;
		case EMobjKind::Zombieman:
			return S_IMP; // reuse
		case EMobjKind::Shotgunner:
			return S_BARON; // reuse with tint
		case EMobjKind::Corpse:
			return S_CORPSE;
		case EMobjKind::ImpFireball:
			return S_FIREBALL;
		case EMobjKind::BaronBall:
			return S_FIREBALL;
		case EMobjKind::CacoBall:
			return S_PLASMA;
		case EMobjKind::RocketProj:
			return S_ROCKET_PROJ;
		case EMobjKind::PlasmaProj:
			return S_PLASMA;
		case EMobjKind::BfgProj:
			return S_BFG_PROJ;
		case EMobjKind::Explosion:
			return S_EXPLOSION;
		case EMobjKind::PickupHealth:
			return S_HEALTH;
		case EMobjKind::PickupArmor:
			return S_ARMOR;
		case EMobjKind::PickupArmorBlue:
			return S_ARMOR;
		case EMobjKind::PickupBullets:
			return S_AMMO_BULLETS;
		case EMobjKind::PickupShells:
			return S_AMMO_SHELLS;
		case EMobjKind::PickupRockets:
			return S_AMMO_ROCKETS;
		case EMobjKind::PickupCells:
			return S_AMMO_CELLS;
		case EMobjKind::PickupShotgun:
			return S_PICKUP_SHOTGUN;
		case EMobjKind::PickupChaingun:
			return S_PICKUP_CHAIN;
		case EMobjKind::PickupRocketLauncher:
			return S_PICKUP_ROCKET;
		case EMobjKind::PickupPlasma:
			return S_PICKUP_PLASMA;
		case EMobjKind::PickupBFG:
			return S_PICKUP_BFG;
		case EMobjKind::KeyBlue:
			return S_KEY_BLUE;
		case EMobjKind::KeyYellow:
			return S_KEY_YELLOW;
		case EMobjKind::KeyRed:
			return S_KEY_RED;
		case EMobjKind::Barrel:
			return S_BARREL;
		case EMobjKind::Light:
			return S_LIGHT;
		default:
			return S_IMP;
		}
	}

	FColor TintForMobj(EMobjKind K, EAIState S)
	{
		if (S == EAIState::Pain)
		{
			return FColor(255, 200, 200, 255);
		}
		switch (K)
		{
		case EMobjKind::Shotgunner:
			return FColor(150, 220, 150, 255);
		case EMobjKind::Zombieman:
			return FColor(180, 180, 200, 255);
		default:
			return FColor(255, 255, 255, 255);
		}
	}

	static void Message(FPlayerState& P, const TCHAR* Text, float Dur)
	{
		P.MessageText = Text;
		P.MessageTimer = Dur;
	}

	// (The original file ends with its private `Frand(ref GameState)` — that LCG lives on
	// the state itself here: `FGameState::Frand()` in DoomTypes.h, bit-identical.)
} // namespace RuiDoom
