// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The first wrapped elements (Phase 2 step 3 — the hand-written pattern setters; the
// remaining widgets production-line onto this shape in the same phase). Props structs +
// element factories only; the adapters live in Private/RuiCoreAdapters.cpp.
//
// Text renders core FRuiTextBlockProps (RUI::TextBlock) — the GetTextElementType contract — so it
// has no props struct here.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "RuiCoreElements.h"
#include "RuiNode.h"
#include "RuiPropsBase.h"
#include "SRuiCanvas.h"			// FRuiDrawFn
#include "Styling/SlateBrush.h" // FSlateBrush (asset brushes, D-17)

/** SVerticalBox (MultiSlot). Layout comes from the children's slot.* props. */
struct REACTIVEUISLATE_API FRuiVerticalBoxProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiVerticalBoxProps, )
};

/** SHorizontalBox (MultiSlot). */
struct REACTIVEUISLATE_API FRuiHorizontalBoxProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiHorizontalBoxProps, )
};

/** SButton (SingleContent) — the event-proxy pattern widget. OnClicked participates in
 *  Equals by IDENTITY (FRuiCallback ==): a fresh closure means new props, exactly React —
 *  otherwise a bailout would keep firing a stale capture. UseCallback restores memo. */
struct REACTIVEUISLATE_API FRuiButtonProps final : public FRuiPropsBase
{
	RUI_PROP_EVENT(OnClicked, 0)
	RUI_PROP(bool, bEnabled, 1)
	RUI_PROP(FMargin, ContentPadding, 2)
	RUI_PROP(bool, bIsFocusable, 3)
	RUI_PROPS_BODY(FRuiButtonProps, RUI_EQ(OnClicked) RUI_EQ(bEnabled) RUI_EQ(ContentPadding) RUI_EQ(bIsFocusable))
};

/** SOverlay (MultiSlot; also the SRuiRoot inner panel). slot.zorder orders the slots. */
struct REACTIVEUISLATE_API FRuiOverlayProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiOverlayProps, )
};

/** SCanvas (MultiSlot) — ABSOLUTE placement: each child positions/sizes itself via
 *  `Slot.Position` + `Slot.Size` (FVector2D or "x,y" literals). Paint order = child order
 *  (SCanvas has no per-slot z; keep emission order stable — the Doom-demo container). */
struct REACTIVEUISLATE_API FRuiCanvasPanelProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiCanvasPanelProps, )
};

/** SBorder (SingleContent). Alignment values: fill|left|center|right / fill|top|center|bottom.
 *  BorderImage takes an FCoreStyle brush NAME (v1 — e.g. "WhiteBrush" for a solid fill
 *  tinted by BorderBackgroundColor; the engine default is a thin frame-type brush). Asset
 *  brushes (textures/materials) are the D-17 work. */
struct REACTIVEUISLATE_API FRuiBorderProps final : public FRuiPropsBase
{
	RUI_PROP(FMargin, Padding, 0)
	RUI_PROP(FLinearColor, BorderBackgroundColor, 1)
	RUI_PROP(FName, HAlign, 2)
	RUI_PROP(FName, VAlign, 3)
	RUI_PROP(FName, BorderImage, 4)
	RUI_PROP(TSharedPtr<FSlateBrush>, BorderImageBrush, 5) // asset brush (D-17); wins over BorderImage name
	RUI_PROPS_BODY(FRuiBorderProps, RUI_EQ(Padding) RUI_EQ(BorderBackgroundColor) RUI_EQ(HAlign) RUI_EQ(VAlign)
										RUI_EQ(BorderImage) RUI_EQ(BorderImageBrush))
};

/** SBox (SingleContent): size overrides + content alignment. Overrides are settable, not
 *  clearable (family removal semantics: plain props don't reset). */
struct REACTIVEUISLATE_API FRuiBoxProps final : public FRuiPropsBase
{
	RUI_PROP(float, WidthOverride, 0)
	RUI_PROP(float, HeightOverride, 1)
	RUI_PROP(float, MinDesiredWidth, 2)
	RUI_PROP(float, MinDesiredHeight, 3)
	RUI_PROP(float, MaxDesiredWidth, 4)
	RUI_PROP(float, MaxDesiredHeight, 5)
	RUI_PROP(FName, HAlign, 6)
	RUI_PROP(FName, VAlign, 7)
	RUI_PROPS_BODY(FRuiBoxProps,
				   RUI_EQ(WidthOverride) RUI_EQ(HeightOverride) RUI_EQ(MinDesiredWidth) RUI_EQ(MinDesiredHeight)
					   RUI_EQ(MaxDesiredWidth) RUI_EQ(MaxDesiredHeight) RUI_EQ(HAlign) RUI_EQ(VAlign))
};

/** SImage (Leaf): tint + desired size + an optional asset brush (D-17). The field is `Image` —
 *  the loyal Unreal name (SImage::SetImage), also the markup attr: `<Image Image={ Brush } />`.
 *  Build the brush ONCE with RUI::Umg::MakeAssetBrush (it GC-roots the texture/material) and
 *  pass it by identity — RUI_EQ(Image) compares the shared pointer, so wrap it in
 *  UseMemo/UseRef to avoid re-applying. (Renamed from `Brush` 2026-07-15, D-33 compliance.) */
