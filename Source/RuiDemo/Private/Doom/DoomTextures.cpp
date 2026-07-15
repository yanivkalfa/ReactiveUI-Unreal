// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Doom/DoomTextures.h"

#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

namespace RuiDoom
{
	namespace
	{
		// ─────────────────────────────────────────────────────────────
		// Deterministic RNG — exact port of C# System.Random
		// ─────────────────────────────────────────────────────────────

		/**
		 * The Unity original seeds `new System.Random(seed)` per texture; to reproduce its
		 * pixel stream byte-for-byte this is the same generator: Knuth's subtractive
		 * lagged-Fibonacci as implemented by .NET (seeded System.Random, all versions).
		 * Next(minInclusive, maxExclusive) matches C# semantics exactly.
		 */
		class FDoomRandom
		{
		public:
			explicit FDoomRandom(int32 Seed)
			{
				const int32 Subtraction = (Seed == MIN_int32) ? MAX_int32 : FMath::Abs(Seed);
				int32 Mj = 161803398 - Subtraction; // MSEED - seed
				SeedArray[55] = Mj;
				int32 Mk = 1;
				for (int32 I = 1; I < 55; ++I)
				{
					const int32 Ii = (21 * I) % 55;
					SeedArray[Ii] = Mk;
					Mk = Mj - Mk;
					if (Mk < 0)
					{
						Mk += MBIG;
					}
					Mj = SeedArray[Ii];
				}
				for (int32 K = 1; K < 5; ++K)
				{
					for (int32 I = 1; I < 56; ++I)
					{
						SeedArray[I] -= SeedArray[1 + (I + 30) % 55];
						if (SeedArray[I] < 0)
						{
							SeedArray[I] += MBIG;
						}
					}
				}
				INext = 0;
				INextP = 21;
			}

			/** System.Random.Next(minInclusive, maxExclusive). */
			int32 Next(int32 MinValue, int32 MaxValue)
			{
				const int64 Range = (int64)MaxValue - (int64)MinValue;
				return (int32)(Sample() * (double)Range) + MinValue;
			}

		private:
			static constexpr int32 MBIG = 2147483647; // Int32.MaxValue

			double Sample() { return InternalSample() * (1.0 / MBIG); }

			int32 InternalSample()
			{
				int32 LocINext = INext + 1;
				if (LocINext >= 56)
				{
					LocINext = 1;
				}
				int32 LocINextP = INextP + 1;
				if (LocINextP >= 56)
				{
					LocINextP = 1;
				}
				int32 RetVal = SeedArray[LocINext] - SeedArray[LocINextP];
				if (RetVal == MBIG)
				{
					--RetVal;
				}
				if (RetVal < 0)
				{
					RetVal += MBIG;
				}
				SeedArray[LocINext] = RetVal;
				INext = LocINext;
				INextP = LocINextP;
				return RetVal;
			}

			int32 SeedArray[56] = {0};
			int32 INext = 0;
			int32 INextP = 0;
		};

		// ─────────────────────────────────────────────────────────────
		// Helpers (Unity Mathf/Color32 semantics)
		// ─────────────────────────────────────────────────────────────

		/** Mathf.Sin: (float)Math.Sin((double)v). */
		float USin(float V)
		{
			return (float)FMath::Sin((double)V);
		}

		/** Mathf.Cos: (float)Math.Cos((double)v). */
		float UCos(float V)
		{
			return (float)FMath::Cos((double)V);
		}

		/** Mathf.Sqrt: (float)Math.Sqrt((double)v). */
		float USqrt(float V)
		{
			return (float)FMath::Sqrt((double)V);
		}

		/** Mathf.Lerp: a + (b - a) * Clamp01(t). */
		float ULerp(float A, float B, float T)
		{
			return A + (B - A) * FMath::Clamp(T, 0.0f, 1.0f);
		}

		uint8 Clamp8(int32 V)
		{
			if (V < 0)
			{
				return 0;
			}
			if (V > 255)
			{
				return 255;
			}
			return (uint8)V;
		}

		FColor Lerp32(const FColor& A, const FColor& B, float T)
		{
			T = FMath::Clamp(T, 0.0f, 1.0f);
			return FColor((uint8)(A.R + (B.R - A.R) * T), (uint8)(A.G + (B.G - A.G) * T),
						  (uint8)(A.B + (B.B - A.B) * T), 255);
		}

		FColor Darken(const FColor& C, int32 Amt)
		{
			return FColor((uint8)FMath::Max(0, C.R - Amt), (uint8)FMath::Max(0, C.G - Amt),
						  (uint8)FMath::Max(0, C.B - Amt), C.A);
		}

		void DrawRect(TArray<FColor>& Px, int32 W, int32 H, int32 X, int32 Y, int32 RectW, int32 RectH, const FColor& C)
		{
			for (int32 Yy = Y; Yy < Y + RectH && Yy < H; ++Yy)
			{
				if (Yy < 0)
				{
					continue;
				}
				for (int32 Xx = X; Xx < X + RectW && Xx < W; ++Xx)
				{
					if (Xx < 0)
					{
						continue;
					}
					Px[Yy * W + Xx] = C;
				}
			}
		}

		void DrawCircle(TArray<FColor>& Px, int32 W, int32 H, int32 Cx, int32 Cy, int32 R, const FColor& C)
		{
			for (int32 Dy = -R; Dy <= R; ++Dy)
			{
				for (int32 Dx = -R; Dx <= R; ++Dx)
				{
					if (Dx * Dx + Dy * Dy <= R * R)
					{
						const int32 Xx = Cx + Dx;
						const int32 Yy = Cy + Dy;
						if (Xx >= 0 && Xx < W && Yy >= 0 && Yy < H)
						{
							Px[Yy * W + Xx] = C;
						}
					}
				}
			}
		}

