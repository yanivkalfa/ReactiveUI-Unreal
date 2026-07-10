// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "SRuiCanvas.h"

void SRuiCanvas::Construct(const FArguments&)
{
}

void SRuiCanvas::SetDrawFn(TSharedPtr<FRuiDrawFn> InDrawFn)
{
	if (DrawFn != InDrawFn) // identity — the family's callback-identity repaint rule
	{
		DrawFn = MoveTemp(InDrawFn);
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void SRuiCanvas::SetRedrawKey(int64 InKey)
{
	if (RedrawKey != InKey)
	{
		RedrawKey = InKey;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void SRuiCanvas::SetCanvasDesiredSize(FVector2D InSize)
{
	if (CanvasDesiredSize != InSize)
	{
		CanvasDesiredSize = InSize;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

int32 SRuiCanvas::OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&,
						  FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const
{
	if (DrawFn.IsValid() && *DrawFn)
	{
		return (*DrawFn)(AllottedGeometry, OutDrawElements, LayerId);
	}
	return LayerId;
}

FVector2D SRuiCanvas::ComputeDesiredSize(float) const
{
	return CanvasDesiredSize;
}
