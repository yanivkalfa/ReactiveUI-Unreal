// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-022 / D-17 — asset brushes. An FSlateBrush that points at a UObject (UTexture2D,
// UMaterialInterface, ...) carries only a RAW ResourceObject pointer; nothing roots it, so GC
// would collect the asset out from under Slate mid-paint. RUI::Umg::MakeAssetBrush builds the
// brush AND registers it with a process-wide FGCObject that keeps every LIVE brush's resource
// object referenced (dead brushes — the props that owned them released — are compacted out).
//
// This lives in ReactiveUIUMG because ReactiveUICore/ReactiveUISlate are UObject-free by law
// (D-27). The brush itself is a plain SlateCore type, so ReactiveUISlate's Image/Border props
// hold it (TSharedPtr<FSlateBrush>) without ever seeing a UObject.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class UObject;

namespace RUI::Umg
{
	/**
	 * Build a GC-rooted asset brush. `ResourceObject` is a UTexture2D / UMaterialInterface / etc.
	 * `ImageSize` is the brush's desired size (0 = use the resource's native size). While the
	 * returned brush is alive, its resource object is kept referenced against GC.
	 */
	REACTIVEUIUMG_API TSharedPtr<FSlateBrush>
	MakeAssetBrush(UObject* ResourceObject, FVector2D ImageSize = FVector2D::ZeroVector,
				   FLinearColor Tint = FLinearColor::White,
				   ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image);

	/** How many live asset brushes the GC root currently tracks (test/diagnostic). */
	REACTIVEUIUMG_API int32 NumTrackedAssetBrushes();
} // namespace RUI::Umg