		/**
		 * Buffer -> transient texture. Row 0 of the buffer is the TOP row: Unreal (like
		 * Godot, unlike Unity) stores row 0 at the top, so — per the Godot port — there is
		 * no vertical flip pass here. PF_B8G8R8A8 mip bytes are written B,G,R,A.
		 */
		UTexture2D* CreateTex(int32 W, int32 H, const TArray<FColor>& Px, bool bWrap)
		{
			UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
			Tex->Filter = TF_Nearest; // FilterMode.Point — crisp retro pixels
			Tex->SRGB = true;
			Tex->NeverStream = true;
			Tex->AddressX = bWrap ? TA_Wrap : TA_Clamp; // sky repeats; everything else clamps
			Tex->AddressY = bWrap ? TA_Wrap : TA_Clamp;
			uint8* Out = (uint8*)Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			for (int32 I = 0; I < Px.Num(); ++I)
			{
				const FColor& C = Px[I];
				*Out++ = C.B;
				*Out++ = C.G;
				*Out++ = C.R;
				*Out++ = C.A;
			}
			Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
			Tex->UpdateResource();
			return Tex;
		}

		/** Fresh zeroed pixel buffer (Color32 default = 0,0,0,0 — transparent). */
		TArray<FColor> NewBuffer(int32 W, int32 H)
		{
			TArray<FColor> Px;
			Px.SetNumZeroed(W * H);
			return Px;
		}

		// ─────────────────────────────────────────────────────────────
		// Wall textures — generators return the 64x64 pixel BUFFER (not the texture) so
		// EnsureBuilt() can bake both the base texture and the WallTiled() strip from ONE
		// run of the generator (the tiled repeats must be identical copies, not re-rolled
		// RNG streams).
		// ─────────────────────────────────────────────────────────────

		TArray<FColor> MakeBrick(int32 Seed, uint8 BaseR, uint8 BaseG, uint8 BaseB, uint8 MortarR, uint8 MortarG,
								 uint8 MortarB)
		{
			TArray<FColor> Px = NewBuffer(TEX_W, TEX_H);
			FDoomRandom Rng(Seed);
			const FColor Mortar(MortarR, MortarG, MortarB, 255);
			for (int32 Y = 0; Y < TEX_H; ++Y)
			{
				const int32 Row = Y / 8;
				const int32 XOff = ((Row & 1) == 0) ? 0 : 16;
				for (int32 X = 0; X < TEX_W; ++X)
				{
					const bool bGrout = (Y % 8 == 0) || (((X + XOff) % 32) == 0);
					if (bGrout)
					{
						Px[Y * TEX_W + X] = Mortar;
						continue;
					}
					const int32 R = BaseR + Rng.Next(-22, 22);
					const int32 G = BaseG + Rng.Next(-15, 15);
					const int32 B = BaseB + Rng.Next(-15, 15);
					Px[Y * TEX_W + X] = FColor(Clamp8(R), Clamp8(G), Clamp8(B), 255);
				}
			}
			return Px;
		}

		TArray<FColor> MakeTechPanel(int32 Seed)
		{
			TArray<FColor> Px = NewBuffer(TEX_W, TEX_H);
			FDoomRandom Rng(Seed);
			const FColor Rivet(180, 180, 190, 255);
			const FColor Bevel(60, 70, 85, 255);
			for (int32 Y = 0; Y < TEX_H; ++Y)
			{
				for (int32 X = 0; X < TEX_W; ++X)
				{
					const int32 R = 70 + Rng.Next(-8, 8);
					const int32 G = 80 + Rng.Next(-8, 8);
					const int32 B = 95 + Rng.Next(-10, 10);
					Px[Y * TEX_W + X] = FColor(Clamp8(R), Clamp8(G), Clamp8(B), 255);
				}
			}
			// panel borders every 32px
			for (int32 Y = 0; Y < TEX_H; ++Y)
			{
				for (int32 X = 0; X < TEX_W; ++X)
				{
					if (X % 32 == 0 || Y % 32 == 0 || X % 32 == 31 || Y % 32 == 31)
					{
						Px[Y * TEX_W + X] = Bevel;
					}
				}
			}
			// rivets
			for (int32 Ry = 4; Ry < TEX_H; Ry += 32)
			{
				for (int32 Rx = 4; Rx < TEX_W; Rx += 32)
				{
					for (int32 Dy = 0; Dy < 2; ++Dy)
					{
						for (int32 Dx = 0; Dx < 2; ++Dx)
						{
							if (Rx + Dx < TEX_W && Ry + Dy < TEX_H)
							{
								Px[(Ry + Dy) * TEX_W + (Rx + Dx)] = Rivet;
							}
						}
					}
				}
			}
			return Px;
		}

		TArray<FColor> MakeWood(int32 Seed)
		{
			TArray<FColor> Px = NewBuffer(TEX_W, TEX_H);
			FDoomRandom Rng(Seed);
			for (int32 Y = 0; Y < TEX_H; ++Y)
			{
				for (int32 X = 0; X < TEX_W; ++X)
				{
					const int32 Grain = (int32)(USin(Y * 0.6f + USin(X * 0.05f) * 2.0f) * 12.0f);
					const int32 R = 110 + Grain + Rng.Next(-6, 6);
					const int32 G = 70 + Grain + Rng.Next(-5, 5);
					const int32 B = 35 + Grain / 2 + Rng.Next(-4, 4);
					Px[Y * TEX_W + X] = FColor(Clamp8(R), Clamp8(G), Clamp8(B), 255);
				}
			}
			// vertical plank seams
			for (int32 X = 0; X < TEX_W; X += 16)
			{
				for (int32 Y = 0; Y < TEX_H; ++Y)
				{
					Px[Y * TEX_W + X] = FColor(45, 25, 15, 255);
				}
			}
			return Px;
		}

		TArray<FColor> MakeMarble(int32 Seed)
		{
			TArray<FColor> Px = NewBuffer(TEX_W, TEX_H);
			FDoomRandom Rng(Seed);
			for (int32 Y = 0; Y < TEX_H; ++Y)
			{
				for (int32 X = 0; X < TEX_W; ++X)
				{
					const float Vein = FMath::Abs(USin(X * 0.12f + Y * 0.07f) * UCos(Y * 0.18f)) * 30.0f;
					const int32 R = 170 + (int32)Vein + Rng.Next(-10, 10);
					const int32 G = 165 + (int32)Vein + Rng.Next(-10, 10);
					const int32 B = 180 + (int32)Vein + Rng.Next(-10, 10);
					Px[Y * TEX_W + X] = FColor(Clamp8(R), Clamp8(G), Clamp8(B), 255);
				}
			}
			return Px;
		}

