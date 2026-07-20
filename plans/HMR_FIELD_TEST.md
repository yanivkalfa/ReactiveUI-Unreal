# HMR field test — the pre-release protocol (HMR v2, Live-Coding-driven)

> **Why this file:** `OWNER_ACCEPTANCE_CHECKLIST_v2.md` §A describes the DELETED HMR v1
> (interpreter overrides, "compiled→interp swap") — do not test against it. HMR v2
> (plans/archive/HMR_V2_PLAN.md, `FUetkxHmrController`) is a Start/Stop MODE: while active,
> any `.uetkx` save regenerates the `.inl`, triggers a Live Coding compile, and on
> patch-complete every live reconciler root re-renders with the freshly-patched COMPILED
> code — full fidelity (imports, hooks, effects), state preserved (hook cells are
> heap-resident; a code patch never touches them). No interpreter, no per-edit branching.
>
> Owner-only: Live Coding needs a running editor + compiler — no headless test can force
> the patch event. Everything below the patch (watcher codegen, debounce, diagnostics) is
> already suite-covered.

## Pre-flight (once, before the session)

- [ ] Baseline green on the fix branch (last verified 2026-07-21, `cab7427`):
      Development Editor build ✓, `RUICompile -check` → 42 files / 0 errors ✓, suites ✓.
- [ ] **Close any external Build.bat loops** — while HMR is active, Live Coding owns the
      compile; external builds pause (the documented tradeoff, by design).
- [ ] Editor open, gallery map loaded, **PIE running**, VS Code (dev-host or installed
      extension) on the same checkout.

## Start / stop

- Window: **Window ▸ ReactiveUetkx Hot Reload** (nomad tab) — the Start/Stop panel; a
  checkbox hides the external Live Coding console window while HMR is active (an engine
  window we can't parent — hidden via SW_HIDE, restored on Stop).
- Console alternative: `ReactiveUetkx.HMR.Start` / `.Stop`.
- Status lines print to **MessageLog ▸ "ReactiveUI"**.

## The matrix (~15 min)

- [ ] **1. Plain edit, state preserved.** In PIE, drive `SimpleCounter` (or `ClickCounter`)
      to a non-default count. Edit visible text/color in its `.uetkx`, save. ✅ UI updates
      after the patch; the count SURVIVES. (Hook cells heap-resident — the core v2 claim.)
- [ ] **2. Hook-order-sensitive state.** Component with `UseMemo`/`UseRef` before two
      `UseState`s; drive both states; save a cosmetic edit. ✅ Both states keep their
      values, no cross-seeding. (The v1 HMR-1 ordinal bug's modern equivalent — should be
      a non-event now; state is untouched, not migrated.)
- [ ] **3. Cross-file: edit an IMPORTED component.** Edit `LabCard`/`DemoContextPanel`
      (something imported by an open screen), save. ✅ Every screen using it re-renders
      patched; importing screens' state survives.
- [ ] **4. Structural edit.** Add/remove a widget in the tree (not just a prop), save.
      ✅ Patch lands; the changed subtree remounts (its OWN local state resets — expected
      React semantics for a structural change); siblings/ancestors keep state.
- [ ] **5. Broken save while HMR is active (the R10–R14 gates in the loop).** Save with a
      typo'd enum value (`Slot.HAlign="cesnter"`), a wrong-cased key (`slot.fill`), and a
      butchered local's usage. ✅ Compile FAILS loudly: errors in MessageLog "ReactiveUI"
      (0106 / 0112 / C++), the running UI keeps the LAST GOOD patch, VS Code shows the
      same diagnostics live. ⚠ Known behavior: the watcher deletes the `.uetkx.inl` on a
      failed markup compile — the next GOOD save regenerates it; do not be alarmed by the
      transient deletion (never commit in that window).
- [ ] **6. Fix the break, save again.** ✅ Patch lands, UI resumes from the same state —
      the error round-trip costs nothing.
- [ ] **7. Rapid saves (debounce).** Save 3–4 edits in quick succession. ✅ One coherent
      final patch (the watcher debounces); no half-applied UI.
- [ ] **8. Stop → external build → restart.** Stop HMR, run the normal Build.bat, restart
      HMR. ✅ Both directions work; Stop restores the Live Coding console visibility.
- [ ] **9. PIE stop/start under HMR.** Stop PIE, start PIE again with HMR still active,
      save an edit. ✅ Fresh session patches normally.

## What "done" means

All nine boxes green in one session ⇒ HMR v2 is field-verified; note the session (date,
engine version) in `plans/PENDING_CHANGELOG.md`'s next drain and proceed to
`release-process` (§0: the pending ledger currently holds the round 10–15 bullets).

## If something fails

Ledger it in `plans/F5_FIELD_TEST_BUGS.md` (the round pattern: symptom → root cause →
fix → pins) — same loop as rounds 1–15. MessageLog "ReactiveUI" + `LogRuiEditor` in the
Output Log are the first evidence to capture.