struct REACTIVEUISLATE_API FRuiImageProps final : public FRuiPropsBase
{
	RUI_PROP(FLinearColor, ColorAndOpacity, 0)
	RUI_PROP(FVector2D, DesiredSizeOverride, 1)
	RUI_PROP(TSharedPtr<FSlateBrush>, Image, 2)
	RUI_PROPS_BODY(FRuiImageProps, RUI_EQ(ColorAndOpacity) RUI_EQ(DesiredSizeOverride) RUI_EQ(Image))
};

/** SScrollBox (MultiSlot). Orientation is runtime-settable (header-sweep verified). */
struct REACTIVEUISLATE_API FRuiScrollBoxProps final : public FRuiPropsBase
{
	RUI_PROP(FName, Orientation, 0) // "vertical" (default) | "horizontal"
	RUI_PROPS_BODY(FRuiScrollBoxProps, RUI_EQ(Orientation))
};

/** SSpacer (Leaf). */
struct REACTIVEUISLATE_API FRuiSpacerProps final : public FRuiPropsBase
{
	RUI_PROP(FVector2D, Size, 0)
	RUI_PROPS_BODY(FRuiSpacerProps, RUI_EQ(Size))
};

/** SEditableTextBox (Leaf) — THE controlled input (D-16): Text is applied skip-when-equal
 *  against the WIDGET's current text so the caret survives the typing round-trip. */
struct REACTIVEUISLATE_API FRuiEditableTextBoxProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)
	RUI_PROP(FText, HintText, 1)
	RUI_PROP(bool, bIsReadOnly, 2)
	RUI_PROP_EVENT(OnTextChanged, 3)
	RUI_PROP_EVENT(OnTextCommitted, 4)

	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiEditableTextBoxProps& Other = static_cast<const FRuiEditableTextBoxProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return SetBits == Other.SetBits && BaseFieldsEqual(Other) && TextEq(Text, Other.Text) &&
			   TextEq(HintText, Other.HintText) && bIsReadOnly == Other.bIsReadOnly &&
			   OnTextChanged == Other.OnTextChanged && OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SCheckBox (SingleContent — the label is the child). */
struct REACTIVEUISLATE_API FRuiCheckBoxProps final : public FRuiPropsBase
{
	RUI_PROP(bool, bIsChecked, 0)
	RUI_PROP_EVENT(OnCheckStateChanged, 1)
	RUI_PROPS_BODY(FRuiCheckBoxProps, RUI_EQ(bIsChecked) RUI_EQ(OnCheckStateChanged))
};

/** SSlider (Leaf). Value applies skip-when-equal (self-notifying family, D-16). */
struct REACTIVEUISLATE_API FRuiSliderProps final : public FRuiPropsBase
{
	RUI_PROP(float, Value, 0)
	RUI_PROP(float, MinValue, 1)
	RUI_PROP(float, MaxValue, 2)
	RUI_PROP_EVENT(OnValueChanged, 3)
	RUI_PROP(float, StepSize, 4)
	RUI_PROPS_BODY(FRuiSliderProps,
				   RUI_EQ(Value) RUI_EQ(MinValue) RUI_EQ(MaxValue) RUI_EQ(OnValueChanged) RUI_EQ(StepSize))
};

/** SProgressBar (Leaf). */
struct REACTIVEUISLATE_API FRuiProgressBarProps final : public FRuiPropsBase
{
	RUI_PROP(float, Percent, 0)
	RUI_PROP(FName, BarFillType, 1)
	RUI_PROPS_BODY(FRuiProgressBarProps, RUI_EQ(Percent) RUI_EQ(BarFillType))
};

/** SRuiCanvas (Leaf) — draw_fn by IDENTITY (wrap in a shared fn once; see MakeDrawFn). */
struct REACTIVEUISLATE_API FRuiCanvasProps final : public FRuiPropsBase
{
	RUI_PROP(TSharedPtr<FRuiDrawFn>, DrawFn, 0)
	RUI_PROP(int64, RedrawKey, 1)
	RUI_PROP(FVector2D, CanvasSize, 2)
	RUI_PROPS_BODY(FRuiCanvasProps, RUI_EQ(DrawFn) RUI_EQ(RedrawKey) RUI_EQ(CanvasSize))
};

// ── Batch 2 (Phase 7 step 8) — the everyday game set (WIDGET_INVENTORY.md) ─────────────────

/** SWidgetSwitcher (MultiSlot): shows exactly one child by index. WidgetIndex is a runtime
 *  setter (SetActiveWidgetIndex) — the classic tab/page panel. */
struct REACTIVEUISLATE_API FRuiWidgetSwitcherProps final : public FRuiPropsBase
{
	RUI_PROP(int32, WidgetIndex, 0)
	RUI_PROPS_BODY(FRuiWidgetSwitcherProps, RUI_EQ(WidgetIndex))
};