		TArray<FColor> MakeHellStone(int32 Seed)
		{
			TArray<FColor> Px = NewBuffer(TEX_W, TEX_H);
			FDoomRandom Rng(Seed);
			for (int32 Y = 0; Y < TEX_H; ++Y)
			{
				for (int32 X = 0; X < TEX_W; ++X)
				{
					const int32 Dark = Rng.Next(0, 35);
					const int32 R = 80 + Dark - Rng.Next(0, 20);
					const int32 G = 25 + Dark / 3;
					const int32 B = 25 + Dark / 3;
					Px[Y * TEX_W + X] = FColor(Clamp8(R), Clamp8(G), Clamp8(B), 255);
				}
			}
			// skull pattern hint
			const int32 Cx = TEX_W / 2;
			const int32 Cy = TEX_H / 2;
			for (int32 Dy = -10; Dy <= 10; ++Dy)
			{
				for (int32 Dx = -10; Dx <= 10; ++Dx)
				{
					const float D = USqrt((float)(Dx * Dx + Dy * Dy));
					if (D > 8 && D < 11)
					{
						Px[(Cy + Dy) * TEX_W + (Cx + Dx)] = FColor(35, 10, 10, 255);
					}
				}
			}
			return Px;
		}

		TArray<FColor> MakeDoor(const FColor& Main, TOptional<FColor> Trim)
		{
			TArray<FColor> Px = NewBuffer(TEX_W, TEX_H);
			const FColor Dark((uint8)(Main.R / 2), (uint8)(Main.G / 2), (uint8)(Main.B / 2), 255);
			const FColor Bright((uint8)FMath::Min(255, Main.R + 40), (uint8)FMath::Min(255, Main.G + 40),
								(uint8)FMath::Min(255, Main.B + 40), 255);
			const FColor TrimColor = Trim.Get(FColor(60, 60, 60, 255));
			for (int32 Y = 0; Y < TEX_H; ++Y)
			{
				for (int32 X = 0; X < TEX_W; ++X)
				{
					const bool bBorder = X < 3 || X >= TEX_W - 3 || Y < 3 || Y >= TEX_H - 3;
					const bool bMiddleStripe = X >= 28 && X <= 35;
					if (bBorder)
					{
						Px[Y * TEX_W + X] = Dark;
					}
					else if (bMiddleStripe)
					{
						Px[Y * TEX_W + X] = TrimColor;
					}
					else
					{
						Px[Y * TEX_W + X] = ((X + Y) % 6 == 0) ? Bright : Main;
					}
				}
			}
			// handle
			for (int32 Y = 28; Y <= 36; ++Y)
			{
				for (int32 X = 8; X <= 14; ++X)
				{
					Px[Y * TEX_W + X] = FColor(60, 60, 60, 255);
				}
			}
			return Px;
		}

		TArray<FColor> MakeExitSign()
		{
			TArray<FColor> Px = NewBuffer(TEX_W, TEX_H);
			const FColor Bg(30, 30, 30, 255);
			const FColor Sign(50, 200, 70, 255);
			for (int32 I = 0; I < Px.Num(); ++I)
			{
				Px[I] = Bg;
			}
			// green "EXIT" panel
			for (int32 Y = 20; Y < 44; ++Y)
			{
				for (int32 X = 8; X < 56; ++X)
				{
					Px[Y * TEX_W + X] = Sign;
				}
			}
			// dark border
			const FColor Border(20, 80, 30, 255);
			for (int32 X = 8; X < 56; ++X)
			{
				Px[20 * TEX_W + X] = Border;
				Px[43 * TEX_W + X] = Border;
			}
			for (int32 Y = 20; Y < 44; ++Y)
			{
				Px[Y * TEX_W + 8] = Border;
				Px[Y * TEX_W + 55] = Border;
			}
			return Px;
		}

		/** The 64x64 pixel buffer for wall Index — same generators/args as the original's
		 *  EnsureBuilt() wall table, dispatched by index. */
		TArray<FColor> BuildWallPx(int32 Index)
		{
			switch (Index)
			{
			case W_BRICK_RED:
				return MakeBrick(/*Seed*/ 7, 130, 40, 30, 35, 25, 20);
			case W_BRICK_GREY:
				return MakeBrick(/*Seed*/ 11, 95, 95, 100, 35, 35, 35);
			case W_TECH_PANEL:
				return MakeTechPanel(/*Seed*/ 13);
			case W_WOOD:
				return MakeWood(/*Seed*/ 17);
			case W_MARBLE:
				return MakeMarble(/*Seed*/ 19);
			case W_HELL_STONE:
				return MakeHellStone(/*Seed*/ 23);
			case W_DOOR:
				return MakeDoor(FColor(150, 110, 60, 255), NullOpt);
			case W_DOOR_BLUE:
				return MakeDoor(FColor(80, 100, 220, 255), FColor(50, 70, 200, 255));
			case W_DOOR_YELLOW:
				return MakeDoor(FColor(220, 200, 60, 255), FColor(180, 160, 40, 255));
			case W_DOOR_RED:
				return MakeDoor(FColor(220, 60, 50, 255), FColor(180, 40, 35, 255));
			case W_EXIT:
				return MakeExitSign();
			case W_BRICK_BLUE:
				return MakeBrick(/*Seed*/ 29, 50, 90, 210, 20, 30, 70);
			default:
				checkNoEntry();
				return NewBuffer(TEX_W, TEX_H);
			}
		}

		/** 64x256 strip: the 64x64 wall buffer stacked vertically WALL_TILE_REPEATS times —
		 *  memcpy'd identical copies (the RNG is NOT re-run per repeat). */
		UTexture2D* MakeTiled(const TArray<FColor>& Src)
		{
			TArray<FColor> Px;
			Px.SetNumUninitialized(TEX_W * TEX_H * WALL_TILE_REPEATS);
			for (int32 Repeat = 0; Repeat < WALL_TILE_REPEATS; ++Repeat)
			{
				FMemory::Memcpy(Px.GetData() + Repeat * Src.Num(), Src.GetData(), Src.Num() * sizeof(FColor));
			}
			return CreateTex(TEX_W, TEX_H * WALL_TILE_REPEATS, Px, false);
		}

