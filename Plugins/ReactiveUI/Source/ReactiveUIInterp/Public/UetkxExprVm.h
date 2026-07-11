// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxExprVm — the D-20 restricted, side-effect-free expression VM over FRuiValue that the
// dev-loop interpreter evaluates `{expr}` / attr expressions / directive conditions with.
// Parse once (definition swap) → evaluate per render against an environment (props fields,
// hook-state values by declared name, loop variables). TOTAL evaluation: anything outside
// the subset fails PARSE with a reason (the component then falls back honestly); runtime
// errors produce Null + a collected note, never a crash.
//
// Subset: literals (numbers/strings/TEXT()/true/false), identifiers, `::`-qualified calls
// into the whitelist registry, method sugar (`x.ToString()`, `x.IsEmpty()`), member access
// on Vector2/Color, unary !/-, arithmetic/comparison/logical operators, ternary. No
// assignment, no indexing (FRuiValue has no array kind — recorded limitation), no `new`, no
// casts, no arbitrary UE API.

#pragma once

#include "CoreMinimal.h"
#include "RuiTypes.h"

enum class EUetkxExprOp : uint8
{
	Literal,
	Ident,
	Member, // Args[0] = receiver; Name = field
	Call,	// Name = qualified fn ("FText::FromString") or method (".ToString", receiver = Args[0])
	Unary,	// Name = "!" | "-"
	Binary, // Name = operator token
	Ternary // Args = {cond, then, else}
};

struct REACTIVEUIINTERP_API FUetkxExprNode
{
	EUetkxExprOp Op = EUetkxExprOp::Literal;
	FRuiValue Literal;
	FString Name;
	TArray<TSharedPtr<FUetkxExprNode>> Args;
};

struct REACTIVEUIINTERP_API FUetkxExprEnv
{
	TMap<FString, FRuiValue> Vars;
	const FUetkxExprEnv* Parent = nullptr;

	bool Resolve(const FString& Ident, FRuiValue& Out) const
	{
		if (const FRuiValue* Found = Vars.Find(Ident))
		{
			Out = *Found;
			return true;
		}
		return Parent ? Parent->Resolve(Ident, Out) : false;
	}
};

class REACTIVEUIINTERP_API FUetkxExprVm
{
public:
	/** nullptr when the expression is outside the interpretable subset (reason in OutWhy). */
	static TSharedPtr<FUetkxExprNode> Parse(const FString& Expr, FString* OutWhy = nullptr);

	/** Total: runtime failures append to OutErrors (if given) and yield Null. */
	static FRuiValue Eval(const FUetkxExprNode& Node, const FUetkxExprEnv& Env, TArray<FString>* OutErrors = nullptr);

	/** Whitelist registry (RUI::RegisterInterpFn surface). Re-registering replaces. */
	static void RegisterFn(FName Qualified, TFunction<FRuiValue(TArrayView<const FRuiValue>)> Fn);
	static bool HasFn(FName Qualified);

	/** Ships FText/FString fmt helpers, FMath, LexToString, value constructors. Idempotent. */
	static void RegisterBuiltins();

	/** Truthiness (the VM's bool coercion — shared with directive conditions). */
	static bool Truthy(const FRuiValue& Value);

	/** Display form (mirrors what generated C++ would print for the same value). */
	static FString ToDisplayString(const FRuiValue& Value);
};