/** SScaleBox (SingleContent): scales its content. Stretch = none|fill|scaleToFit|scaleToFitX|
 *  scaleToFitY|scaleToFill|scaleBySafeZone; StretchDirection = both|downOnly|upOnly.
 *  HAlign/VAlign place the scaled content inside the box (default center|center). */
struct REACTIVEUISLATE_API FRuiScaleBoxProps final : public FRuiPropsBase
{
	RUI_PROP(FName, Stretch, 0)
	RUI_PROP(FName, StretchDirection, 1)
	RUI_PROP(FName, HAlign, 2)
	RUI_PROP(FName, VAlign, 3)
	RUI_PROPS_BODY(FRuiScaleBoxProps, RUI_EQ(Stretch) RUI_EQ(StretchDirection) RUI_EQ(HAlign) RUI_EQ(VAlign))
};

/** SThrobber (Leaf): a busy indicator. Animate = all|vertical|horizontal|opacity|
 *  verticalAndOpacity|none. */
struct REACTIVEUISLATE_API FRuiThrobberProps final : public FRuiPropsBase
{
	RUI_PROP(int32, NumPieces, 0)
	RUI_PROP(FName, Animate, 1)
	RUI_PROPS_BODY(FRuiThrobberProps, RUI_EQ(NumPieces) RUI_EQ(Animate))
};

/** SWrapBox (MultiSlot): flows children onto new lines. Orientation = horizontal (default) |
 *  vertical. WrapSize is the wrap threshold (ignored while bUseAllottedSize). */
struct REACTIVEUISLATE_API FRuiWrapBoxProps final : public FRuiPropsBase
{
	RUI_PROP(FName, Orientation, 0)
	RUI_PROP(float, WrapSize, 1)
	RUI_PROP(FVector2D, InnerSlotPadding, 2)
	RUI_PROP(bool, bUseAllottedSize, 3)
	RUI_PROPS_BODY(FRuiWrapBoxProps,
				   RUI_EQ(Orientation) RUI_EQ(WrapSize) RUI_EQ(InnerSlotPadding) RUI_EQ(bUseAllottedSize))
};

/** SMultiLineEditableTextBox (Leaf) — multi-line controlled input; same D-16 caret rule as
 *  SEditableTextBox (Text applied skip-when-equal against the WIDGET's live text). */
struct REACTIVEUISLATE_API FRuiMultiLineEditableTextBoxProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)
	RUI_PROP(FText, HintText, 1)
	RUI_PROP(bool, bIsReadOnly, 2)
	RUI_PROP_EVENT(OnTextChanged, 3)
	RUI_PROP_EVENT(OnTextCommitted, 4)

	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiMultiLineEditableTextBoxProps& Other =
			static_cast<const FRuiMultiLineEditableTextBoxProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return SetBits == Other.SetBits && BaseFieldsEqual(Other) && TextEq(Text, Other.Text) &&
			   TextEq(HintText, Other.HintText) && bIsReadOnly == Other.bIsReadOnly &&
			   OnTextChanged == Other.OnTextChanged && OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SSearchBox (Leaf) — an SEditableTextBox specialization with a search affordance. The search
 *  text flows through OnTextChanged/OnTextCommitted (SSearchBox::OnSearch is up/down navigation,
 *  not a text callback). Text is the same controlled-input caret rule (D-16). */
struct REACTIVEUISLATE_API FRuiSearchBoxProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)
	RUI_PROP(FText, HintText, 1)
	RUI_PROP_EVENT(OnTextChanged, 2)
	RUI_PROP_EVENT(OnTextCommitted, 3)

	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiSearchBoxProps& Other = static_cast<const FRuiSearchBoxProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return SetBits == Other.SetBits && BaseFieldsEqual(Other) && TextEq(Text, Other.Text) &&
			   TextEq(HintText, Other.HintText) && OnTextChanged == Other.OnTextChanged &&
			   OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SSafeZone (SingleContent): pads content into the device title/action safe area. */
struct REACTIVEUISLATE_API FRuiSafeZoneProps final : public FRuiPropsBase
{
	RUI_PROP(bool, bIsTitleSafe, 0)
	RUI_PROP(bool, bPadLeft, 1)
	RUI_PROP(bool, bPadRight, 2)
	RUI_PROP(bool, bPadTop, 3)
	RUI_PROP(bool, bPadBottom, 4)
	RUI_PROPS_BODY(FRuiSafeZoneProps,
				   RUI_EQ(bIsTitleSafe) RUI_EQ(bPadLeft) RUI_EQ(bPadRight) RUI_EQ(bPadTop) RUI_EQ(bPadBottom))
};

/** SDPIScaler (SingleContent): scales its content by a DPI factor. */
struct REACTIVEUISLATE_API FRuiDPIScalerProps final : public FRuiPropsBase
{
	RUI_PROP(float, DPIScale, 0)
	RUI_PROPS_BODY(FRuiDPIScalerProps, RUI_EQ(DPIScale))
};

/** SSeparator (Leaf): a styled line. Orientation (horizontal|vertical) + Thickness are
 *  CONSTRUCT-ONLY (Slate bakes them at build) — a change replaces the widget (TD-011 reconstruct
 *  mask, the first shipped widget to exercise it). ColorAndOpacity is a live setter. */
