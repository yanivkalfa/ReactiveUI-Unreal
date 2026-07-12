// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuiContext — the render context handed to every component: the 23 family hooks as
// member functions over the current fiber's FRuiComponentState (hooks.gd's static `_cur`
// replaced by an explicit reference — D-05; a debug thread-local still asserts in-render
// use, since capturing Ctx in a stored lambda compiles and corrupts cursors at runtime —
// the honest limitation the plan records).
//
// RULES OF HOOKS: top level of the render function only — stable call order every render.
// The positional-slot model depends on it; the dev validator (rui.HookValidation) diagnoses
// violations.

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiComponentState.h"
#include "RuiFiber.h"
#include "RuiContextHandle.h"
#include "RuiCoreMisc.h"
#include "RuiHostConfig.h"

class FRuiReconciler;

/** UseSafeArea result (pixels). */
struct FRuiSafeArea
{
	float Left = 0, Top = 0, Right = 0, Bottom = 0;
};

class REACTIVEUICORE_API FRuiContext
{
public:
	FRuiContext(const TSharedRef<FRuiComponentState>& InState, FRuiFiber& InFiber, FRuiReconciler& InReconciler,
				IRuiHostConfig& InHost)
		: StateShared(InState), State(InState.Get()), Fiber(InFiber), Reconciler(InReconciler), Host(InHost)
	{
	}

	// Non-copyable, non-movable: hardening against storing/capturing the context (D-05).
	FRuiContext(const FRuiContext&) = delete;
	FRuiContext& operator=(const FRuiContext&) = delete;

	// ── state ────────────────────────────────────────────────────────────────────────

