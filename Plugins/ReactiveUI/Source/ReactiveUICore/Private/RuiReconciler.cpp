// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Port of reconciler.gd (read in full as the spec) with the D-08 adoptions. Faithful-first:
// section order and comments track the original so future syncs diff cleanly. Documented
// divergences: (1) SUBTREE-SKIP bailout consumes bSubtreeHasUpdates (React
// bailoutOnAlreadyFinishedWork — the WIP adopts the CURRENT child chain wholesale, exactly
// React's no-clone fast path); (2) no raw-string child normalization (C++ arrays are
// homogeneous — RUI::Text is explicit; the family's flatten becomes a no-op); (3) refs
// follow the React lifecycle (attach on placement, detach on deletion — D-08.4), not
// call-every-commit; (4) minimal-move reordering is HOST-side (the Phase 2 spike decides
// the Slate strategy) — the core marks structural frames and calls ReorderChildren, same
// contract as the family's enforce-order.

#include "RuiReconciler.h"
#include "RuiContext.h"
#include "RuiCoreElements.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiReconciler, Log, All);

const TArray<FRuiNode> FRuiReconciler::EmptyChildren;

FRuiReconciler::FRuiReconciler(IRuiHostConfig& InHost, FRuiHostHandle InRootContainer)
	: Host(InHost), RootContainer(MoveTemp(InRootContainer))
{
	FRuiFiber* Root = Slab.Acquire();
	Root->Tag = ERuiFiberTag::Root;
	Root->Node = RootContainer;
	RootCurrent = Root;
}

