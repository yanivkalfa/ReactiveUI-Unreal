// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TRuiSignal<T> — reactive value stores OUTSIDE the component tree (signal_store.gd /
// signal_registry.gd), plus the UseSignal/UseSignalKey hooks implemented with React's
// useSyncExternalStore discipline (D-08.5): render reads the snapshot; SUBSCRIPTION happens
// in a passive effect with a post-subscribe re-check — restart-safe by construction, which
// fixes the Godot port's subscribe-during-render leak window.

#pragma once

#include "CoreMinimal.h"
#include "RuiContext.h"

/** Type-erased base so the FName registry can hold heterogeneous signals. */
class REACTIVEUICORE_API FRuiSignalBase
{
public:
	virtual ~FRuiSignalBase() = default;
};

template <typename T> class TRuiSignal final : public FRuiSignalBase
{
public:
	explicit TRuiSignal(T Initial = T()) : Value(MoveTemp(Initial)) {}

	const T& Get() const { return Value; }

	/** Set + notify on change. Change detection: operator== (value semantics — C++ arrays/
	 *  maps are values; wrap in TSharedPtr for identity semantics, §5 deps decision). */
	void Set(T NewValue)
	{
		if (Value == NewValue)
		{
			return;
		}
		Value = MoveTemp(NewValue);
		// Copy: a subscriber may unsubscribe during notify (family rule).
		TArray<TPair<int32, TFunction<void()>>> Copy = Subs;
		for (const TPair<int32, TFunction<void()>>& Sub : Copy)
		{
			Sub.Value();
		}
	}

	/** Functional update. */
	void Update(TFunction<T(const T&)> Fn) { Set(Fn(Value)); }

	/** Subscribe; returns an unsubscribe function. */
	TFunction<void()> Subscribe(TFunction<void()> OnChanged)
	{
		const int32 Id = NextSubId++;
		Subs.Emplace(Id, MoveTemp(OnChanged));
		TWeakPtr<int32> Alive = AliveToken;
		return [this, Id, Alive]()
		{
			if (Alive.IsValid()) // signal itself may be gone — the token guards the `this`
			{
				Subs.RemoveAll([Id](const TPair<int32, TFunction<void()>>& S) { return S.Key == Id; });
			}
		};
	}

	int32 NumSubscribers() const { return Subs.Num(); }

private:
	T Value;
	TArray<TPair<int32, TFunction<void()>>> Subs;
	int32 NextSubId = 0;
	TSharedRef<int32> AliveToken = MakeShared<int32>(0);
};

/** Process-wide FName-keyed shared signals (signal_registry.gd). Keyed signals OUTLIVE the
 *  components that read them — that is the point (shared app state). Runtime type check on
 *  key collision (family: the registry is honest about misuse, never silent). */
namespace RUI
{
	REACTIVEUICORE_API TSharedPtr<FRuiSignalBase>* FindOrAddSignalSlot(FName Key);
	REACTIVEUICORE_API TSharedPtr<FRuiSignalBase> TryGetSignal(FName Key);
	REACTIVEUICORE_API bool HasSignal(FName Key);
	/** Drop all keyed signals (subscribers NOT notified) — full session reset. */
	REACTIVEUICORE_API void ClearSignals();

	template <typename T> TSharedRef<TRuiSignal<T>> GetOrCreateSignal(FName Key, T Initial = T())
	{
		TSharedPtr<FRuiSignalBase>* Slot = FindOrAddSignalSlot(Key);
		if (!Slot->IsValid())
		{
			*Slot = MakeShared<TRuiSignal<T>>(MoveTemp(Initial));
		}
		return StaticCastSharedRef<TRuiSignal<T>>(Slot->ToSharedRef());
	}
} // namespace RUI

// ─────────────────────────────────────────────────────────────────────────────────────────
// The signal hooks. Cell holds the SNAPSHOT + the live unsubscribe (dtor releases it —
// teardown-by-destructor, the C++ half of _dispose_fiber_state).
// ─────────────────────────────────────────────────────────────────────────────────────────

