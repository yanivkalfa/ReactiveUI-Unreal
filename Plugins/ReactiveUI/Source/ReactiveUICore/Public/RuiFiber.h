// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuiFiber — a node in the persistent work tree, current/WIP paired via Alternate
// (double buffering, D-06). Fibers live in the reconciler-owned SLAB (FRuiFiberSlab):
// fixed addresses, free-list reuse, RAW intra-tree pointers, zero ref counting inside the
// tree. The Godot port's "sever cycles explicitly" generalizes here to "no cycles exist" —
// only three ownership edges leave the slab (host handle, shared state, shared props).

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiComponentState.h"

enum class ERuiFiberTag : uint8
{
	Function,
	Host,
	Fragment,
	Portal,
	ErrorBoundary,
	Root,
};

/** Effect flags — what commit must do (family bitmask, incl. the PortalRetarget addition). */
enum ERuiEffect : uint8
{
	RuiEffect_None = 0,
	RuiEffect_Placement = 1 << 0,
	RuiEffect_Update = 1 << 1,
	RuiEffect_Deletion = 1 << 2,
	RuiEffect_Layout = 1 << 3,
	RuiEffect_Passive = 1 << 4,
	RuiEffect_PortalRetarget = 1 << 5,
};

struct REACTIVEUICORE_API FRuiFiber
{
	FRuiFiber() = default;
	// Never copied/moved: fibers are slab-owned with RAW pointers into them everywhere
	// (Parent/Child/Sibling/Alternate, state back-pointers) — address stability is the
	// contract (D-06).
	FRuiFiber(const FRuiFiber&) = delete;
	FRuiFiber& operator=(const FRuiFiber&) = delete;

	// --- tree (raw, slab-owned) ---
	FRuiFiber* Parent = nullptr;
	FRuiFiber* Child = nullptr;
	FRuiFiber* Sibling = nullptr;
	int32 Index = 0;

	// --- identity ---
	ERuiFiberTag Tag = ERuiFiberTag::Host;
	FRuiKey Key;
	FRuiElementTypeId ElementType;              // HOST
	FName ComponentId;                          // FUNCTION (registry identity, D-05)
	TSharedPtr<FRuiComponentInvoke> Invoke;     // FUNCTION

	// --- props ---
	TSharedPtr<const FRuiPropsBase> Props;        // committed (null = never rendered)
	TSharedPtr<const FRuiPropsBase> PendingProps;
	FRuiChildren InputChildren;                   // child vnodes to reconcile (shared list)

	// --- host ---
	FRuiHostHandle Node;

	// --- portal ---
	FRuiPortalHandle PortalTarget;

	// --- error boundary (structural, D-10) ---
	bool bEbActive = false;
	FString EbLastError;
	FRuiKey EbResetKey;
	TSharedPtr<FRuiNode> EbFallback;
	TFunction<void(const FString&)> EbOnError;
	FRuiChildren EbChildren;

	// --- reconciliation / double buffer ---
	FRuiFiber* Alternate = nullptr;
	uint8 EffectTag = RuiEffect_None;
	FRuiFiber* NextEffect = nullptr;            // singly-linked post-order effect list
	bool bHasDeletions = false;                 // this fiber recorded deletions this pass
	bool bMatchedPass = false;                  // full-keyed mark-and-sweep (GO-08)

	// --- context ---
	/** Values THIS fiber provides (keyed by context-handle identity). Type-erased holder +
	 *  the typed compare/propagate closures live with the context implementation. */
	TSharedPtr<TMap<const void*, TSharedPtr<void>>> ProvidedContext;
	bool bReadsContext = false;

	// --- bailout / dirty tracking ---
	bool bHasPendingUpdate = false;
	/** Written on schedule AND CONSUMED by the subtree-skip bailout (D-08.1) — the one the
	 *  Godot port wrote but never consumed. */
	bool bSubtreeHasUpdates = false;

	// --- function-component state (SHARED across alternates) ---
	TSharedPtr<FRuiComponentState> State;

	bool IsPortal() const { return Tag == ERuiFiberTag::Portal; }
	bool IsRoot() const { return Tag == ERuiFiberTag::Root; }