struct REACTIVEUISLATE_API FRuiSeparatorProps final : public FRuiPropsBase
{
	RUI_PROP(FName, Orientation, 0)			   // construct-only
	RUI_PROP(float, Thickness, 1)			   // construct-only
	RUI_PROP(FLinearColor, ColorAndOpacity, 2) // runtime
	RUI_PROPS_BODY(FRuiSeparatorProps, RUI_EQ(Orientation) RUI_EQ(Thickness) RUI_EQ(ColorAndOpacity))
};

/** SSpinBox<float> (Leaf): numeric drag/type input. Value applies skip-when-equal (D-16
 *  self-notifying). Delta is the drag step (0 = continuous). */
struct REACTIVEUISLATE_API FRuiSpinBoxProps final : public FRuiPropsBase
{
	RUI_PROP(float, Value, 0)
	RUI_PROP(float, MinValue, 1)
	RUI_PROP(float, MaxValue, 2)
	RUI_PROP(float, Delta, 3)
	RUI_PROP_EVENT(OnValueChanged, 4)
	RUI_PROPS_BODY(FRuiSpinBoxProps,
				   RUI_EQ(Value) RUI_EQ(MinValue) RUI_EQ(MaxValue) RUI_EQ(Delta) RUI_EQ(OnValueChanged))
};

/** SUniformWrapPanel (MultiSlot): a wrap panel that gives every child the same cell size. */
struct REACTIVEUISLATE_API FRuiUniformWrapPanelProps final : public FRuiPropsBase
{
	RUI_PROP(float, SlotPadding, 0)
	RUI_PROP(FName, HAlign, 1)
	RUI_PROPS_BODY(FRuiUniformWrapPanelProps, RUI_EQ(SlotPadding) RUI_EQ(HAlign))
};

/** SRichTextBlock (Leaf): text with inline markup (default decorator set). AutoWrapText wraps. */
struct REACTIVEUISLATE_API FRuiRichTextBlockProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)
	RUI_PROP(bool, bAutoWrapText, 1)

	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiRichTextBlockProps& Other = static_cast<const FRuiRichTextBlockProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return SetBits == Other.SetBits && BaseFieldsEqual(Other) && TextEq(Text, Other.Text) &&
			   bAutoWrapText == Other.bAutoWrapText;
	}
};

/** SGridPanel (MultiSlot): places children by slot.column / slot.row (both default 0). */
struct REACTIVEUISLATE_API FRuiGridPanelProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiGridPanelProps, )
};

/** SUniformGridPanel (MultiSlot): uniform cells placed by slot.column / slot.row. */
struct REACTIVEUISLATE_API FRuiUniformGridPanelProps final : public FRuiPropsBase
{
	RUI_PROP(float, SlotPadding, 0)
	RUI_PROP(float, MinDesiredSlotWidth, 1)
	RUI_PROP(float, MinDesiredSlotHeight, 2)
	RUI_PROPS_BODY(FRuiUniformGridPanelProps,
				   RUI_EQ(SlotPadding) RUI_EQ(MinDesiredSlotWidth) RUI_EQ(MinDesiredSlotHeight))
};

// ── Batch 3 wave 1 (WIDGET_COMPLETION_PLAN) ────────────────────────────────────────────────

/** SColorBlock (Leaf): a color swatch. ALL props are construct-only (no engine setters) —
 *  every bit is on the reconstruct mask; the leaf is cheap to rebuild. AlphaDisplayMode =
 *  combined|separate|ignore. */
struct REACTIVEUISLATE_API FRuiColorBlockProps final : public FRuiPropsBase
{
	RUI_PROP(FLinearColor, Color, 0)
	RUI_PROP(FVector2D, Size, 1)
	RUI_PROP(bool, bUseSRGB, 2)
	RUI_PROP(bool, bShowBackgroundForAlpha, 3)
	RUI_PROP(bool, bColorIsHSV, 4)
	RUI_PROP(FName, AlphaDisplayMode, 5)
	RUI_PROPS_BODY(FRuiColorBlockProps, RUI_EQ(Color) RUI_EQ(Size) RUI_EQ(bUseSRGB) RUI_EQ(bShowBackgroundForAlpha)
											RUI_EQ(bColorIsHSV) RUI_EQ(AlphaDisplayMode))
};

/** SSimpleGradient (Leaf-ish paint widget): two-stop gradient. Construct-only (no setters) —
 *  fully masked. Orientation = vertical (default) | horizontal. */
struct REACTIVEUISLATE_API FRuiSimpleGradientProps final : public FRuiPropsBase
{
	RUI_PROP(FLinearColor, StartColor, 0)
	RUI_PROP(FLinearColor, EndColor, 1)
	RUI_PROP(FName, Orientation, 2)
	RUI_PROP(bool, bHasAlphaBackground, 3)
	RUI_PROPS_BODY(FRuiSimpleGradientProps,
				   RUI_EQ(StartColor) RUI_EQ(EndColor) RUI_EQ(Orientation) RUI_EQ(bHasAlphaBackground))
};

