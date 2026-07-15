// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Doom demo — screen geometry builders. See DoomScreenGeometry.h for the model and the pass
// inventory. Sources: Unity `DoomGameScreenLogic.uitkx` (the list builders + every number)
// merged with the per-quad math from `DoomGameScreen.uitkx`'s @foreach passes; the Godot
// sibling's `doom_game_screen_logic.gd` contributes the documented non-z improvements
// (merged band runs, key schemes, sprite run-clipping, MIN_WALL_SHADE, x snapping).

// Own header first (UBT IWYU rule); it includes Doom/DoomTypes.h as ITS first include, so
// the MAX_PITCH engine-macro undef still precedes everything else here.
#include "Doom/DoomScreenGeometry.h"

#include "Doom/DoomGameLogic.h"
#include "Doom/DoomTextures.h"

#include "Engine/Texture2D.h"

namespace RuiDoom
{
	// The original's private near-plane constant for tracer clipping.
	static constexpr float TRACER_NEAR = 0.15f;

	void FDoomFrameGeometry::Reset()
	{
		Sky.Reset();
		FloorBackstop.Reset();
		CeilingBands.Reset();
		FloorBands.Reset();
		FloorRims.Reset();
		ExtraSegs.Reset();
		RiserRims.Reset();
		Walls.Reset();
		Sprites.Reset();
		Tracers.Reset();
		Overlay.Reset();
	}

	float SpriteScale(EMobjKind K)
	{
		switch (K)
		{
		case EMobjKind::Baron:
			return 1.6f;
		case EMobjKind::Cacodemon:
			return 1.4f;
		case EMobjKind::Demon:
			return 1.2f;
		case EMobjKind::Barrel:
			return 0.8f;
		case EMobjKind::PickupHealth:
		case EMobjKind::PickupArmor:
		case EMobjKind::PickupArmorBlue:
		case EMobjKind::PickupBullets:
		case EMobjKind::PickupShells:
		case EMobjKind::PickupRockets:
		case EMobjKind::PickupCells:
			return 0.6f;
		case EMobjKind::KeyBlue:
		case EMobjKind::KeyYellow:
		case EMobjKind::KeyRed:
			return 0.55f;
		case EMobjKind::ImpFireball:
		case EMobjKind::BaronBall:
		case EMobjKind::CacoBall:
		case EMobjKind::PlasmaProj:
		case EMobjKind::RocketProj:
		case EMobjKind::BfgProj:
			return 0.6f;
		case EMobjKind::Explosion:
			return 1.6f;
		default:
			return 1.0f;
		}
	}

	float SpriteVerticalAnchor(EMobjKind K)
	{
		if (K == EMobjKind::Cacodemon)
		{
			return 0.f;
		}
		if (K == EMobjKind::LostSoul)
		{
			return -0.05f;
		}
		if (IsProjectile(K))
		{
			return -0.05f;
		}
		if (K == EMobjKind::Explosion)
		{
			return -0.05f;
		}
		return 0.15f;
	}

