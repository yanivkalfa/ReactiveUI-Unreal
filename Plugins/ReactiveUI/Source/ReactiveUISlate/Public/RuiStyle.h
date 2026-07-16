// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Style v1 (D-13, load-bearing): the central style-key registry. Hot-path keys map to
// SETTERS (never construct args — a style tweak must never destroy a widget) with explicit
// RESET behavior: style keys DO reset when removed between renders (unlike plain props —
// the family contract). Two layers ship in v1: inline `style` dicts and the `classes`
// merge layer (classes apply in order, inline style wins). The stylesheet layer is TD-002.
//
// Key names are 1:1 with the Unreal setter/property they drive (D-33). Generic keys (any
// SWidget): RenderOpacity, Visibility (Visible|Collapsed|Hidden|HitTestInvisible|
// SelfHitTestInvisible), Enabled, RenderTranslation (Vector2), RenderScale (Float),
// RenderTransformAngle (Float, degrees), RenderTransformPivot (Vector2). Widget-specific
// keys route through IRuiElementAdapter::ApplyStyleKey (e.g. TextBlock "ColorAndOpacity",
// "Font.Size", "Justification", "AutoWrapText") — same semantics, adapter-owned setters.
// FName matching is case-insensitive; docs use the Unreal casing.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "RuiTypes.h"
#include "Types/SlateEnums.h"
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

	// ── TD-002: the THIRD layer — @theme tokens + @uss stylesheets ─────────────────────────
	//
	// Cascade (lowest -> highest): theme tokens (resolved into values) < class rules < inline
	// style. A style value that is a String beginning with `$` is a TOKEN REFERENCE, resolved
	// against the ACTIVE theme when the effective style is built (missing token -> warn + kept).

	/** Register/replace a named theme (a token name -> FRuiValue map). */
	REACTIVEUISLATE_API void RegisterTheme(FName ThemeName, FRuiStyleDict Tokens);

	/** Select the active theme used to resolve `$token` references. NAME_None = no theme. */
	REACTIVEUISLATE_API void SetActiveTheme(FName ThemeName);
	REACTIVEUISLATE_API FName GetActiveTheme();

	/** Resolve a token against the active theme (null if unknown / no active theme). */
	REACTIVEUISLATE_API const FRuiValue* ResolveThemeToken(FName TokenName);

	/** Parse a `.uss`-style stylesheet source and register its `@theme <name> { ... }` blocks
	 *  and `.<class> { key: value; }` rules. Values: `#rrggbb[aa]` color, numbers (int/float),
	 *  true/false, `$token` refs, "quoted" strings, else a bare Name. Returns the count of
	 *  (themes + classes) registered. Idempotent — re-loading replaces. */
	REACTIVEUISLATE_API int32 LoadStylesheet(const FString& Source);

	/** Parse one style value literal into an FRuiValue (the stylesheet grammar; also handy for
	 *  the markup codegen's @uss lowering). */
	REACTIVEUISLATE_API FRuiValue ParseStyleValue(const FString& Literal);
} // namespace RUI::Slate

namespace RUI
{
	// TD-013: fluent, compile-time-safe authoring for style + slot.* dicts. One method per
	// registered v1 key — FName keys and FRuiValue kinds match the .uetkx markup EXACTLY, so a
	// C++-authored dict behaves identically to markup (single vocabulary). `Set` is the forward-
	// compat escape hatch for keys not yet surfaced as a typed method. Header-only: it just
	// populates the FRuiStyleDict (TMap) the props take; no runtime cost beyond the map.

	/** `RUI::Style().RenderOpacity(0.5f).ColorAndOpacity(FLinearColor::Red)` -> the style dict. */
	struct FRuiStyleBuilder
	{
		TSharedRef<FRuiStyleDict> Dict = MakeShared<FRuiStyleDict>();

		FRuiStyleBuilder& RenderOpacity(float V) { return Set(FName(TEXT("RenderOpacity")), FRuiValue(V)); }
		FRuiStyleBuilder& Visibility(FName V) { return Set(FName(TEXT("Visibility")), FRuiValue(V)); }
		FRuiStyleBuilder& Enabled(bool V) { return Set(FName(TEXT("Enabled")), FRuiValue(V)); }
		FRuiStyleBuilder& RenderTranslation(const FVector2D& V)
		{
			return Set(FName(TEXT("RenderTranslation")), FRuiValue(V));
		}
		FRuiStyleBuilder& RenderScale(float V) { return Set(FName(TEXT("RenderScale")), FRuiValue(V)); }
		FRuiStyleBuilder& RenderTransformAngle(float Degrees)
		{
			return Set(FName(TEXT("RenderTransformAngle")), FRuiValue(Degrees));
		}
		FRuiStyleBuilder& RenderTransformPivot(const FVector2D& V)
		{
			return Set(FName(TEXT("RenderTransformPivot")), FRuiValue(V));
		}
		FRuiStyleBuilder& ColorAndOpacity(const FLinearColor& V)
		{
			return Set(FName(TEXT("ColorAndOpacity")), FRuiValue(V));
		}
		FRuiStyleBuilder& FontSize(float V) { return Set(FName(TEXT("Font.Size")), FRuiValue(V)); }
		FRuiStyleBuilder& Justification(FName V) { return Set(FName(TEXT("Justification")), FRuiValue(V)); }
		FRuiStyleBuilder& AutoWrapText(bool V) { return Set(FName(TEXT("AutoWrapText")), FRuiValue(V)); }
		FRuiStyleBuilder& FillColorAndOpacity(const FLinearColor& V)
		{
			return Set(FName(TEXT("FillColorAndOpacity")), FRuiValue(V));
		}
		FRuiStyleBuilder& Clipping(FName V) { return Set(FName(TEXT("Clipping")), FRuiValue(V)); }
		FRuiStyleBuilder& ToolTipText(const FText& V) { return Set(FName(TEXT("ToolTipText")), FRuiValue(V)); }
		FRuiStyleBuilder& LineHeightPercentage(float V)
		{
			return Set(FName(TEXT("LineHeightPercentage")), FRuiValue(V));
		}
		FRuiStyleBuilder& OverflowPolicy(FName V) { return Set(FName(TEXT("OverflowPolicy")), FRuiValue(V)); }

