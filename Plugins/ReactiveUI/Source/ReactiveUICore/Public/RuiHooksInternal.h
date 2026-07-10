// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Hook storage internals: dependency arrays, type-erased hook cells, setter handles.
// Ports hooks.gd's positional-slot model — one array of cells with THREE separate cursors
// (state hooks / passive effects / layout effects), so effect insertion can never shift
// state slots. \internal — public only so the host-project test suites can introspect.

#pragma once

#include "CoreMinimal.h"
#include "RuiTypes.h"

class FRuiComponentState;

// ─────────────────────────────────────────────────────────────────────────────────────────
// Dependencies (§5 decision, replacing the family's per-language equality quirks):
// VALUE equality for value kinds, IDENTITY for shared refs. Godot deep-compares deps
// ([G-09]); React identity-compares each. C++ deps are built explicitly via RUI::Deps(...),
// so the kind split is visible at the call site instead of implicit in the language.
// ─────────────────────────────────────────────────────────────────────────────────────────

struct FRuiDep
{
	bool bIdentity = false;
	const void* Ptr = nullptr; // identity kind
	FRuiValue Value;           // value kind

	bool operator==(const FRuiDep& Other) const
	{
		if (bIdentity != Other.bIdentity)
		{
			return false;
		}
		return bIdentity ? Ptr == Other.Ptr : Value == Other.Value;
	}
};

/** TOptional-wrapped: unset = "no deps" = run every commit (family: deps == null).
 *  An EMPTY set array = mount-only (family: []). */
using FRuiDeps = TOptional<TArray<FRuiDep>>;

namespace RUI
{
	namespace Private
	{
		template <typename T> struct TIsSharedPtrLike : std::false_type {};
		template <typename T, ESPMode M> struct TIsSharedPtrLike<TSharedPtr<T, M>> : std::true_type {};
		template <typename T, ESPMode M> struct TIsSharedPtrLike<TSharedRef<T, M>> : std::true_type {};

		/** Single template + if-constexpr dispatch: overload sets recursing into each other
		 *  trip C++ two-phase lookup (an overload can't see ones declared after it). */
		template <typename... TArgs>
		void AddDep(TArray<FRuiDep>& Out, const TArgs&... Args)
		{
			(
				[&Out](const auto& Head)
				{
					using THead = std::decay_t<decltype(Head)>;
					FRuiDep D;
					if constexpr (TIsSharedPtrLike<THead>::value)
					{
						D.bIdentity = true;
						D.Ptr = &*Head; // TSharedPtr::Get() / TSharedRef deref — object identity
					}
					else if constexpr (std::is_same_v<THead, const TCHAR*> || std::is_same_v<THead, TCHAR*>)
					{
						// String literals are VALUES, not identities (address compare would be
						// a per-TU footgun).
						D.Value = FRuiValue(Head);
					}
					else if constexpr (std::is_pointer_v<THead>)
					{
						D.bIdentity = true;
						D.Ptr = Head;
					}
					else
					{
						D.Value = FRuiValue(Head); // value kinds via FRuiValue's constructors
					}
					Out.Add(D);
				}(Args),
				...
			);
		}
	}

	/** Build a deps array: RUI::Deps(Count, Name, SomeSharedPtr). Empty call = mount-only. */
	template <typename... TArgs>
	FRuiDeps Deps(const TArgs&... Args)
	{
		TArray<FRuiDep> Out;
		Out.Reserve(sizeof...(Args));
		Private::AddDep(Out, Args...);
		return FRuiDeps(MoveTemp(Out));
	}

	/** No-deps sentinel: the effect runs every commit (family deps == null). */
	inline FRuiDeps EveryCommit() { return FRuiDeps(); }

