// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Foundational value types for the engine-blind core. HARD CONSTRAINT (MASTER_PLAN D-27):
// this module sees Core only — no CoreUObject, no Slate. Everything engine-facing goes
// through IRuiHostConfig (RuiHostConfig.h) over the opaque handle types defined here.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"

/**
 * Reconciliation key — the family's `key` prop. A key is either unset, an integer, or a
 * string (FName-interned). Unkeyed children get NAMESPACED positional keys during keyed
 * reconciliation (see FRuiReconciler) so an integer user key can never collide with a
 * positional index (the Godot port's [audit M1]).
 */
struct FRuiKey
{
	enum class EKind : uint8
	{
		None,
		Int,
		Name
	};

	EKind Kind = EKind::None;
	int64 IntValue = 0;
	FName NameValue;

	FRuiKey() = default;
	FRuiKey(int32 In) : Kind(EKind::Int), IntValue(In) {}
	FRuiKey(int64 In) : Kind(EKind::Int), IntValue(In) {}
	FRuiKey(FName In) : Kind(EKind::Name), NameValue(In) {}
	FRuiKey(const TCHAR* In) : Kind(EKind::Name), NameValue(In) {}
	FRuiKey(const FString& In) : Kind(EKind::Name), NameValue(*In) {}

	bool IsSet() const { return Kind != EKind::None; }

	bool operator==(const FRuiKey& Other) const
	{
		if (Kind != Other.Kind)
		{
			return false;
		}
		switch (Kind)
		{
		case EKind::None:
			return true;
		case EKind::Int:
			return IntValue == Other.IntValue;
		case EKind::Name:
			return NameValue == Other.NameValue;
		}
		return false;
	}
	bool operator!=(const FRuiKey& Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FRuiKey& K)
	{
		switch (K.Kind)
		{
		case EKind::None:
			return 0;
		case EKind::Int:
			return HashCombine(1u, GetTypeHash(K.IntValue));
		case EKind::Name:
			return HashCombine(2u, GetTypeHash(K.NameValue));
		}
		return 0;
	}
};

/**
 * The blessed value variant for style dicts and generic prop plumbing (the LSP/markup layer
 * and the mock host speak this; typed props structs are the fast path — D-04). Core-module
 * types only: no FMargin (SlateCore), no UObject*.
 */
struct FRuiValue
{
	enum class EKind : uint8
	{
		Null,
		Bool,
		Int,
		Float,
		String,
		Name,
		Text,
		Vector2,
		Color,
		Opaque
	};

	EKind Kind = EKind::Null;
	bool BoolValue = false;
	int64 IntValue = 0;
	double FloatValue = 0.0;
	FString StringValue;
	FName NameValue;
	FText TextValue;
	FVector2D Vector2Value = FVector2D::ZeroVector;
	FLinearColor ColorValue = FLinearColor::White;
	TSharedPtr<void> OpaqueValue;

	FRuiValue() = default;
	FRuiValue(bool In) : Kind(EKind::Bool), BoolValue(In) {}
	FRuiValue(int32 In) : Kind(EKind::Int), IntValue(In) {}
	FRuiValue(int64 In) : Kind(EKind::Int), IntValue(In) {}
	FRuiValue(float In) : Kind(EKind::Float), FloatValue(In) {}
	FRuiValue(double In) : Kind(EKind::Float), FloatValue(In) {}
	FRuiValue(const TCHAR* In) : Kind(EKind::String), StringValue(In) {}
	FRuiValue(const FString& In) : Kind(EKind::String), StringValue(In) {}
	FRuiValue(FName In) : Kind(EKind::Name), NameValue(In) {}
	FRuiValue(const FText& In) : Kind(EKind::Text), TextValue(In) {}
	FRuiValue(const FVector2D& In) : Kind(EKind::Vector2), Vector2Value(In) {}
	FRuiValue(const FLinearColor& In) : Kind(EKind::Color), ColorValue(In) {}

