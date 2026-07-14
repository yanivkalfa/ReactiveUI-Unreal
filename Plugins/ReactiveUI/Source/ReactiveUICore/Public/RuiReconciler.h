// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuiReconciler — the fiber reconciler, ported from reconciler.gd with the D-08 React
// adoptions the Godot port hasn't made yet:
//
//   render phase  -> begin work descending (reconcile children, run components with
//                    bailout; SUBTREE-SKIP: a clean fiber with no dirty descendants
//                    returns null and skips its whole subtree — bSubtreeHasUpdates is
//                    CONSUMED here, the family's known missing piece), then complete
//                    ascending (host-node effects; provider stacks pop here, including
//                    on every skip path). Post-order singly-linked effect list (17-style —
//                    right for a synchronous renderer).
//   commit phase  -> deletions -> placement/update/portal-retarget/layout effects ->
//                    reorder (structural frames only) -> current<->WIP swap -> two-pass
//                    passive flush -> deferred-update replay.
//
// Child reconciliation: fast-leaf-list (+ GO-09 reuse_by_slot) -> keys-stable positional ->
// full keyed mark-and-sweep with a persistent key map (GO-08). Updates coalesce to one
// host RequestFrame; setState mid-render restarts from the root (25-restart guard);
// setState mid-commit defers and replays. Double-buffered fibers from the slab (D-06).

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiFiber.h"
#include "RuiHostConfig.h"
#include "RuiCoreMisc.h"
#include "RuiContextHandle.h"

class REACTIVEUICORE_API FRuiReconciler
{
public:
	/** Host outlives the reconciler (the root object owns both — mount-surface contract). */
	FRuiReconciler(IRuiHostConfig& InHost, FRuiHostHandle InRootContainer);
	~FRuiReconciler();

	FRuiReconciler(const FRuiReconciler&) = delete;
	FRuiReconciler& operator=(const FRuiReconciler&) = delete;

	/** Mount / replace the root vnode. Initial render is always synchronous (no empty
	 *  first frame). */
	void Render(FRuiNode RootNode);

	/** Mark a fiber dirty and coalesce a re-render (hook setters land here). */
	void ScheduleUpdateOnFiber(FRuiFiber* Fiber);
	void RequestUpdate();

	/** Run any pending work NOW, synchronously and unsliced (tests, HMR, mount surfaces). */
	void FlushSync();

	/** HMR: mark EVERY function fiber dirty (defeats bailout caches — component definitions
	 *  may have been swapped under their ComponentIds) and coalesce a re-render. */
	void HmrRefreshAll();

	/** Every live reconciler (mounted roots register in ctor/dtor) — how HMR reaches the
	 *  running UIs without plumbing. Game-thread only. */
	static void ForEachLive(TFunctionRef<void(FRuiReconciler&)> Fn);

	/** Tear down: cleanups, refs nulled, host nodes released, fibers freed. */
	void Unmount();

	bool IsMounted() const { return RootCurrent != nullptr; }

	// --- introspection (tests / rui.Stats / the Inspector) ---
	FRuiFiber* GetRootFiber() const { return RootCurrent; }
	int32 NumLiveFibers() const { return Slab.NumLive(); }
	IRuiHostConfig& GetHost() { return Host; }

	// --- internal API for the hooks layer (FRuiContext) — \internal ---
	void NotifyEffectKinds(FRuiFiber& Fiber, bool bHasPassive, bool bHasLayout);
	static const IRuiProvidedValue* ResolveProvidedOnCommitted(const FRuiFiber* From, const void* Key);
	void OnProvidedValueChanged(FRuiFiber& ProviderFiber, const void* Key);

private:
	// --- scheduling ---
	void EnsureTick();
	void Tick();

	// --- render phase ---
	void BeginRender();
	FRuiFiber* PerformUnit(FRuiFiber* Fiber);
	FRuiFiber* BeginFunction(FRuiFiber* Fiber);
	FRuiFiber* BeginErrorBoundary(FRuiFiber* Fiber);
	void RenderComponent(FRuiFiber* Fiber); // output lands in State->LastOutput (shared once)
	void CompleteWork(FRuiFiber* Fiber);
	void PushProvidedContext(FRuiFiber* Fiber);
	void PopProvidedContext(FRuiFiber* Fiber);

