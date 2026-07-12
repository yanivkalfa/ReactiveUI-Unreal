// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SExpandableArea, the collapsible section. It is the family's first TWO-NAMED-SLOT
// widget: a header and a body, addressed by `slot.role = "header" | "body"` on the children (each a
// single content slot). SExpandableArea's HeaderContent/BodyContent are CONSTRUCT-time named slots
// with no runtime setters, so SRuiExpandableArea builds it once around two persistent SBox holders
// and reparents the Rui children into them — the reconciler's child ops route by role.
//
// Expansion is a CONTROLLED prop (React parity): `bIsExpanded` is applied skip-when-equal against
// the widget's live state each render (the self-notifying family rule, D-16), and OnExpansionChanged
// fires the user's toggle so the parent can drive state. Headless-safe: SetExpanded/IsExpanded and
// the expansion delegate are directly exercisable without a paint loop.

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiPropsBase.h"
#include "Widgets/SCompoundWidget.h"

class SExpandableArea;
class SBox;

/** SExpandableArea (two role slots): children carry `slot.role = "header" | "body"`. `bIsExpanded`
 *  is the controlled expansion state; OnExpansionChanged fires (Value = bool) when the user toggles. */
struct REACTIVEUISLATE_API FRuiExpandableAreaProps final : public FRuiPropsBase
{
	RUI_PROP(bool, bIsExpanded, 0)
	RUI_PROP_EVENT(OnExpansionChanged, 1)
	RUI_PROPS_BODY(FRuiExpandableAreaProps, RUI_EQ(bIsExpanded) RUI_EQ(OnExpansionChanged))
};

/** Wraps SExpandableArea with two SBox content holders the reconciler reparents children into. */
class REACTIVEUISLATE_API SRuiExpandableArea final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuiExpandableArea) : _InitiallyExpanded(true) {}
	SLATE_ARGUMENT(bool, InitiallyExpanded)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetExpanded(bool bExpanded);
	bool IsExpanded() const;
	void SetOnExpansionChanged(FRuiCallback InCb) { OnExpansionChanged = MoveTemp(InCb); }

	/** Route a child into the header or body holder by role ("header" -> header, else body). */
	void SetRoleContent(FName Role, const TSharedPtr<SWidget>& Content);
	/** Clear whichever holder currently shows `Content`. */
	void ClearContent(const TSharedRef<SWidget>& Content);
	/** The widget currently in the header ("header") or body holder (any other role); null if empty. */
	TSharedPtr<SWidget> GetRoleContent(FName Role) const;

private:
	void HandleExpansionChanged(bool bExpanded);

	TSharedPtr<SExpandableArea> Area;
	TSharedPtr<SBox> HeaderBox;
	TSharedPtr<SBox> BodyBox;
	FRuiCallback OnExpansionChanged;
};

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId ExpandableAreaType();

	/** A collapsible section. Give it two children with `slot.role="header"` and `slot.role="body"`. */
	REACTIVEUISLATE_API FRuiNode ExpandableArea(FRuiExpandableAreaProps Props = FRuiExpandableAreaProps(),
												TArray<FRuiNode> Children = TArray<FRuiNode>(),
												FRuiKey Key = FRuiKey());

	namespace Detail
	{
		void RegisterExpandableAreaAdapter();
	}
} // namespace RUI::Slate
