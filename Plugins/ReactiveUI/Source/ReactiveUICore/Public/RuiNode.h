// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuiNode — the virtual node: a lightweight description of a piece of UI, diffed by the
// reconciler against the persistent fiber tree. Mirrors the family's five kinds
// (HOST/FUNCTION/FRAGMENT/PORTAL/ERROR_BOUNDARY).
//
// LIFETIME (differs from the plan's original "per-frame arena" idea — superseded with
// reasoning): vnodes are NOT single-frame. The bailout path reuses a component's cached
// last render output (FRuiComponentState::LastOutput) across frames, and committed props
// live on fibers until replaced — so both nodes and props are shared-ptr persistent, and
// only the FIBERS get slab allocation. (MASTER_PLAN Phase 1 note updated at commit.)

#pragma once

#include "CoreMinimal.h"
#include "RuiTypes.h"
#include "RuiPropsBase.h"

struct FRuiNode;
class FRuiContext;

/** A component's render function output. */
using FRuiNodeArray = TArray<FRuiNode>;

/**
 * Type-erased component invoker: created by RUI::FC from a typed free function; carries the
 * typed props and calls the function with them. The INVOKER is per-vnode; the component's
 * IDENTITY for reconciliation is the registered FName (D-05 — raw fn pointers break across
 * Live Coding relocations, so identity lives in the registry, never in the pointer).
 */
using FRuiComponentInvoke = TFunction<FRuiNodeArray(FRuiContext&, const FRuiPropsBase*, const TArray<FRuiNode>&)>;

/** The five node kinds (family parity). */
enum class ERuiNodeKind : uint8
{
	Host,
	Function,
	Fragment,
	Portal,
	ErrorBoundary,
};

/** Shared, immutable child list. SHARED (not by-value) because fibers keep referencing a
 *  node's children across frames for the bailout children-identity comparison — a value
 *  copy per fiber would be the exact allocation churn D-06 exists to avoid, and a view
 *  would dangle when a re-render replaces the cached output that owns it. Pointer equality
 *  of two child lists == the family's vnode-identity children_same check. */
using FRuiChildren = TSharedPtr<const TArray<FRuiNode>>;

struct REACTIVEUICORE_API FRuiNode
{
	ERuiNodeKind Kind = ERuiNodeKind::Fragment;

	/** HOST: the interned element type (adapter registry key). */
	FRuiElementTypeId ElementType;

	/** FUNCTION: registered identity + the typed invoker. */
	FName ComponentId;
	TSharedPtr<FRuiComponentInvoke> Invoke;

	/** Props (shared, immutable once built; pointer identity = memo fast path). */
	TSharedPtr<const FRuiPropsBase> Props;

	/** Children (shared; null = none). */
	FRuiChildren Children;

	FRuiKey Key;

	/** PORTAL: opaque host target. */
	FRuiPortalHandle PortalTarget;

	// --- ERROR_BOUNDARY fields (props-as-fields; boundaries are structural — D-10) ---
	TSharedPtr<FRuiNode> EbFallback;
	TFunction<void(const FString&)> EbOnError;
	FRuiKey EbResetKey;

	int32 NumChildren() const { return Children.IsValid() ? Children->Num() : 0; }

	/** Structural equality is NOT defined — nodes compare by (Kind, identity fields) inside
	 *  the reconciler only; children lists compare by POINTER (see FRuiChildren). */
};

namespace RUI
{
	/** Build a shared child list (the factories' common path). */
	REACTIVEUICORE_API FRuiChildren MakeChildren(TArray<FRuiNode> InChildren);
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Component registry (D-05): stable FName identity surviving Live Coding.
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace RUI
{
	/** Register/refresh a component id → nothing to store beyond the name's existence; the
	 *  fn-pointer → FName map lets FC() resolve identity fast and RE-RESOLVE after Live
	 *  Coding relocates code (pointers change; names don't). */
	REACTIVEUICORE_API FName RegisterComponentId(void* FnPtr, FName Id);

	/** The registered id for a fn pointer (NAME_None if unregistered — lambda components:
	 *  documented always-re-render semantics via a per-call unique id). */
	REACTIVEUICORE_API FName FindComponentId(void* FnPtr);
}

/**
 * Declare a component's stable identity next to its definition:
 *
 *   FRuiNodeArray Counter(FRuiContext& Ctx, const FCounterProps& Props, const TArray<FRuiNode>& Children);
 *   RUI_COMPONENT(Counter)
 *
 * The .uetkx codegen emits the same macro; hand-written and generated components are
 * indistinguishable to the reconciler.
 */
#define RUI_COMPONENT(FnName) \
	static const FName FnName##_RuiId = RUI::RegisterComponentId((void*)&FnName, FName(TEXT(#FnName)));

// ─────────────────────────────────────────────────────────────────────────────────────────
// Node factories — the RUI:: builder surface's structural pieces (element builders arrive
// with the host adapters; these are the engine-blind ones).
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace RUI
{
	/** The shape of a typed component function. */
	template <typename TProps>
	using TRuiComponentFn = FRuiNodeArray (*)(FRuiContext&, const TProps&, const TArray<FRuiNode>&);

	/** Function component node from a typed free function (identity = registered FName). */
	template <typename TProps>
	FRuiNode FC(TRuiComponentFn<TProps> Fn,
	            TProps InProps = TProps(), TArray<FRuiNode> InChildren = TArray<FRuiNode>(), FRuiKey InKey = FRuiKey())
	{
		static_assert(std::is_base_of_v<FRuiPropsBase, TProps>, "component props must derive FRuiPropsBase");
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Function;
		Node.ComponentId = FindComponentId((void*)Fn);
		if (Node.ComponentId.IsNone())
		{
			// Unregistered (e.g. a local lambda-ish fn): mint a per-pointer id. Identity is
			// then stable for the process but resets across Live Coding — the documented
			// always-may-reset semantics for unregistered components.
			Node.ComponentId = RegisterComponentId((void*)Fn, FName(*FString::Printf(TEXT("__anon_%p"), (void*)Fn)));
		}
		TSharedRef<const TProps> Shared = MakeShared<const TProps>(MoveTemp(InProps));
		Node.Props = Shared;
		Node.Invoke = MakeShared<FRuiComponentInvoke>(
			[Fn](FRuiContext& Ctx, const FRuiPropsBase* Props, const TArray<FRuiNode>& Children) -> FRuiNodeArray
			{
				// Invariant: the reconciler only pairs a fiber with vnodes of the SAME
				// ComponentId, and FC always stores TProps for that id — the cast is sound.
				return Fn(Ctx, *static_cast<const TProps*>(Props), Children);
			});
		Node.Children = MakeChildren(MoveTemp(InChildren));
		Node.Key = InKey;
		return Node;
	}

	REACTIVEUICORE_API FRuiNode Fragment(TArray<FRuiNode> Children, FRuiKey Key = FRuiKey());

	REACTIVEUICORE_API FRuiNode Portal(FRuiPortalHandle Target, TArray<FRuiNode> Children, FRuiKey Key = FRuiKey());

	/**
	 * Structural error boundary (family semantics, D-10): renders Fallback when activated —
	 * by the cooperative error latch (RUI::FailRender) or imperatively — and resets when
	 * ResetKey changes. Not a markup tag (family convention): an escape-hatch call.
	 */
	REACTIVEUICORE_API FRuiNode ErrorBoundary(FRuiNode Fallback, TArray<FRuiNode> Children,
	                                          FRuiKey ResetKey = FRuiKey(),
	                                          TFunction<void(const FString&)> OnError = nullptr,
	                                          FRuiKey Key = FRuiKey());
}