	/** auto [Value, SetValue] = Ctx.UseState(0); — setter identity stable; equal-set bails. */
	template <typename T> TTuple<T, TRuiSetter<T>> UseState(T Initial = T())
	{
		Record(ERuiHookKind::State);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			// TD-019: on a migrating HMR reset the reconciler stashed the old cell's exported value
			// at this slot — seed the fresh FRuiValue cell from it (the interpreter's cell type)
			// instead of the interp Init, so numeric/string/bool/text state survives the first swap.
			if constexpr (std::is_same_v<T, FRuiValue>)
			{
				if (State.MigratedState.IsValidIndex(i) && !State.MigratedState[i].IsNull())
				{
					Initial = State.MigratedState[i];
				}
			}
			State.Hooks.Emplace(MakeUnique<TRuiStateCell<T>>(MoveTemp(Initial)));
		}
		TRuiStateCell<T>* Cell = static_cast<TRuiStateCell<T>*>(State.Hooks[i].Get());
		return MakeTuple(Cell->Value, TRuiSetter<T>(StateWeak(), i));
	}

	/** auto [Value, Dispatch] = Ctx.UseReducer<T, FAction>(Reducer, Initial); — dispatch
	 *  identity stable; the reducer closure is refreshed every render (family parity). */
	template <typename T, typename TAction>
	TTuple<T, TFunction<void(TAction)>> UseReducer(TFunction<T(const T&, const TAction&)> Reducer, T Initial = T())
	{
		Record(ERuiHookKind::Reducer);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<TRuiReducerCell<T, TAction>>(MoveTemp(Initial)));
		}
		TRuiReducerCell<T, TAction>* Cell = static_cast<TRuiReducerCell<T, TAction>*>(State.Hooks[i].Get());
		Cell->Reducer = MoveTemp(Reducer);
		TWeakPtr<FRuiComponentState> Weak = StateWeak();
		TFunction<void(TAction)> Dispatch = [Weak, i](TAction Action)
		{
			TSharedPtr<FRuiComponentState> S = Weak.Pin();
			if (!S.IsValid() || i >= S->Hooks.Num()) // torn down — ignore late dispatch
			{
				return;
			}
			TRuiReducerCell<T, TAction>* C = static_cast<TRuiReducerCell<T, TAction>*>(S->Hooks[i].Get());
			T Next = C->Reducer(C->Value, Action);
			if (C->Value == Next)
			{
				return;
			}
			C->Value = MoveTemp(Next);
			S->NotifyStateUpdated();
		};
		return MakeTuple(Cell->Value, MoveTemp(Dispatch));
	}

	/** Stable box; mutating Current never re-renders. */
	template <typename T> TSharedRef<TRuiRef<T>> UseRef(T Initial = T())
	{
		Record(ERuiHookKind::Ref);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<TRuiRefCell<T>>(MoveTemp(Initial)));
		}
		return static_cast<TRuiRefCell<T>*>(State.Hooks[i].Get())->Box;
	}

	/** Recompute only when deps change (shallow — RUI::Deps semantics). */
	template <typename T> const T& UseMemo(TFunction<T()> Factory, FRuiDeps Deps)
	{
		Record(ERuiHookKind::Memo);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			TUniquePtr<TRuiMemoCell<T>> Cell = MakeUnique<TRuiMemoCell<T>>();
			Cell->Value = Factory();
			Cell->LastDeps = MoveTemp(Deps);
			State.Hooks.Emplace(MoveTemp(Cell));
			return static_cast<TRuiMemoCell<T>*>(State.Hooks[i].Get())->Value;
		}
		TRuiMemoCell<T>* Cell = static_cast<TRuiMemoCell<T>*>(State.Hooks[i].Get());
		if (RUI::DepsChanged(Cell->LastDeps, Deps))
		{
			Cell->Value = Factory();
			Cell->LastDeps = MoveTemp(Deps);
		}
		return Cell->Value;
	}

	/** A stable FRuiCallback while deps are unchanged (memo-friendly event props). */
	FRuiCallback UseCallback(TFunction<void(const FRuiValue&)> Fn, FRuiDeps Deps)
	{
		return UseMemo<FRuiCallback>([&Fn]() { return FRuiCallback::Create(MoveTemp(Fn)); }, MoveTemp(Deps));
	}
	FRuiCallback UseCallback(TFunction<void()> Fn, FRuiDeps Deps)
	{
		return UseMemo<FRuiCallback>([&Fn]() { return FRuiCallback::Create(MoveTemp(Fn)); }, MoveTemp(Deps));
	}

	/** = UseMemo (family alias). */
	template <typename T> const T& UseImperativeHandle(TFunction<T()> Factory, FRuiDeps Deps)
	{
		return UseMemo<T>(MoveTemp(Factory), MoveTemp(Deps));
	}

	// ── effects ──────────────────────────────────────────────────────────────────────

	/** Passive: after commit, two-pass (all cleanups, then all setups). Deps: RUI::Deps(...)
	 *  = when changed; RUI::Deps() = mount-only; RUI::EveryCommit() = every commit.
	 *  Accepts a lambda returning either void or an FRuiEffectCleanup (if-constexpr dispatch
	 *  — twin TFunction overloads are ambiguous for lambdas). */
	template <typename TFn> void UseEffect(TFn&& Effect, FRuiDeps Deps)
	{
		UseEffectImpl(WrapEffect(Forward<TFn>(Effect)), MoveTemp(Deps));
	}

	/** Layout: synchronously during commit (pre-paint), cleanup-then-setup per fiber. */
	template <typename TFn> void UseLayoutEffect(TFn&& Effect, FRuiDeps Deps)
	{
		UseLayoutEffectImpl(WrapEffect(Forward<TFn>(Effect)), MoveTemp(Deps));
	}

	void UseEffectImpl(TFunction<FRuiEffectCleanup()> Effect, FRuiDeps Deps)
	{
		Record(ERuiHookKind::Effect); // own cursor; logged for order validation like any hook
		const int32 i = State.EffectIndex++;
		if (i >= State.Effects.Num())
		{
			State.Effects.AddDefaulted();
		}
		State.Effects[i].Factory = MoveTemp(Effect);
		State.Effects[i].Deps = MoveTemp(Deps);
		NotifyEffects();
	}

	void UseLayoutEffectImpl(TFunction<FRuiEffectCleanup()> Effect, FRuiDeps Deps)
	{
		Record(ERuiHookKind::LayoutEffect);
		const int32 i = State.LayoutIndex++;
		if (i >= State.LayoutEffects.Num())
		{
			State.LayoutEffects.AddDefaulted();
		}
		State.LayoutEffects[i].Factory = MoveTemp(Effect);
		State.LayoutEffects[i].Deps = MoveTemp(Deps);
		NotifyEffects();
	}

	// ── context ──────────────────────────────────────────────────────────────────────

	/** Nearest provided value (O(1) via the handle's render stack — D-08.3), or the
	 *  handle's default. Consumes NO hook slot; records the read for change re-rendering. */
	template <typename T> const T& UseContext(const TRuiContext<T>& Handle)
	{
		Fiber.bReadsContext = true;
		const TArray<const T*>& Stack = Handle.GetCore().RenderStack;
		const T& Current = Stack.IsEmpty() ? Handle.GetDefault() : *Stack.Last();

		FRuiComponentState::FContextDep Dep;
		Dep.Key = Handle.Key();
		Dep.HasChanged = [Handle, Old = Current](const FRuiFiber* F) -> bool
		{
			const T* Now = ResolveTyped(Handle, F);
			const T& NowRef = Now ? *Now : Handle.GetDefault();
			return !(NowRef == Old);
		};
		State.ContextDeps.Add(MoveTemp(Dep));
		return Current;
	}

	/** Provide `Value` under `Handle` to this component's subtree (hook-style provider —
	 *  family API, no wrapper element). On change, marks consuming descendants dirty so
	 *  they re-render through bailouts (eager propagation). */
	template <typename T> void ProvideContext(const TRuiContext<T>& Handle, T Value)
	{
		if (!Fiber.ProvidedContext.IsValid())
		{
			Fiber.ProvidedContext = MakeShared<TMap<const void*, TSharedPtr<void>>>();
		}
		TSharedPtr<TRuiProvidedValue<T>> Holder = MakeShared<TRuiProvidedValue<T>>(Handle, MoveTemp(Value));

		bool bChanged = true;
		if (const FRuiFiber* Alt = Fiber.Alternate)
		{
			if (Alt->ProvidedContext.IsValid())
			{
				if (const TSharedPtr<void>* Prev = Alt->ProvidedContext->Find(Handle.Key()))
				{
					const IRuiProvidedValue* PrevHolder = static_cast<const IRuiProvidedValue*>(Prev->Get());
					bChanged = !Holder->ValueEquals(*PrevHolder);
				}
			}
		}
		Fiber.ProvidedContext->Add(Handle.Key(), Holder);
		if (bChanged)
		{
			ReconcilerOnProvidedChanged(Handle.Key());
		}
	}

	// ── concurrency (family semantics, D-08 kept-from-Godot list) ─────────────────────

	/** Returns the PREVIOUS value on the render where `Value` changes, then commits the
	 *  target on a deferred next-frame tick (one extra re-render). */
	template <typename T> T UseDeferredValue(T Value, FRuiDeps Deps = FRuiDeps())
	{
		Record(ERuiHookKind::Deferred);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			TUniquePtr<TRuiDeferredCell<T>> Cell = MakeUnique<TRuiDeferredCell<T>>();
			Cell->Value = Value;
			Cell->Target = Value;
			Cell->Deps = MoveTemp(Deps);
			State.Hooks.Emplace(MoveTemp(Cell));
			return Value;
		}
		TRuiDeferredCell<T>* Cell = static_cast<TRuiDeferredCell<T>*>(State.Hooks[i].Get());
		bool bChanged;
		if (Deps.IsSet())
		{
			bChanged = RUI::DepsChanged(Cell->Deps, Deps);
			Cell->Deps = MoveTemp(Deps);
		}
		else
		{
			bChanged = !(Cell->Value == Value);
		}
		if (bChanged && !(Cell->Value == Value))
		{
			Cell->Target = Value;
			if (!Cell->bPending)
			{
				Cell->bPending = true;
				ScheduleDeferredCommit<T>(i);
			}
		}
		return Cell->Value;
	}

	/** [bIsPending(false), StartTransition] — synchronous no-op, faithful to the family. */
	TTuple<bool, TFunction<void(TFunction<void()>)>> UseTransition()
	{
		Record(ERuiHookKind::Transition);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<FRuiTransitionCell>());
		}
		return MakeTuple(false, TFunction<void(TFunction<void()>)>(
									[](TFunction<void()> Action)
									{
										if (Action)
										{
											Action();
										}
									}));
	}

	// ── stable callbacks (identity never changes; body refreshed every render) ─────────

	FRuiCallback UseStableCallback(TFunction<void()> Fn)
	{
		return UseStableAction(TFunction<void(const FRuiValue&)>(
			[Inner = MoveTemp(Fn)](const FRuiValue&)
			{
				if (Inner)
				{
					Inner();
				}
			}));
	}
	FRuiCallback UseStableFunc(TFunction<void()> Fn) { return UseStableCallback(MoveTemp(Fn)); }

	FRuiCallback UseStableAction(TFunction<void(const FRuiValue&)> Fn)
	{
		Record(ERuiHookKind::Stable);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			TUniquePtr<FRuiStableCell> Cell = MakeUnique<FRuiStableCell>();
			TSharedRef<TFunction<void(const FRuiValue&)>> Inner = Cell->Inner;
			Cell->Wrapper = FRuiCallback::Create(TFunction<void(const FRuiValue&)>(
				[Inner](const FRuiValue& Arg)
				{
					if (*Inner)
					{
						(*Inner)(Arg);
					}
				}));
			State.Hooks.Emplace(MoveTemp(Cell));
		}
		FRuiStableCell* Cell = static_cast<FRuiStableCell*>(State.Hooks[i].Get());
		*Cell->Inner = MoveTemp(Fn);
		return Cell->Wrapper;
	}

	// ── platform ─────────────────────────────────────────────────────────────────────

	/** Device safe-area insets — host-supplied (mock test value → Slate FDisplayMetrics →
	 *  CommonUI override; the D-27 owner chain). */
	FRuiSafeArea UseSafeArea()
	{
		Record(ERuiHookKind::SafeArea);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<FRuiMarkerCell>(ERuiHookKind::SafeArea));
		}
		FRuiSafeArea Out;
		Host.GetSafeArea(Out.Left, Out.Top, Out.Right, Out.Bottom);
		return Out;
	}

	// ── animation (Phase 7) ───────────────────────────────────────────────────────────

	/**
	 * Animate towards Target, re-rendering every frame until settled (a self-re-arming
	 * RequestFrame chain — the Suspense poll-driver pattern). Retargeting mid-flight starts
	 * from the CURRENT value (no snap). First render primes at Target (no mount animation;
	 * use UseAnimate for enter transitions). Time comes from the host clock (deterministic
	 * under the mock host).
	 */
	template <typename T> T UseTweenValue(const T& Target, float DurationSec = 0.25f, ERuiEase Ease = ERuiEase::InOut)
	{
		return TweenSlot<T>(ERuiHookKind::TweenValue, Target, DurationSec, Ease);
	}

	/** Float convenience over UseTweenValue. */
	float UseTween(float Target, float DurationSec = 0.25f, ERuiEase Ease = ERuiEase::InOut)
	{
		return TweenSlot<float>(ERuiHookKind::Tween, Target, DurationSec, Ease);
	}

	/** 0..1 progress towards bIn — the enter/exit-animation driver. */
	float UseAnimate(bool bIn, float DurationSec = 0.25f, ERuiEase Ease = ERuiEase::InOut)
	{
		return TweenSlot<float>(ERuiHookKind::Animate, bIn ? 1.0f : 0.0f, DurationSec, Ease);
	}

	/**
	 * A play-sound callback for a named bus. Dispatch goes through the process-wide sfx sink
	 * (RUI::SetSfxSink) — the game registers HOW to play (world context, audio assets); an
	 * unset sink is a quiet no-op. The callback payload rides through to the sink.
	 */
	FRuiCallback UseSfx(FName Bus = NAME_None)
	{
		Record(ERuiHookKind::Sfx);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<FRuiMarkerCell>(ERuiHookKind::Sfx));
		}
		return FRuiCallback::Create([Bus](const FRuiValue& Payload) { RUI::DispatchSfx(Bus, Payload); });
	}

	// ── signals (UseSignal/UseSignalKey live in RuiSignal.h as templates over this) ───

	/** \internal — the shared tween slot: prime → retarget-from-current → ease → re-arm. */
	template <typename T> T TweenSlot(ERuiHookKind Kind, const T& Target, float DurationSec, ERuiEase Ease)
	{
		Record(Kind);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			TUniquePtr<TRuiTweenCell<T>> Cell = MakeUnique<TRuiTweenCell<T>>(Kind);
			Cell->From = Target;
			Cell->To = Target;
			Cell->Current = Target; // prime at target — no mount animation
			State.Hooks.Emplace(MoveTemp(Cell));
		}
		TRuiTweenCell<T>* Cell = static_cast<TRuiTweenCell<T>*>(State.Hooks[i].Get());
		const double Now = Host.GetTimeSeconds();
		if (!(Cell->To == Target))
		{
			Cell->From = Cell->Current; // retarget mid-flight: continue from where we are
			Cell->To = Target;
			Cell->StartTime = Now;
			Cell->Duration = FMath::Max(DurationSec, 0.0001f);
			Cell->bActive = true;
		}
		if (Cell->bActive)
		{
			const float Progress =
				FMath::Clamp(static_cast<float>((Now - Cell->StartTime) / Cell->Duration), 0.0f, 1.0f);
			Cell->Current = RUI::LerpTween(Cell->From, Cell->To, RUI::ApplyEase(Ease, Progress));
			if (Progress >= 1.0f)
			{
				Cell->Current = Cell->To;
				Cell->bActive = false;
			}
			else
			{
				// self-re-arming frame chain: one coalesced re-render per frame while animating
				TWeakPtr<FRuiComponentState> Weak = StateWeak();
				Host.RequestFrame(
					[Weak]()
					{
						if (TSharedPtr<FRuiComponentState> Live = Weak.Pin())
						{
							Live->NotifyStateUpdated();
						}
					});
			}
		}
		return Cell->Current;
	}

	/** \internal — signal hook plumbing (see RuiSignal.h). */
	template <typename TCell, typename... TCellArgs> TCell* AcquireCell(ERuiHookKind Kind, TCellArgs&&... Args)
	{
		Record(Kind);
		const int32 i = State.HookIndex++;
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<TCell>(Forward<TCellArgs>(Args)...));
		}
		return static_cast<TCell*>(State.Hooks[i].Get());
	}

	/** \internal */
	TWeakPtr<FRuiComponentState> StateWeak() const;
	/** \internal */
	FRuiComponentState& GetState() { return State; }
	FRuiFiber& GetFiber() { return Fiber; }
	IRuiHostConfig& GetHost() { return Host; }
	void InternalUseEffect(TFunction<FRuiEffectCleanup()> Effect, FRuiDeps Deps)
	{
		UseEffect(MoveTemp(Effect), MoveTemp(Deps));
	}

