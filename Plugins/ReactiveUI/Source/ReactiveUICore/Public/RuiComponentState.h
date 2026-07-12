// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Per-function-component hook & effect storage — the port of component_state.gd. SHARED
// across a fiber's alternates by TSharedPtr (never copied), so hook state survives the
// current<->WIP double-buffer swap; `Fiber` is re-pointed to the live fiber each render
// (D-06's ownership edge #2: the back-pointer is RAW and nulled on teardown).

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiHooksInternal.h"

struct FRuiFiber;

class REACTIVEUICORE_API FRuiComponentState
{
public:
	FRuiComponentState() = default;
	// Never copied: state is SHARED across alternates by pointer (component_state.gd's
	// contract) — a copy would silently fork hook slots. Also required mechanically: the
	// dllexport'd class would otherwise instantiate an implicit copy-assign over the
	// move-only hook-cell array.
	FRuiComponentState(const FRuiComponentState&) = delete;
	FRuiComponentState& operator=(const FRuiComponentState&) = delete;

	/** Back-pointer to the LIVE fiber (re-pointed by the reconciler; raw by design — the
	 *  slab owns fibers, state must never extend their life). */
	FRuiFiber* Fiber = nullptr;

	/** Positional hook slots + cursor (reset each render). */
	TArray<TUniquePtr<IRuiHookCell>> Hooks;
	int32 HookIndex = 0;

	/** Passive + layout effects, each with their OWN cursor (insertion of an effect can
	 *  never shift state slots — the family's three-cursor rule). */
	TArray<FRuiEffect> Effects;
	int32 EffectIndex = 0;
	TArray<FRuiEffect> LayoutEffects;
	int32 LayoutIndex = 0;

	/** Context reads recorded this render, OUT of the slot array (useContext consumes no
	 *  hook slot). Each entry re-checks "did my context change?" against the live tree. */
	struct FContextDep
	{
		const void* Key = nullptr;					  // context handle identity
		TFunction<bool(const FRuiFiber*)> HasChanged; // resolve + compare captured value
	};
	TArray<FContextDep> ContextDeps;

	/** () -> reconciler.ScheduleUpdateOnFiber(state->Fiber). Cleared on teardown. */
	TFunction<void()> OnStateUpdated;

	bool bIsRendering = false;

	/** Cached render output — reused verbatim on bailout, as the SAME shared list (pointer
	 *  identity is what lets grandchildren bail via children_same; a fresh copy per bailed
	 *  render would silently defeat descendant bailouts). Why vnodes are cross-frame; see
	 *  FRuiNode's lifetime note. */
	FRuiChildren LastOutput;

	// --- dev diagnostics (rui.HookValidation) ---
	TArray<ERuiHookKind> HookLog;		 // this render (transient)
	TArray<ERuiHookKind> HookSignatures; // primed first render
	bool bHookOrderPrimed = false;
	TSet<FName> DiagWarned; // warn-once keys

	/** The definition-override generation this state last rendered under (HMR). A fiber
	 *  seeing a NEWER generation with bResetState pending runs a deliberate state reset. */
	uint32 HmrGeneration = 0;

	/** TD-019: one-shot state migration across the compiled→interp HMR swap. When the reconciler
	 *  resets hooks for a same-shape representation swap it snapshots the old cells' exported
	 *  FRuiValues here (by hook slot); the next render's UseState seeds fresh FRuiValue cells from
	 *  it instead of the interpreter's Init, so numeric/string/bool/text state survives the FIRST
	 *  save too. Cleared after the migrating render. Null entries (non-migratable slots) re-init. */
	TArray<FRuiValue> MigratedState;

	/** The HMR hook-shape reset: run effect cleanups, then drop every hook slot so the next
	 *  render re-initializes (cell destructors release external subscriptions). Scheduling
	 *  wiring (OnStateUpdated/Fiber) and LastOutput stay — the fiber is still mounted. */
	void HmrResetHooks()
	{
		for (FRuiEffect& Effect : LayoutEffects)
		{
			if (Effect.Cleanup)
			{
				Effect.Cleanup();
				Effect.Cleanup = nullptr;
			}
		}
		for (FRuiEffect& Effect : Effects)
		{
			if (Effect.Cleanup)
			{
				Effect.Cleanup();
				Effect.Cleanup = nullptr;
			}
		}
		Hooks.Empty();
		Effects.Empty();
		LayoutEffects.Empty();
		ContextDeps.Empty();
		HookIndex = 0;
		EffectIndex = 0;
		LayoutIndex = 0;
		bHookOrderPrimed = false;
		HookLog.Reset();
		HookSignatures.Reset();
	}

	/** Deliberate full teardown (unmount / HMR hook-shape reset). Cell destructors release
	 *  external subscriptions (signal cells unsubscribe in ~cell); effects' cleanups must
	 *  have been run by the reconciler BEFORE this (family order). */
	void Dispose()
	{
		Hooks.Empty();
		Effects.Empty();
		LayoutEffects.Empty();
		ContextDeps.Empty();
		LastOutput.Reset();
		OnStateUpdated = nullptr;
		Fiber = nullptr;
	}

	/** Notify the reconciler (setters/dispatch/signal-fire path). */
	void NotifyStateUpdated()
	{
		if (OnStateUpdated)
		{
			OnStateUpdated();
		}
	}
};

// ── TRuiSetter implementation (needs FRuiComponentState) ─────────────────────────────────

template <typename T> void TRuiSetter<T>::operator()(T NewValue) const
{
	TSharedPtr<FRuiComponentState> S = State.Pin();
	if (!S.IsValid() || Slot >= S->Hooks.Num()) // torn down — ignore late calls [audit C3]
	{
		return;
	}
	TRuiStateCell<T>* Cell = static_cast<TRuiStateCell<T>*>(S->Hooks[Slot].Get());
	if (Cell->Value == NewValue) // equal-set bails (no re-render)
	{
		return;
	}
	Cell->Value = MoveTemp(NewValue);
	S->NotifyStateUpdated();
}

template <typename T> void TRuiSetter<T>::operator()(TFunction<T(const T&)> Updater) const
{
	TSharedPtr<FRuiComponentState> S = State.Pin();
	if (!S.IsValid() || Slot >= S->Hooks.Num())
	{
		return;
	}
	TRuiStateCell<T>* Cell = static_cast<TRuiStateCell<T>*>(S->Hooks[Slot].Get());
	T Next = Updater(Cell->Value);
	if (Cell->Value == Next)
	{
		return;
	}
	Cell->Value = MoveTemp(Next);
	S->NotifyStateUpdated();
}
