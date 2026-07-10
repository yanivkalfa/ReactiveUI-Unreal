// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The two element/component pieces the CORE itself must know:
//   Text     — the auto-wrap target for raw string children (family: String -> V.text);
//              hosts register their adapter under the same interned tag ("Text").
//   Suspense — the family's declarative boundary polyfill (NO throw-to-suspend — D-10's
//              no-exceptions reality): a plain function component over the hooks, driven by
//              an IsReady poll (re-armed per frame via the host's RequestFrame).

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiContext.h"

/** Text element props ("Text" tag; hosts map to STextBlock / mock text). */
struct FRuiTextBlockProps final : public FRuiPropsBase
{
	RUI_PROP(FText, Text, 0)

	// Hand-written (FText has no operator== — identity first, then display-string compare).
	virtual bool Equals(const FRuiPropsBase& Other) const override
	{
		const FRuiTextBlockProps* Typed = static_cast<const FRuiTextBlockProps*>(&Other);
		if (!BaseFieldsEqual(Other))
		{
			return false;
		}
		return Text.IdenticalTo(Typed->Text) || Text.ToString() == Typed->Text.ToString();
	}
};

namespace RUI
{
	/** The interned "Text" element id (stable across the process). */
	REACTIVEUICORE_API FRuiElementTypeId TextBlockElementType();

	/** Text node factory (also what raw-string children auto-wrap into). */
	REACTIVEUICORE_API FRuiNode TextBlock(FText InText, FRuiKey Key = FRuiKey());
	REACTIVEUICORE_API FRuiNode TextBlock(const FString& InText, FRuiKey Key = FRuiKey());

	/** Suspense props. */
	struct FRuiSuspenseProps final : public FRuiPropsBase
	{
		/** Shown while not ready (empty node = render nothing). */
		TSharedPtr<FRuiNode> Fallback;
		/** Polled once immediately, then per frame until true. */
		TFunction<bool()> IsReady;

		virtual bool Equals(const FRuiPropsBase& Other) const override
		{
			// Function fields are identity-less; Fallback is a node — Suspense re-renders
			// when its parent does (never bails on props), which matches the family's
			// function-component polyfill behavior.
			return false;
		}
	};

	/** The Suspense component function (registered; use via RUI::Suspense below). */
	REACTIVEUICORE_API FRuiNodeArray SuspenseComponent(FRuiContext& Ctx, const FRuiSuspenseProps& Props,
													   const TArray<FRuiNode>& Children);

	/** Declarative boundary: fallback until IsReady() flips true, then the children. */
	REACTIVEUICORE_API FRuiNode Suspense(TFunction<bool()> IsReady, FRuiNode Fallback, TArray<FRuiNode> Children,
										 FRuiKey Key = FRuiKey());
} // namespace RUI
