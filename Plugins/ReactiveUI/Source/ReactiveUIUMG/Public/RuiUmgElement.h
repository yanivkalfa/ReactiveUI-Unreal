// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// "Theirs inside ours": a UMG UUserWidget hosted as a Rui element. The adapter creates the
// widget ONCE per mounted node (CreateWidget + TakeWidget — SObjectWidget holds the strong
// UObject ref, so GC safety rides UMG's own mechanism) and replaces it when the class or
// owner changes (reconstruct mask). Prop application into the hosted widget goes through its
// own Blueprint/C++ surface — this element is the EMBEDDING seam, not a prop-map bridge
// (per-class prop maps + delegate trampolines are the recorded follow-up).

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiPropsBase.h"
#include "Templates/SubclassOf.h"

class UUserWidget;
class UWorld;

struct REACTIVEUIUMG_API FRuiUmgProps final : public FRuiPropsBase
{
	RUI_PROP(TSubclassOf<UUserWidget>, WidgetClass, 0)
	RUI_PROP(TWeakObjectPtr<UWorld>, World, 1)
	RUI_PROPS_BODY(FRuiUmgProps, RUI_EQ(WidgetClass) RUI_EQ(World))
};

namespace RUI::Umg
{
	/** A UMG widget as a Rui node. The widget is created against World (its owning player
	 *  context); replacing WidgetClass replaces the widget. */
	REACTIVEUIUMG_API FRuiNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuiKey Key = FRuiKey());

	/** Register the adapter (module startup; idempotent). */
	REACTIVEUIUMG_API void RegisterUmgAdapters();
} // namespace RUI::Umg