		FRuiStyleBuilder& Set(FName Key, FRuiValue V)
		{
			Dict->Add(Key, MoveTemp(V));
			return *this;
		}
		operator TSharedPtr<FRuiStyleDict>() const { return Dict; }
		TSharedPtr<FRuiStyleDict> Build() const { return Dict; }
	};

	inline FRuiStyleBuilder Style()
	{
		return FRuiStyleBuilder();
	}

	/** `RUI::Slot().Padding(8).HAlign(HAlign_Center).Fill(1.f)` -> the slot.* dict. */
	struct FRuiSlotBuilder
	{
		TSharedRef<FRuiStyleDict> Dict = MakeShared<FRuiStyleDict>();

		FRuiSlotBuilder& Padding(const FMargin& M)
		{
			return Set(FName(TEXT("slot.padding")),
					   FRuiValue(FString::Printf(TEXT("%g,%g,%g,%g"), M.Left, M.Top, M.Right, M.Bottom)));
		}
		FRuiSlotBuilder& Padding(float Uniform) { return Set(FName(TEXT("slot.padding")), FRuiValue(Uniform)); }
		FRuiSlotBuilder& HAlign(EHorizontalAlignment H)
		{
			return Set(FName(TEXT("slot.halign")), FRuiValue(HAlignName(H)));
		}
		FRuiSlotBuilder& VAlign(EVerticalAlignment V)
		{
			return Set(FName(TEXT("slot.valign")), FRuiValue(VAlignName(V)));
		}
		FRuiSlotBuilder& Fill(float Coefficient) { return Set(FName(TEXT("slot.fill")), FRuiValue(Coefficient)); }
		FRuiSlotBuilder& ZOrder(int32 Z) { return Set(FName(TEXT("Slot.ZOrder")), FRuiValue(static_cast<int64>(Z))); }
		FRuiSlotBuilder& Position(const FVector2D& V) { return Set(FName(TEXT("Slot.Position")), FRuiValue(V)); }
		FRuiSlotBuilder& Size(const FVector2D& V) { return Set(FName(TEXT("Slot.Size")), FRuiValue(V)); }
		FRuiSlotBuilder& Offset(const FMargin& M)
		{
			return Set(FName(TEXT("Slot.Offset")),
					   FRuiValue(FString::Printf(TEXT("%g,%g,%g,%g"), M.Left, M.Top, M.Right, M.Bottom)));
		}
		FRuiSlotBuilder& Anchors(const FVector2D& MinMax)
		{
			return Set(FName(TEXT("Slot.Anchors")), FRuiValue(MinMax));
		}
		FRuiSlotBuilder& Alignment(const FVector2D& V) { return Set(FName(TEXT("Slot.Alignment")), FRuiValue(V)); }
		FRuiSlotBuilder& AutoSize(bool V) { return Set(FName(TEXT("Slot.AutoSize")), FRuiValue(V)); }
		FRuiSlotBuilder& Role(FName V) { return Set(FName(TEXT("Slot.Role")), FRuiValue(V)); }

		FRuiSlotBuilder& Set(FName Key, FRuiValue V)
		{
			Dict->Add(Key, MoveTemp(V));
			return *this;
		}
		operator TSharedPtr<FRuiStyleDict>() const { return Dict; }
		TSharedPtr<FRuiStyleDict> Build() const { return Dict; }

	private:
		static FName HAlignName(EHorizontalAlignment H)
		{
			switch (H)
			{
			case HAlign_Left:
				return FName(TEXT("left"));
			case HAlign_Center:
				return FName(TEXT("center"));
			case HAlign_Right:
				return FName(TEXT("right"));
			default:
				return FName(TEXT("fill"));
			}
		}
		static FName VAlignName(EVerticalAlignment V)
		{
			switch (V)
			{
			case VAlign_Top:
				return FName(TEXT("top"));
			case VAlign_Center:
				return FName(TEXT("center"));
			case VAlign_Bottom:
				return FName(TEXT("bottom"));
			default:
				return FName(TEXT("fill"));
			}
		}
	};

	inline FRuiSlotBuilder Slot()
	{
		return FRuiSlotBuilder();
	}
} // namespace RUI
