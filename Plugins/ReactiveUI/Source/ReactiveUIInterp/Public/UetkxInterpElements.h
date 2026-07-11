// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The interpreter's element vocabulary: tag -> a builder closure that constructs the TYPED
// props struct, applies attrs/styles/slots/classes/events, and calls the same RUI::Slate
// factory the generated C++ calls — one VNode input format into one reconciler, so
// interpreted and compiled components co-exist in one fiber tree. Registrations MUST stay in
// sync with the codegen vocabulary (the Hmr suite asserts every schema element registers).

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiTypes.h"

struct REACTIVEUIINTERP_API FUetkxInterpElementInputs
{
	TMap<FName, FRuiValue> Attrs;		 // typed attr values (evaluated)
	TMap<FName, FRuiCallback> Events;	 // event attr -> bound callback
	TSharedPtr<FRuiStyleDict> Style;	 // style keys (may be null)
	TSharedPtr<FRuiStyleDict> SlotProps; // Slot.* keys (may be null)
	TArray<FName> Classes;
	TArray<FRuiNode> Children;
	FRuiKey Key;
};

using FUetkxInterpElementBuilder = TFunction<FRuiNode(FUetkxInterpElementInputs&&)>;

class REACTIVEUIINTERP_API FUetkxInterpElements
{
public:
	static void Register(FName Tag, FUetkxInterpElementBuilder Builder);
	static bool Has(FName Tag);
	static FRuiNode Build(FName Tag, FUetkxInterpElementInputs&& Inputs); // empty node when unknown

	/** The 15-tag Slate vocabulary (module startup; idempotent). */
	static void RegisterBuiltins();

	/** The style-key vocabulary — must match the codegen schema's styleKeys (parity-tested
	 *  by ReactiveUI.Hmr against the exported schema). */
	static const TSet<FString>& StyleKeys();
};