template <typename TSelected> struct TRuiSignalCell final : IRuiHookCell
{
	TSelected Value{};
	TFunction<void()> Unsub;
	const void* BoundSignal = nullptr; // identity of the subscribed signal (re-subscribe detection)

	virtual ~TRuiSignalCell() override
	{
		if (Unsub)
		{
			Unsub();
		}
	}
	virtual ERuiHookKind GetKind() const override { return ERuiHookKind::Signal; }
};

namespace RUI
{
	/** UseSignal with selector: re-renders when the SELECTED slice changes. */
	template <typename T, typename TSelected>
	TSelected UseSignal(FRuiContext& Ctx, const TSharedRef<TRuiSignal<T>>& Sig, TFunction<TSelected(const T&)> Selector)
	{
		TRuiSignalCell<TSelected>* Cell = Ctx.template AcquireCell<TRuiSignalCell<TSelected>>(ERuiHookKind::Signal);
		const bool bFirst = (Cell->BoundSignal == nullptr);

		// Render reads the snapshot directly from the store (never stale).
		Cell->Value = Selector(Sig->Get());

		// Subscribe in an EFFECT keyed on the signal's identity; the effect body re-checks
		// the snapshot (the useSyncExternalStore tear-window re-check).
		TWeakPtr<FRuiComponentState> Weak = Ctx.StateWeak();
		const int32 SlotIndex = Ctx.GetState().HookIndex - 1;
		TSharedRef<TRuiSignal<T>> SigCopy = Sig;
		TFunction<TSelected(const T&)> SelCopy = Selector;
		Ctx.InternalUseEffect(
			[Weak, SlotIndex, SigCopy, SelCopy]() -> FRuiEffectCleanup
			{
				auto ReadAndMaybeNotify = [Weak, SlotIndex, SigCopy, SelCopy]()
				{
					TSharedPtr<FRuiComponentState> S = Weak.Pin();
					if (!S.IsValid() || SlotIndex >= S->Hooks.Num())
					{
						return;
					}
					TRuiSignalCell<TSelected>* C = static_cast<TRuiSignalCell<TSelected>*>(S->Hooks[SlotIndex].Get());
					TSelected Now = SelCopy(SigCopy->Get());
					if (!(C->Value == Now))
					{
						C->Value = MoveTemp(Now);
						S->NotifyStateUpdated();
					}
				};

				TSharedPtr<FRuiComponentState> S = Weak.Pin();
				if (!S.IsValid() || SlotIndex >= S->Hooks.Num())
				{
					return FRuiEffectCleanup();
				}
				TRuiSignalCell<TSelected>* C = static_cast<TRuiSignalCell<TSelected>*>(S->Hooks[SlotIndex].Get());
				if (C->Unsub) // re-subscribing (signal instance changed)
				{
					C->Unsub();
				}
				C->BoundSignal = &SigCopy.Get();
				C->Unsub = SigCopy->Subscribe(ReadAndMaybeNotify);
				ReadAndMaybeNotify(); // tear-window re-check: value may have moved between render and effect
				return FRuiEffectCleanup(
					[Weak, SlotIndex]()
					{
						TSharedPtr<FRuiComponentState> S2 = Weak.Pin();
						if (!S2.IsValid() || SlotIndex >= S2->Hooks.Num())
						{
							return;
						}
						TRuiSignalCell<TSelected>* C2 =
							static_cast<TRuiSignalCell<TSelected>*>(S2->Hooks[SlotIndex].Get());
						if (C2->Unsub)
						{
							C2->Unsub();
							C2->Unsub = nullptr;
							C2->BoundSignal = nullptr;
						}
					});
			},
			RUI::Deps(&SigCopy.Get()));
		(void)bFirst;
		return Cell->Value;
	}

	/** UseSignal without selector: the whole value. */
	template <typename T> T UseSignal(FRuiContext& Ctx, const TSharedRef<TRuiSignal<T>>& Sig)
	{
		return UseSignal<T, T>(Ctx, Sig, [](const T& V) { return V; });
	}

	/** Process-wide keyed signal (created lazily; shared by every reader of the key). */
	template <typename T> T UseSignalKey(FRuiContext& Ctx, FName Key, T Initial = T())
	{
		return UseSignal<T>(Ctx, GetOrCreateSignal<T>(Key, MoveTemp(Initial)));
	}
} // namespace RUI
