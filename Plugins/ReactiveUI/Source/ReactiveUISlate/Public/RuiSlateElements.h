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
#include "SRuiCanvas.h" // FRuiDrawFn

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
	RUI_PROPS_BODY(FRuiButtonProps, RUI_EQ(OnClicked) RUI_EQ(bEnabled) RUI_EQ(ContentPadding))
};

/** SOverlay (MultiSlot; also the SRuiRoot inner panel). slot.zorder orders the slots. */
struct REACTIVEUISLATE_API FRuiOverlayProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiOverlayProps, )
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
	RUI_PROPS_BODY(FRuiBorderProps,
				   RUI_EQ(Padding) RUI_EQ(BorderBackgroundColor) RUI_EQ(HAlign) RUI_EQ(VAlign) RUI_EQ(BorderImage))
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

/** SImage (Leaf) v1: tint + desired size. Brush CONTENT (textures/materials) is the D-17
 *  GC-root work — it arrives with the style/asset layer, not here. */
struct REACTIVEUISLATE_API FRuiImageProps final : public FRuiPropsBase
{
	RUI_PROP(FLinearColor, ColorAndOpacity, 0)
	RUI_PROP(FVector2D, DesiredSizeOverride, 1)
	RUI_PROPS_BODY(FRuiImageProps, RUI_EQ(ColorAndOpacity) RUI_EQ(DesiredSizeOverride))
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
	RUI_PROPS_BODY(FRuiProgressBarProps, RUI_EQ(Percent))
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
 *  scaleToFitY|scaleToFill|scaleBySafeZone; StretchDirection = both|downOnly|upOnly. */
struct REACTIVEUISLATE_API FRuiScaleBoxProps final : public FRuiPropsBase
{
	RUI_PROP(FName, Stretch, 0)
	RUI_PROP(FName, StretchDirection, 1)
	RUI_PROPS_BODY(FRuiScaleBoxProps, RUI_EQ(Stretch) RUI_EQ(StretchDirection))
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

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId VerticalBoxType();
	REACTIVEUISLATE_API FRuiElementTypeId HorizontalBoxType();
	REACTIVEUISLATE_API FRuiElementTypeId ButtonType();
	REACTIVEUISLATE_API FRuiElementTypeId OverlayType();

	REACTIVEUISLATE_API FRuiNode VerticalBox(FRuiVerticalBoxProps Props = FRuiVerticalBoxProps(),
											 TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode HorizontalBox(FRuiHorizontalBoxProps Props = FRuiHorizontalBoxProps(),
											   TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Button(FRuiButtonProps Props = FRuiButtonProps(),
										TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Overlay(FRuiOverlayProps Props = FRuiOverlayProps(),
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

	/** Wrap a paint lambda ONCE (UseMemo/UseRef it) — the canvas repaints on identity change. */
	REACTIVEUISLATE_API TSharedPtr<FRuiDrawFn> MakeDrawFn(FRuiDrawFn Fn);

	/** Register the built-in adapters (module startup; idempotent — replaces on re-run). */
	REACTIVEUISLATE_API void RegisterBuiltinAdapters();
} // namespace RUI::Slate