/** SComplexGradient (Leaf-ish paint widget): N-stop gradient. Construct-only — fully masked. */
struct REACTIVEUISLATE_API FRuiComplexGradientProps final : public FRuiPropsBase
{
	RUI_PROP(TArray<FLinearColor>, GradientColors, 0)
	RUI_PROP(FName, Orientation, 1)
	RUI_PROP(bool, bHasAlphaBackground, 2)
	RUI_PROP(FVector2D, DesiredSizeOverride, 3)
	RUI_PROPS_BODY(FRuiComplexGradientProps,
				   RUI_EQ(GradientColors) RUI_EQ(Orientation) RUI_EQ(bHasAlphaBackground) RUI_EQ(DesiredSizeOverride))
};

/** SHyperlink (Leaf): a link. Text/Padding are construct-only (the inner text block bakes at
 *  Construct) — masked; OnNavigate binds at construction via the event proxy. */
struct REACTIVEUISLATE_API FRuiHyperlinkProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)
	RUI_PROP(FMargin, Padding, 1)
	RUI_PROP(FRuiCallback, OnNavigate, 2)
	// FText has no operator== — identity first, then display-string compare (the TextBlock rule).
	virtual bool Equals(const FRuiPropsBase& Other) const override
	{
		const FRuiHyperlinkProps* Typed = static_cast<const FRuiHyperlinkProps*>(&Other);
		if (!BaseFieldsEqual(Other))
		{
			return false;
		}
		if (!(Padding == Typed->Padding))
		{
			return false;
		}
		return Text.IdenticalTo(Typed->Text) || Text.ToString() == Typed->Text.ToString();
	}
};

/** SEnableBox (SingleContent): renders its child as if every ancestor were enabled. */
struct REACTIVEUISLATE_API FRuiEnableBoxProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiEnableBoxProps, )
};

/** SScissorRectBox (SingleContent): hardware scissor-clips its child (render transforms
 *  included — unlike Clipping="clipToBounds"'s rect). */
struct REACTIVEUISLATE_API FRuiScissorRectBoxProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiScissorRectBoxProps, )
};

/** SBackgroundBlur (SingleContent): post-process blur behind the content. Live setters. */
struct REACTIVEUISLATE_API FRuiBackgroundBlurProps final : public FRuiPropsBase
{
	RUI_PROP(float, BlurStrength, 0)
	RUI_PROP(int32, BlurRadius, 1)
	RUI_PROP(bool, bApplyAlphaToBlur, 2)
	RUI_PROP(FMargin, Padding, 3)
	RUI_PROPS_BODY(FRuiBackgroundBlurProps,
				   RUI_EQ(BlurStrength) RUI_EQ(BlurRadius) RUI_EQ(bApplyAlphaToBlur) RUI_EQ(Padding))
};

/** SInvalidationPanel (SingleContent): opt-in retained-paint cache around static subtrees. */
struct REACTIVEUISLATE_API FRuiInvalidationPanelProps final : public FRuiPropsBase
{
	RUI_PROP(bool, bCanCache, 0)
	RUI_PROPS_BODY(FRuiInvalidationPanelProps, RUI_EQ(bCanCache))
};

// ── Batch 3 wave 2 (WIDGET_COMPLETION_PLAN) ────────────────────────────────────────────────

/** SVolumeControl (Leaf): slider + mute composite. Volume/Muted are engine ATTRIBUTES with no
 *  setters — controlled via the reconstruct mask (D-16 semantics ride the rebuild); the two
 *  events report user edits back. */
struct REACTIVEUISLATE_API FRuiVolumeControlProps final : public FRuiPropsBase
{
	RUI_PROP(float, Volume, 0)
	RUI_PROP(bool, bMuted, 1)
	RUI_PROP_EVENT(OnVolumeChanged, 2)
	RUI_PROP_EVENT(OnMuteChanged, 3)
	RUI_PROPS_BODY(FRuiVolumeControlProps, RUI_EQ(Volume) RUI_EQ(bMuted) RUI_EQ(OnVolumeChanged) RUI_EQ(OnMuteChanged))
};

/** STextScroller (SingleContent): marquee auto-scroll around single-line text. Options are
 *  construct-only (masked); Start/Suspend/Reset ride P2 (`WidgetFromHandle<STextScroller>`). */
struct REACTIVEUISLATE_API FRuiTextScrollerProps final : public FRuiPropsBase
{
	RUI_PROP(float, Speed, 0)
	RUI_PROP(float, StartDelay, 1)
	RUI_PROP(float, EndDelay, 2)
	RUI_PROP(FName, ScrollOrientation, 3)
	RUI_PROPS_BODY(FRuiTextScrollerProps, RUI_EQ(Speed) RUI_EQ(StartDelay) RUI_EQ(EndDelay) RUI_EQ(ScrollOrientation))
};

/** SRadialBox (MultiSlot, bare slots — children place around the arc in declaration order).
 *  PreferredWidth is construct-only (masked); the angle/distribution params are live. */
