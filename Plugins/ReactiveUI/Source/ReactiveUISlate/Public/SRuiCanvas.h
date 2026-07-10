// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// SRuiCanvas — the family's draw_fn surface (D-12's one sanctioned custom leaf): a paint
// TRAMPOLINE registered once; re-renders swap the inner function by IDENTITY and invalidate
// Paint only when it (or the redraw key) actually changed. The draw function is engine-facing
// on purpose: it receives the real Slate paint triplet and returns the new max layer id.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/** User draw function: paint into OutDrawElements at LayerId+, return the max layer used. */
using FRuiDrawFn =
	TFunction<int32(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId)>;

class REACTIVEUISLATE_API SRuiCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRuiCanvas) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Identity-compared swap (UseCallback-style shared fn); repaints only on change. */
	void SetDrawFn(TSharedPtr<FRuiDrawFn> InDrawFn);

	/** Data-version escape hatch: bump to force a repaint with the same function. */
	void SetRedrawKey(int64 InKey);

	void SetCanvasDesiredSize(FVector2D InSize);

protected:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
						  FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
						  bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	TSharedPtr<FRuiDrawFn> DrawFn;
	int64 RedrawKey = 0;
	FVector2D CanvasDesiredSize = FVector2D(100.0f, 100.0f);
};