private:
	/** Normalize an effect lambda (void- or cleanup-returning) into the canonical factory. */
	template <typename TFn> static TFunction<FRuiEffectCleanup()> WrapEffect(TFn&& Effect)
	{
		if constexpr (std::is_void_v<std::invoke_result_t<TFn>>)
		{
			return TFunction<FRuiEffectCleanup()>(
				[Fn = Forward<TFn>(Effect)]() -> FRuiEffectCleanup
				{
					Fn();
					return FRuiEffectCleanup();
				});
		}
		else
		{
			return TFunction<FRuiEffectCleanup()>(Forward<TFn>(Effect));
		}
	}

	void Record(ERuiHookKind Kind);
	void WarnOnce(FName Key, const FString& Msg);
	void NotifyEffects();
	void ReconcilerOnProvidedChanged(const void* Key);
	void StubSlot(ERuiHookKind Kind, const TCHAR* HookName, const TCHAR* Owner);

	template <typename T> void ScheduleDeferredCommit(int32 SlotIndex)
	{
		TWeakPtr<FRuiComponentState> Weak = StateWeak();
		Host.RequestFrame(
			[Weak, SlotIndex]()
			{
				TSharedPtr<FRuiComponentState> S = Weak.Pin();
				if (!S.IsValid() || SlotIndex >= S->Hooks.Num())
				{
					return;
				}
				TRuiDeferredCell<T>* Cell = static_cast<TRuiDeferredCell<T>*>(S->Hooks[SlotIndex].Get());
				Cell->bPending = false;
				if (!(Cell->Value == Cell->Target))
				{
					Cell->Value = Cell->Target;
					S->NotifyStateUpdated();
				}
			});
	}

	/** Resolve a context on the COMMITTED tree (bailout re-checks; propagation is untyped). */
	template <typename T> static const T* ResolveTyped(const TRuiContext<T>& Handle, const FRuiFiber* From);

	TSharedRef<FRuiComponentState> StateShared;
	FRuiComponentState& State;
	FRuiFiber& Fiber;
	FRuiReconciler& Reconciler;
	IRuiHostConfig& Host;
};

// ── template definitions needing FRuiFiber/complete types ────────────────────────────────

template <typename T> const T* FRuiContext::ResolveTyped(const TRuiContext<T>& Handle, const FRuiFiber* From)
{
	for (const FRuiFiber* F = From; F != nullptr; F = F->Parent)
	{
		if (F->ProvidedContext.IsValid())
		{
			if (const TSharedPtr<void>* Found = F->ProvidedContext->Find(Handle.Key()))
			{
				return &static_cast<const TRuiProvidedValue<T>*>(Found->Get())->Value;
			}
		}
	}
	return nullptr;
}