struct REACTIVEUISLATE_API FRuiRadialBoxProps final : public FRuiPropsBase
{
	RUI_PROP(float, PreferredWidth, 0)
	RUI_PROP(bool, bUseAllottedWidth, 1)
	RUI_PROP(float, StartingAngle, 2)
	RUI_PROP(bool, bDistributeItemsEvenly, 3)
	RUI_PROP(float, AngleBetweenItems, 4)
	RUI_PROP(float, SectorCentralAngle, 5)
	RUI_PROPS_BODY(FRuiRadialBoxProps,
				   RUI_EQ(PreferredWidth) RUI_EQ(bUseAllottedWidth) RUI_EQ(StartingAngle) RUI_EQ(bDistributeItemsEvenly)
					   RUI_EQ(AngleBetweenItems) RUI_EQ(SectorCentralAngle))
};

/** SColorWheel (Leaf): HSV wheel. SelectedColor is HSV-space, attribute-only (no setter) —
 *  controlled via the reconstruct mask; drag edits report through OnValueChanged. */
struct REACTIVEUISLATE_API FRuiColorWheelProps final : public FRuiPropsBase
{
	RUI_PROP(FLinearColor, SelectedColor, 0)
	RUI_PROP_EVENT(OnValueChanged, 1)
	RUI_PROP_EVENT(OnMouseCaptureBegin, 2)
	RUI_PROP_EVENT(OnMouseCaptureEnd, 3)
	RUI_PROPS_BODY(FRuiColorWheelProps,
				   RUI_EQ(SelectedColor) RUI_EQ(OnValueChanged) RUI_EQ(OnMouseCaptureBegin) RUI_EQ(OnMouseCaptureEnd))
};

/** SColorSpectrum (Leaf): saturation/value box — same controlled contract as ColorWheel. */
struct REACTIVEUISLATE_API FRuiColorSpectrumProps final : public FRuiPropsBase
{
	RUI_PROP(FLinearColor, SelectedColor, 0)
	RUI_PROP_EVENT(OnValueChanged, 1)
	RUI_PROP_EVENT(OnMouseCaptureBegin, 2)
	RUI_PROP_EVENT(OnMouseCaptureEnd, 3)
	RUI_PROPS_BODY(FRuiColorSpectrumProps,
				   RUI_EQ(SelectedColor) RUI_EQ(OnValueChanged) RUI_EQ(OnMouseCaptureBegin) RUI_EQ(OnMouseCaptureEnd))
};

/** SLayeredImage (Leaf): SImage + N overlay layers, all live (RemoveAllLayers + AddLayer on a
 *  layer-list change; brushes by identity like Image). */
struct REACTIVEUISLATE_API FRuiLayeredImageProps final : public FRuiPropsBase
{
	RUI_PROP(FLinearColor, ColorAndOpacity, 0)
	RUI_PROP(FVector2D, DesiredSizeOverride, 1)
	RUI_PROP(TSharedPtr<FSlateBrush>, Image, 2)
	RUI_PROP(TArray<TSharedPtr<FSlateBrush>>, Layers, 3)
	RUI_PROPS_BODY(FRuiLayeredImageProps,
				   RUI_EQ(ColorAndOpacity) RUI_EQ(DesiredSizeOverride) RUI_EQ(Image) RUI_EQ(Layers))
};

/** SInputKeySelector (Leaf composite): key-binding capture. SelectedKey is a live setter (key
 *  NAME; modifiers are the TD-016 multi-field payload trigger — key-only in v1). The
 *  capture-behavior args are construct-only (masked). */
struct REACTIVEUISLATE_API FRuiInputKeySelectorProps final : public FRuiPropsBase
{
	RUI_PROP(FName, SelectedKey, 0)
	RUI_PROP(FText, KeySelectionText, 1)
	RUI_PROP(FText, NoKeySpecifiedText, 2)
	RUI_PROP(bool, bAllowModifierKeys, 3)
	RUI_PROP(bool, bAllowGamepadKeys, 4)
	RUI_PROP(bool, bEscapeCancelsSelection, 5)
	RUI_PROP_EVENT(OnKeySelected, 6)
	RUI_PROP_EVENT(OnIsSelectingKeyChanged, 7)

	// FText fields have no operator== — hand-written Equals (the EditableTextBox rule).
	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiInputKeySelectorProps& Other = static_cast<const FRuiInputKeySelectorProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && SelectedKey == Other.SelectedKey &&
			   TextEq(KeySelectionText, Other.KeySelectionText) &&
			   TextEq(NoKeySpecifiedText, Other.NoKeySpecifiedText) && bAllowModifierKeys == Other.bAllowModifierKeys &&
			   bAllowGamepadKeys == Other.bAllowGamepadKeys &&
			   bEscapeCancelsSelection == Other.bEscapeCancelsSelection && OnKeySelected == Other.OnKeySelected &&
			   OnIsSelectingKeyChanged == Other.OnIsSelectingKeyChanged;
	}
};

/** SEditableText (Leaf): the RAW single-line text edit (no box chrome) — full live setters;
 *  Text follows the D-16 controlled rule (skip-when-equal against the widget). */