	bool IsNull() const { return Kind == EKind::Null; }

	/** Value equality for value kinds; IDENTITY for Opaque (the family's Object.is split). */
	bool operator==(const FRuiValue& Other) const
	{
		if (Kind != Other.Kind)
		{
			return false;
		}
		switch (Kind)
		{
		case EKind::Null:
			return true;
		case EKind::Bool:
			return BoolValue == Other.BoolValue;
		case EKind::Int:
			return IntValue == Other.IntValue;
		case EKind::Float:
			return FloatValue == Other.FloatValue;
		case EKind::String:
			return StringValue == Other.StringValue;
		case EKind::Name:
			return NameValue == Other.NameValue;
		case EKind::Text:
			return TextValue.IdenticalTo(Other.TextValue) || TextValue.ToString() == Other.TextValue.ToString();
		case EKind::Vector2:
			return Vector2Value == Other.Vector2Value;
		case EKind::Color:
			return ColorValue == Other.ColorValue;
		case EKind::Opaque:
			return OpaqueValue == Other.OpaqueValue;
		}
		return false;
	}
	bool operator!=(const FRuiValue& Other) const { return !(*this == Other); }
};

/** Style dict — the inline `style={...}` layer (v1 styling, D-13). Host maps keys to setters. */
using FRuiStyleDict = TMap<FName, FRuiValue>;

/**
 * An event callback prop. TFunction has NO operator== (the C++ reality that shapes the whole
 * event design — MASTER_PLAN D-11): equality here is IDENTITY of the shared inner, which is
 * exactly what UseCallback/UseStableCallback hand out. A freshly-minted lambda every render
 * compares unequal — same as React; wrap in UseCallback for memo-friendliness.
 */
class FRuiCallback
{
public:
	FRuiCallback() = default;

	/** 1-arg form (event payload). */
	static FRuiCallback Create(TFunction<void(const FRuiValue&)> InFn)
	{
		FRuiCallback Out;
		if (InFn)
		{
			Out.Inner = MakeShared<TFunction<void(const FRuiValue&)>>(MoveTemp(InFn));
		}
		return Out;
	}

	/** 0-arg convenience (onClick etc.). */
	static FRuiCallback Create(TFunction<void()> InFn)
	{
		if (!InFn)
		{
			return FRuiCallback();
		}
		return Create(TFunction<void(const FRuiValue&)>([Fn = MoveTemp(InFn)](const FRuiValue&) { Fn(); }));
	}

	bool IsBound() const { return Inner.IsValid(); }
	void Execute(const FRuiValue& Arg = FRuiValue()) const
	{
		if (Inner.IsValid())
		{
			(*Inner)(Arg);
		}
	}

	/** Identity compare (see class comment). */
	bool operator==(const FRuiCallback& Other) const { return Inner == Other.Inner; }
	bool operator!=(const FRuiCallback& Other) const { return !(*this == Other); }

private:
	TSharedPtr<TFunction<void(const FRuiValue&)>> Inner;
};

/**
 * Opaque host-node handle. The reconciler stores and threads these; only the host config
 * interprets them (Slate: a type-erased TSharedPtr<SWidget>; mock host: a test node).
 * TSharedPtr<void> keeps ownership semantics without the core knowing the concrete type.
 */
using FRuiHostHandle = TSharedPtr<void>;

/** Portal target — same opacity as node handles (an overlay slot, a window, a mock node). */
using FRuiPortalHandle = TSharedPtr<void>;

/** Registry-interned host element type (the markup tag / RUI:: factory → adapter key). */
struct FRuiElementTypeId
{
	uint16 Value = 0; // 0 = invalid/none
	bool IsValid() const { return Value != 0; }
	bool operator==(const FRuiElementTypeId& O) const { return Value == O.Value; }
	bool operator!=(const FRuiElementTypeId& O) const { return Value != O.Value; }
	friend uint32 GetTypeHash(const FRuiElementTypeId& Id) { return Id.Value; }
};
