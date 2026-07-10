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
#include "RuiCoreElements.h"
#include "RuiNode.h"
#include "RuiPropsBase.h"

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

/** SButton (SingleContent) — the event-proxy pattern widget. */
struct REACTIVEUISLATE_API FRuiButtonProps final : public FRuiPropsBase
{
	RUI_PROP_EVENT(OnClicked, 0)
	RUI_PROP(bool, bEnabled, 1)
	RUI_PROPS_BODY(FRuiButtonProps, RUI_EQ(bEnabled))
};

/** SOverlay (MultiSlot; also the SRuiRoot inner panel). slot.zorder orders the slots. */
struct REACTIVEUISLATE_API FRuiOverlayProps final : public FRuiPropsBase
{
	RUI_PROPS_BODY(FRuiOverlayProps, )
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

	/** Register the built-in adapters (module startup; idempotent — replaces on re-run). */
	REACTIVEUISLATE_API void RegisterBuiltinAdapters();
} // namespace RUI::Slate
