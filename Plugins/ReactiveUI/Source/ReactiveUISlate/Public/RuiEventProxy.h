// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuiEventProxy — the bind-once-swap-inner event seam (D-11). Slate delegates are bound
// ONCE at widget construction to this per-node proxy (CreateSP with the event's slot index
// as payload); every later render just swaps the slot's inner FRuiCallback. No rebinding,
// and TFunction's missing operator== never matters.
//
// Handler policy (decided Phase 2 step 1, MASTER_PLAN): user callbacks are void-returning;
// FReply-shaped delegates get FReply::Handled() from the proxy. Where pass-through matters
// (mouse/key), a per-slot FReply-returning override can be installed instead — the void
// path stays the default the 15 core widgets ship on.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "RuiTypes.h"
#include "Styling/SlateTypes.h" // ECheckBoxState
#include "Types/SlateEnums.h"	// ETextCommit

class REACTIVEUISLATE_API FRuiEventProxy : public TSharedFromThis<FRuiEventProxy>
{
public:
	/** Swap the slot's inner callback (unbound FRuiCallback clears it). Slot indices are the
	 *  props struct's event bit indices — the adapter owns the mapping. */
	void SetHandler(int32 Slot, FRuiCallback Handler)
	{
		EnsureSlot(Slot);
		Slots[Slot].Handler = MoveTemp(Handler);
	}

	/** Install an FReply-returning override for pass-through events (rare; see policy). */
	void SetReplyHandler(int32 Slot, TFunction<FReply(const FRuiValue&)> Handler)
	{
		EnsureSlot(Slot);
		Slots[Slot].ReplyHandler = MoveTemp(Handler);
	}

	/** Drop every inner callback (release: user closures must not outlive the node). */
	void ClearAll() { Slots.Reset(); }

	// ── bind targets (delegates bind these once, payload = slot index) ────────────────────

	FReply HandleReply(int32 Slot) { return Fire(Slot, FRuiValue()); }
	void HandleVoid(int32 Slot) { Fire(Slot, FRuiValue()); }
	void HandleText(const FText& Value, int32 Slot) { Fire(Slot, FRuiValue(Value)); }
	void HandleTextCommit(const FText& Value, ETextCommit::Type, int32 Slot) { Fire(Slot, FRuiValue(Value)); }
	void HandleChecked(ECheckBoxState Value, int32 Slot) { Fire(Slot, FRuiValue(Value == ECheckBoxState::Checked)); }
	void HandleFloat(float Value, int32 Slot) { Fire(Slot, FRuiValue(Value)); }
	void HandleBool(bool Value, int32 Slot) { Fire(Slot, FRuiValue(Value)); }

private:
	struct FSlot
	{
		FRuiCallback Handler;
		TFunction<FReply(const FRuiValue&)> ReplyHandler;
	};

	void EnsureSlot(int32 Slot)
	{
		checkf(Slot >= 0 && Slot < 64, TEXT("ReactiveUI: event slot index out of range"));
		if (Slots.Num() <= Slot)
		{
			Slots.SetNum(Slot + 1);
		}
	}

	FReply Fire(int32 Slot, const FRuiValue& Value)
	{
		if (Slots.IsValidIndex(Slot))
		{
			if (Slots[Slot].ReplyHandler)
			{
				return Slots[Slot].ReplyHandler(Value);
			}
			Slots[Slot].Handler.Execute(Value);
		}
		return FReply::Handled();
	}

	TArray<FSlot, TInlineAllocator<4>> Slots;
};