struct REACTIVEUISLATE_API FRuiEditableTextProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)
	RUI_PROP(FText, HintText, 1)
	RUI_PROP(bool, bIsReadOnly, 2)
	RUI_PROP(bool, bIsPassword, 3)
	RUI_PROP(float, MinDesiredWidth, 4)
	RUI_PROP_EVENT(OnTextChanged, 5)
	RUI_PROP_EVENT(OnTextCommitted, 6)

	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiEditableTextProps& Other = static_cast<const FRuiEditableTextProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && TextEq(Text, Other.Text) && TextEq(HintText, Other.HintText) &&
			   bIsReadOnly == Other.bIsReadOnly && bIsPassword == Other.bIsPassword &&
			   MinDesiredWidth == Other.MinDesiredWidth && OnTextChanged == Other.OnTextChanged &&
			   OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SInlineEditableTextBlock (Leaf): text that turns into an editor on slow-click/F2.
 *  bMultiLine is construct-only (masked); Enter/ExitEditingMode ride P2. */
struct REACTIVEUISLATE_API FRuiInlineEditableTextBlockProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)
	RUI_PROP(FText, HintText, 1)
	RUI_PROP(bool, bIsReadOnly, 2)
	RUI_PROP(float, WrapTextAt, 3)
	RUI_PROP(bool, bMultiLine, 4)
	RUI_PROP_EVENT(OnTextCommitted, 5)

	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiInlineEditableTextBlockProps& Other = static_cast<const FRuiInlineEditableTextBlockProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && TextEq(Text, Other.Text) && TextEq(HintText, Other.HintText) &&
			   bIsReadOnly == Other.bIsReadOnly && WrapTextAt == Other.WrapTextAt && bMultiLine == Other.bMultiLine &&
			   OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SVirtualKeyboardEntry (Leaf): the mobile OS-keyboard text field. Text is live (D-16);
 *  HintText/bIsReadOnly/KeyboardType are construct-only (masked). Ticks. */
struct REACTIVEUISLATE_API FRuiVirtualKeyboardEntryProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)
	RUI_PROP(FText, HintText, 1)
	RUI_PROP(bool, bIsReadOnly, 2)
	RUI_PROP(FName, KeyboardType, 3)
	RUI_PROP_EVENT(OnTextChanged, 4)
	RUI_PROP_EVENT(OnTextCommitted, 5)

	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiVirtualKeyboardEntryProps& Other = static_cast<const FRuiVirtualKeyboardEntryProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && TextEq(Text, Other.Text) && TextEq(HintText, Other.HintText) &&
			   bIsReadOnly == Other.bIsReadOnly && KeyboardType == Other.KeyboardType &&
			   OnTextChanged == Other.OnTextChanged && OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SColorGradingWheel (Leaf; the AdvancedWidgets module — live one; the Slate-module twin is
 *  deprecated 5.5): all attrs have live attribute setters. SelectedColor is HSV-space. */