	/** Can this fiber be reused for `vnode`? (family matches()) */
	bool Matches(const FRuiNode& VNode) const
	{
		switch (VNode.Kind)
		{
		case ERuiNodeKind::Host: return Tag == ERuiFiberTag::Host && ElementType == VNode.ElementType;
		case ERuiNodeKind::Function: return Tag == ERuiFiberTag::Function && ComponentId == VNode.ComponentId;
		case ERuiNodeKind::Fragment: return Tag == ERuiFiberTag::Fragment;
		case ERuiNodeKind::Portal: return Tag == ERuiFiberTag::Portal;
		case ERuiNodeKind::ErrorBoundary: return Tag == ERuiFiberTag::ErrorBoundary;
		}
		return false;
	}

	static ERuiFiberTag TagForNode(const FRuiNode& VNode)
	{
		switch (VNode.Kind)
		{
		case ERuiNodeKind::Host: return ERuiFiberTag::Host;
		case ERuiNodeKind::Function: return ERuiFiberTag::Function;
		case ERuiNodeKind::Fragment: return ERuiFiberTag::Fragment;
		case ERuiNodeKind::Portal: return ERuiFiberTag::Portal;
		case ERuiNodeKind::ErrorBoundary: return ERuiFiberTag::ErrorBoundary;
		}
		return ERuiFiberTag::Host;
	}

	/** Full reset for slab reuse (every field to fresh-fiber state). */
	void ResetForReuse()
	{
		Parent = Child = Sibling = nullptr;
		Index = 0;
		Tag = ERuiFiberTag::Host;
		Key = FRuiKey();
		ElementType = FRuiElementTypeId();
		ComponentId = NAME_None;
		Invoke.Reset();
		Props.Reset();
		PendingProps.Reset();
		InputChildren.Reset();
		Node.Reset();
		PortalTarget.Reset();
		bEbActive = false;
		EbLastError.Empty();
		EbResetKey = FRuiKey();
		EbFallback.Reset();
		EbOnError = nullptr;
		EbChildren.Reset();
		Alternate = nullptr;
		EffectTag = RuiEffect_None;
		NextEffect = nullptr;
		bHasDeletions = false;
		bMatchedPass = false;
		ProvidedContext.Reset();
		bReadsContext = false;
		bHasPendingUpdate = false;
		bSubtreeHasUpdates = false;
		State.Reset();
	}
};

/**
 * The fiber slab: paged storage with a free list. Fixed addresses for the lifetime of the
 * reconciler (closures/state back-pointers rely on it), O(1) acquire/release, zero
 * steady-state allocation once warmed (stable trees reuse alternates and never touch the
 * slab at all). Per-reconciler — a torn-down root frees exactly its own pages.
 */
class REACTIVEUICORE_API FRuiFiberSlab
{
public:
	static constexpr int32 PageSize = 256;

	~FRuiFiberSlab()
	{
		for (FPage* Page : Pages)
		{
			delete Page;
		}
	}

	FRuiFiber* Acquire()
	{
		if (FreeList != nullptr)
		{
			FRuiFiber* Out = FreeList;
			FreeList = Out->Sibling; // free list threads through Sibling
			Out->Sibling = nullptr;
			++LiveCount;
			return Out;
		}
		if (Pages.IsEmpty() || Pages.Last()->Used == PageSize)
		{
			Pages.Add(new FPage());
		}
		FPage* Page = Pages.Last();
		FRuiFiber* Out = &Page->Fibers[Page->Used++];
		++LiveCount;
		return Out;
	}

	/** Return ONE fiber (caller handles the alternate pair — see reconciler Release()). */
	void Release(FRuiFiber* Fiber)
	{
		Fiber->ResetForReuse();
		Fiber->Sibling = FreeList;
		FreeList = Fiber;
		--LiveCount;
	}

	int32 NumLive() const { return LiveCount; }
	int32 NumPages() const { return Pages.Num(); }

private:
	struct FPage
	{
		FRuiFiber Fibers[PageSize];
		int32 Used = 0;
	};

	TArray<FPage*> Pages;
	FRuiFiber* FreeList = nullptr;
	int32 LiveCount = 0;
};
