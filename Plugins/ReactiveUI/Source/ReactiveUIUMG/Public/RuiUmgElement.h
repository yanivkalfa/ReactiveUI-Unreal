// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// "Theirs inside ours": a UMG UUserWidget hosted as a Rui element. The adapter creates the
// widget ONCE per mounted node (CreateWidget + TakeWidget — SObjectWidget holds the strong
// UObject ref, so GC safety rides UMG's own mechanism) and replaces it when the class or
// owner changes (reconstruct mask). The PROP-MAP bridge (TD-021) applies declarative Rui props
// to the hosted widget's UPROPERTYs by reflection each commit (name -> FRuiValue, type-matched),
// so a hosted UUserWidget receives Rui-driven data without a hand-written binding.

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiPropsBase.h"
#include "RuiTypes.h" // FRuiStyleDict
#include "Templates/SubclassOf.h"

class UUserWidget;
class UWorld;

struct REACTIVEUIUMG_API FRuiUmgProps final : public FRuiPropsBase
{
	RUI_PROP(TSubclassOf<UUserWidget>, WidgetClass, 0)
	RUI_PROP(TWeakObjectPtr<UWorld>, World, 1)
	/** name -> value, applied to the hosted widget's matching UPROPERTYs by reflection each commit. */
	RUI_PROP(FRuiStyleDict, WidgetProps, 2)

	virtual bool Equals(const FRuiPropsBase& OtherBase) const override
	{
		const FRuiUmgProps& Other = static_cast<const FRuiUmgProps&>(OtherBase);
		if (!BaseFieldsEqual(OtherBase) || !(WidgetClass == Other.WidgetClass) || !(World == Other.World))
		{
			return false;
		}
		if (HasWidgetProps() != Other.HasWidgetProps())
		{
			return false;
		}
		return !HasWidgetProps() || WidgetProps.OrderIndependentCompareEqual(Other.WidgetProps);
	}
};

namespace RUI::Umg
{
	/** A UMG widget as a Rui node. The widget is created against World (its owning player
	 *  context); replacing WidgetClass replaces the widget. */
	REACTIVEUIUMG_API FRuiNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuiKey Key = FRuiKey());

	/** As above, plus a declarative prop map applied to the hosted widget's UPROPERTYs. */
	REACTIVEUIUMG_API FRuiNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World,
										  FRuiStyleDict WidgetProps, FRuiKey Key = FRuiKey());

	/** Apply a prop map to a widget's UPROPERTYs by reflection (int/float/bool/string/text/name,
	 *  type-matched to the FRuiValue kind). Unknown names and type mismatches are skipped. Returns
	 *  the number of properties actually set. Public so tools/tests can drive it directly. */
	REACTIVEUIUMG_API int32 ApplyPropMap(UUserWidget* Widget, const FRuiStyleDict& WidgetProps);

	/** Register the adapter (module startup; idempotent). */
	REACTIVEUIUMG_API void RegisterUmgAdapters();
} // namespace RUI::Umg