		// ─────────────────────────────────────────────────────────────
		// Floor textures
		// ─────────────────────────────────────────────────────────────

		UTexture2D* MakeNoiseTile(int32 Seed, uint8 BaseR, uint8 BaseG, uint8 BaseB, int32 Range)
		{
			TArray<FColor> Px = NewBuffer(TEX_W, TEX_H);
			FDoomRandom Rng(Seed);
			for (int32 I = 0; I < Px.Num(); ++I)
			{
				const int32 N = Rng.Next(-Range, Range);
				Px[I] = FColor(Clamp8(BaseR + N), Clamp8(BaseG + N), Clamp8(BaseB + N), 255);
			}
			return CreateTex(TEX_W, TEX_H, Px, false);
		}

		UTexture2D* MakeTileFloor(int32 Seed)
		{
			TArray<FColor> Px = NewBuffer(TEX_W, TEX_H);
			FDoomRandom Rng(Seed);
			const FColor Grout(40, 40, 40, 255);
			for (int32 Y = 0; Y < TEX_H; ++Y)
			{
				for (int32 X = 0; X < TEX_W; ++X)
				{
					const bool bGrout = (X % 16 == 0) || (Y % 16 == 0);
					if (bGrout)
					{
						Px[Y * TEX_W + X] = Grout;
						continue;
					}
					const int32 N = Rng.Next(-12, 12);
					Px[Y * TEX_W + X] = FColor(Clamp8(110 + N), Clamp8(108 + N), Clamp8(105 + N), 255);
				}
			}
			return CreateTex(TEX_W, TEX_H, Px, false);
		}

		// ─────────────────────────────────────────────────────────────
		// Sky
		// ─────────────────────────────────────────────────────────────

		UTexture2D* MakeSky()
		{
			const int32 W = 512;
			const int32 H = 100;
			TArray<FColor> Px = NewBuffer(W, H);
			FDoomRandom Rng(7);
			for (int32 Y = 0; Y < H; ++Y)
			{
				// gradient: dark red at top -> reddish at horizon
				const float Yy = Y / (float)H;
				const int32 R = (int32)ULerp(35, 110, Yy);
				const int32 G = (int32)ULerp(15, 35, Yy);
				const int32 B = (int32)ULerp(20, 30, Yy);
				for (int32 X = 0; X < W; ++X)
				{
					// distant mountain silhouettes
					const int32 MountainH = (int32)(USin(X * 0.04f) * 6 + USin(X * 0.013f + 0.5f) * 12 + 30);
					if (H - Y < MountainH)
					{
						Px[Y * W + X] = FColor(20, 8, 12, 255);
					}
					else
					{
						const int32 Rn = Rng.Next(-6, 6);
						Px[Y * W + X] = FColor(Clamp8(R + Rn), Clamp8(G + Rn), Clamp8(B + Rn), 255);
					}
				}
			}
			return CreateTex(W, H, Px, true); // TextureWrapMode.Repeat
		}

		// ─────────────────────────────────────────────────────────────
		// Sprites
		// ─────────────────────────────────────────────────────────────