	void BuildSpriteList(const FGameState& State, const FVector2D& ViewportSize, TArray<FSpriteEntry>& Out)
	{
		Out.Reset();
		const FPlayerState& P = State.Player;
		const float ViewportW = static_cast<float>(ViewportSize.X);
		const float ViewportH = static_cast<float>(ViewportSize.Y);
		const float CosA = FMath::Cos(P.Angle);
		const float SinA = FMath::Sin(P.Angle);
		const float HalfH = ViewportH * 0.5f;
		const float PlaneScale = HalfH / FMath::Tan(C::HALF_FOV);
		const TArray<float>& Depth = State.Frame.DepthBuffer;
		const float WidthScale = ViewportW / static_cast<float>(C::VIEW_W);
		// Phase 7 (original): eye Z so sprites reproject correctly when the player jumps/crouches.
		const float ViewZ = P.Z + (P.ViewHeight <= 0.f ? 0.6f : P.ViewHeight);

		const int32 Last = FMath::Min(State.MobjCount, State.Mobjs.Num() - 1);
		for (int32 i = 1; i <= Last; ++i)
		{
			const FMobj& M = State.Mobjs[i];
			if (M.Id == 0)
			{
				continue;
			}
			if (M.Kind == EMobjKind::Light)
			{
				continue;
			}
			if (IsMonster(M.Kind) && M.State == EAIState::Dead)
			{
				continue;
			}

			const float Dx = M.X - P.X;
			const float Dy = M.Y - P.Y;
			// forward = (cos a, sin a); right = (-sin a, cos a)
			const float Ty = Dx * CosA + Dy * SinA;	 // depth along view direction
			const float Tx = -Dx * SinA + Dy * CosA; // lateral, +right
			if (Ty < 0.2f)
			{
				continue;
			}

			const float ScreenX = (ViewportW * 0.5f) + (Tx / Ty) * PlaneScale;
			const float SpriteH = (1.0f / Ty) * PlaneScale * SpriteScale(M.Kind);
			// Sprite anchor: feet (M.Z) for floor-standing things, slight lift for floating
			// mobjs — combined into world Z relative to ViewZ.
			const float AnchorWorldZ = M.Z + SpriteVerticalAnchor(M.Kind);
			const float ScreenY = (ViewportH * 0.5f + P.Pitch) - ((AnchorWorldZ - ViewZ) / Ty) * PlaneScale;

			const int32 Dcol = static_cast<int32>(ScreenX / WidthScale);
			if (Dcol >= 0 && Dcol < Depth.Num() && Ty > Depth[Dcol] + 0.05f)
			{
				continue;
			}
			// Phase 8 (original): floor-step occlusion. If the column's first riser sits in
			// front of the sprite AND the sprite's anchor Z is below the riser top, the riser
			// hides the sprite. Stops monsters from showing through stair treads /
			// second-story plateaus.
			if (Dcol >= 0 && Dcol < State.Frame.Columns.Num())
			{
				const FColumnInfo& Ci = State.Frame.Columns[Dcol];
				if (Ty > Ci.FloorOccluderDist + 0.02f && AnchorWorldZ < Ci.FloorOccluderZ - 0.05f)
				{
					continue;
				}
				// Phase 9 (original): looking-UP occlusion against a ceiling-slab underside.
				// Mirror of the floor-occluder test: if the column has a slab bottom in front
				// of the sprite AND the sprite's anchor sits at or above the slab bottom, the
				// slab hides it.
				if (Ty > Ci.CeilingOccluderDist + 0.02f && AnchorWorldZ > Ci.CeilingOccluderZ - 0.05f)
				{
					continue;
				}
				// Phase 8 (original): looking-DOWN occlusion. The first floor band in the ray
				// is the player's own sector floor; its TopPx is the cliff/portal silhouette.
				// A sprite in a sector with LOWER floor than that — i.e. the courtyard seen
				// over a plateau edge — is partially or fully hidden by the upper floor's
				// visible region. Cull when the sprite anchor projects BELOW (greater ScreenY
				// than) that silhouette AND the sprite floor is below the upper-band floor.
				if (Ci.FloorBands.Num() > 0)
				{
					const FFloorBand* B0 = Ci.FloorBands[0];
					// Projected silhouette of the upper floor's far edge at this column.
					const float SilhY = B0->TopPx;
					if (AnchorWorldZ < B0->FloorZ - 0.1f && ScreenY > SilhY + 1.f)
					{
						continue;
					}
				}
			}

			const float LightFade = FMath::Clamp(1.f - Ty / 16.f, 0.f, 1.f);
			const float Light = 0.35f + 0.65f * LightFade;

			FSpriteEntry& E = Out.AddDefaulted_GetRef();
			E.Id = M.Id;
			E.SpriteIdx = SpriteIndexForMobj(M.Kind);
			E.ScreenX = ScreenX;
			E.ScreenY = ScreenY;
			E.Size = SpriteH;
			E.Distance = Ty;
			E.Tint = TintForMobj(M.Kind, M.State);
			E.Light = Light;
		}
		// Far -> near, the ONE pre-emission sort the original performs (never re-sort quads).
		Out.Sort([](const FSpriteEntry& A, const FSpriteEntry& B) { return A.Distance > B.Distance; });
	}

