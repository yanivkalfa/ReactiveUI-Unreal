// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The geometry-builder → markup contract for the Doom screen. Builders (DoomScreenGeometry)
// turn FFrameData + FGameState into flat per-pass FDoomQuad lists in PAINTER'S ORDER; the
// .uetkx screen renders them as keyed <Image> children of ONE <Canvas> — every quad is a real
// widget diffed by the reconciler (the family thesis; no custom draw).
//
// Rendering model decided by the Phase-0 spike (DOOM_DEMO_PLAN §1b):
//   · absolute placement via Canvas Slot.Position/Slot.Size (LIVE in-place slot mutation)
//   · z = child order only — passes emit in stable painter's order, never re-sorted
//   · wall strips sample a PRE-TILED 64×256 texture through a per-strip brush UVRegion
//   · solid quads (floor/ceiling bands, flashes, rims) share ONE white brush + per-widget tint

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class UTexture2D;

namespace RuiDoom
{
	/** One rendered quad. POD-ish; rebuilt per frame by the builders (TArray reuse, no churn). */
	struct FDoomQuad
	{
		/** Stable reconciler key within its pass ("w12", "s7_3", …); NAME_None = unkeyed
		 *  (uniform slot-reuse groups — same widget type, count-only churn). */
		FName Key;
		FVector2D Position = FVector2D::ZeroVector; // canvas px (top-left)
		FVector2D Size = FVector2D(1.0, 1.0);		// canvas px
		/** The brush to draw (pooled per key, or FDoomBrushPool::Solid()). */
		TSharedPtr<FSlateBrush> Brush;
		/** Widget ColorAndOpacity (light level / hurt flash / tracer color ride here). */
		FLinearColor Tint = FLinearColor::White;
		/** Rotation in degrees (tracers) — 0 for everything else. */
		float Angle = 0.0f;
	};

	/**
	 * Per-key brush pool. A quad key's brush identity FLIPS between an A/B pair every acquire,
	 * so the Image prop diff always sees a pointer change and re-applies — brush-content
	 * mutations (UVRegion/texture) can never go stale behind a pointer-equality bailout.
	 * Slot position/size changes ride the Canvas in-place slot path independently.
	 */
	class FDoomBrushPool
	{
	public:
		/** The shared solid-fill brush (white; tint via the widget) — bands/rims/flashes. */
		static const TSharedRef<FSlateBrush>& Solid();

		/** Acquire the key's brush configured for `Texture` with an optional UV window
		 *  (walls: the column slice into the pre-tiled texture). Image-drawn, no tiling,
		 *  clamp addressing (the textures are created TA_Clamp). */
		TSharedRef<FSlateBrush> GetTextured(FName Key, UTexture2D* Texture, const FBox2f& UVRegion = FBox2f(ForceInit));

		/** Drop every pooled brush (level restart / teardown). */
		void Reset();

	private:
		struct FPair
		{
			TSharedPtr<FSlateBrush> A;
			TSharedPtr<FSlateBrush> B;
			bool bUseB = false;
		};
		TMap<FName, FPair> Pool;
	};
} // namespace RuiDoom