struct REACTIVEUISLATE_API FRuiColorGradingWheelProps final : public FRuiPropsBase
{
	RUI_PROP(FLinearColor, SelectedColor, 0)
	RUI_PROP(int32, DesiredWheelSize, 1)
	RUI_PROP(float, ExponentDisplacement, 2)
	RUI_PROP_EVENT(OnValueChanged, 3)
	RUI_PROP_EVENT(OnMouseCaptureBegin, 4)
	RUI_PROP_EVENT(OnMouseCaptureEnd, 5)
	RUI_PROPS_BODY(FRuiColorGradingWheelProps,
				   RUI_EQ(SelectedColor) RUI_EQ(DesiredWheelSize) RUI_EQ(ExponentDisplacement) RUI_EQ(OnValueChanged)
					   RUI_EQ(OnMouseCaptureBegin) RUI_EQ(OnMouseCaptureEnd))
};

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId VerticalBoxType();
	REACTIVEUISLATE_API FRuiElementTypeId HorizontalBoxType();
	REACTIVEUISLATE_API FRuiElementTypeId ButtonType();
	REACTIVEUISLATE_API FRuiElementTypeId OverlayType();
	REACTIVEUISLATE_API FRuiElementTypeId CanvasType();

	REACTIVEUISLATE_API FRuiNode VerticalBox(FRuiVerticalBoxProps Props = FRuiVerticalBoxProps(),
											 TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode HorizontalBox(FRuiHorizontalBoxProps Props = FRuiHorizontalBoxProps(),
											   TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Button(FRuiButtonProps Props = FRuiButtonProps(),
										TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Overlay(FRuiOverlayProps Props = FRuiOverlayProps(),
										 TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Canvas(FRuiCanvasPanelProps Props = FRuiCanvasPanelProps(),
										TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());

	REACTIVEUISLATE_API FRuiNode Border(FRuiBorderProps Props = FRuiBorderProps(),
										TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Box(FRuiBoxProps Props = FRuiBoxProps(),
									 TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Image(FRuiImageProps Props = FRuiImageProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ScrollBox(FRuiScrollBoxProps Props = FRuiScrollBoxProps(),
										   TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Spacer(FRuiSpacerProps Props = FRuiSpacerProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode EditableTextBox(FRuiEditableTextBoxProps Props = FRuiEditableTextBoxProps(),
												 FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode CheckBox(FRuiCheckBoxProps Props = FRuiCheckBoxProps(),
										  TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Slider(FRuiSliderProps Props = FRuiSliderProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ProgressBar(FRuiProgressBarProps Props = FRuiProgressBarProps(),
											 FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode RuiCanvas(FRuiCanvasProps Props = FRuiCanvasProps(), FRuiKey Key = FRuiKey());

	// ── Batch 2 (Phase 7 step 8) factories ────────────────────────────────────────────────
	REACTIVEUISLATE_API FRuiNode WidgetSwitcher(FRuiWidgetSwitcherProps Props = FRuiWidgetSwitcherProps(),
												TArray<FRuiNode> Children = TArray<FRuiNode>(),
												FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ScaleBox(FRuiScaleBoxProps Props = FRuiScaleBoxProps(),
										  TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Throbber(FRuiThrobberProps Props = FRuiThrobberProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode WrapBox(FRuiWrapBoxProps Props = FRuiWrapBoxProps(),
										 TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode MultiLineEditableTextBox(
		FRuiMultiLineEditableTextBoxProps Props = FRuiMultiLineEditableTextBoxProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode SearchBox(FRuiSearchBoxProps Props = FRuiSearchBoxProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode SafeZone(FRuiSafeZoneProps Props = FRuiSafeZoneProps(),
										  TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode DPIScaler(FRuiDPIScalerProps Props = FRuiDPIScalerProps(),
										   TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Separator(FRuiSeparatorProps Props = FRuiSeparatorProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode SpinBox(FRuiSpinBoxProps Props = FRuiSpinBoxProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode UniformWrapPanel(FRuiUniformWrapPanelProps Props = FRuiUniformWrapPanelProps(),
												  TArray<FRuiNode> Children = TArray<FRuiNode>(),
												  FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode RichTextBlock(FRuiRichTextBlockProps Props = FRuiRichTextBlockProps(),
											   FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode GridPanel(FRuiGridPanelProps Props = FRuiGridPanelProps(),
										   TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ColorBlock(FRuiColorBlockProps Props = FRuiColorBlockProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode SimpleGradient(FRuiSimpleGradientProps Props = FRuiSimpleGradientProps(),
												FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ComplexGradient(FRuiComplexGradientProps Props = FRuiComplexGradientProps(),
												 FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Hyperlink(FRuiHyperlinkProps Props = FRuiHyperlinkProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode EnableBox(FRuiEnableBoxProps Props = FRuiEnableBoxProps(),
										   TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ScissorRectBox(FRuiScissorRectBoxProps Props = FRuiScissorRectBoxProps(),
												TArray<FRuiNode> Children = TArray<FRuiNode>(),
												FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode BackgroundBlur(FRuiBackgroundBlurProps Props = FRuiBackgroundBlurProps(),
												TArray<FRuiNode> Children = TArray<FRuiNode>(),
												FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode InvalidationPanel(FRuiInvalidationPanelProps Props = FRuiInvalidationPanelProps(),
												   TArray<FRuiNode> Children = TArray<FRuiNode>(),
												   FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode VolumeControl(FRuiVolumeControlProps Props = FRuiVolumeControlProps(),
											   FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode TextScroller(FRuiTextScrollerProps Props = FRuiTextScrollerProps(),
											  TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode RadialBox(FRuiRadialBoxProps Props = FRuiRadialBoxProps(),
										   TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ColorWheel(FRuiColorWheelProps Props = FRuiColorWheelProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ColorSpectrum(FRuiColorSpectrumProps Props = FRuiColorSpectrumProps(),
											   FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode LayeredImage(FRuiLayeredImageProps Props = FRuiLayeredImageProps(),
											  FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode InputKeySelector(FRuiInputKeySelectorProps Props = FRuiInputKeySelectorProps(),
												  FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode EditableText(FRuiEditableTextProps Props = FRuiEditableTextProps(),
											  FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode InlineEditableTextBlock(
		FRuiInlineEditableTextBlockProps Props = FRuiInlineEditableTextBlockProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode VirtualKeyboardEntry(
		FRuiVirtualKeyboardEntryProps Props = FRuiVirtualKeyboardEntryProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ColorGradingWheel(FRuiColorGradingWheelProps Props = FRuiColorGradingWheelProps(),
												   FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode UniformGridPanel(FRuiUniformGridPanelProps Props = FRuiUniformGridPanelProps(),
												  TArray<FRuiNode> Children = TArray<FRuiNode>(),
												  FRuiKey Key = FRuiKey());

	/** Wrap a paint lambda ONCE (UseMemo/UseRef it) — the canvas repaints on identity change. */
	REACTIVEUISLATE_API TSharedPtr<FRuiDrawFn> MakeDrawFn(FRuiDrawFn Fn);

	/** Register the built-in adapters (module startup; idempotent — replaces on re-run). */
	REACTIVEUISLATE_API void RegisterBuiltinAdapters();
} // namespace RUI::Slate