	// --- child reconciliation ---
	FRuiFiber* ReconcileFiber(FRuiFiber* ParentFiber, FRuiFiber* OldFiber, const FRuiNode& VNode, int32 Index);
	/** Returns true when the fast-leaf-list path fully handled the children in place. */
	bool ReconcileChildren(FRuiFiber* ParentFiber, FRuiFiber* OldFirst, const FRuiChildren& ChildVNodes);
	bool TryFastLeafList(FRuiFiber* ParentFiber, FRuiFiber* OldFirst, const TArray<FRuiNode>& VNodes, bool bIgnoreKeys);
	bool KeysStable(FRuiFiber* OldFirst, const TArray<FRuiNode>& VNodes) const;
	void DeleteFiber(FRuiFiber* ParentFiber, FRuiFiber* OldFiber);
	void MarkReorder(FRuiFiber* ParentFiber);
	bool HasContextChanged(const FRuiFiber* Fiber) const;
	void AppendEffect(FRuiFiber* Fiber);

	// --- commit phase ---
	void CommitRoot();
	void CommitPlacement(FRuiFiber* Fiber);
	void CommitUpdate(FRuiFiber* Fiber);
	void CommitDeletion(FRuiFiber* Fiber);
	void CommitPortalRetarget(FRuiFiber* Fiber);
	void CommitLayoutEffects(FRuiFiber* Fiber);
	void FlushPassive();
	void RunCleanups(FRuiFiber* Fiber);
	void RunCleanupsRecursive(FRuiFiber* Fiber);
	void DisposeFiberState(FRuiFiber* Fiber);
	void NullRefsRecursive(FRuiFiber* Fiber);
	void EnforceChildOrder(FRuiFiber* ParentFiber);
	void CollectHostChildren(FRuiFiber* Fiber, TArray<FRuiHostHandle>& Out) const;
	void ReleaseHostNodes(FRuiFiber* Fiber);
	void DetachPortalChildren(FRuiFiber* Fiber);
	FRuiHostHandle HostParentNode(FRuiFiber* Fiber, bool& bOutViaPortal, FRuiPortalHandle& OutPortal) const;

	// --- error latch (D-10) ---
	void HandleRenderFailure(FRuiFiber* FailedFiber, const FString& Reason);
	void AdoptPendingEbActivation(FRuiFiber* BoundaryFiber);

	// --- tree lifecycle ---
	void ReleaseFiberTree(FRuiFiber* Fiber);
	/** Reclaim the fresh WIP fibers of an abandoned pass (restart / aborted mount). */
	void ReleaseAbandonedChildren(FRuiFiber* Parent);

	// --- helpers ---
	static FRuiFiber* OldFirst(FRuiFiber* Fiber) { return Fiber ? Fiber->Child : nullptr; }
	static bool AnyKeyed(const TArray<FRuiNode>& VNodes);
	static FRuiKey FiberKey(const FRuiFiber* F);
	static FRuiKey VNodeKey(const FRuiNode& VNode, int32 Index);
	static bool ChildrenSame(const FRuiChildren& A, const FRuiChildren& B);
	bool PropsEqual(const FRuiFiber* Fiber) const;
	const TArray<FRuiNode>& NormalizedChildren(const FRuiChildren& Children) const;

private:
	IRuiHostConfig& Host;
	FRuiHostHandle RootContainer;
	FRuiFiberSlab Slab;

	TOptional<FRuiNode> RootVNode;
	FRuiFiber* RootCurrent = nullptr;

	FRuiFiber* WipRoot = nullptr;
	FRuiFiber* NextUnit = nullptr;

	FRuiFiber* FirstEffect = nullptr;
	FRuiFiber* LastEffect = nullptr;
	TArray<FRuiFiber*> Deletions;
	TSet<FRuiFiber*> ReorderSet;
	TArray<FRuiFiber*> PendingPassive;

	/** Persistent full-keyed map (GO-08): cleared per call, never re-allocated per frame. */
	TMap<FRuiKey, FRuiFiber*> KeyMap;

	/** Providers pushed this pass (parallel stack so ascend/skip paths pop exactly what
	 *  descend pushed, even across restarts). */
	TArray<FRuiFiber*> ProviderStack;

	/** A mount-pass render failure records its boundary activation by key-path — the WIP
	 *  fiber that carried it is abandoned with the pass and has no committed twin, so the
	 *  activation must survive OUTSIDE the fiber tree until the restart pass re-adopts it
	 *  (deterministic positional keys make the path stable across identical passes). */
	struct FPendingEbActivation
	{
		TArray<FRuiKey> Path; // boundary-up-to-root key path
		FString Reason;
	};
	TArray<FPendingEbActivation> PendingEbActivations;

	bool bIsCommitting = false;
	TArray<FRuiFiber*> DeferredUpdates;
	bool bWorkActive = false;
	bool bRestart = false;
	bool bTickPending = false;
	int32 RestartCount = 0;

	static constexpr int32 MaxRestarts = 25;

	/** Scratch for NormalizedChildren's empty case. */
	static const TArray<FRuiNode> EmptyChildren;
};
