// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Doom demo — procedural textures. Faithful port of the Unity ReactiveUIToolKit `DoomGame`
// sample's `DoomTextures.uitkx` (cross-checked against the Godot sibling's doom_textures.gd):
// every texture is generated per-pixel in code, no image assets. Same per-pixel formulas as
// the original; Texture2D/SetPixels32/Apply -> UTexture2D::CreateTransient + mip-0 BGRA write.
//
// Row order: the Unity original's Apply() flips the pixel buffer vertically because Unity's
// Texture2D stores row 0 at the BOTTOM while the buffer is authored with row 0 at the TOP
// (UI coords). Unreal — like Godot — stores row 0 at the TOP, matching the authoring
// convention directly, so this port writes pixels straight into the mip with no flip pass
// and produces the same right-side-up result the original's flip achieves.
//
// Determinism: the original seeds C# System.Random per texture. This port implements that
// exact generator (Knuth subtractive; see FDoomRandom in the .cpp) so the noise streams are
// byte-identical to Unity's — the Godot port had to substitute Godot's own RNG there.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

namespace RuiDoom
{
	// ───── Constants ─────
	constexpr int32 TEX_W = 64;
	constexpr int32 TEX_H = 64;
	constexpr int32 SPRITE_W = 64;
	constexpr int32 SPRITE_H = 64;

	// ───── Wall texture indices ─────
	constexpr int32 W_BRICK_RED = 0;
	constexpr int32 W_BRICK_GREY = 1;
	constexpr int32 W_TECH_PANEL = 2;
	constexpr int32 W_WOOD = 3;
	constexpr int32 W_MARBLE = 4;
	constexpr int32 W_HELL_STONE = 5;
	constexpr int32 W_DOOR = 6;
	constexpr int32 W_DOOR_BLUE = 7;
	constexpr int32 W_DOOR_YELLOW = 8;
	constexpr int32 W_DOOR_RED = 9;
	constexpr int32 W_EXIT = 10;
	constexpr int32 W_BRICK_BLUE = 11;
	constexpr int32 W_COUNT = 12;

	// ───── Floor texture indices ─────
	constexpr int32 F_CONCRETE = 0;
	constexpr int32 F_TILE = 1;
	constexpr int32 F_GRASS = 2;
	constexpr int32 F_LAVA = 3;
	constexpr int32 F_BLOOD = 4;
	constexpr int32 F_NUKAGE = 5;
	constexpr int32 F_BLUE = 6;
	constexpr int32 F_COUNT = 7;

	// ───── Sprite indices ─────
	constexpr int32 S_IMP = 0;
	constexpr int32 S_DEMON = 1;
	constexpr int32 S_BARON = 2;
	constexpr int32 S_CACO = 3;
	constexpr int32 S_LOSTSOUL = 4;
	constexpr int32 S_CORPSE = 5;
	constexpr int32 S_FIREBALL = 6;
	constexpr int32 S_ROCKET_PROJ = 7;
	constexpr int32 S_PLASMA = 8;
	constexpr int32 S_BFG_PROJ = 9;
	constexpr int32 S_EXPLOSION = 10;
	constexpr int32 S_HEALTH = 11;
	constexpr int32 S_ARMOR = 12;
	constexpr int32 S_AMMO_BULLETS = 13;
	constexpr int32 S_AMMO_SHELLS = 14;
	constexpr int32 S_AMMO_ROCKETS = 15;
	constexpr int32 S_AMMO_CELLS = 16;
	constexpr int32 S_PICKUP_SHOTGUN = 17;
	constexpr int32 S_PICKUP_CHAIN = 18;
	constexpr int32 S_PICKUP_ROCKET = 19;
	constexpr int32 S_PICKUP_PLASMA = 20;
	constexpr int32 S_PICKUP_BFG = 21;
	constexpr int32 S_KEY_BLUE = 22;
	constexpr int32 S_KEY_YELLOW = 23;
	constexpr int32 S_KEY_RED = 24;
	constexpr int32 S_BARREL = 25;
	constexpr int32 S_LIGHT = 26;
	constexpr int32 S_COUNT = 27;

	/**
	 * Lazy build-once texture provider (the original's static module with EnsureBuilt()).
	 * Every accessor routes through EnsureBuilt(), so consumers never observe the unbuilt
	 * state. Textures are transient UTexture2Ds pinned by TStrongObjectPtr — the demo owns
	 * their lifetimes without reflection.
	 */
	class RUIDEMO_API FDoomTextures
	{
	public:
		/** Builds every texture once; idempotent. */
		static void EnsureBuilt();

		/** Wall texture (W_* index), 64x64. */
		static UTexture2D* Wall(int32 Index);

		/** Floor texture (F_* index), 64x64. */
		static UTexture2D* Floor(int32 Index);

		/** Sprite (S_* index), 64x64, transparent background. */
		static UTexture2D* Sprite(int32 Index);

		/** Sky strip, 512x100, horizontally wrapping. */
		static UTexture2D* Sky();

		/** Doomguy face mug, 48x56 — 8 frames: god/hp100/hp75/hp50/hp25/hp0/pain/death. */
		static UTexture2D* Face(int32 Index);

		/** Player weapon HUD overlay, 256x192 — 8: fist, pistol, shotgun, chaingun,
		 *  rocket, plasma, bfg, muzzle-flash overlay. */
		static UTexture2D* Weapon(int32 Index);
	};
} // namespace RuiDoom