FRuiReconciler::~FRuiReconciler()
{
	if (RootCurrent != nullptr)
	{
		Unmount();
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Scheduling
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuiReconciler::Render(FRuiNode RootNode)
{
	checkf(IsInGameThread(), TEXT("ReactiveUI: Render must run on the game thread (D-15)"));
	if (RootCurrent == nullptr)
	{
		return; // torn down — a render after unmount is a no-op, not a crash [audit]
	}
	// Initial / top-level mount is always synchronous (no empty first frame).
	bTickPending = false; // pre-empt any parked sliced render (its Tick self-guards)
	RootVNode = MoveTemp(RootNode);
	RootCurrent->bHasPendingUpdate = true;
	bWorkActive = true;
	bRestart = false;
	BeginRender();
	int32 MountRestarts = 0;
	for (;;)
	{
		while (NextUnit != nullptr && !bRestart)
		{
			NextUnit = PerformUnit(NextUnit);
		}
		if (!bRestart)
		{
			break;
		}
		// The pass was poisoned mid-mount (a render failure activated a boundary, or a
		// setState fired during render). Godot commits the poisoned pass and re-ticks; we
		// restart from the root NOW instead — mount promises a coherent first frame, and a
		// boundary fallback must never lose to a half-built tree (React's render-phase-
		// update semantics; documented divergence).
		if (++MountRestarts > MaxRestarts)
		{
			UE_LOG(LogRuiReconciler, Error,
				   TEXT("[ReactiveUI] Too many re-renders during mount (setState during render?). Aborting mount."));
			bRestart = false;
			bWorkActive = false;
			NextUnit = nullptr;
			return; // abandoned WIP is reclaimed by the next BeginRender / Unmount
		}
		bRestart = false;
		BeginRender();
	}
	bWorkActive = false;
	CommitRoot();
}

void FRuiReconciler::ScheduleUpdateOnFiber(FRuiFiber* Fiber)
{
	checkf(
		IsInGameThread(),
		TEXT(
			"ReactiveUI: state updates must run on the game thread (D-15; use RUI::PostToGameThread from async code)"));
	if (RootCurrent == nullptr)
	{
		return; // torn down — ignore late setState/effect callbacks [audit]
	}
	FRuiFiber* Target = Fiber ? Fiber : RootCurrent;
	Target->bHasPendingUpdate = true;
	for (FRuiFiber* P = Target->Parent; P != nullptr; P = P->Parent)
	{
		P->bSubtreeHasUpdates = true;
	}
	if (bIsCommitting)
	{
		DeferredUpdates.Add(Target);
		return;
	}
	if (bWorkActive)
	{
		bRestart = true; // update mid-render -> rebuild from root next tick
	}
	EnsureTick();
}

void FRuiReconciler::RequestUpdate()
{
	ScheduleUpdateOnFiber(RootCurrent);
}

void FRuiReconciler::EnsureTick()
{
	if (bTickPending)
	{
		return;
	}
	bTickPending = true;
	Host.RequestFrame([this]() { Tick(); });
	// NOTE on lifetime: the mount surface owns reconciler + host and tears both down
	// together; Unmount() flips RootCurrent null and Tick() self-guards. The mock host
	// drops queued frames on destruction; the Slate host unregisters its OnPreTick.
}

void FRuiReconciler::FlushSync()
{
	if (bTickPending || bWorkActive)
	{
		bTickPending = false;
		const bool bWasSlicing = FRuiConfig::IsTimeSlicing();
		// Run one full unsliced pass now (tests/HMR/mount surfaces).
		Tick();
		(void)bWasSlicing;
	}
}

void FRuiReconciler::Tick()
{
	bTickPending = false;
	if (!RootVNode.IsSet() || RootCurrent == nullptr)
	{
		bWorkActive = false;
		return;
	}
	if (!bWorkActive || bRestart)
	{
		if (bRestart)
		{
			++RestartCount;
			if (RestartCount > MaxRestarts)
			{
				UE_LOG(LogRuiReconciler, Error,
					   TEXT("[ReactiveUI] Too many re-renders (setState during render?). Aborting pass."));
				bWorkActive = false;
				bRestart = false;
				RestartCount = 0;
				return;
			}
		}
		else
		{
			RestartCount = 0;
		}
		BeginRender();
		bRestart = false;
		bWorkActive = true;
	}

	const double Start = FPlatformTime::Seconds();
	const bool bSliced = FRuiConfig::IsTimeSlicing();
	const double BudgetSec = FRuiConfig::FrameBudgetMs() / 1000.0;
	while (NextUnit != nullptr)
	{
		NextUnit = PerformUnit(NextUnit);
		if (bRestart)
		{
			break;
		}
		if (bSliced && (FPlatformTime::Seconds() - Start) >= BudgetSec)
		{
			break;
		}
	}

	if (bRestart)
	{
		EnsureTick();
		return;
	}
	if (NextUnit == nullptr)
	{
		bWorkActive = false;
		RestartCount = 0;
		CommitRoot();
	}
	else
	{
		EnsureTick(); // park: resume on the next frame (time-slicing only)
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Render phase
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuiReconciler::BeginRender()
{
	// Reclaim an abandoned pass first (restart / aborted mount): its fresh WIP fibers would
	// otherwise leak in the slab, and shared component states could keep pointing at them.
	// (WipRoot is nulled on commit, so non-null here always means "abandoned".)
	if (WipRoot != nullptr)
	{
		ReleaseAbandonedChildren(WipRoot);
	}

	FirstEffect = nullptr;
	LastEffect = nullptr;
	Deletions.Reset();
	ReorderSet.Reset();
	PendingPassive.Reset();
	// An aborted (restarted) pass may have left provider stacks pushed — pop them all.
	while (!ProviderStack.IsEmpty())
	{
		PopProvidedContext(ProviderStack.Last());
	}

	// Reuse the root's ping-pong buddy (double buffering) instead of allocating. [perf #1]
	FRuiFiber* Wip = RootCurrent->Alternate;
	if (Wip == nullptr)
	{
		Wip = Slab.Acquire();
		RootCurrent->Alternate = Wip;
	}
	Wip->Alternate = RootCurrent;
	Wip->Tag = ERuiFiberTag::Root;
	Wip->Node = RootContainer;
	Wip->Child = nullptr;
	Wip->Sibling = nullptr;
	Wip->Parent = nullptr;
	Wip->EffectTag = RuiEffect_None;
	Wip->NextEffect = nullptr;
	Wip->bHasDeletions = false;
	Wip->InputChildren = MakeShared<const TArray<FRuiNode>>(TArray<FRuiNode>{RootVNode.GetValue()});
	Wip->bHasPendingUpdate = RootCurrent->bHasPendingUpdate;
	Wip->bSubtreeHasUpdates = RootCurrent->bSubtreeHasUpdates;
	WipRoot = Wip;
	NextUnit = Wip;
}

FRuiFiber* FRuiReconciler::PerformUnit(FRuiFiber* Fiber)
{
	// begin-work (inlined)
	FRuiFiber* Next = nullptr;
	switch (Fiber->Tag)
	{
	case ERuiFiberTag::Function:
		Next = BeginFunction(Fiber);
		break;
	case ERuiFiberTag::ErrorBoundary:
		Next = BeginErrorBoundary(Fiber);
		break;
	default:
	{
		// ROOT / HOST / FRAGMENT / PORTAL: reconcile declared children.
		// Leaf fast-path: nothing declared now AND nothing before -> skip entirely. [perf]
		FRuiFiber* Alt = Fiber->Alternate;
		const bool bNoDeclared = !Fiber->InputChildren.IsValid() || Fiber->InputChildren->IsEmpty();
		if (bNoDeclared && (Alt == nullptr || Alt->Child == nullptr))
		{
			Next = nullptr;
		}
		else if (ReconcileChildren(Fiber, OldFirst(Alt), Fiber->InputChildren))
		{
			Next = nullptr; // fast-list handled the children in place; don't descend
		}
		else
		{
			Next = Fiber->Child;
		}
		break;
	}
	}

	if (Next != nullptr)
	{
		// Descending into children: providers push their values for the subtree (popped in
		// the ascend loop below — INCLUDING skip paths, the D-08.3 correctness trap).
		PushProvidedContext(Fiber);
		return Next;
	}

	// no child -> complete this fiber, then sibling / climb.
	FRuiFiber* F = Fiber;
	while (F != nullptr)
	{
		CompleteWork(F);
		if (F->Sibling != nullptr)
		{
			return F->Sibling;
		}
		F = F->Parent;
	}
	return nullptr;
}

FRuiFiber* FRuiReconciler::BeginFunction(FRuiFiber* Fiber)
{
	if (Fiber->State.IsValid())
	{
		Fiber->State->Fiber = Fiber; // re-point the shared state at the live fiber
	}
	FRuiFiber* Alt = Fiber->Alternate;

	const bool bPropsEqual = PropsEqual(Fiber);
	const bool bContextOk = !Fiber->bReadsContext || !HasContextChanged(Fiber);
	const bool bChildrenSame = ChildrenSame(Alt ? Alt->InputChildren : FRuiChildren(), Fiber->InputChildren);
	const bool bCanBail = !Fiber->bHasPendingUpdate && bContextOk && bPropsEqual && bChildrenSame;

	// ── SUBTREE-SKIP (D-08.1, React bailoutOnAlreadyFinishedWork) ──────────────────────
	// Clean fiber + clean subtree -> adopt the CURRENT child chain wholesale and skip the
	// entire subtree (no clones — React's fast path; the shared children stay validly
	// paired with their alternates for future passes).
	if (bCanBail && !Fiber->bSubtreeHasUpdates && Alt != nullptr)
	{
		Fiber->Props = Fiber->PendingProps;
		Fiber->Child = Alt->Child;
		return nullptr;
	}

	FRuiChildren OutChildren;
	if (bCanBail && Fiber->State.IsValid())
	{
		OutChildren = Fiber->State->LastOutput; // SAME shared list — grandchildren can bail
	}
	else
	{
		Fiber->bHasPendingUpdate = false;
		RenderComponent(Fiber);
		OutChildren = Fiber->State.IsValid() ? Fiber->State->LastOutput : FRuiChildren();
	}
	Fiber->bSubtreeHasUpdates = false;
	Fiber->Props = Fiber->PendingProps;

	if (ReconcileChildren(Fiber, OldFirst(Alt), OutChildren))
	{
		return nullptr;
	}
	return Fiber->Child;
}

void FRuiReconciler::RenderComponent(FRuiFiber* Fiber)
{
	TSharedPtr<FRuiComponentState>& State = Fiber->State;
	check(State.IsValid());

	auto RunOnce = [&]() -> FRuiNodeArray
	{
		// _begin
		State->HookIndex = 0;
		State->EffectIndex = 0;
		State->LayoutIndex = 0;
		State->ContextDeps.Reset();
		State->bIsRendering = true;
		if (FRuiConfig::IsHookValidationEnabled())
		{
			State->HookLog.Reset();
		}
		RUI::SetRendering(true);

		FRuiContext Ctx(State.ToSharedRef(), *Fiber, *this, Host);
		FRuiNodeArray Result = (*Fiber->Invoke)(Ctx, Fiber->PendingProps.Get(),
												Fiber->InputChildren.IsValid() ? *Fiber->InputChildren : EmptyChildren);

		// _end
		RUI::SetRendering(false);
		State->bIsRendering = false;
		if (FRuiConfig::IsHookValidationEnabled())
		{
			if (!State->bHookOrderPrimed)
			{
				State->HookSignatures = State->HookLog;
				State->bHookOrderPrimed = true;
			}
			else
			{
				const TArray<ERuiHookKind>& Prev = State->HookSignatures;
				const TArray<ERuiHookKind>& Now = State->HookLog;
				const int32 N = FMath::Min(Prev.Num(), Now.Num());
				for (int32 i = 0; i < N; ++i)
				{
					if (Prev[i] != Now[i])
					{
						const FString Msg = FString::Printf(
							TEXT("[Hooks][order] %s: hook #%d changed '%s' -> '%s' across renders — hooks must run in "
								 "the same order every render."),
							*Fiber->ComponentId.ToString(), i, RuiHookKindName(Prev[i]), RuiHookKindName(Now[i]));
						FRuiDiagnostics::Emit(Msg);
						UE_LOG(LogRuiReconciler, Error, TEXT("%s"), *Msg);
						break;
					}
				}
				if (Prev.Num() != Now.Num())
				{
					const FString Msg = FString::Printf(TEXT("[Hooks][order] %s: hook count changed %d -> %d across "
															 "renders — a hook is being called conditionally."),
														*Fiber->ComponentId.ToString(), Prev.Num(), Now.Num());
					FRuiDiagnostics::Emit(Msg);
					UE_LOG(LogRuiReconciler, Error, TEXT("%s"), *Msg);
				}
			}
		}
		return Result;
	};

	FRuiNodeArray Result = RunOnce();
	if (FRuiConfig::IsStrictModeEnabled())
	{
		Result = RunOnce(); // double-invoke, first result discarded (impure-render flusher)
	}

	// Cooperative error latch (D-10): a failed render unwinds to the nearest boundary.
	if (TOptional<FString> Failure = RUI::ConsumeRenderFailure())
	{
		HandleRenderFailure(Fiber, Failure.GetValue());
		Result.Reset();
	}

	FRuiDiagnostics::OnRender();
	State->LastOutput = RUI::MakeChildren(MoveTemp(Result)); // shared ONCE; bailouts reuse it
	if (!State->Effects.IsEmpty())
	{
		Fiber->EffectTag |= RuiEffect_Passive;
	}
	if (!State->LayoutEffects.IsEmpty())
	{
		Fiber->EffectTag |= RuiEffect_Layout;
	}
}

FRuiFiber* FRuiReconciler::BeginErrorBoundary(FRuiFiber* Fiber)
{
	// A mount-pass failure recorded its activation by key-path (the WIP fiber that carried
	// it was abandoned with that pass) — re-adopt it before deciding what to render.
	if (!PendingEbActivations.IsEmpty())
	{
		AdoptPendingEbActivation(Fiber);
	}
	// Structural boundary (family): renders the fallback while active; ResetKey change
	// clears. The latch (HandleRenderFailure) is what activates it without exceptions.
	// A missing alternate is a MOUNT, not a reset — there is nothing to clear, and a
	// just-adopted mount-pass activation must survive.
	FRuiFiber* Alt = Fiber->Alternate;
	const bool bResetRequested = (Alt != nullptr) && !(Alt->EbResetKey == Fiber->EbResetKey);
	if (bResetRequested)
	{
		Fiber->bEbActive = false;
		Fiber->EbLastError.Empty();
	}
	FRuiChildren Children;
	if (Fiber->bEbActive && !bResetRequested)
	{
		if (Fiber->EbFallback.IsValid())
		{
			Children = MakeShared<const TArray<FRuiNode>>(TArray<FRuiNode>{*Fiber->EbFallback});
		}
	}
	else
	{
		Children = Fiber->EbChildren;
	}
	if (ReconcileChildren(Fiber, OldFirst(Alt), Children))
	{
		return nullptr;
	}
	return Fiber->Child;
}

void FRuiReconciler::HandleRenderFailure(FRuiFiber* FailedFiber, const FString& Reason)
{
	// Find the nearest boundary above the failed component (WIP chain — pre-commit, safe).
	// An ALREADY-ACTIVE boundary can't capture again this pass (its fallback is what just
	// failed) — skip upward, or the failure loops forever (React's captured-boundary rule).
	for (FRuiFiber* F = FailedFiber; F != nullptr; F = F->Parent)
	{
		if (F->Tag == ERuiFiberTag::ErrorBoundary && !F->bEbActive)
		{
			F->bEbActive = true;
			F->EbLastError = Reason;
			if (F->Alternate != nullptr)
			{
				// The committed twin carries the activation into the restart pass.
				F->Alternate->bEbActive = true;
				F->Alternate->EbLastError = Reason;
			}
			else
			{
				// Mount-pass boundary: the WIP fiber is abandoned with this pass and has no
				// committed twin, so record the activation by key-path for the restart pass
				// to re-adopt (BeginErrorBoundary). If an ancestor renders differently and
				// the path misses, the child just fails again and re-records — self-healing,
				// bounded by the restart guard.
				FPendingEbActivation& Pending = PendingEbActivations.AddDefaulted_GetRef();
				Pending.Reason = Reason;
				for (const FRuiFiber* P = F; P != nullptr && P->Tag != ERuiFiberTag::Root; P = P->Parent)
				{
					Pending.Path.Add(FiberKey(P));
				}
			}
			if (F->EbOnError)
			{
				F->EbOnError(Reason);
			}
			// Restart from the root: the boundary renders its fallback next pass; the
			// poisoned WIP is abandoned (never committed) and reclaimed by BeginRender.
			bRestart = true;
			UE_LOG(LogRuiReconciler, Error, TEXT("[ReactiveUI] render failed: %s (caught by error boundary)"), *Reason);
			return;
		}
	}
	UE_LOG(LogRuiReconciler, Error, TEXT("[ReactiveUI] render failed with no error boundary above: %s"), *Reason);
}

void FRuiReconciler::AdoptPendingEbActivation(FRuiFiber* BoundaryFiber)
{
	TArray<FRuiKey> Path;
	for (const FRuiFiber* P = BoundaryFiber; P != nullptr && P->Tag != ERuiFiberTag::Root; P = P->Parent)
	{
		Path.Add(FiberKey(P));
	}
	for (int32 i = 0; i < PendingEbActivations.Num(); ++i)
	{
		if (PendingEbActivations[i].Path == Path)
		{
			BoundaryFiber->bEbActive = true;
			BoundaryFiber->EbLastError = PendingEbActivations[i].Reason;
			PendingEbActivations.RemoveAt(i);
			return;
		}
	}
}

void FRuiReconciler::PushProvidedContext(FRuiFiber* Fiber)
{
	if (!Fiber->ProvidedContext.IsValid() || Fiber->ProvidedContext->IsEmpty())
	{
		return;
	}
	for (const TPair<const void*, TSharedPtr<void>>& Pair : *Fiber->ProvidedContext)
	{
		static_cast<IRuiProvidedValue*>(Pair.Value.Get())->PushOnRenderStack();
	}
	ProviderStack.Push(Fiber);
}

void FRuiReconciler::PopProvidedContext(FRuiFiber* Fiber)
{
	if (!ProviderStack.IsEmpty() && ProviderStack.Last() == Fiber)
	{
		for (const TPair<const void*, TSharedPtr<void>>& Pair : *Fiber->ProvidedContext)
		{
			static_cast<IRuiProvidedValue*>(Pair.Value.Get())->PopFromRenderStack();
		}
		ProviderStack.Pop(EAllowShrinking::No);
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Child reconciliation
// ─────────────────────────────────────────────────────────────────────────────────────────

FRuiFiber* FRuiReconciler::ReconcileFiber(FRuiFiber* ParentFiber, FRuiFiber* OldFiber, const FRuiNode& VNode,
										  int32 Index)
{
	const bool bReuse = (OldFiber != nullptr) && OldFiber->Matches(VNode);
	FRuiFiber* Fiber;
	if (bReuse)
	{
		Fiber = OldFiber->Alternate;
		if (Fiber == nullptr)
		{
			Fiber = Slab.Acquire();
		}
		Fiber->Alternate = OldFiber;
		OldFiber->Alternate = Fiber;
	}
	else
	{
		Fiber = Slab.Acquire();
		Fiber->Alternate = nullptr;
		if (OldFiber != nullptr)
		{
			DeleteFiber(ParentFiber, OldFiber);
		}
	}

	// --- render-scoped fields (reset every render — the buddy holds stale data) ---
	Fiber->Parent = ParentFiber;
	Fiber->Child = nullptr;
	Fiber->Sibling = nullptr;
	Fiber->Index = Index;
	Fiber->EffectTag = RuiEffect_None;
	Fiber->NextEffect = nullptr;
	Fiber->bHasDeletions = false;
	Fiber->Key = VNode.Key;
	Fiber->PendingProps = VNode.Props;
	Fiber->InputChildren = VNode.Children;
	Fiber->Tag = FRuiFiber::TagForNode(VNode);
	if (VNode.Kind == ERuiNodeKind::Host)
	{
		Fiber->ElementType = VNode.ElementType;
		Fiber->ComponentId = NAME_None;
		Fiber->Invoke.Reset();
		Fiber->PortalTarget.Reset();
	}
	else
	{
		Fiber->ElementType = FRuiElementTypeId();
		Fiber->ComponentId = (VNode.Kind == ERuiNodeKind::Function) ? VNode.ComponentId : NAME_None;
		Fiber->Invoke = (VNode.Kind == ERuiNodeKind::Function) ? VNode.Invoke : nullptr;
		Fiber->PortalTarget = (VNode.Kind == ERuiNodeKind::Portal) ? VNode.PortalTarget : nullptr;
	}
	if (VNode.Kind == ERuiNodeKind::ErrorBoundary)
	{
		Fiber->EbFallback = VNode.EbFallback;
		Fiber->EbOnError = VNode.EbOnError;
		Fiber->EbResetKey = VNode.EbResetKey;
		Fiber->EbChildren = VNode.Children;
	}

	if (bReuse)
	{
		// carry committed baseline + live node/state/context from the current fiber
		Fiber->Node = OldFiber->Node;
		Fiber->State = OldFiber->State;
		Fiber->Props = OldFiber->Props;
		Fiber->bReadsContext = OldFiber->bReadsContext;
		Fiber->bHasPendingUpdate = OldFiber->bHasPendingUpdate;
		Fiber->bSubtreeHasUpdates = OldFiber->bSubtreeHasUpdates;
		// Carry provided context (DUPLICATED so provider change-detection vs the alternate
		// works, and a bailed-out provider keeps providing). [audit C1]
		if (OldFiber->ProvidedContext.IsValid())
		{
			TSharedRef<TMap<const void*, TSharedPtr<void>>> Dup = MakeShared<TMap<const void*, TSharedPtr<void>>>();
			for (const TPair<const void*, TSharedPtr<void>>& Pair : *OldFiber->ProvidedContext)
			{
				Dup->Add(Pair.Key, static_cast<IRuiProvidedValue*>(Pair.Value.Get())->Duplicate());
			}
			Fiber->ProvidedContext = Dup;
		}
		else
		{
			Fiber->ProvidedContext.Reset();
		}
		if (Fiber->Tag == ERuiFiberTag::ErrorBoundary)
		{
			Fiber->bEbActive = OldFiber->bEbActive;
			Fiber->EbLastError = OldFiber->EbLastError;
		}
	}
	else
	{
		Fiber->Node.Reset();
		Fiber->State.Reset();
		Fiber->Props.Reset();
		Fiber->bReadsContext = false;
		Fiber->bHasPendingUpdate = false;
		Fiber->bSubtreeHasUpdates = false;
		Fiber->ProvidedContext.Reset();
		Fiber->bEbActive = false;
	}

	if (Fiber->Tag == ERuiFiberTag::Function && !Fiber->State.IsValid())
	{
		TSharedRef<FRuiComponentState> NewState = MakeShared<FRuiComponentState>();
		NewState->Fiber = Fiber;
		TWeakPtr<FRuiComponentState> Weak = NewState;
		NewState->OnStateUpdated = [this, Weak]()
		{
			TSharedPtr<FRuiComponentState> S = Weak.Pin();
			if (S.IsValid())
			{
				ScheduleUpdateOnFiber(S->Fiber);
			}
		};
		Fiber->State = NewState;
	}
	return Fiber;
}

bool FRuiReconciler::ReconcileChildren(FRuiFiber* ParentFiber, FRuiFiber* OldFirstFiber,
									   const FRuiChildren& ChildVNodes)
{
	const TArray<FRuiNode>& VNodes = NormalizedChildren(ChildVNodes);

	// FAST-LIST PATH (+ GO-09 reuse_by_slot; see TryFastLeafList).
	const bool bReuseBySlot = ParentFiber->PendingProps.IsValid() && ParentFiber->PendingProps->bReuseBySlot;
	if (OldFirstFiber != nullptr && !VNodes.IsEmpty() &&
		TryFastLeafList(ParentFiber, OldFirstFiber, VNodes, bReuseBySlot))
	{
		return true;
	}

	ParentFiber->Child = nullptr;
	if (VNodes.IsEmpty())
	{
		FRuiFiber* Oc = OldFirstFiber;
		while (Oc != nullptr)
		{
			FRuiFiber* Nxt = Oc->Sibling;
			DeleteFiber(ParentFiber, Oc);
			Oc = Nxt;
		}
		return false;
	}

	FRuiFiber* Prev = nullptr;
	bool bStructural = false;
	if (AnyKeyed(VNodes))
	{
		// FAST PATH: positionally-stable keyed list — no key map. [perf P2]
		if (KeysStable(OldFirstFiber, VNodes))
		{
			FRuiFiber* Ocs = OldFirstFiber;
			for (int32 i = 0; i < VNodes.Num(); ++i)
			{
				FRuiFiber* Cf = ReconcileFiber(ParentFiber, Ocs, VNodes[i], i);
				if (Prev == nullptr)
				{
					ParentFiber->Child = Cf;
				}
				else
				{
					Prev->Sibling = Cf;
				}
				Prev = Cf;
				Ocs = Ocs->Sibling;
			}
			return false; // stable -> no structural change -> no reorder
		}
		// Full keyed mark-and-sweep with the persistent key map (GO-08). Unkeyed children
		// get NAMESPACED positional keys (FRuiKey int with a reserved marker cannot collide
		// with user keys because user int keys and positional keys live in the same space —
		// so positional sentinels use FName "\x01idx%d"-style names, family [audit M1]).
		KeyMap.Reset();
		FRuiFiber* Ock = OldFirstFiber;
		while (Ock != nullptr)
		{
			KeyMap.Add(FiberKey(Ock), Ock);
			Ock->bMatchedPass = false;
			Ock = Ock->Sibling;
		}
		for (int32 i = 0; i < VNodes.Num(); ++i)
		{
			const FRuiNode& Vn = VNodes[i];
			FRuiFiber* OldMatch = KeyMap.FindRef(VNodeKey(Vn, i));
			if (OldMatch != nullptr && (OldMatch->bMatchedPass || !OldMatch->Matches(Vn)))
			{
				OldMatch = nullptr;
			}
			if (OldMatch != nullptr)
			{
				OldMatch->bMatchedPass = true;
				if (OldMatch->Index != i)
				{
					bStructural = true; // moved
				}
			}
			else
			{
				bStructural = true; // new placement
			}
			FRuiFiber* Cf = ReconcileFiber(ParentFiber, OldMatch, Vn, i);
			if (Prev == nullptr)
			{
				ParentFiber->Child = Cf;
			}
			else
			{
				Prev->Sibling = Cf;
			}
			Prev = Cf;
		}
		FRuiFiber* Ocd = OldFirstFiber;
		while (Ocd != nullptr)
		{
			FRuiFiber* Nxtd = Ocd->Sibling;
			if (!Ocd->bMatchedPass)
			{
				DeleteFiber(ParentFiber, Ocd);
			}
			Ocd = Nxtd;
		}
		KeyMap.Reset();
	}
	else
	{
		// index (positional) path
		FRuiFiber* Oci = OldFirstFiber;
		for (int32 i = 0; i < VNodes.Num(); ++i)
		{
			FRuiFiber* OldMatch = Oci;
			if (OldMatch == nullptr || !OldMatch->Matches(VNodes[i]))
			{
				bStructural = true;
			}
			FRuiFiber* Cf = ReconcileFiber(ParentFiber, OldMatch, VNodes[i], i);
			if (Prev == nullptr)
			{
				ParentFiber->Child = Cf;
			}
			else
			{
				Prev->Sibling = Cf;
			}
			Prev = Cf;
			if (Oci != nullptr)
			{
				Oci = Oci->Sibling;
			}
		}
		while (Oci != nullptr)
		{
			FRuiFiber* Nxti = Oci->Sibling;
			DeleteFiber(ParentFiber, Oci);
			Oci = Nxti;
		}
	}

	if (bStructural)
	{
		MarkReorder(ParentFiber); // only when the SET changed [perf #2]
	}
	return false;
}

bool FRuiReconciler::TryFastLeafList(FRuiFiber* ParentFiber, FRuiFiber* OldFirstFiber, const TArray<FRuiNode>& VNodes,
									 bool bIgnoreKeys)
{
	const int32 N = VNodes.Num();
	// 1. Eligibility scan (read-only).
	FRuiFiber* Oc = OldFirstFiber;
	for (int32 i = 0; i < N; ++i)
	{
		if (Oc == nullptr)
		{
			return false;
		}
		const FRuiNode& Vn = VNodes[i];
		if (Vn.Kind != ERuiNodeKind::Host || Oc->Tag != ERuiFiberTag::Host || !(Oc->ElementType == Vn.ElementType) ||
			(!bIgnoreKeys && !(Oc->Key == Vn.Key)))
		{
			return false;
		}
		if (Oc->Child != nullptr || Vn.NumChildren() != 0)
		{
			return false; // leaves on both sides only
		}
		Oc = Oc->Sibling;
	}
	if (Oc != nullptr)
	{
		return false; // old list longer -> count changed
	}
	// 2. Reconcile in place (no buddy swap, no per-child begin/complete).
	ParentFiber->Child = OldFirstFiber;
	Oc = OldFirstFiber;
	for (int32 i = 0; i < N; ++i)
	{
		const FRuiNode& Vn = VNodes[i];
		if (bIgnoreKeys)
		{
			Oc->Key = Vn.Key; // adopt the new key so the slot stays fast next frame
		}
		Oc->Parent = ParentFiber;
		Oc->Index = i;
		Oc->EffectTag = RuiEffect_None;
		Oc->NextEffect = nullptr;
		Oc->InputChildren = Vn.Children;
		const TSharedPtr<const FRuiPropsBase>& Np = Vn.Props;
		Oc->PendingProps = Np;
		const bool bChanged = (Np != Oc->Props) && !(Np.IsValid() && Oc->Props.IsValid() && Np->Equals(*Oc->Props));
		if (bChanged)
		{
			Oc->EffectTag = RuiEffect_Update;
			AppendEffect(Oc);
		}
		Oc = Oc->Sibling;
	}
	return true;
}

bool FRuiReconciler::KeysStable(FRuiFiber* OldFirstFiber, const TArray<FRuiNode>& VNodes) const
{
	FRuiFiber* Oc = OldFirstFiber;
	for (int32 i = 0; i < VNodes.Num(); ++i)
	{
		if (Oc == nullptr)
		{
			return false;
		}
		const FRuiNode& Vn = VNodes[i];
		if (Oc->Key.IsSet() || Vn.Key.IsSet())
		{
			if (!(Oc->Key == Vn.Key))
			{
				return false;
			}
		}
		else if (Oc->Index != i)
		{
			return false; // both unkeyed -> must be the same position
		}
		if (!Oc->Matches(Vn))
		{
			return false;
		}
		Oc = Oc->Sibling;
	}
	return Oc == nullptr; // exactly the same length
}

void FRuiReconciler::DeleteFiber(FRuiFiber* ParentFiber, FRuiFiber* OldFiber)
{
	OldFiber->EffectTag |= RuiEffect_Deletion;
	ParentFiber->bHasDeletions = true;
	Deletions.Add(OldFiber);
	MarkReorder(ParentFiber);
}

void FRuiReconciler::MarkReorder(FRuiFiber* ParentFiber)
{
	for (FRuiFiber* F = ParentFiber; F != nullptr; F = F->Parent)
	{
		if (F->IsPortal() || F->Node.IsValid())
		{
			ReorderSet.Add(F);
			return;
		}
	}
}

bool FRuiReconciler::HasContextChanged(const FRuiFiber* Fiber) const
{
	if (!Fiber->State.IsValid())
	{
		return false;
	}
	for (const FRuiComponentState::FContextDep& Dep : Fiber->State->ContextDeps)
	{
		// Resolve against the COMMITTED tree via the alternate chain (Godot semantics).
		if (Dep.HasChanged && Dep.HasChanged(Fiber->Alternate ? Fiber->Alternate : Fiber))
		{
			return true;
		}
	}
	return false;
}

void FRuiReconciler::AppendEffect(FRuiFiber* Fiber)
{
	Fiber->NextEffect = nullptr;
	if (FirstEffect == nullptr)
	{
		FirstEffect = Fiber;
	}
	else
	{
		LastEffect->NextEffect = Fiber;
	}
	LastEffect = Fiber;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Complete phase
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuiReconciler::CompleteWork(FRuiFiber* Fiber)
{
	switch (Fiber->Tag)
	{
	case ERuiFiberTag::Host:
		if (!Fiber->Node.IsValid())
		{
			check(Fiber->PendingProps.IsValid());
			Fiber->Node = Host.CreateInstance(Fiber->ElementType, *Fiber->PendingProps);
			Fiber->Props = Fiber->PendingProps;
			Fiber->EffectTag |= RuiEffect_Placement;
		}
		else if (!PropsEqual(Fiber))
		{
			Fiber->EffectTag |= RuiEffect_Update;
		}
		break;
	case ERuiFiberTag::Portal:
		if (Fiber->Alternate != nullptr && Fiber->Alternate->PortalTarget != Fiber->PortalTarget)
		{
			Fiber->EffectTag |= RuiEffect_PortalRetarget;
			ReorderSet.Add(Fiber); // re-assert order at the new target [audit M6]
		}
		break;
	default:
		break;
	}
	if (Fiber->EffectTag != RuiEffect_None)
	{
		AppendEffect(Fiber);
	}
	PopProvidedContext(Fiber); // providers pop on ascend — every path
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Commit phase
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuiReconciler::CommitRoot()
{
	bIsCommitting = true;
	FRuiDiagnostics::OnCommit();

	for (FRuiFiber* D : Deletions)
	{
		CommitDeletion(D);
	}
	Deletions.Reset();

	FRuiFiber* F = FirstEffect;
	while (F != nullptr)
	{
		const uint8 Tag = F->EffectTag;
		if (Tag & RuiEffect_Placement)
		{
			CommitPlacement(F);
		}
		if (Tag & RuiEffect_Update)
		{
			CommitUpdate(F);
		}
		if (Tag & RuiEffect_PortalRetarget)
		{
			CommitPortalRetarget(F);
		}
		if (Tag & RuiEffect_Layout)
		{
			CommitLayoutEffects(F);
		}
		if (Tag & RuiEffect_Passive)
		{
			PendingPassive.Add(F);
		}
		FRuiFiber* Nxt = F->NextEffect;
		F->EffectTag = RuiEffect_None;
		F->NextEffect = nullptr;
		F = Nxt;
	}
	FirstEffect = nullptr;
	LastEffect = nullptr;

	for (FRuiFiber* Hp : ReorderSet)
	{
		EnforceChildOrder(Hp);
	}
	ReorderSet.Reset();

	RootCurrent = WipRoot;
	WipRoot = nullptr;			  // non-null WipRoot now always means "abandoned pass" (BeginRender reclaims)
	PendingEbActivations.Reset(); // activations that never found their boundary are stale now
	bIsCommitting = false;

	FlushPassive();

	if (!DeferredUpdates.IsEmpty())
	{
		TArray<FRuiFiber*> Deferred = MoveTemp(DeferredUpdates);
		DeferredUpdates.Reset();
		for (FRuiFiber* Target : Deferred)
		{
			ScheduleUpdateOnFiber(Target);
		}
	}
}

void FRuiReconciler::CommitPlacement(FRuiFiber* Fiber)
{
	if (!Fiber->Node.IsValid())
	{
		return;
	}
	bool bViaPortal = false;
	FRuiPortalHandle Portal;
	FRuiHostHandle ParentNode = HostParentNode(Fiber, bViaPortal, Portal);
	// APPEND (index -1) + let the structural-frame reorder assert exact order — the family
	// model (add_child then enforce order). Fiber->Index is the index among sibling VNODES,
	// NOT the flattened host index (fragments/components collapse), so it must not be used
	// as an insertion position.
	if (bViaPortal)
	{
		Host.InsertPortalChild(Portal, Fiber->Node, INDEX_NONE);
	}
	else
	{
		Host.InsertChild(ParentNode, Fiber->Node, INDEX_NONE);
	}
	// React ref lifecycle (D-08.4): attach after placement.
	if (Fiber->PendingProps.IsValid() && Fiber->PendingProps->Ref)
	{
		Fiber->PendingProps->Ref(Fiber->Node);
	}
	FRuiDiagnostics::OnPlacement();
}

void FRuiReconciler::CommitUpdate(FRuiFiber* Fiber)
{
	if (!Fiber->Node.IsValid() || !Fiber->PendingProps.IsValid())
	{
		return;
	}
	Host.CommitUpdate(Fiber->Node, Fiber->ElementType, Fiber->Props.Get(), *Fiber->PendingProps);
	Fiber->Props = Fiber->PendingProps;
	FRuiDiagnostics::OnUpdate();
}

void FRuiReconciler::CommitDeletion(FRuiFiber* Fiber)
{
	FRuiDiagnostics::OnDeletion();
	NullRefsRecursive(Fiber);
	RunCleanupsRecursive(Fiber);
	ReleaseHostNodes(Fiber);
	ReleaseFiberTree(Fiber);
}

void FRuiReconciler::NullRefsRecursive(FRuiFiber* Fiber)
{
	// React detach: refs cleared on unmount so callback refs never dangle. [audit C2]
	if (Fiber->Props.IsValid() && Fiber->Props->Ref)
	{
		Fiber->Props->Ref(FRuiHostHandle());
	}
	for (FRuiFiber* C = Fiber->Child; C != nullptr; C = C->Sibling)
	{
		NullRefsRecursive(C);
	}
}

void FRuiReconciler::CommitPortalRetarget(FRuiFiber* Fiber)
{
	if (!Fiber->PortalTarget.IsValid())
	{
		return;
	}
	TArray<FRuiHostHandle> Ordered;
	CollectHostChildren(Fiber, Ordered);
	for (const FRuiHostHandle& Child : Ordered)
	{
		// Old target detach happens host-side inside InsertPortalChild's re-parent; the
		// portal is in the reorder set (see CompleteWork) so exact order is asserted after.
		Host.InsertPortalChild(Fiber->PortalTarget, Child, INDEX_NONE);
	}
}

void FRuiReconciler::CommitLayoutEffects(FRuiFiber* Fiber)
{
	if (!Fiber->State.IsValid())
	{
		return;
	}
	for (FRuiEffect& E : Fiber->State->LayoutEffects)
	{
		if (!E.bEverRan || RUI::DepsChanged(E.LastDeps, E.Deps))
		{
			if (E.Cleanup)
			{
				E.Cleanup();
				E.Cleanup = nullptr;
			}
			E.Cleanup = E.Factory ? E.Factory() : FRuiEffectCleanup();
			E.LastDeps = E.Deps;
			E.bEverRan = true;
		}
	}
}

void FRuiReconciler::FlushPassive()
{
	// Two passes across all collected fibers: every cleanup first, then every setup.
	for (FRuiFiber* Fiber : PendingPassive)
	{
		if (!Fiber->State.IsValid())
		{
			continue;
		}
		for (FRuiEffect& E : Fiber->State->Effects)
		{
			if ((!E.bEverRan || RUI::DepsChanged(E.LastDeps, E.Deps)) && E.Cleanup)
			{
				E.Cleanup();
				E.Cleanup = nullptr;
			}
		}
	}
	for (FRuiFiber* Fiber : PendingPassive)
	{
		if (!Fiber->State.IsValid())
		{
			continue;
		}
		for (FRuiEffect& E : Fiber->State->Effects)
		{
			if (!E.bEverRan || RUI::DepsChanged(E.LastDeps, E.Deps))
			{
				E.Cleanup = E.Factory ? E.Factory() : FRuiEffectCleanup();
				E.LastDeps = E.Deps;
				E.bEverRan = true;
			}
		}
	}
	PendingPassive.Reset();
}

void FRuiReconciler::RunCleanups(FRuiFiber* Fiber)
{
	if (!Fiber->State.IsValid())
	{
		return;
	}
	for (TArray<FRuiEffect>* Arr : {&Fiber->State->Effects, &Fiber->State->LayoutEffects})
	{
		for (FRuiEffect& E : *Arr)
		{
			if (E.Cleanup)
			{
				E.Cleanup();
				E.Cleanup = nullptr;
			}
		}
	}
}

void FRuiReconciler::RunCleanupsRecursive(FRuiFiber* Fiber)
{
	RunCleanups(Fiber);
	for (FRuiFiber* C = Fiber->Child; C != nullptr; C = C->Sibling)
	{
		RunCleanupsRecursive(C);
	}
	DisposeFiberState(Fiber);
}

void FRuiReconciler::DisposeFiberState(FRuiFiber* Fiber)
{
	if (Fiber->State.IsValid())
	{
		Fiber->State->Dispose(); // cell dtors release external subscriptions
		Fiber->State.Reset();
	}
}

void FRuiReconciler::EnforceChildOrder(FRuiFiber* ParentFiber)
{
	FRuiHostHandle PNode;
	bool bPortal = false;
	if (ParentFiber->IsPortal())
	{
		PNode = ParentFiber->PortalTarget;
		bPortal = true;
	}
	else if (ParentFiber->Node.IsValid())
	{
		PNode = ParentFiber->Node;
	}
	if (!PNode.IsValid())
	{
		return;
	}
	TArray<FRuiHostHandle> Ordered;
	CollectHostChildren(ParentFiber, Ordered);
	(void)bPortal;
	Host.ReorderChildren(PNode, Ordered);
}

void FRuiReconciler::CollectHostChildren(FRuiFiber* Fiber, TArray<FRuiHostHandle>& Out) const
{
	for (FRuiFiber* C = Fiber->Child; C != nullptr; C = C->Sibling)
	{
		if (C->Tag == ERuiFiberTag::Portal)
		{
			continue; // portal children live under the portal target, not here
		}
		if (C->Node.IsValid())
		{
			Out.Add(C->Node);
		}
		else
		{
			CollectHostChildren(C, Out);
		}
	}
}

void FRuiReconciler::ReleaseHostNodes(FRuiFiber* Fiber)
{
	if (Fiber->Node.IsValid() && !Fiber->IsRoot())
	{
		const bool bChildless = (Fiber->Child == nullptr);
		Host.ReleaseInstance(Fiber->Node, Fiber->ElementType, Fiber->Props.Get(), bChildless);
		return; // the host released/pooled the whole subtree root; children released with it
	}
	for (FRuiFiber* C = Fiber->Child; C != nullptr; C = C->Sibling)
	{
		ReleaseHostNodes(C);
	}
}

FRuiHostHandle FRuiReconciler::HostParentNode(FRuiFiber* Fiber, bool& bOutViaPortal, FRuiPortalHandle& OutPortal) const
{
	bOutViaPortal = false;
	for (FRuiFiber* P = Fiber->Parent; P != nullptr; P = P->Parent)
	{
		if (P->IsPortal() && P->PortalTarget.IsValid())
		{
			bOutViaPortal = true;
			OutPortal = P->PortalTarget;
			return nullptr;
		}
		if (P->Node.IsValid())
		{
			return P->Node;
		}
	}
	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Context plumbing (untyped halves — typed halves live in FRuiContext templates)
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuiReconciler::NotifyEffectKinds(FRuiFiber& Fiber, bool bHasPassive, bool bHasLayout)
{
	if (bHasPassive)
	{
		Fiber.EffectTag |= RuiEffect_Passive;
	}
	if (bHasLayout)
	{
		Fiber.EffectTag |= RuiEffect_Layout;
	}
}

const IRuiProvidedValue* FRuiReconciler::ResolveProvidedOnCommitted(const FRuiFiber* From, const void* Key)
{
	for (const FRuiFiber* F = From; F != nullptr; F = F->Parent)
	{
		if (F->ProvidedContext.IsValid())
		{
			if (const TSharedPtr<void>* Found = F->ProvidedContext->Find(Key))
			{
				return static_cast<const IRuiProvidedValue*>(Found->Get());
			}
		}
	}
	return nullptr;
}

void FRuiReconciler::OnProvidedValueChanged(FRuiFiber& ProviderFiber, const void* Key)
{
	// Eager propagation over the COMMITTED subtree (Godot _propagate_context_change): mark
	// consumers of Key dirty so they re-render through bailouts; intermediate ancestors get
	// the subtree flag; stop at shadowing providers.
	FRuiFiber* Alt = ProviderFiber.Alternate;
	if (Alt == nullptr || Alt->Child == nullptr)
	{
		return;
	}

	struct FWalker
	{
		const void* Key;
		bool Walk(FRuiFiber* First)
		{
			bool bAny = false;
			for (FRuiFiber* F = First; F != nullptr; F = F->Sibling)
			{
				if (F->ProvidedContext.IsValid() && F->ProvidedContext->Contains(Key))
				{
					continue; // shadowed below this provider
				}
				bool bSelfMarked = false;
				if (F->bReadsContext && F->State.IsValid())
				{
					for (const FRuiComponentState::FContextDep& Dep : F->State->ContextDeps)
					{
						if (Dep.Key == Key)
						{
							F->bHasPendingUpdate = true;
							bSelfMarked = true;
							bAny = true;
							break;
						}
					}
				}
				if (F->Child != nullptr)
				{
					if (Walk(F->Child))
					{
						bAny = true;
						if (!bSelfMarked)
						{
							F->bSubtreeHasUpdates = true;
						}
					}
				}
			}
			return bAny;
		}
	};
	FWalker Walker{Key};
	Walker.Walk(Alt->Child);
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────────────────

bool FRuiReconciler::AnyKeyed(const TArray<FRuiNode>& VNodes)
{
	for (const FRuiNode& Vn : VNodes)
	{
		if (Vn.Key.IsSet())
		{
			return true;
		}
	}
	return false;
}

FRuiKey FRuiReconciler::FiberKey(const FRuiFiber* F)
{
	if (F->Key.IsSet())
	{
		return F->Key;
	}
	// Namespaced positional key: control-char-prefixed name — can never equal a user key
	// (user FName keys can't contain \x01; user int keys are a different FRuiKey kind).
	return FRuiKey(FName(*FString::Printf(TEXT("\x01idx%d"), F->Index)));
}

FRuiKey FRuiReconciler::VNodeKey(const FRuiNode& VNode, int32 Index)
{
	if (VNode.Key.IsSet())
	{
		return VNode.Key;
	}
	return FRuiKey(FName(*FString::Printf(TEXT("\x01idx%d"), Index)));
}

bool FRuiReconciler::ChildrenSame(const FRuiChildren& A, const FRuiChildren& B)
{
	// Pointer identity of shared child lists == the family's vnode-identity children check.
	const bool bAEmpty = !A.IsValid() || A->IsEmpty();
	const bool bBEmpty = !B.IsValid() || B->IsEmpty();
	if (bAEmpty && bBEmpty)
	{
		return true;
	}
	return A == B;
}

bool FRuiReconciler::PropsEqual(const FRuiFiber* Fiber) const
{
	if (!Fiber->Props.IsValid())
	{
		return false; // never rendered
	}
	if (Fiber->PendingProps == Fiber->Props) // identity fast-path (memoized props) [perf P3]
	{
		return true;
	}
	if (!Fiber->PendingProps.IsValid())
	{
		return false;
	}
	if (Fiber->PendingProps->MemoEquals) // V.memo custom comparer
	{
		return Fiber->PendingProps->MemoEquals(*Fiber->Props, *Fiber->PendingProps);
	}
	return Fiber->PendingProps->Equals(*Fiber->Props);
}

const TArray<FRuiNode>& FRuiReconciler::NormalizedChildren(const FRuiChildren& Children) const
{
	// The family flattened nested arrays and auto-wrapped raw strings; C++ child lists are
	// flat + homogeneous by construction (RUI::Text is explicit), so this is a passthrough.
	return Children.IsValid() ? *Children : EmptyChildren;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Teardown
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuiReconciler::ReleaseFiberTree(FRuiFiber* Fiber)
{
	if (Fiber == nullptr)
	{
		return;
	}
	FRuiFiber* C = Fiber->Child;
	while (C != nullptr)
	{
		FRuiFiber* Nxt = C->Sibling;
		ReleaseFiberTree(C);
		C = Nxt;
	}
	FRuiFiber* Alt = Fiber->Alternate;
	Slab.Release(Fiber); // ResetForReuse severs everything
	if (Alt != nullptr)
	{
		// Release the buddy too (its children are buddies of ours, already handled).
		Slab.Release(Alt);
	}
}

void FRuiReconciler::ReleaseAbandonedChildren(FRuiFiber* Parent)
{
	FRuiFiber* Child = Parent->Child;
	Parent->Child = nullptr;
	// COMMITTED-CHAIN adoption: this WIP fiber shares the committed twin's child chain
	// (same objects, not copies) — via SUBTREE-SKIP (children still parented to the twin)
	// or via the fast-leaf-list, which additionally REPARENTED the committed leaves onto
	// the WIP fiber. Keep the chain, and repair Parent back to the committed twin so the
	// committed tree stays self-consistent (ScheduleUpdateOnFiber walks Parent between
	// passes — it must never climb into a reclaimed WIP chain).
	if (Child != nullptr && Parent->Alternate != nullptr && Parent->Alternate->Child == Child)
	{
		for (FRuiFiber* C = Child; C != nullptr; C = C->Sibling)
		{
			if (C->Parent == Parent)
			{
				C->Parent = Parent->Alternate;
			}
		}
		return;
	}
	while (Child != nullptr)
	{
		FRuiFiber* Next = Child->Sibling;
		if (Child->Parent != Parent)
		{
			break; // defense-in-depth: any other shared-chain flavor is not ours to free
		}
		ReleaseAbandonedChildren(Child);
		if (Child->Alternate != nullptr)
		{
			// Sever the pairing from the committed side; it re-pairs fresh next pass.
			Child->Alternate->Alternate = nullptr;
		}
		if (Child->State.IsValid() && Child->State->Fiber == Child)
		{
			// A shared state must never keep pointing at a released fiber (setters would
			// misdirect ScheduleUpdateOnFiber) — repoint at the committed twin, if any.
			Child->State->Fiber = Child->Alternate;
		}
		Slab.Release(Child);
		Child = Next;
	}
}

void FRuiReconciler::Unmount()
{
	if (RootCurrent == nullptr)
	{
		return;
	}
	bTickPending = false;
	if (WipRoot != nullptr)
	{
		ReleaseAbandonedChildren(WipRoot); // an abandoned pass dies with the root
	}
	PendingEbActivations.Reset();
	for (FRuiFiber* C = RootCurrent->Child; C != nullptr; C = C->Sibling)
	{
		NullRefsRecursive(C);
		RunCleanupsRecursive(C);
		ReleaseHostNodes(C);
	}
	ReleaseFiberTree(RootCurrent);
	RootCurrent = nullptr;
	WipRoot = nullptr;
	NextUnit = nullptr;
	RootVNode.Reset();
}
