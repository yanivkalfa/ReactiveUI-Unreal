# Exit-animation protocol (delayed unmount) — design

> Status: **IMPLEMENTED** (TD-003, 2026-07-12) — `RuiPresence.h/.cpp` in ReactiveUICore, test
> `ReactiveUI.Core.Presence`. Family context: neither sibling ships exit animations in v1; all
> three need the same protocol, so the design is written once here and reviewed cross-family
> before any implementation.
>
> **Implementation note (chosen over the reconciler-diversion sketch below):** shipped as a pure
> userland composition — a `Presence` function component that remembers exiting keyed children in
> its own state and keeps rendering them, plus a `PresenceChild` wrapper that provides the
> per-child `{bPresent, NotifyDone}` context and owns the timeout fence. This is the
> React-community shape (framer-motion's `AnimatePresence` is itself a component, not a renderer
> patch). It needs ZERO reconciler surgery: state preservation falls out because the boundary
> keeps rendering the same keyed child, so the reconciler keeps the same fiber (and its tween
> cell). The one genuine core change it forced was a latent-bug fix, not a feature hook:
> `ScheduleUpdateOnFiber` now marks alternate twins (React `markUpdateLaneFromFiberToRoot`
> parity) so an async setState reaching a bailed-out intermediate isn't skipped. The
> reconciler-touch-points section below is retained as the ALTERNATIVE that was considered and
> rejected for higher core risk.

## Problem

Enter animations are trivial with `UseAnimate` (mount → animate 0→1). Exit animations are
not: by the time a component knows it should animate OUT, the reconciler has already decided
to DELETE its fiber — the widget is gone before any animation could play.

## The protocol (React-community shape: "presence")

1. **A presence boundary element** `<Presence>` (core, host-agnostic): its CHILDREN are keyed;
   when a keyed child disappears from the parent's render output, the boundary KEEPS the old
   fiber mounted ("exiting" state) instead of deleting it.
2. **The exiting signal** reaches the kept child through context: `UseAnimate`'s driver value
   flips to `bIn = false` via a boundary-provided context (`UsePresence()` → `{ bPresent,
   NotifyDone }`). The child's existing `UseAnimate(bPresent)` drives opacity/translation
   through the style keys it already uses.
3. **Completion**: the child calls `NotifyDone()` when its exit animation settles (UseAnimate
   already knows: progress reached 0 with `bIn == false`). The boundary then performs the
   REAL deletion (the deferred reconciler delete).
4. **Timeout fence**: a boundary-level `MaxExitSeconds` (default 2s) force-deletes children
   that never notify — a missing NotifyDone must cost an animation, not a leak.
5. **Re-entry**: a key REAPPEARING while its old fiber is still exiting CANCELS the exit
   (bPresent flips true; the same fiber continues — state preserved). This is the fiddly
   case; the keyed-diff pass must check the exiting set before minting a new fiber.

## Reconciler touch points (why this is a protocol, not a widget feature)

- Child reconciliation: deletions of keyed children under a Presence fiber divert into the
  boundary's exiting set instead of the deletion effect list.
- The exiting set re-renders with `bPresent = false` context each pass (bailout defeated for
  those fibers only).
- `NotifyDone`/timeout moves the fiber into the normal deletion path (cleanups/refs/host
  release — the existing teardown machinery unchanged).

## Non-goals (v1 of the feature)

- No FLIP/shared-element transitions.
- No automatic style capture — the child animates itself via `UseAnimate` + style keys.
- No cross-boundary coordination (staggered lists ride a `Delay` prop on the child).

## Cross-family notes

- Unity/Godot reconcile children in the same mark-and-sweep shape; the boundary diverting
  keyed deletions is portable as-is.
- `UseAnimate` exists in all three dialects (Unreal's is host-clock driven as of Phase 7).
