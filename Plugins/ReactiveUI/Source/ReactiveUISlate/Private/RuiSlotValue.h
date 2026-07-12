// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Kind-aware readers for `Slot.*` values. The toolchain emits every LITERAL slot attribute as a String
// (`FRuiValue(TEXT("1"))`), while the expression form emits a typed Int/Float — so a reader that only
// consults IntValue/FloatValue silently reads a zero for the (common) literal form (bughunt SLOT-1 /
// SLOT-2, same class as UMG B13). Every slot reader routes through here so String, Name, Int and Float
// forms all resolve identically, everywhere.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "RuiTypes.h"

namespace RUI::Slate::SlotValue
{
	/** Read a slot value as a float, parsing the String/Name literal forms. */
	inline float AsFloat(const FRuiValue& V, float Def = 0.0f)
	{
		switch (V.Kind)
		{
		case FRuiValue::EKind::Int:
			return static_cast<float>(V.IntValue);
		case FRuiValue::EKind::Float:
			return static_cast<float>(V.FloatValue);
		case FRuiValue::EKind::String:
			return V.StringValue.IsEmpty() ? Def : static_cast<float>(FCString::Atof(*V.StringValue));
		case FRuiValue::EKind::Name:
			return static_cast<float>(FCString::Atof(*V.NameValue.ToString()));
		default:
			return Def;
		}
	}

	/** Read a slot value as an int32, parsing the String/Name literal forms. */
	inline int32 AsInt(const FRuiValue& V, int32 Def = 0)
	{
		switch (V.Kind)
		{
		case FRuiValue::EKind::Int:
			return static_cast<int32>(V.IntValue);
		case FRuiValue::EKind::Float:
			return static_cast<int32>(V.FloatValue);
		case FRuiValue::EKind::String:
			return V.StringValue.IsEmpty() ? Def : FCString::Atoi(*V.StringValue);
		case FRuiValue::EKind::Name:
			return FCString::Atoi(*V.NameValue.ToString());
		default:
			return Def;
		}
	}

	/** Read a slot value as an FMargin: uniform (Int/Float), Vector2 (h,v), or the String/Name comma
	 *  forms `"u"` / `"h,v"` / `"l,t,r,b"`. Mirrors ParsePadding so every panel honors slot.padding. */
	inline FMargin AsMargin(const FRuiValue& V)
	{
		switch (V.Kind)
		{
		case FRuiValue::EKind::Int:
			return FMargin(static_cast<float>(V.IntValue));
		case FRuiValue::EKind::Float:
			return FMargin(static_cast<float>(V.FloatValue));
		case FRuiValue::EKind::Vector2:
			return FMargin(static_cast<float>(V.Vector2Value.X), static_cast<float>(V.Vector2Value.Y));
		case FRuiValue::EKind::String:
		case FRuiValue::EKind::Name:
		{
			const FString S = V.Kind == FRuiValue::EKind::Name ? V.NameValue.ToString() : V.StringValue;
			TArray<FString> Parts;
			S.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 4)
			{
				return FMargin(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]),
							   FCString::Atof(*Parts[3]));
			}
			if (Parts.Num() == 2)
			{
				return FMargin(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
			}
			if (Parts.Num() == 1)
			{
				return FMargin(FCString::Atof(*Parts[0]));
			}
			break;
		}
		default:
			break;
		}
		return FMargin(0.0f);
	}
} // namespace RUI::Slate::SlotValue