	void BuildTracerList(const FGameState& State, const FVector2D& ViewportSize, TArray<FTracerEntry>& Out)
	{
		Out.Reset();
		if (State.Tracers.Num() == 0)
		{
			return;
		}
		const FPlayerState& P = State.Player;
		const float ViewportW = static_cast<float>(ViewportSize.X);
		const float ViewportH = static_cast<float>(ViewportSize.Y);
		const float CosA = FMath::Cos(P.Angle);
		const float SinA = FMath::Sin(P.Angle);
		const float HalfH = ViewportH * 0.5f;
		const float PlaneScale = HalfH / FMath::Tan(C::HALF_FOV);
		const float Horizon = HalfH + P.Pitch + P.ViewShiftPx;
		const float ViewZ = P.Z + (P.ViewHeight <= 0.f ? 0.6f : P.ViewHeight);

		for (int32 i = 0; i < State.Tracers.Num(); ++i)
		{
			const FTracer& T = State.Tracers[i];
			if (T.AgeMs >= C::TRACER_LIFE_MS)
			{
				continue;
			}

			// Camera space: forward = (cos a, sin a); right = (-sin a, cos a).
			const float Adx = T.Ax - P.X;
			const float Ady = T.Ay - P.Y;
			const float Bdx = T.Bx - P.X;
			const float Bdy = T.By - P.Y;
			float Ay = Adx * CosA + Ady * SinA;	 // depth
			float Ax = -Adx * SinA + Ady * CosA; // lateral
			float By = Bdx * CosA + Bdy * SinA;
			float Bx = -Bdx * SinA + Bdy * CosA;
			float Az = T.Az;
			float Bz = T.Bz;

			// Both endpoints behind near plane → skip.
			if (Ay < TRACER_NEAR && By < TRACER_NEAR)
			{
				continue;
			}
			// Clip the segment against the near plane in camera space.
			if (Ay < TRACER_NEAR)
			{
				const float K = (TRACER_NEAR - Ay) / (By - Ay);
				Ax += (Bx - Ax) * K;
				Az += (Bz - Az) * K;
				Ay = TRACER_NEAR;
			}
			else if (By < TRACER_NEAR)
			{
				const float K = (TRACER_NEAR - By) / (Ay - By);
				Bx += (Ax - Bx) * K;
				Bz += (Az - Bz) * K;
				By = TRACER_NEAR;
			}

			const float ScreenAx = (ViewportW * 0.5f) + (Ax / Ay) * PlaneScale;
			const float ScreenAy = Horizon - ((Az - ViewZ) / Ay) * PlaneScale;
			const float ScreenBx = (ViewportW * 0.5f) + (Bx / By) * PlaneScale;
			const float ScreenBy = Horizon - ((Bz - ViewZ) / By) * PlaneScale;

			const float Ddx = ScreenBx - ScreenAx;
			const float Ddy = ScreenBy - ScreenAy;
			const float Length = FMath::Sqrt(Ddx * Ddx + Ddy * Ddy);
			if (Length < 4.f)
			{
				continue; // too short to read — muzzle flash covers it
			}
			const float AngleDeg = FMath::Atan2(Ddy, Ddx) * (180.f / UE_PI);
			const float Alpha = 1.f - (T.AgeMs / C::TRACER_LIFE_MS);

			FTracerEntry& E = Out.AddDefaulted_GetRef();
			E.Slot = i;
			E.Left = ScreenAx;
			E.Top = ScreenAy - C::TRACER_THICKNESS_PX * 0.5f;
			E.Length = Length;
			E.AngleDeg = AngleDeg;
			E.Alpha = Alpha;
			E.ColorIdx = T.ColorIdx;
		}
	}

	// UVRegion window into the pre-tiled 64x256 wall texture for one strip (the spike's
	// verdict, DOOM_DEMO_PLAN §1b). U selects the texel-column slice; V selects the
	// fractional world-repeat range. One world-Z unit = one 64-texel tile = `TexPx` screen px
	// (TexPx = ViewportH / distance — the Unity BackgroundSize/RepeatY math), so a strip
	// `SegHeightPx` tall spans SegHeightPx/TexPx repeats. The raycaster's TexOffsetPx (screen
	// px the top was clipped by, negative) pegs texel row 0 to the UNCLIPPED wall top; whole
	// repeats are folded away (the 4 stacked copies are identical) so V stays inside [0,1].
	static FBox2f WallUVRegion(const FWallSeg& Seg, float SegHeightPx, float ViewportH)
	{
		const int32 TexCol = FMath::Clamp(FMath::FloorToInt32(Seg.TexU * TEX_W), 0, TEX_W - 1);
		const float U0 = TexCol / static_cast<float>(TEX_W);
		const float U1 = (TexCol + 1) / static_cast<float>(TEX_W);
		const float TexPx = ViewportH / FMath::Max(0.1f, Seg.Distance);
		const float Tiles = FMath::Max(0.f, SegHeightPx / TexPx); // world repeats spanned
		float VTop = FMath::Fmod(-Seg.TexOffsetPx / TexPx, 1.f);
		if (VTop < 0.f)
		{
			VTop += 1.f;
		}
		const float V0 = VTop / static_cast<float>(WALL_TILE_REPEATS);
		const float V1 = FMath::Min(1.f, V0 + Tiles / static_cast<float>(WALL_TILE_REPEATS));
		return FBox2f(FVector2f(U0, V0), FVector2f(U1, V1));
	}

