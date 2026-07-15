// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The single FRuiValue ↔ UPROPERTY conversion table (see RuiMarshal.h). The write half is the
// prop-map bridge's former inner loop, promoted verbatim (bughunt B13's kind-validation rules
// preserved); the read half mirrors it for the reverse direction.

#include "RuiMarshal.h"

#include "UObject/UnrealType.h"

bool RUI::Umg::MarshalToProperty(UObject* Object, FName PropertyName, const FRuiValue& Value)
{
	if (Object == nullptr)
	{
		return false;
	}
	FProperty* Prop = Object->GetClass()->FindPropertyByName(PropertyName);
	if (Prop == nullptr)
	{
		return false;
	}
	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);
	const FRuiValue& V = Value;

	// Dispatch on the DESTINATION property type but validate against V.Kind (bughunt B13): FRuiValue
	// stores each kind in a separate zero-defaulted member, so reading (e.g.) V.FloatValue on an
	// Int-kind value silently yields 0.0. Numeric props coerce Int<->Float; every other category
	// requires a compatible kind, else nothing is written (the documented "mismatches are skipped").
	using EKind = FRuiValue::EKind;
	const bool bNumeric = (V.Kind == EKind::Int || V.Kind == EKind::Float);
	const double NumD = (V.Kind == EKind::Float) ? V.FloatValue : static_cast<double>(V.IntValue);
	const int64 NumI = (V.Kind == EKind::Float) ? static_cast<int64>(V.FloatValue) : V.IntValue;
	const bool bStringy = (V.Kind == EKind::String || V.Kind == EKind::Text || V.Kind == EKind::Name);
	auto AsString = [&V]() -> FString
	{
		return V.Kind == EKind::Text ? V.TextValue.ToString()
									 : (V.Kind == EKind::Name ? V.NameValue.ToString() : V.StringValue);
	};

	if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
	{
		return bNumeric && (IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(NumI)), true);
	}
	if (FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
	{
		return bNumeric && (Int64Prop->SetPropertyValue(ValuePtr, NumI), true);
	}
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		return bNumeric && (FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(NumD)), true);
	}
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		return bNumeric && (DoubleProp->SetPropertyValue(ValuePtr, NumD), true);
	}
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		return (V.Kind == EKind::Bool) && (BoolProp->SetPropertyValue(ValuePtr, V.BoolValue), true);
	}
	if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		return bStringy && (StrProp->SetPropertyValue(ValuePtr, AsString()), true);
	}
	if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
	{
		return bStringy && (TextProp->SetPropertyValue(ValuePtr, V.Kind == EKind::Text ? V.TextValue
																					   : FText::FromString(AsString())),
							true);
	}
	if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
	{
		return bStringy &&
			   (NameProp->SetPropertyValue(ValuePtr, V.Kind == EKind::Name ? V.NameValue : FName(*AsString())), true);
	}
	return false; // an unsupported property type — skipped, not an error
}

bool RUI::Umg::MarshalFromProperty(const UObject* Object, FName PropertyName, FRuiValue& OutValue)
{
	if (Object == nullptr)
	{
		return false;
	}
	const FProperty* Prop = Object->GetClass()->FindPropertyByName(PropertyName);
	if (Prop == nullptr)
	{
		return false;
	}
	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);

	if (const FIntProperty* IntProp = CastField<FIntProperty>(Prop))
	{
		OutValue = FRuiValue(static_cast<int64>(IntProp->GetPropertyValue(ValuePtr)));
		return true;
	}
	if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
	{
		OutValue = FRuiValue(Int64Prop->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		OutValue = FRuiValue(static_cast<double>(FloatProp->GetPropertyValue(ValuePtr)));
		return true;
	}
	if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		OutValue = FRuiValue(DoubleProp->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		OutValue = FRuiValue(BoolProp->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		OutValue = FRuiValue(StrProp->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
	{
		OutValue = FRuiValue(TextProp->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
	{
		OutValue = FRuiValue(NameProp->GetPropertyValue(ValuePtr));
		return true;
	}
	return false;
}
