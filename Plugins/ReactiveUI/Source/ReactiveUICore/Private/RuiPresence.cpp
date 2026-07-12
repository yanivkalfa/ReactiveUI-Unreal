// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiPresence.h"

#include "RuiContext.h"
#include "RuiContextHandle.h"
#include "RuiCoreMisc.h"
#include "RuiHostConfig.h"
#include "RuiPropsBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiPresence, Log, All);

// ─────────────────────────────────────────────────────────────────────────────────────────
// The single context handle both the boundary provides on and UsePresence reads. A file-local
// static so the Key() (its Core shared-ptr address) is identical for provide + read.
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	const TRuiContext<FRuiPresenceState>& PresenceContext()
	{
		static const TRuiContext<FRuiPresenceState> Handle(FRuiPresenceState{true, FRuiCallback()},
														   FName(TEXT("RuiPresence")));
		return Handle;
	}

	/** Positional-index fallback key for an unkeyed child (mirrors the reconciler's namespace). */
	FRuiKey KeyForChild(const FRuiNode& Child, int32 Index)
	{
		return Child.Key.IsSet() ? Child.Key : FRuiKey(Index);
	}

	// ── PresenceChild: wraps ONE kept child, owns its bPresent context + exit timeout ──────

	struct FRuiPresenceChildProps final : public FRuiPropsBase
	{
		RUI_PROP(bool, bPresent, 0)
		RUI_PROP(float, MaxExitSeconds, 1)
		RUI_PROP_EVENT(OnExited, 2) // the boundary's "drop my key" — fired by NotifyDone or timeout
		RUI_PROPS_BODY(FRuiPresenceChildProps, RUI_EQ(bPresent) RUI_EQ(MaxExitSeconds))
	};

	/** Self-re-arming host-clock timeout: fires OnExited once elapsed >= MaxSeconds, unless the
	 *  Cancel flag (flipped by the effect cleanup on unmount / re-entry) got set first. */
	void PollExitTimeout(IRuiHostConfig& Host, TSharedRef<bool> Cancel, double StartTime, float MaxSeconds,
						 FRuiCallback OnExited)
	{
		if (*Cancel)
		{
			return;
		}
		if (Host.GetTimeSeconds() - StartTime >= MaxSeconds)
		{
			OnExited.Execute();
			return;
		}
		Host.RequestFrame([&Host, Cancel, StartTime, MaxSeconds, OnExited]()
						  { PollExitTimeout(Host, Cancel, StartTime, MaxSeconds, OnExited); });
	}

	FRuiNodeArray PresenceChildComp(FRuiContext& Ctx, const FRuiPresenceChildProps& Props,
									const TArray<FRuiNode>& Children)
	{
		const bool bPresent = Props.bPresent;
		const FRuiCallback OnExited = Props.OnExited;

		// Once-guard so NotifyDone AND the timeout can't double-fire the drop. Reset whenever the
		// child is present again (a re-entry re-arms a future exit).
		TSharedRef<TRuiRef<bool>> DoneRef = Ctx.UseRef<bool>(false);
		if (bPresent)
		{
			DoneRef->Current = false;
		}

		// Stable identity (so the provided context only "changes" when bPresent flips, not every
		// render) but the body — and thus the captured latest OnExited — refreshes each render.
		FRuiCallback NotifyDone = Ctx.UseStableCallback(
			[DoneRef, OnExited]()
			{
				if (DoneRef->Current)
				{
					return;
				}
				DoneRef->Current = true;
				OnExited.Execute();
			});

		Ctx.ProvideContext(PresenceContext(), FRuiPresenceState{bPresent, NotifyDone});

		// Timeout fence: while exiting, force the drop after MaxExitSeconds if nobody notifies.
		IRuiHostConfig* Host = &Ctx.GetHost();
		const float MaxExit = Props.MaxExitSeconds;
		Ctx.UseEffect(
			[bPresent, Host, MaxExit, OnExited]() -> FRuiEffectCleanup
			{
				if (bPresent)
				{
					return FRuiEffectCleanup();
				}
				TSharedRef<bool> Cancel = MakeShared<bool>(false);
				PollExitTimeout(*Host, Cancel, Host->GetTimeSeconds(), MaxExit, OnExited);
				return [Cancel]() { *Cancel = true; };
			},
			RUI::Deps(bPresent ? 0 : 1));

		return Children; // render the wrapped child unchanged (context reaches it as our descendant)
	}
	RUI_COMPONENT(PresenceChildComp)

	// ── Presence: the boundary that remembers exiting keys and keeps them mounted ──────────

	struct FRuiPresenceProps final : public FRuiPropsBase
	{
		RUI_PROP(float, MaxExitSeconds, 0)
		RUI_PROPS_BODY(FRuiPresenceProps, RUI_EQ(MaxExitSeconds))
	};

	/** One remembered child: its key, its last-seen vnode, and whether it is currently leaving. */
	struct FPresenceSlot
	{
		FRuiKey Key;
		FRuiNode Vnode;
		bool bExiting = false;
	};

	FRuiNodeArray PresenceComp(FRuiContext& Ctx, const FRuiPresenceProps& Props, const TArray<FRuiNode>& Children)
	{
		TSharedRef<TRuiRef<TArray<FPresenceSlot>>> SlotsRef = Ctx.UseRef<TArray<FPresenceSlot>>();
		// A version bump is the ONLY reason Presence itself re-renders (a completed exit drops a
		// key); entering an exit is driven by the PARENT re-rendering with new children.
		TTuple<int32, TRuiSetter<int32>> Version = Ctx.UseState<int32>(0);
		const TRuiSetter<int32>& SetVersion = Version.Value;
		// Unconditional (rules of hooks): a one-shot latch for the unkeyed-child warning below.
		TSharedRef<TRuiRef<bool>> Warned = Ctx.UseRef<bool>(false);

		// Incoming key -> its index in Children (insertion order preserved).
		TMap<FRuiKey, int32> IncomingIndex;
		IncomingIndex.Reserve(Children.Num());
		bool bAnyUnkeyed = false;
		for (int32 i = 0; i < Children.Num(); ++i)
		{
			if (!Children[i].Key.IsSet())
			{
				bAnyUnkeyed = true;
			}
			IncomingIndex.Add(KeyForChild(Children[i], i), i);
		}
		if (bAnyUnkeyed && !Warned->Current)
		{
			Warned->Current = true;
			UE_LOG(LogRuiPresence, Warning,
				   TEXT("RUI::Presence child without a key: exit tracking falls back to position. "
						"Key every direct child of <Presence>."));
		}

		TArray<FPresenceSlot>& Slots = SlotsRef->Current;
		TArray<FPresenceSlot> Next;
		Next.Reserve(FMath::Max(Slots.Num(), Children.Num()));
		TSet<FRuiKey> Emitted;

		// 1. Existing slots keep their order. Present -> refresh vnode + clear exiting (this also
		//    CANCELS an in-flight exit on re-entry). Absent -> mark exiting, keep the old vnode.
		for (FPresenceSlot& Slot : Slots)
		{
			if (const int32* Idx = IncomingIndex.Find(Slot.Key))
			{
				Slot.Vnode = Children[*Idx];
				Slot.bExiting = false;
			}
			else
			{
				Slot.bExiting = true;
			}
			Emitted.Add(Slot.Key);
			Next.Add(Slot);
		}
		// 2. Brand-new incoming keys append in incoming order.
		for (int32 i = 0; i < Children.Num(); ++i)
		{
			const FRuiKey Key = KeyForChild(Children[i], i);
			if (!Emitted.Contains(Key))
			{
				Emitted.Add(Key);
				Next.Add(FPresenceSlot{Key, Children[i], false});
			}
		}
		Slots = MoveTemp(Next);

		// 3. Render each slot through a PresenceChild wrapper (keyed by the child's key so the
		//    fiber — and the child's tween state — is preserved for the whole enter/exit/re-entry).
		const float MaxExit = Props.MaxExitSeconds;
		FRuiNodeArray Out;
		Out.Reserve(Slots.Num());
		for (const FPresenceSlot& Slot : Slots)
		{
			const FRuiKey Key = Slot.Key;
			FRuiPresenceChildProps ChildProps;
			ChildProps.SetbPresent(!Slot.bExiting);
			ChildProps.SetMaxExitSeconds(MaxExit);
			ChildProps.SetOnExited(FRuiCallback::Create(
				[SlotsRef, SetVersion, Key]()
				{
					TArray<FPresenceSlot>& Live = SlotsRef->Current;
					const int32 Removed = Live.RemoveAll([&Key](const FPresenceSlot& S) { return S.Key == Key; });
					if (Removed > 0)
					{
						SetVersion([](const int32& V) { return V + 1; });
					}
				}));

			Out.Add(RUI::FC<FRuiPresenceChildProps>(&PresenceChildComp, MoveTemp(ChildProps),
													TArray<FRuiNode>{Slot.Vnode}, Key));
		}
		return Out;
	}
	RUI_COMPONENT(PresenceComp)
} // namespace

FRuiNode RUI::Presence(TArray<FRuiNode> Children, float MaxExitSeconds, FRuiKey Key)
{
	FRuiPresenceProps Props;
	Props.SetMaxExitSeconds(MaxExitSeconds);
	return RUI::FC<FRuiPresenceProps>(&PresenceComp, MoveTemp(Props), MoveTemp(Children), Key);
}

FRuiPresenceState UsePresence(FRuiContext& Ctx)
{
	return Ctx.UseContext(PresenceContext());
}
