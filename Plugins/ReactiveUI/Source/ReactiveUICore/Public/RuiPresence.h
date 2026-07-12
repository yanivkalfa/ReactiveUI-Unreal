// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-003 — the exit-animation (delayed-unmount) protocol. Enter animations are trivial
// (mount -> UseAnimate 0->1); exits are not, because by the time a component knows it should
// animate OUT the reconciler has already decided to delete its fiber. `<Presence>` solves this
// the React-community way (framer-motion's AnimatePresence): a boundary component that KEEPS an
// exiting keyed child mounted, flips a `bPresent = false` signal into it via context, and does
// the REAL unmount only once the child calls NotifyDone() (or a MaxExitSeconds timeout fires).
//
// This is a pure userland composition over the existing hooks (UseState/UseRef/UseEffect/
// UseContext) — NOT reconciler surgery. The state-preservation the design doc wanted from a
// reconciler-level "exiting set" falls out for free: because Presence keeps rendering the same
// keyed child, the reconciler keeps the SAME fiber, so the child's tween state survives the exit
// (and re-entry cancels it, continuing from the current value). Portable to the Unity/Godot
// siblings as-is (EXIT_ANIMATION_DESIGN.md).

#pragma once

#include "CoreMinimal.h"
#include "RuiTypes.h"
#include "RuiNode.h"

class FRuiContext;

/**
 * What UsePresence() returns, and the value a <Presence> boundary provides to each child:
 *   bPresent  — false once this child has been removed from the parent's render but is being
 *               kept alive to animate out (true otherwise, including a cancelled re-entry).
 *   NotifyDone — call it when the exit animation has settled; the boundary then really unmounts.
 * Outside any <Presence> the default is { true, unbound } — a plain component is always present
 * and NotifyDone is a quiet no-op, so UsePresence is always safe to call.
 */
struct REACTIVEUICORE_API FRuiPresenceState
{
	bool bPresent = true;
	FRuiCallback NotifyDone;

	bool operator==(const FRuiPresenceState& Other) const
	{
		return bPresent == Other.bPresent && NotifyDone == Other.NotifyDone;
	}
	bool operator!=(const FRuiPresenceState& Other) const { return !(*this == Other); }
};

namespace RUI
{
	/**
	 * Keep exiting keyed children mounted until they animate out. Children MUST be keyed (a
	 * child's identity is how the boundary tells "removed" from "reordered"); an unkeyed child
	 * falls back to its positional index with a dev warning. `MaxExitSeconds` force-unmounts a
	 * child that never calls NotifyDone — a missing NotifyDone costs an animation, not a leak.
	 */
	REACTIVEUICORE_API FRuiNode Presence(TArray<FRuiNode> Children, float MaxExitSeconds = 2.0f,
										 FRuiKey Key = FRuiKey());
} // namespace RUI

/** Read the nearest <Presence> boundary's signal for THIS child. Safe anywhere (see default). */
REACTIVEUICORE_API FRuiPresenceState UsePresence(FRuiContext& Ctx);