	void BuildFrameGeometry(const FGameState& State, FDoomBrushPool& Brushes, const FVector2D& ViewportSize,
							FDoomFrameGeometry& Out)
	{
		Out.Reset();
		const FPlayerState& P = State.Player;
		const float ViewportW = static_cast<float>(ViewportSize.X);
		const float ViewportH = static_cast<float>(ViewportSize.Y);
		const float StripW = ViewportW / static_cast<float>(C::VIEW_W);
		const int32 NumCols = FMath::Min(C::VIEW_W, State.Frame.Columns.Num());
		const TSharedRef<FSlateBrush>& Solid = FDoomBrushPool::Solid();

		// ── Pass 1: Sky ── the Unity markup's scrolling BackgroundPosition window, mapped to
		// a UVRegion into the 512x100 wrap-around sky strip. Background frame: BgW x BgH px
		// (VIEWPORT_W*1.5 x VIEWPORT_H*0.5+250), scrolled left by Angle/(2π)*BgW px. Clamp
		// addressing forbids U outside [0,1], so the window splits into two quads at the wrap.
		{
			const float SkyH = ViewportH * 0.5f + P.Pitch + P.ViewShiftPx;
			if (SkyH >= 1.f)
			{
				const float BgW = ViewportW * 1.5f;
				const float BgH = ViewportH * 0.5f + 250.f;
				float Scroll = FMath::Fmod((P.Angle / (2.f * UE_PI)) * BgW, BgW);
				if (Scroll < 0.f)
				{
					Scroll += BgW;
				}
				const float U0 = Scroll / BgW;
				const float USpan = ViewportW / BgW;
				const float V1 = FMath::Clamp(SkyH / BgH, 0.f, 1.f);
				UTexture2D* SkyTex = FDoomTextures::Sky();
				if (U0 + USpan <= 1.f)
				{
					FDoomQuad& Q = Out.Sky.AddDefaulted_GetRef();
					Q.Key = FName(TEXT("sky0"));
					Q.Position = FVector2D(0.f, 0.f);
					Q.Size = FVector2D(ViewportW, SkyH);
					Q.Brush = Brushes.GetTextured(Q.Key, SkyTex, FBox2f(FVector2f(U0, 0.f), FVector2f(U0 + USpan, V1)));
				}
				else
				{
					const float WrapX = (1.f - U0) * BgW;
					FDoomQuad& Q0 = Out.Sky.AddDefaulted_GetRef();
					Q0.Key = FName(TEXT("sky0"));
					Q0.Position = FVector2D(0.f, 0.f);
					Q0.Size = FVector2D(WrapX, SkyH);
					Q0.Brush = Brushes.GetTextured(Q0.Key, SkyTex, FBox2f(FVector2f(U0, 0.f), FVector2f(1.f, V1)));
					FDoomQuad& Q1 = Out.Sky.AddDefaulted_GetRef();
					Q1.Key = FName(TEXT("sky1"));
					Q1.Position = FVector2D(WrapX, 0.f);
					Q1.Size = FVector2D(ViewportW - WrapX, SkyH);
					Q1.Brush = Brushes.GetTextured(Q1.Key, SkyTex,
												   FBox2f(FVector2f(0.f, 0.f), FVector2f(U0 + USpan - 1.f, V1)));
				}
			}
		}

		// ── Pass 2: Floor backstop ── solid ground-color rect from horizon to bottom so any
		// seam between merged floor bands reveals ground, not the dark void.
		{
			FDoomQuad& Q = Out.FloorBackstop.AddDefaulted_GetRef();
			Q.Key = FName(TEXT("floorbk"));
			Q.Position = FVector2D(0.f, ViewportH * 0.5f + P.Pitch);
			Q.Size = FVector2D(ViewportW, ViewportH);
			Q.Brush = Solid;
			Q.Tint = FLinearColor(0.34f, 0.29f, 0.22f, 1.f);
		}

		// ── Passes 3-5: ceiling bands, floor bands, floor rims ── merged same-slab runs per
		// column (GODOT ADDITION), emitted FAR -> NEAR per column so the closer band lands
		// later in the child list and paints OVER farther bands (Unity's deterministic
		// per-column reverse loop — no global sort: an unstable sort on TopPx ties would
		// randomly invert order per column and produce flicker / covered closer bands).
		// Cross-column ordering is irrelevant because bands at different X don't overlap.
		for (int32 i = 0; i < NumCols; ++i)
		{
			const FColumnInfo& Ci = State.Frame.Columns[i];
			const float ColX = i * StripW;

			// Ceiling bands (painted BEFORE floor bands and walls; sky sectors emit none, the
			// sky backdrop shows through).
			{
				TArray<FDoomFrameGeometry::FBandGroup>& Groups = Out.BandScratch;
				Groups.Reset();
				const TArray<FCeilingBand*>& Bands = Ci.CeilingBands;
				int32 G = 0;
				while (G < Bands.Num())
				{
					const int32 Slab = FMath::RoundToInt32(Bands[G]->CeilingZ * 5.f);
					FDoomFrameGeometry::FBandGroup Grp;
					Grp.Top = TNumericLimits<float>::Max();
					Grp.Bot = -TNumericLimits<float>::Max();
					const FCeilingBand* Near = Bands[G];
					int32 E = G;
					while (E < Bands.Num() && FMath::RoundToInt32(Bands[E]->CeilingZ * 5.f) == Slab)
					{
						const FCeilingBand* B = Bands[E];
						Grp.Top = FMath::Min(Grp.Top, B->TopPx);
						Grp.Bot = FMath::Max(Grp.Bot, B->BotPx);
						if (B->Distance < Near->Distance)
						{
							Near = B;
						}
						++E;
					}
					G = E;
					Grp.Z = Near->CeilingZ;
					Grp.Light = Near->Light;
					Grp.Distance = Near->Distance;
					Groups.Add(Grp);
				}
				for (int32 Gi = Groups.Num() - 1; Gi >= 0; --Gi)
				{
					const FDoomFrameGeometry::FBandGroup& Grp = Groups[Gi];
					const float CTop = Grp.Top - 2.f;		 // 2px overdraw upward to seal sky seam
					const float Ch = (Grp.Bot + 4.f) - CTop; // 4px down so wall texels overlap (no black hairline)
					if (Ch < 1.f)
					{
						continue;
					}
					const float CLift = FMath::Clamp(Grp.Z * 0.04f, 0.f, 0.20f);
					const float CLight = Grp.Light / 255.f;
					// Ceiling base color: cool grey-blue, brightened by sector light. Always
					// >= backstop brightness so distant bands don't read as holes.
					const float CBright = FMath::Max(1.f, 0.55f + CLight * 0.85f);
					FDoomQuad& Q = Out.CeilingBands.AddDefaulted_GetRef();
					Q.Key = FName(*FString::Printf(TEXT("cb%d_%d"), i, Gi));
					Q.Position = FVector2D(FMath::FloorToFloat(ColX - 1.f), CTop);
					Q.Size = FVector2D(StripW + 2.f, Ch);
					Q.Brush = Solid;
					Q.Tint = FLinearColor(FMath::Clamp((0.22f + CLift) * CBright, 0.f, 1.f),
										  FMath::Clamp((0.22f + CLift) * CBright, 0.f, 1.f),
										  FMath::Clamp((0.26f + CLift) * CBright, 0.f, 1.f), 1.f);
				}
			}

			// Floor bands + step-down rims.
			{
				TArray<FDoomFrameGeometry::FBandGroup>& Groups = Out.BandScratch;
				Groups.Reset();
				const TArray<FFloorBand*>& Bands = Ci.FloorBands;
				int32 G = 0;
				while (G < Bands.Num())
				{
					const int32 Slab = FMath::RoundToInt32(Bands[G]->FloorZ * 5.f);
					FDoomFrameGeometry::FBandGroup Grp;
					Grp.Top = TNumericLimits<float>::Max();
					Grp.Bot = -TNumericLimits<float>::Max();
					const FFloorBand* Near = Bands[G];
					int32 E = G;
					while (E < Bands.Num() && FMath::RoundToInt32(Bands[E]->FloorZ * 5.f) == Slab)
					{
						const FFloorBand* B = Bands[E];
						Grp.Top = FMath::Min(Grp.Top, B->TopPx);
						Grp.Bot = FMath::Max(Grp.Bot, B->BotPx);
						if (B->Distance < Near->Distance)
						{
							Near = B;
						}
						// Far-most band's flag survives — the group's far edge is its far edge.
						Grp.bRimAtFar = B->RimAtFar;
						++E;
					}
					G = E;
					Grp.Z = Near->FloorZ;
					Grp.Light = Near->Light;
					Grp.Distance = Near->Distance;
					Groups.Add(Grp);
				}
				for (int32 Gi = Groups.Num() - 1; Gi >= 0; --Gi)
				{
					const FDoomFrameGeometry::FBandGroup& Grp = Groups[Gi];
					// Step-down rim: the visible silhouette where a platform ends is the TOP
					// edge of the front floor band at the portal hop into a lower sector.
					// Independent of the band's own h-cull (Unity emits rims unconditionally).
					if (Grp.bRimAtFar)
					{
						FDoomQuad& R = Out.FloorRims.AddDefaulted_GetRef();
						R.Key = FName(*FString::Printf(TEXT("frim%d_%d"), i, Gi));
						R.Position = FVector2D(ColX - 0.5f, Grp.Top - 1.f);
						R.Size = FVector2D(StripW + 1.f, 2.f);
						R.Brush = Solid;
						R.Tint = FLinearColor(0.95f, 0.85f, 0.55f, 1.f);
					}
					const float Fh = (Grp.Bot - Grp.Top) + 4.f; // bottom-extend 4px to seal sub-pixel
					if (Fh < 1.f)
					{
						continue;
					}
					const float Lift = FMath::Clamp(Grp.Z * 0.10f, -0.1f, 0.25f);
					const float LightF = Grp.Light / 255.f;
					// Floor band must NEVER render darker than the backstop floor color
					// (0.34, 0.29, 0.22) — otherwise distant bands paint as "dark holes" on
					// top of the brighter backstop, looking like see-through. Light
					// attenuation lifts brightness above the backstop near, fades toward the
					// backstop level far away.
					const float Bright = FMath::Max(1.f, 0.6f + LightF * 0.8f);
					FDoomQuad& Q = Out.FloorBands.AddDefaulted_GetRef();
					Q.Key = FName(*FString::Printf(TEXT("fb%d_%d"), i, Gi));
					Q.Position = FVector2D(FMath::FloorToFloat(ColX - 0.5f), Grp.Top);
					Q.Size = FVector2D(StripW + 2.f, Fh);
					Q.Brush = Solid;
					Q.Tint = FLinearColor(FMath::Clamp((0.34f + Lift) * Bright, 0.f, 1.f),
										  FMath::Clamp((0.29f + Lift) * Bright, 0.f, 1.f),
										  FMath::Clamp((0.22f + Lift * 0.5f) * Bright, 0.f, 1.f), 1.f);
				}
			}
		}

		// ── Passes 6-7: portal/stair extra segs + riser rims ── rendered BEFORE the main
		// strips so the closer terminal wall stays on top. Rim: chalk-line at the top edge of
		// every step riser; adjacent ray columns share the same step boundary so these stack
		// into one continuous outline along the platform's outer rim.
		for (int32 i = 0; i < NumCols; ++i)
		{
			const TArray<FWallSeg*>& Extras = State.Frame.Columns[i].Extras;
			for (int32 j = 0; j < Extras.Num(); ++j)
			{
				const FWallSeg& ECol = *Extras[j];
				const float Eh = ECol.BotPx - ECol.TopPx;
				if (Eh < 1.f)
				{
					continue;
				}
				// Floor min-brightness so distant step risers don't go black (MIN_WALL_SHADE,
				// the Godot knob; the Unity original used 0.45 here).
				const float ELightF = FMath::Max(MIN_WALL_SHADE, ECol.LightLevel / 255.f);
				FDoomQuad& Q = Out.ExtraSegs.AddDefaulted_GetRef();
				Q.Key = FName(*FString::Printf(TEXT("x%d_%d"), i, j));
				Q.Position = FVector2D(FMath::FloorToFloat(i * StripW), ECol.TopPx);
				Q.Size = FVector2D(StripW + 2.f, Eh);
				Q.Brush = Brushes.GetTextured(Q.Key, FDoomTextures::WallTiled(ECol.WallTexIdx),
											  WallUVRegion(ECol, Eh, ViewportH));
				Q.Tint = FLinearColor(ELightF, ELightF, ELightF, 1.f);
				if (ECol.IsRiser)
				{
					FDoomQuad& R = Out.RiserRims.AddDefaulted_GetRef();
					R.Key = FName(*FString::Printf(TEXT("rim%d_%d"), i, j));
					R.Position = FVector2D(i * StripW, ECol.TopPx);
					R.Size = FVector2D(StripW + 2.f, 2.f);
					R.Brush = Solid;
					R.Tint = FLinearColor(0.95f, 0.85f, 0.55f, 1.f);
				}
			}
		}

		// ── Pass 8: the terminal wall strips ── one per view column; sky hits emit nothing
		// (the sky backdrop shows through).
		for (int32 i = 0; i < NumCols; ++i)
		{
			const FWallSeg& Col = State.Frame.Columns[i].Main;
			if (Col.IsSky)
			{
				continue;
			}
			const float H = Col.BotPx - Col.TopPx;
			const float LightF = Col.LightLevel / 255.f;
			const float VertDim = Col.HitVertical ? 0.85f : 1.f;
			const float Shade = FMath::Max(MIN_WALL_SHADE, LightF * VertDim);
			FDoomQuad& Q = Out.Walls.AddDefaulted_GetRef();
			Q.Key = FName(*FString::Printf(TEXT("w%d"), i));
			Q.Position = FVector2D(FMath::FloorToFloat(i * StripW), Col.TopPx);
			Q.Size = FVector2D(StripW + 2.f, H);
			Q.Brush =
				Brushes.GetTextured(Q.Key, FDoomTextures::WallTiled(Col.WallTexIdx), WallUVRegion(Col, H, ViewportH));
			Q.Tint = FLinearColor(Shade, Shade, Shade, 1.f);
		}

		// ── Pass 9: sprite billboards ── far -> near (BuildSpriteList's sort). A billboard is
		// ONE flat quad, so painter's order alone can't clip it per column: a monster whose
		// edge peeks past a doorway jamb would bleed its whole body over the nearer wall. So
		// depth-clip HERE in screen columns (GODOT ADDITION): walk the columns the sprite
		// covers, keep those where it's in front of that column's wall, and emit one quad per
		// contiguous visible run — the un-occluded slice, CUT (not squashed) at the wall edge
		// via the brush's U window. Usually 1 quad; 2 when a wall edge crosses it.
		{
			BuildSpriteList(State, ViewportSize, Out.SpriteScratch);
			const TArray<float>& Depth = State.Frame.DepthBuffer;
			for (const FSpriteEntry& E : Out.SpriteScratch)
			{
				if (E.SpriteIdx < 0 || E.SpriteIdx >= S_COUNT || E.Size <= 0.5f)
				{
					continue;
				}
				UTexture2D* Tex = FDoomTextures::Sprite(E.SpriteIdx);
				const float LeftPx = E.ScreenX - E.Size * 0.5f;
				const float RightPx = E.ScreenX + E.Size * 0.5f;
				const float TopPx = E.ScreenY - E.Size * 0.5f;
				const FLinearColor Mod(E.Tint.R / 255.f * E.Light, E.Tint.G / 255.f * E.Light,
									   E.Tint.B / 255.f * E.Light, 1.f);
				const int32 C0 = FMath::Max(0, FMath::FloorToInt32(LeftPx / StripW));
				const int32 C1 = FMath::Min(C::VIEW_W - 1, FMath::FloorToInt32((RightPx - 0.001f) / StripW));

				auto EmitRun = [&](int32 Run0, int32 Run1)
				{
					const float Gx0 = FMath::Max(LeftPx, Run0 * StripW);
					const float Gx1 = FMath::Min(RightPx, (Run1 + 1) * StripW);
					if (Gx1 <= Gx0 + 0.5f)
					{
						return;
					}
					const float U0 = (Gx0 - LeftPx) / E.Size;
					// Min one texel of window width (the Godot atlas-region max(1.0, ...)).
					const float U1 =
						FMath::Min(1.f, FMath::Max((Gx1 - LeftPx) / E.Size, U0 + 1.f / static_cast<float>(SPRITE_W)));
					FDoomQuad& Q = Out.Sprites.AddDefaulted_GetRef();
					// Keyed by sprite id + run start column — stable identity as the clip shifts.
					Q.Key = FName(*FString::Printf(TEXT("s%d_%d"), E.Id, Run0));
					Q.Position = FVector2D(Gx0, TopPx);
					Q.Size = FVector2D(Gx1 - Gx0, E.Size);
					Q.Brush = Brushes.GetTextured(Q.Key, Tex, FBox2f(FVector2f(U0, 0.f), FVector2f(U1, 1.f)));
					Q.Tint = Mod;
				};

				int32 Rs = -1;
				for (int32 ColIdx = C0; ColIdx <= C1; ++ColIdx)
				{
					const float DWall = Depth.IsValidIndex(ColIdx) ? Depth[ColIdx] : 1e9f;
					const bool bVis = E.Distance <= DWall + 0.05f;
					if (bVis && Rs < 0)
					{
						Rs = ColIdx;
					}
					else if (!bVis && Rs >= 0)
					{
						EmitRun(Rs, ColIdx - 1);
						Rs = -1;
					}
				}
				if (Rs >= 0)
				{
					EmitRun(Rs, C1);
				}
			}
		}

		// ── Pass 10: hitscan tracer streaks ── one rotated rect per live tracer, painted
		// between sprites and the weapon overlay so they read as in-world (over enemies) but
		// never cover the gun. The markup pins RenderTransformPivot (0, 0.5) — Unity's
		// TransformOrigin (0px, thickness/2).
		{
			BuildTracerList(State, ViewportSize, Out.TracerScratch);
			for (const FTracerEntry& Tr : Out.TracerScratch)
			{
				// 0 = yellow pistol/chaingun, 1 = red-orange shotgun pellet.
				const float Tcr = 1.00f;
				const float Tcg = Tr.ColorIdx == 1 ? 0.55f : 0.92f;
				const float Tcb = Tr.ColorIdx == 1 ? 0.25f : 0.45f;
				FDoomQuad& Q = Out.Tracers.AddDefaulted_GetRef();
				Q.Key = FName(*FString::Printf(TEXT("tr%d"), Tr.Slot));
				Q.Position = FVector2D(Tr.Left, Tr.Top);
				Q.Size = FVector2D(Tr.Length, C::TRACER_THICKNESS_PX);
				Q.Brush = Solid;
				Q.Tint = FLinearColor(Tcr, Tcg, Tcb, Tr.Alpha);
				Q.Angle = Tr.AngleDeg;
			}
		}

		// ── Pass 11: overlay ── weapon bob, muzzle flash, crosshair, hurt/pickup flashes.
		// All five always emitted (alpha-driven visibility) so the widget set stays constant.
		{
			const float WeaponX = (ViewportW - 256.f) * 0.5f + FMath::Sin(P.BobT) * 6.f;
			// Unity anchors Bottom = -10 + |sin(BobT)|*6 on a 192px-tall image.
			const float WeaponY = ViewportH - 192.f - (-10.f + FMath::Abs(FMath::Sin(P.BobT)) * 6.f);

			FDoomQuad& Wpn = Out.Overlay.AddDefaulted_GetRef();
			Wpn.Key = FName(TEXT("wpn"));
			Wpn.Position = FVector2D(WeaponX, WeaponY);
			Wpn.Size = FVector2D(256.f, 192.f);
			Wpn.Brush = Brushes.GetTextured(Wpn.Key, FDoomTextures::Weapon(static_cast<int32>(P.Weapon)));

			const float MuzzleAlpha = P.MuzzleFlash > 0.f ? 1.f : 0.f;
			FDoomQuad& Muzzle = Out.Overlay.AddDefaulted_GetRef();
			Muzzle.Key = FName(TEXT("muzzle"));
			Muzzle.Position = FVector2D(WeaponX, WeaponY);
			Muzzle.Size = FVector2D(256.f, 192.f);
			Muzzle.Brush = Brushes.GetTextured(Muzzle.Key, FDoomTextures::Weapon(7));
			Muzzle.Tint = FLinearColor(1.f, 1.f, 1.f, MuzzleAlpha);

			// Crosshair (the Unity original rounds it into a dot via border-radius; a solid
			// 8x8 square here — brushes carry no corner radius in this contract).
			FDoomQuad& Xhair = Out.Overlay.AddDefaulted_GetRef();
			Xhair.Key = FName(TEXT("xhair"));
			Xhair.Position = FVector2D(ViewportW * 0.5f - 4.f, ViewportH * 0.5f - 4.f);
			Xhair.Size = FVector2D(8.f, 8.f);
			Xhair.Brush = Solid;
			Xhair.Tint = FLinearColor(0.95f, 0.95f, 0.5f, 0.7f);

			FDoomQuad& Hurt = Out.Overlay.AddDefaulted_GetRef();
			Hurt.Key = FName(TEXT("hurt"));
			Hurt.Position = FVector2D(0.f, 0.f);
			Hurt.Size = FVector2D(ViewportW, ViewportH);
			Hurt.Brush = Solid;
			Hurt.Tint = FLinearColor(0.85f, 0.05f, 0.05f, P.HurtFlash * 0.6f);

			FDoomQuad& Pickup = Out.Overlay.AddDefaulted_GetRef();
			Pickup.Key = FName(TEXT("pickup"));
			Pickup.Position = FVector2D(0.f, 0.f);
			Pickup.Size = FVector2D(ViewportW, ViewportH);
			Pickup.Brush = Solid;
			Pickup.Tint = FLinearColor(0.95f, 0.85f, 0.2f, P.PickupFlash * 0.35f);
		}
	}
} // namespace RuiDoom