	/** Shallow deps comparison (family _deps_changed): unset on either side => changed. */
	REACTIVEUICORE_API bool DepsChanged(const FRuiDeps& Prev, const FRuiDeps& Next);
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Hook cells
// ─────────────────────────────────────────────────────────────────────────────────────────

/** Hook kinds — doubles as the hook-order validation signature alphabet (hooks.gd _record).
 *  Effect/LayoutEffect don't consume state slots (own cursors) but ARE order-relevant, so
 *  they log too. */
enum class ERuiHookKind : uint8
{
	State,
	Reducer,
	Ref,
	Memo,
	Deferred,
	Transition,
	Stable,
	SafeArea,
	Signal,
	Tween,
	TweenValue,
	Animate,
	Sfx,
	Effect,
	LayoutEffect,
};

REACTIVEUICORE_API const TCHAR* RuiHookKindName(ERuiHookKind Kind);

/** Type-erased hook slot. Destructors double as teardown (a signal cell's dtor
 *  unsubscribes) — the C++ answer to _dispose_fiber_state's explicit unsub pass. */
struct IRuiHookCell
{
	virtual ~IRuiHookCell() = default;
	virtual ERuiHookKind GetKind() const = 0;
};

template <typename T>
struct TRuiStateCell final : IRuiHookCell
{
	T Value;
	explicit TRuiStateCell(T InValue) : Value(MoveTemp(InValue)) {}
	virtual ERuiHookKind GetKind() const override { return ERuiHookKind::State; }
};

template <typename T, typename TAction>
struct TRuiReducerCell final : IRuiHookCell
{
	T Value;
	TFunction<T(const T&, const TAction&)> Reducer; // refreshed every render (family parity)
	explicit TRuiReducerCell(T InValue) : Value(MoveTemp(InValue)) {}
	virtual ERuiHookKind GetKind() const override { return ERuiHookKind::Reducer; }
};

/** UseRef box — stable across renders; mutating Current never re-renders. */
template <typename T>
struct TRuiRef
{
	T Current{};
};

template <typename T>
struct TRuiRefCell final : IRuiHookCell
{
	TSharedRef<TRuiRef<T>> Box;
	explicit TRuiRefCell(T Initial) : Box(MakeShared<TRuiRef<T>>()) { Box->Current = MoveTemp(Initial); }
	virtual ERuiHookKind GetKind() const override { return ERuiHookKind::Ref; }
};

template <typename T>
struct TRuiMemoCell final : IRuiHookCell
{
	T Value;
	FRuiDeps LastDeps;
	virtual ERuiHookKind GetKind() const override { return ERuiHookKind::Memo; }
};

template <typename T>
struct TRuiDeferredCell final : IRuiHookCell
{
	T Value{};
	T Target{};
	FRuiDeps Deps;
	bool bPending = false;
	virtual ERuiHookKind GetKind() const override { return ERuiHookKind::Deferred; }
};

struct FRuiTransitionCell final : IRuiHookCell
{
	virtual ERuiHookKind GetKind() const override { return ERuiHookKind::Transition; }
};

/** Stable-callback cell: the WRAPPER's identity never changes; the inner body is refreshed
 *  every render (useStableCallback/Func/Action). */
struct FRuiStableCell final : IRuiHookCell
{
	TSharedRef<TFunction<void(const FRuiValue&)>> Inner = MakeShared<TFunction<void(const FRuiValue&)>>();
	FRuiCallback Wrapper; // minted once, reads Inner
	virtual ERuiHookKind GetKind() const override { return ERuiHookKind::Stable; }
};

/** Order-validation-only cell (UseSafeArea and the Phase-7-owned stubs record their slot). */
struct FRuiMarkerCell final : IRuiHookCell
{
	ERuiHookKind Kind;
	bool bWarned = false; // warn-once for not-yet-wired stubs
	explicit FRuiMarkerCell(ERuiHookKind InKind) : Kind(InKind) {}
	virtual ERuiHookKind GetKind() const override { return Kind; }
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Effects (recorded during render, run during commit — reconciler drives)
// ─────────────────────────────────────────────────────────────────────────────────────────

using FRuiEffectCleanup = TFunction<void()>;

struct FRuiEffect
{
	TFunction<FRuiEffectCleanup()> Factory;
	FRuiDeps Deps;      // this render's deps
	FRuiDeps LastDeps;  // deps at last run (unset = never ran)
	FRuiEffectCleanup Cleanup;
	bool bEverRan = false;
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Setter handle
// ─────────────────────────────────────────────────────────────────────────────────────────

/**
 * The value+setter pair's setter half: a small copyable handle {weak state, slot}. Safe
 * after teardown (weak + slot-count guard — the family's [audit C3]); stable identity ==
 * the (state, slot) pair, so passing setters as props stays memo-friendly.
 * Implementation lives with FRuiComponentState (needs its definition).
 */
template <typename T>
class TRuiSetter
{
public:
	TRuiSetter() = default;
	TRuiSetter(TWeakPtr<FRuiComponentState> InState, int32 InSlot) : State(MoveTemp(InState)), Slot(InSlot) {}

	void operator()(T NewValue) const;                       // set value
	void operator()(TFunction<T(const T&)> Updater) const;   // functional update

	bool operator==(const TRuiSetter& Other) const
	{
		return Slot == Other.Slot && State == Other.State;
	}

private:
	TWeakPtr<FRuiComponentState> State;
	int32 Slot = INDEX_NONE;
};
