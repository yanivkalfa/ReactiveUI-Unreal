---
name: field-test-editor
description: The human-in-the-loop bug loop for the real Unreal editor — the AI prepares, fixes, verifies, and applies; the owner tests in a live UE editor; repeat until dead. Use for bugs only reproducible in-editor/PIE, and for packaged-fidelity verification.
---

# Field-test loop for the Unreal editor

You (the AI) do everything except the actual in-editor testing. The human tests, reports, and
decides "fixed" or "persists". Loop until fixed. Production-grade fixes only — root cause, never
a bandaid.

## Environment facts (verify, don't assume, if anything fails)

- **Live tree** (where the owner tests): `C:\Yanivs\GameDev\ReactiveUI\ReactiveUI-Unreal`.
  NEVER edit it while the owner's Unreal editor is open (UE locks module DLLs — your edits +
  rebuild will fail loudly anyway); NEVER kill their editor process.
- **Work tree** (where you develop): create a git worktree (e.g.
  `C:\Yanivs\GameDev\ReactiveUI\RUIU-work`) on first need; branches off `origin/dev`.
- Engine path: per the `test-run` skill's environment facts. ALWAYS redirect engine output to a
  file.
- **Live Coding** (Ctrl+Alt+F11) hot-patches `.cpp` function bodies only. Any header change,
  UPROPERTY/UFUNCTION change, module add/remove, or Build.cs change ⇒ tell the owner to CLOSE
  the editor and rebuild — the analogue of Godot's "plugin scripts don't hot-swap reliably;
  restart".

## The loop

1. **Reproduce & fix** (work tree, feature branch off `origin/dev`): root cause first; extend a
   `ReactiveUI.*` suite to catch it when possible (per-section markers so hangs name their
   culprit).
2. **Verify before handing over**: the dev-process gate ladder — build, `RUICompile -check`
   (Phase 3+), affected suites, `ReactiveUI.Boot`.
3. **Commit** on the feature branch (this loop is a standing ask to commit; author is the owner —
   no Co-Authored-By).
4. **Apply to the live tree**: if it's clean, fetch + checkout the branch (ask before switching
   their checkout); otherwise copy the changed `Plugins/ReactiveUI/**` files over. Then tell the
   owner whether **Live Coding suffices** (cpp-body-only change) **or the editor must restart**
   (anything else — when in doubt, restart).
5. **Owner tests.** Ask for: what they did, what they saw, `Saved/Logs/ReactiveUIUnrealDemo.log`,
   the Message Log "ReactiveUI" page, and any scratch error file they pasted into.
6. **Fixed?** Merge flow per dev-process (PR → dev, owner merges, fast-forward main), changelog +
   bump BEFORE the PR. **Persists?** Back to 1 with the new evidence — never re-try the same
   theory twice; add instrumentation instead.

## Packaged-fidelity test (when the change affects packaging)

Test what a store user gets: `scripts/package-plugin.ps1`, unzip
`ReactiveUI-<ver>-UE<ver>.zip` into a FRESH project's `Plugins/`, open, enable the plugin,
expect the log banner `ReactiveUI <ver> loaded` and the demo/gallery to render. A missing banner
means the plugin is NOT running — silence is never "nothing to do".

## Scar tissue (why these steps exist)

- **Two-tree separation** exists because editing the live tree under an open editor produced
  locked-file half-states in the sibling repo's loop; the worktree makes it structurally
  impossible.
- **"When in doubt, restart"**: Live Coding accepting a patch is not proof the patch is LOADED —
  reinstanced objects with stale layouts produced ghost bugs that vanished on restart; a
  restart is cheaper than a ghost hunt.
- **Ask for the log FILE, not a description** — "it crashed" debugging burned loops until the
  house rule became: no theory without the actual log text.
- **The banner check** is the packaged-fidelity keystone — a broken store zip once looked
  "installed fine" with the plugin silently not loading.
