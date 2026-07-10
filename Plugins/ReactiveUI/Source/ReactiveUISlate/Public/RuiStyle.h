// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Style v1 (D-13, load-bearing): the central style-key registry. Hot-path keys map to
// SETTERS (never construct args — a style tweak must never destroy a widget) with explicit
// RESET behavior: style keys DO reset when removed between renders (unlike plain props —
// the family contract). Two layers ship in v1: inline `style` dicts and the `classes`
// merge layer (classes apply in order, inline style wins). The stylesheet layer is TD-002.
//
// Generic keys (any SWidget): opacity, visibility (visible|collapsed|hidden|hitTestInvisible),
// enabled, translation (Vector2), scale (Float), angle (Float, degrees).
// Widget-specific keys route through IRuiElementAdapter::ApplyStyleKey (e.g. Text "color",
// "fontSize") — same registry semantics, adapter-owned setters.

#pragma once

#include "CoreMinimal.h"
#include "RuiTypes.h"
#include "Widgets/SWidget.h"

class IRuiElementAdapter;

namespace RUI::Slate
{
	/** Register/replace a named style class (the `classes` layer). */
	REACTIVEUISLATE_API void RegisterStyleClass(FName ClassName, FRuiStyleDict Style);

	REACTIVEUISLATE_API const FRuiStyleDict* FindStyleClass(FName ClassName);

	/** Build the effective dict: classes in order, then inline style overrides. Returns
	 *  null when nothing contributes. */
	REACTIVEUISLATE_API TSharedPtr<FRuiStyleDict> BuildEffectiveStyle(const TArray<FName>& Classes,
																	  const TSharedPtr<FRuiStyleDict>& InlineStyle);

	/** Diff-apply Old -> New on the widget: changed/new keys apply; keys present in Old but
	 *  absent in New RESET to their defaults. Unknown keys warn once per key name. Adapter
	 *  handles widget-specific keys first (may be null). */
	REACTIVEUISLATE_API void ApplyStyleDiff(SWidget& Widget, IRuiElementAdapter* Adapter, const FRuiStyleDict* Old,
											const FRuiStyleDict* New);
} // namespace RUI::Slate
