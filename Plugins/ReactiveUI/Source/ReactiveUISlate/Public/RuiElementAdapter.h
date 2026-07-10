// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// IRuiElementAdapter + the adapter registry (D-11) — one adapter per wrapped Slate widget
// type, keyed by the interned element type id. The Unity ITypedElementAdapter shape: the
// host stays generic; everything widget-specific (SNew args, setter rows, event tables,
// child mechanics, slot props) lives in the adapter.
//
// Contract (templates/widget_wrapper.template.cpp is the normative fill-in shape):
// - CreateWidget consumes construct-only args; the HOST then calls ApplyDiff(W, null, Props)
//   so full-apply and diff-apply are one code path.
// - ApplyDiff is memberwise compare-and-set (no hashing, no reflection). Attributes receive
//   CONSTANT values via engine setters, never delegate TAttributes (D-12). Self-notifying
//   setters skip when equal (D-16).
// - GetReconstructMask: construct-only prop bits — a diff touching them requires widget
//   replacement. Hot-path style keys must never appear here (D-13).
// - Events bind ONCE at construction to the node's FRuiEventProxy; SyncEventHandlers swaps
//   the proxy's inner callbacks every commit (bind-once-swap-inner).
// - Child ops run on the PARENT's adapter (panel APIs differ per widget).

#pragma once

#include "CoreMinimal.h"
#include "RuiElementRegistry.h"
#include "RuiPropsBase.h"
#include "RuiTypes.h"
#include "Widgets/SWidget.h"

class FRuiEventProxy;

enum class ERuiChildKind : uint8
{
	Leaf,		   // no children (warn if the tree declares any)
	SingleContent, // one content slot (SetContent; warn_capacity — extra children warn, last wins)
	MultiSlot,	   // a panel (Insert/Remove/Reorder + slot.* props)
};

class REACTIVEUISLATE_API IRuiElementAdapter
{
public:
	virtual ~IRuiElementAdapter() = default;

	virtual ERuiChildKind GetChildKind() const = 0;

	/** SNew with construct-only args, and bind every event delegate ONCE — to the proxy via
	 *  CreateSP + slot-index payload (many Slate events are construct-time SLATE_EVENT args,
	 *  which is why the proxy arrives here; it is non-null iff HasEvents()). The host follows
	 *  with ApplyDiff(W, nullptr, Props). */
	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>& Proxy) = 0;

	/** Memberwise compare-and-set. Old == nullptr means "apply everything set" (fresh widget
	 *  or pool reuse without stashed props). */
	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) = 0;

	/** Construct-only prop bits (see class comment). Default: none. */
	virtual uint64 GetReconstructMask() const { return 0; }

	/** True when the adapter binds any events — the host mints a proxy only when needed. */
	virtual bool HasEvents() const { return false; }

	/** Swap the proxy's inner callbacks from the new props (every commit; cheap). */
	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) {}

	// ── children (MultiSlot) ──────────────────────────────────────────────────────────────

	/** Insert Child at Index (Index < 0 = append), applying the child's slot.* props. */
	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32 Index,
							 const FRuiStyleDict* SlotProps)
	{
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) {}

	/** Enforce the exact child order (structural frames only). Minimal-move preferred. */
	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf)
	{
	}

	/** Re-apply a child's slot.* props after they changed (v1 may remove+reinsert). */
	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child, const FRuiStyleDict* SlotProps)
	{
	}

	// ── children (SingleContent) ──────────────────────────────────────────────────────────

	/** Set/replace the single content (null clears to SNullWidget). */
	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) {}
};

namespace RUI::Slate
{
	/** Register an adapter for an element type. Re-registration replaces (Live Coding). */
	REACTIVEUISLATE_API void RegisterAdapter(FRuiElementTypeId Type, TUniquePtr<IRuiElementAdapter> Adapter);

	/** Convenience: intern the tag name and register in one call; returns the id. */
	REACTIVEUISLATE_API FRuiElementTypeId RegisterAdapter(FName TagName, TUniquePtr<IRuiElementAdapter> Adapter);

	REACTIVEUISLATE_API IRuiElementAdapter* FindAdapter(FRuiElementTypeId Type);
} // namespace RUI::Slate
