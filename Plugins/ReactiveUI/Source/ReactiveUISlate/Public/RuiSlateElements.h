// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The first wrapped elements (Phase 2 step 3 — the hand-written pattern setters; the
// remaining widgets production-line onto this shape in the same phase). Props structs +
// element factories only; the adapters live in Private/RuiCoreAdapters.cpp.
//
// Text renders core FRuiTextProps (RUI::Text) — the GetTextElementType contract — so it
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
	RUI_PROPS_BODY(FRuiButtonProps, RUI_EQ(OnClicked) RUI_EQ(bEnabled))
};

/** SOverlay (MultiSlot; also the SRuiRoot inner panel). slot.zorder orders the slots. */
struct REACTIVEUISLATE_API FRuiOverlayProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiOverlayProps, )
};

/** SBorder (SingleContent). Alignment values: fill|left|center|right / fill|top|center|bottom. */
struct REACTIVEUISLATE_API FRuiBorderProps final : public FRuiPropsBase
{
	RUI_PROP(FMargin, Padding, 0)
	RUI_PROP(FLinearColor, BorderColor, 1)
	RUI_PROP(FName, HAlign, 2)
	RUI_PROP(FName, VAlign, 3)
	RUI_PROPS_BODY(FRuiBorderProps, RUI_EQ(Padding) RUI_EQ(BorderColor) RUI_EQ(HAlign) RUI_EQ(VAlign))
};

/** SBox (SingleContent): size overrides + content alignment. Overrides are settable, not
 *  clearable (family removal semantics: plain props don't reset). */
struct REACTIVEUISLATE_API FRuiBoxProps final : public FRuiPropsBase
{
	RUI_PROP(float, Width, 0)
	RUI_PROP(float, Height, 1)
	RUI_PROP(float, MinWidth, 2)
	RUI_PROP(float, MinHeight, 3)
	RUI_PROP(float, MaxWidth, 4)
	RUI_PROP(float, MaxHeight, 5)
	RUI_PROP(FName, HAlign, 6)
	RUI_PROP(FName, VAlign, 7)
	RUI_PROPS_BODY(FRuiBoxProps, RUI_EQ(Width) RUI_EQ(Height) RUI_EQ(MinWidth) RUI_EQ(MinHeight) RUI_EQ(MaxWidth)
									 RUI_EQ(MaxHeight) RUI_EQ(HAlign) RUI_EQ(VAlign))
};

/** SImage (Leaf) v1: tint + desired size. Brush CONTENT (textures/materials) is the D-17
 *  GC-root work — it arrives with the style/asset layer, not here. */
struct REACTIVEUISLATE_API FRuiImageProps final : public FRuiPropsBase
{
	RUI_PROP(FLinearColor, Color, 0)
	RUI_PROP(FVector2D, DesiredSize, 1)
	RUI_PROPS_BODY(FRuiImageProps, RUI_EQ(Color) RUI_EQ(DesiredSize))
};

/** SScrollBox (MultiSlot). Orientation is CONSTRUCT-ONLY (reconstruct mask bit 0). */
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
struct REACTIVEUISLATE_API FRuiEditableTextProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)
	RUI_PROP(FText, HintText, 1)
	RUI_PROP(bool, bIsReadOnly, 2)
	RUI_PROP_EVENT(OnTextChanged, 3)
	RUI_PROP_EVENT(OnTextCommitted, 4)

	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiEditableTextProps& Other = static_cast<const FRuiEditableTextProps&>(OtherBase);
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
	RUI_PROP_EVENT(OnCheckedChanged, 1)
	RUI_PROPS_BODY(FRuiCheckBoxProps, RUI_EQ(bIsChecked) RUI_EQ(OnCheckedChanged))
};

/** SSlider (Leaf). Value applies skip-when-equal (self-notifying family, D-16). */
struct REACTIVEUISLATE_API FRuiSliderProps final : public FRuiPropsBase
{
	RUI_PROP(float, Value, 0)
	RUI_PROP(float, MinValue, 1)
	RUI_PROP(float, MaxValue, 2)
	RUI_PROP_EVENT(OnValueChanged, 3)
	RUI_PROPS_BODY(FRuiSliderProps, RUI_EQ(Value) RUI_EQ(MinValue) RUI_EQ(MaxValue) RUI_EQ(OnValueChanged))
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

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId VerticalBoxType();
	REACTIVEUISLATE_API FRuiElementTypeId HorizontalBoxType();
	REACTIVEUISLATE_API FRuiElementTypeId ButtonType();
	REACTIVEUISLATE_API FRuiElementTypeId OverlayType();

	REACTIVEUISLATE_API FRuiNode VBox(FRuiVerticalBoxProps Props = FRuiVerticalBoxProps(),
									  TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode HBox(FRuiHorizontalBoxProps Props = FRuiHorizontalBoxProps(),
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
	REACTIVEUISLATE_API FRuiNode EditableText(FRuiEditableTextProps Props = FRuiEditableTextProps(),
											  FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode CheckBox(FRuiCheckBoxProps Props = FRuiCheckBoxProps(),
										  TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Slider(FRuiSliderProps Props = FRuiSliderProps(), FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode ProgressBar(FRuiProgressBarProps Props = FRuiProgressBarProps(),
											 FRuiKey Key = FRuiKey());
	REACTIVEUISLATE_API FRuiNode Canvas(FRuiCanvasProps Props = FRuiCanvasProps(), FRuiKey Key = FRuiKey());

	/** Wrap a paint lambda ONCE (UseMemo/UseRef it) — the canvas repaints on identity change. */
	REACTIVEUISLATE_API TSharedPtr<FRuiDrawFn> MakeDrawFn(FRuiDrawFn Fn);

	/** Register the built-in adapters (module startup; idempotent — replaces on re-run). */
	REACTIVEUISLATE_API void RegisterBuiltinAdapters();
} // namespace RUI::Slate
