// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The FRuiValue ↔ reflected-UPROPERTY marshaling helpers (the research-promised "marshaling
// helper", promoted from the UMG prop-map bridge's internals so every seam shares ONE
// conversion table): the prop-map (ApplyPropMap), host-props consumers, and any game code
// moving values between Rui state and Blueprint-visible properties.
//
// Supported property types: bool, int32/int64, float/double, FString, FText, FName.
// Rules: numeric kinds coerce Int↔Float; stringy kinds coerce String↔Text↔Name; any other
// kind/type mismatch returns false and writes nothing ("mismatches are skipped, never
// mangled" — the documented prop-map contract, bughunt B13).

#pragma once

#include "CoreMinimal.h"
#include "RuiTypes.h" // FRuiValue

namespace RUI::Umg
{
	/** Write `Value` into `Object`'s reflected property. False = missing property, unsupported
	 *  property type, or incompatible value kind (nothing written). */
	REACTIVEUIUMG_API bool MarshalToProperty(UObject* Object, FName PropertyName, const FRuiValue& Value);

	/** Read `Object`'s reflected property into `OutValue` (kind follows the property type).
	 *  False = missing property or unsupported type (OutValue untouched). */
	REACTIVEUIUMG_API bool MarshalFromProperty(const UObject* Object, FName PropertyName, FRuiValue& OutValue);
} // namespace RUI::Umg