		UTexture2D* MakeMonster(const FColor& Body, const FColor& Detail, bool bEyes = false, bool bFangs = false,
								bool bHorns = false)
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const int32 Cx = SPRITE_W / 2;
			// body: oval
			for (int32 Y = 0; Y < SPRITE_H; ++Y)
			{
				for (int32 X = 0; X < SPRITE_W; ++X)
				{
					const float Dx = (X - Cx) / 22.0f;
					const float Dy = (Y - 36) / 28.0f;
					if (Dx * Dx + Dy * Dy <= 1.0f)
					{
						// (The original also computes an unused shade value `sh` here.)
						Px[Y * SPRITE_W + X] = Lerp32(Body, Detail, Dx * Dx);
						if (((X + Y) & 7) == 0)
						{
							Px[Y * SPRITE_W + X] = Darken(Px[Y * SPRITE_W + X], 30);
						}
					}
				}
			}
			// head
			for (int32 Y = 6; Y < 28; ++Y)
			{
				for (int32 X = Cx - 10; X <= Cx + 10; ++X)
				{
					const float Dx = (X - Cx) / 10.0f;
					const float Dy = (Y - 17) / 10.0f;
					if (Dx * Dx + Dy * Dy <= 1.0f)
					{
						Px[Y * SPRITE_W + X] = Body;
					}
				}
			}
			if (bEyes)
			{
				for (int32 Dy = 0; Dy < 3; ++Dy)
				{
					for (int32 Dx = 0; Dx < 3; ++Dx)
					{
						Px[(15 + Dy) * SPRITE_W + (Cx - 5 + Dx)] = FColor(255, 240, 100, 255);
						Px[(15 + Dy) * SPRITE_W + (Cx + 3 + Dx)] = FColor(255, 240, 100, 255);
					}
				}
			}
			if (bFangs)
			{
				for (int32 Y = 22; Y <= 26; ++Y)
				{
					Px[Y * SPRITE_W + Cx - 3] = FColor(240, 240, 230, 255);
					Px[Y * SPRITE_W + Cx + 3] = FColor(240, 240, 230, 255);
				}
			}
			if (bHorns)
			{
				for (int32 Y = 0; Y < 8; ++Y)
				{
					Px[Y * SPRITE_W + Cx - 9 + Y / 2] = FColor(40, 25, 15, 255);
					Px[Y * SPRITE_W + Cx + 9 - Y / 2] = FColor(40, 25, 15, 255);
				}
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakeCacoSprite()
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const int32 Cx = SPRITE_W / 2;
			const int32 Cy = 32;
			for (int32 Y = 0; Y < SPRITE_H; ++Y)
			{
				for (int32 X = 0; X < SPRITE_W; ++X)
				{
					const float D = USqrt((float)((X - Cx) * (X - Cx) + (Y - Cy) * (Y - Cy)));
					if (D < 26)
					{
						const int32 N = (int32)(D * 2);
						Px[Y * SPRITE_W + X] =
							FColor((uint8)FMath::Clamp(150 - N, 30, 255), (uint8)FMath::Clamp(40 - N / 2, 10, 100),
								   (uint8)FMath::Clamp(150 - N, 30, 255), 255);
					}
				}
			}
			// big eye
			for (int32 Dy = -4; Dy <= 4; ++Dy)
			{
				for (int32 Dx = -4; Dx <= 4; ++Dx)
				{
					if (Dx * Dx + Dy * Dy <= 16)
					{
						Px[(Cy + Dy) * SPRITE_W + (Cx + Dx)] = FColor(240, 220, 80, 255);
					}
				}
			}
			Px[Cy * SPRITE_W + Cx] = FColor(20, 20, 20, 255);
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakeLostSoul()
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const int32 Cx = SPRITE_W / 2;
			const int32 Cy = 32;
			// skull
			for (int32 Y = 0; Y < SPRITE_H; ++Y)
			{
				for (int32 X = 0; X < SPRITE_W; ++X)
				{
					const float D = USqrt((float)((X - Cx) * (X - Cx) + (Y - Cy) * (Y - Cy)));
					if (D < 16)
					{
						Px[Y * SPRITE_W + X] = FColor(220, 210, 190, 255);
					}
					else if (D < 24)
					{
						Px[Y * SPRITE_W + X] = FColor(255, 180, 60, (uint8)(255 - (int32)((D - 16) * 30)));
					}
				}
			}
			// eye sockets
			for (int32 Dy = -2; Dy <= 2; ++Dy)
			{
				for (int32 Dx = -2; Dx <= 2; ++Dx)
				{
					Px[(Cy - 3 + Dy) * SPRITE_W + (Cx - 5 + Dx)] = FColor(20, 5, 5, 255);
					Px[(Cy - 3 + Dy) * SPRITE_W + (Cx + 5 + Dx)] = FColor(20, 5, 5, 255);
				}
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakeCorpse()
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const FColor Blood(120, 25, 25, 255);
			for (int32 Y = 50; Y < SPRITE_H; ++Y)
			{
				for (int32 X = 8; X < SPRITE_W - 8; ++X)
				{
					const float Dy = (Y - 56) / 8.0f;
					const float Dx = (X - 32) / 22.0f;
					if (Dx * Dx + Dy * Dy <= 1.0f)
					{
						Px[Y * SPRITE_W + X] = Blood;
					}
				}
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakeBall(const FColor& Inner, const FColor& Core)
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const int32 Cx = SPRITE_W / 2;
			const int32 Cy = SPRITE_H / 2;
			for (int32 Y = 0; Y < SPRITE_H; ++Y)
			{
				for (int32 X = 0; X < SPRITE_W; ++X)
				{
					const float D = USqrt((float)((X - Cx) * (X - Cx) + (Y - Cy) * (Y - Cy)));
					if (D < 10)
					{
						Px[Y * SPRITE_W + X] = Core;
					}
					else if (D < 18)
					{
						Px[Y * SPRITE_W + X] = Inner;
					}
					else if (D < 24)
					{
						Px[Y * SPRITE_W + X] = FColor(Inner.R, Inner.G, Inner.B, (uint8)(255 - (int32)((D - 18) * 40)));
					}
				}
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakeRocketSprite()
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const int32 Cx = SPRITE_W / 2;
			for (int32 Y = 16; Y < 48; ++Y)
			{
				for (int32 X = Cx - 4; X <= Cx + 4; ++X)
				{
					Px[Y * SPRITE_W + X] = FColor(80, 80, 90, 255);
				}
			}
			// tip
			for (int32 Y = 12; Y < 16; ++Y)
			{
				for (int32 X = Cx - 2; X <= Cx + 2; ++X)
				{
					Px[Y * SPRITE_W + X] = FColor(180, 60, 50, 255);
				}
			}
			// flame trail
			for (int32 Y = 48; Y < 56; ++Y)
			{
				for (int32 X = Cx - 3; X <= Cx + 3; ++X)
				{
					Px[Y * SPRITE_W + X] = FColor(255, 180, 50, 255);
				}
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakeExplosionFrame()
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const int32 Cx = SPRITE_W / 2;
			const int32 Cy = SPRITE_H / 2;
			FDoomRandom Rng(99);
			for (int32 Y = 0; Y < SPRITE_H; ++Y)
			{
				for (int32 X = 0; X < SPRITE_W; ++X)
				{
					const float D = USqrt((float)((X - Cx) * (X - Cx) + (Y - Cy) * (Y - Cy)));
					if (D < 28)
					{
						const int32 N = Rng.Next(-30, 30);
						if (D < 10)
						{
							Px[Y * SPRITE_W + X] = FColor(Clamp8(255 + N), Clamp8(220 + N), Clamp8(80 + N), 255);
						}
						else if (D < 18)
						{
							Px[Y * SPRITE_W + X] = FColor(Clamp8(220 + N), Clamp8(120 + N), Clamp8(40 + N), 255);
						}
						else
						{
							Px[Y * SPRITE_W + X] = FColor(Clamp8(150 + N), Clamp8(60 + N), Clamp8(20 + N),
														  (uint8)FMath::Clamp(255 - (int32)((D - 18) * 25), 0, 255));
						}
					}
				}
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakePickup(const FColor& Main, TCHAR Letter)
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const int32 Cx = SPRITE_W / 2;
			const int32 Cy = 38;
			const FColor Dark = Darken(Main, 50);
			// box body
			for (int32 Y = 28; Y < 52; ++Y)
			{
				for (int32 X = 22; X < 42; ++X)
				{
					const bool bBorder = X == 22 || X == 41 || Y == 28 || Y == 51;
					Px[Y * SPRITE_W + X] = bBorder ? Dark : Main;
				}
			}
			// bright "letter" mark in center
			const FColor Mark(255, 255, 255, 255);
			if (Letter == TEXT('+'))
			{
				for (int32 I = 32; I < 42; ++I)
				{
					Px[40 * SPRITE_W + I - 5] = Mark;
				}
				for (int32 J = 32; J < 46; ++J)
				{
					Px[J * SPRITE_W + Cx] = Mark;
				}
			}
			else
			{
				// small bright square as generic mark
				for (int32 Dy = -2; Dy <= 2; ++Dy)
				{
					for (int32 Dx = -3; Dx <= 3; ++Dx)
					{
						Px[(Cy + Dy) * SPRITE_W + (Cx + Dx)] = Mark;
					}
				}
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakeKey(const FColor& C)
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const int32 Cx = SPRITE_W / 2;
			// bow
			for (int32 Dy = -6; Dy <= 6; ++Dy)
			{
				for (int32 Dx = -6; Dx <= 6; ++Dx)
				{
					if (Dx * Dx + Dy * Dy <= 36 && Dx * Dx + Dy * Dy >= 12)
					{
						Px[(34 + Dy) * SPRITE_W + (Cx + Dx)] = C;
					}
				}
			}
			// shaft
			for (int32 Y = 38; Y < 52; ++Y)
			{
				Px[Y * SPRITE_W + Cx] = C;
				Px[Y * SPRITE_W + Cx - 1] = C;
			}
			// teeth
			for (int32 X = Cx - 4; X <= Cx + 4; ++X)
			{
				Px[50 * SPRITE_W + X] = C;
				Px[51 * SPRITE_W + X] = C;
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakeBarrel()
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const FColor Body(110, 70, 40, 255);
			const FColor Rim(60, 35, 20, 255);
			const FColor Liquid(80, 220, 90, 255);
			for (int32 Y = 18; Y < 56; ++Y)
			{
				for (int32 X = 18; X < 46; ++X)
				{
					const bool bBorder = X == 18 || X == 45 || Y == 18 || Y == 55;
					const bool bRing = (Y % 8 == 4);
					if (bBorder)
					{
						Px[Y * SPRITE_W + X] = Rim;
					}
					else if (bRing)
					{
						Px[Y * SPRITE_W + X] = Rim;
					}
					else
					{
						Px[Y * SPRITE_W + X] = Body;
					}
				}
			}
			// glowing top
			for (int32 Y = 20; Y < 24; ++Y)
			{
				for (int32 X = 22; X < 42; ++X)
				{
					Px[Y * SPRITE_W + X] = Liquid;
				}
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		UTexture2D* MakeLight()
		{
			TArray<FColor> Px = NewBuffer(SPRITE_W, SPRITE_H);
			const int32 Cx = SPRITE_W / 2;
			// pole
			for (int32 Y = 8; Y < 50; ++Y)
			{
				Px[Y * SPRITE_W + Cx] = FColor(60, 60, 60, 255);
				Px[Y * SPRITE_W + Cx + 1] = FColor(60, 60, 60, 255);
			}
			// lamp
			for (int32 Dy = -4; Dy <= 4; ++Dy)
			{
				for (int32 Dx = -6; Dx <= 6; ++Dx)
				{
					if (Dx * Dx + Dy * Dy <= 30)
					{
						Px[(12 + Dy) * SPRITE_W + (Cx + Dx)] = FColor(255, 240, 180, 255);
					}
				}
			}
			return CreateTex(SPRITE_W, SPRITE_H, Px, false);
		}

		// ─────────────────────────────────────────────────────────────
		// Doomguy face mug (8 frames: god / hp100 / hp75 / hp50 / hp25 / hp0 / pain / death)
		// ─────────────────────────────────────────────────────────────

		UTexture2D* MakeFace(int32 FrameIdx)
		{
			const int32 W = 48;
			const int32 H = 56;
			TArray<FColor> Px = NewBuffer(W, H);
			const FColor Skin(220, 175, 120, 255);
			const FColor BloodySkin(180, 80, 70, 255);
			const FColor Hair(120, 70, 30, 255);
			const FColor Eye(80, 220, 80, 255);
			const FColor Mouth(80, 30, 25, 255);
			const FColor Bg(20, 20, 20, 0);

			const bool bDead = (FrameIdx == 7);
			const bool bHurt = (FrameIdx == 6);
			const bool bGod = (FrameIdx == 0);
			const int32 HpBucket = FrameIdx;
			FColor UseSkin = (HpBucket >= 5 || bHurt) ? BloodySkin : Skin;
			if (bGod)
			{
				UseSkin = FColor(180, 220, 255, 255);
			}

			for (int32 I = 0; I < Px.Num(); ++I)
			{
				Px[I] = Bg;
			}
			// face oval
			const int32 Cx = W / 2;
			const int32 Cy = H / 2 + 4;
			for (int32 Y = 0; Y < H; ++Y)
			{
				for (int32 X = 0; X < W; ++X)
				{
					const float Dx = (X - Cx) / 18.0f;
					const float Dy = (Y - Cy) / 22.0f;
					if (Dx * Dx + Dy * Dy <= 1.0f)
					{
						Px[Y * W + X] = UseSkin;
					}
				}
			}
			// hair
			for (int32 Y = Cy - 22; Y < Cy - 8; ++Y)
			{
				for (int32 X = Cx - 16; X <= Cx + 16; ++X)
				{
					const float Dx = (X - Cx) / 16.0f;
					const float Dy = (Y - (Cy - 14)) / 8.0f;
					if (Dx * Dx + Dy * Dy <= 1.0f && Y >= 0)
					{
						Px[Y * W + X] = Hair;
					}
				}
			}
			if (bDead)
			{
				// X eyes
				for (int32 I = -3; I <= 3; ++I)
				{
					Px[(Cy - 3 + I) * W + (Cx - 7 + I)] = FColor(40, 40, 40, 255);
					Px[(Cy - 3 - I) * W + (Cx - 7 + I)] = FColor(40, 40, 40, 255);
					Px[(Cy - 3 + I) * W + (Cx + 5 + I)] = FColor(40, 40, 40, 255);
					Px[(Cy - 3 - I) * W + (Cx + 5 + I)] = FColor(40, 40, 40, 255);
				}
			}
			else
			{
				// eyes
				for (int32 Dy = -2; Dy <= 2; ++Dy)
				{
					for (int32 Dx = -2; Dx <= 2; ++Dx)
					{
						if (Cy - 4 + Dy >= 0)
						{
							Px[(Cy - 4 + Dy) * W + (Cx - 6 + Dx)] = Eye;
							Px[(Cy - 4 + Dy) * W + (Cx + 4 + Dx)] = Eye;
						}
					}
				}
			}
			// mouth
			if (!bDead)
			{
				for (int32 X = Cx - 5; X <= Cx + 5; ++X)
				{
					for (int32 Y = Cy + 6; Y <= Cy + 8; ++Y)
					{
						if (Y < H)
						{
							Px[Y * W + X] = bHurt ? FColor(120, 30, 30, 255) : Mouth;
						}
					}
				}
			}
			return CreateTex(W, H, Px, false);
		}

		// ─────────────────────────────────────────────────────────────
		// Player weapon sprites (drawn HUD-overlay, ~256x192)
		// ─────────────────────────────────────────────────────────────

		UTexture2D* MakeWeaponSprite(int32 Kind)
		{
			const int32 W = 256;
			const int32 H = 192;
			TArray<FColor> Px = NewBuffer(W, H); // zeroed = transparent background
			const FColor Metal(110, 110, 120, 255);
			const FColor Dark(40, 40, 45, 255);
			const FColor Wood(110, 70, 35, 255);
			const FColor Hand(220, 175, 120, 255);
			const int32 Cx = W / 2;

			switch (Kind)
			{
			case 0: // fist - just hand at the side
				DrawRect(Px, W, H, Cx + 30, H - 60, 60, 60, Hand);
				DrawRect(Px, W, H, Cx + 35, H - 30, 50, 30, Dark);
				break;
			case 1:													 // pistol
				DrawRect(Px, W, H, Cx - 18, H - 90, 36, 50, Metal);	 // barrel
				DrawRect(Px, W, H, Cx - 14, H - 110, 28, 22, Metal); // top
				DrawRect(Px, W, H, Cx - 14, H - 50, 28, 30, Dark);	 // grip
				DrawRect(Px, W, H, Cx - 30, H - 40, 60, 40, Hand);	 // hand
				break;
			case 2:													  // shotgun - longer barrel, two-hand
				DrawRect(Px, W, H, Cx - 60, H - 100, 130, 18, Metal); // barrel
				DrawRect(Px, W, H, Cx - 30, H - 80, 70, 22, Wood);	  // pump
				DrawRect(Px, W, H, Cx - 10, H - 60, 30, 35, Dark);	  // grip
				DrawRect(Px, W, H, Cx + 10, H - 65, 50, 30, Hand);
				DrawRect(Px, W, H, Cx - 60, H - 65, 50, 30, Hand);
				break;
			case 3: // chaingun - thick barrel cluster
				DrawRect(Px, W, H, Cx - 60, H - 110, 130, 40, Metal);
				// 6 round muzzles
				for (int32 I = 0; I < 6; ++I)
				{
					DrawCircle(Px, W, H, Cx + 60, H - 105 + (I - 2) * 6, 4, Dark);
				}
				DrawRect(Px, W, H, Cx - 40, H - 60, 80, 35, Dark);
				DrawRect(Px, W, H, Cx - 50, H - 40, 100, 35, Hand);
				break;
			case 4: // rocket launcher - big tube
				DrawRect(Px, W, H, Cx - 70, H - 110, 150, 30, Metal);
				DrawCircle(Px, W, H, Cx + 68, H - 95, 16, Dark);
				DrawRect(Px, W, H, Cx - 30, H - 70, 50, 30, Dark);
				DrawRect(Px, W, H, Cx - 50, H - 45, 100, 35, Hand);
				break;
			case 5: // plasma rifle - tech panel
				DrawRect(Px, W, H, Cx - 50, H - 110, 130, 35, Metal);
				DrawRect(Px, W, H, Cx - 45, H - 105, 50, 25, FColor(60, 180, 255, 255));
				DrawRect(Px, W, H, Cx - 30, H - 65, 50, 30, Dark);
				DrawRect(Px, W, H, Cx - 50, H - 40, 100, 35, Hand);
				break;
			case 6: // BFG
				DrawRect(Px, W, H, Cx - 70, H - 120, 160, 50, Metal);
				DrawCircle(Px, W, H, Cx + 50, H - 95, 25, FColor(120, 255, 70, 255));
				DrawRect(Px, W, H, Cx - 40, H - 60, 80, 35, Dark);
				DrawRect(Px, W, H, Cx - 60, H - 35, 120, 35, Hand);
				break;
			case 7: // muzzle flash overlay
				DrawCircle(Px, W, H, Cx + 5, H - 110, 35, FColor(255, 240, 100, 200));
				DrawCircle(Px, W, H, Cx + 5, H - 110, 22, FColor(255, 255, 200, 255));
				break;
			}
			return CreateTex(W, H, Px, false);
		}

		// ─────────────────────────────────────────────────────────────
		// Lazy caches — built once by FDoomTextures::EnsureBuilt(); every public accessor
		// routes through it, so consumers never observe the unbuilt state. TStrongObjectPtr
		// pins the transient textures against GC without reflection.
		// ─────────────────────────────────────────────────────────────

		bool GBuilt = false;
		TStrongObjectPtr<UTexture2D> GWalls[W_COUNT];
		TStrongObjectPtr<UTexture2D> GWallsTiled[W_COUNT];
		TStrongObjectPtr<UTexture2D> GFloors[F_COUNT];
		TStrongObjectPtr<UTexture2D> GSprites[S_COUNT];
		TStrongObjectPtr<UTexture2D> GSky;
		TStrongObjectPtr<UTexture2D> GFaces[8];
		TStrongObjectPtr<UTexture2D> GWeapons[8];
	} // namespace

	void FDoomTextures::EnsureBuilt()
	{
		if (GBuilt)
		{
			return;
		}
		GBuilt = true;

		// One generator run per wall: the same buffer feeds the base 64x64 texture and the
		// 64x256 WallTiled strip (identical stacked copies — the RNG is NOT re-run).
		for (int32 I = 0; I < W_COUNT; ++I)
		{
			const TArray<FColor> WallPx = BuildWallPx(I);
			GWalls[I].Reset(CreateTex(TEX_W, TEX_H, WallPx, false));
			GWallsTiled[I].Reset(MakeTiled(WallPx));
		}

		GFloors[F_CONCRETE].Reset(MakeNoiseTile(/*Seed*/ 31, 90, 88, 92, 25));
		GFloors[F_TILE].Reset(MakeTileFloor(/*Seed*/ 37));
		GFloors[F_GRASS].Reset(MakeNoiseTile(/*Seed*/ 41, 60, 90, 50, 35));
		GFloors[F_LAVA].Reset(MakeNoiseTile(/*Seed*/ 43, 200, 80, 30, 60));
		GFloors[F_BLOOD].Reset(MakeNoiseTile(/*Seed*/ 47, 130, 20, 25, 40));
		GFloors[F_NUKAGE].Reset(MakeNoiseTile(/*Seed*/ 53, 110, 200, 60, 50));
		GFloors[F_BLUE].Reset(MakeNoiseTile(/*Seed*/ 59, 50, 90, 210, 35));

		GSky.Reset(MakeSky());

		GSprites[S_IMP].Reset(MakeMonster(FColor(140, 70, 35, 255), FColor(220, 110, 30, 255), /*bEyes*/ true));
		GSprites[S_DEMON].Reset(
			MakeMonster(FColor(150, 80, 60, 255), FColor(220, 50, 40, 255), /*bEyes*/ true, /*bFangs*/ true));
		GSprites[S_BARON].Reset(MakeMonster(FColor(180, 130, 60, 255), FColor(120, 220, 80, 255), /*bEyes*/ true,
											/*bFangs*/ false, /*bHorns*/ true));
		GSprites[S_CACO].Reset(MakeCacoSprite());
		GSprites[S_LOSTSOUL].Reset(MakeLostSoul());
		GSprites[S_CORPSE].Reset(MakeCorpse());
		GSprites[S_FIREBALL].Reset(MakeBall(FColor(255, 140, 30, 255), FColor(255, 230, 80, 255)));
		GSprites[S_ROCKET_PROJ].Reset(MakeRocketSprite());
		GSprites[S_PLASMA].Reset(MakeBall(FColor(60, 180, 255, 255), FColor(220, 240, 255, 255)));
		GSprites[S_BFG_PROJ].Reset(MakeBall(FColor(120, 255, 70, 255), FColor(240, 255, 200, 255)));
		GSprites[S_EXPLOSION].Reset(MakeExplosionFrame());
		GSprites[S_HEALTH].Reset(MakePickup(FColor(220, 30, 30, 255), TEXT('+')));
		GSprites[S_ARMOR].Reset(MakePickup(FColor(40, 90, 220, 255), TEXT('A')));
		GSprites[S_AMMO_BULLETS].Reset(MakePickup(FColor(220, 200, 40, 255), TEXT('B')));
		GSprites[S_AMMO_SHELLS].Reset(MakePickup(FColor(220, 160, 40, 255), TEXT('S')));
		GSprites[S_AMMO_ROCKETS].Reset(MakePickup(FColor(180, 80, 40, 255), TEXT('R')));
		GSprites[S_AMMO_CELLS].Reset(MakePickup(FColor(80, 200, 220, 255), TEXT('C')));
		GSprites[S_PICKUP_SHOTGUN].Reset(MakePickup(FColor(140, 100, 60, 255), TEXT('G')));
		GSprites[S_PICKUP_CHAIN].Reset(MakePickup(FColor(120, 120, 130, 255), TEXT('H')));
		GSprites[S_PICKUP_ROCKET].Reset(MakePickup(FColor(160, 70, 50, 255), TEXT('L')));
		GSprites[S_PICKUP_PLASMA].Reset(MakePickup(FColor(80, 180, 220, 255), TEXT('P')));
		GSprites[S_PICKUP_BFG].Reset(MakePickup(FColor(120, 220, 80, 255), TEXT('*')));
		GSprites[S_KEY_BLUE].Reset(MakeKey(FColor(60, 110, 220, 255)));
		GSprites[S_KEY_YELLOW].Reset(MakeKey(FColor(220, 200, 60, 255)));
		GSprites[S_KEY_RED].Reset(MakeKey(FColor(220, 60, 50, 255)));
		GSprites[S_BARREL].Reset(MakeBarrel());
		GSprites[S_LIGHT].Reset(MakeLight());

		for (int32 I = 0; I < 8; ++I)
		{
			GFaces[I].Reset(MakeFace(I));
		}

		GWeapons[0].Reset(MakeWeaponSprite(0)); // fist
		GWeapons[1].Reset(MakeWeaponSprite(1)); // pistol
		GWeapons[2].Reset(MakeWeaponSprite(2)); // shotgun
		GWeapons[3].Reset(MakeWeaponSprite(3)); // chaingun
		GWeapons[4].Reset(MakeWeaponSprite(4)); // rocket
		GWeapons[5].Reset(MakeWeaponSprite(5)); // plasma
		GWeapons[6].Reset(MakeWeaponSprite(6)); // bfg
		GWeapons[7].Reset(MakeWeaponSprite(7)); // muzzle flash overlay
	}

	UTexture2D* FDoomTextures::Wall(int32 Index)
	{
		EnsureBuilt();
		check(Index >= 0 && Index < W_COUNT);
		return GWalls[Index].Get();
	}

	UTexture2D* FDoomTextures::WallTiled(int32 Index)
	{
		EnsureBuilt();
		check(Index >= 0 && Index < W_COUNT);
		return GWallsTiled[Index].Get();
	}

	UTexture2D* FDoomTextures::Floor(int32 Index)
	{
		EnsureBuilt();
		check(Index >= 0 && Index < F_COUNT);
		return GFloors[Index].Get();
	}

	UTexture2D* FDoomTextures::Sprite(int32 Index)
	{
		EnsureBuilt();
		check(Index >= 0 && Index < S_COUNT);
		return GSprites[Index].Get();
	}

	UTexture2D* FDoomTextures::Sky()
	{
		EnsureBuilt();
		return GSky.Get();
	}

	UTexture2D* FDoomTextures::Face(int32 Index)
	{
		EnsureBuilt();
		check(Index >= 0 && Index < 8);
		return GFaces[Index].Get();
	}

	UTexture2D* FDoomTextures::Weapon(int32 Index)
	{
		EnsureBuilt();
		check(Index >= 0 && Index < 8);
		return GWeapons[Index].Get();
	}
} // namespace RuiDoom
