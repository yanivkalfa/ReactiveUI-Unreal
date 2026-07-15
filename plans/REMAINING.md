# Remaining work — the consolidated backlog

> **What this file is.** Every open work item across the project, in one place, categorized —
> distilled 2026-07-15 from the plan corpus when the completed campaign docs moved to
> `plans/archive/`. Division of labor between the living docs:
> [MASTER_PLAN.md](MASTER_PLAN.md) holds the locked decisions (D-01..D-33) and the phase
> record; [ROADMAP.md](ROADMAP.md) is the plain-English status table;
> [TECH_DEBT.md](TECH_DEBT.md) keeps the per-item deep entries (TD-###) this file references;
> [WIDGET_INVENTORY.md](WIDGET_INVENTORY.md) is the authoritative per-widget tracker.
> **This file is the answer to "what is left?"** — when an item finishes, strike it here
> (and resolve its TD entry / gate row in the same PR).

## 1. v1 ship-gate items (block the 1.0 tag — MASTER_PLAN §3 gate)

- ~~**Localization (FText gathering + culture change)**~~ — ✅ SHIPPED 2026-07-15
  (`feat/v1-gate-closeout`, plugin 0.4.0): verified gather pipeline (`*.inl` masks; reference
  target `Config/Localization/RuiDemo_Gather.ini` + committed output), culture-change → root
  re-render (`RuiCultureSync`), suite `ReactiveUI.Loc`, docs Localization guide.
- ~~**TD-020 tail — embedded-C++ COMPLETION via clangd**~~ — ✅ SHIPPED 2026-07-15 (extensions
  0.2.0): completion forwards over the virtual doc with range translation; hover/definition/
  completion all clangd-backed inside setup/hook/module bodies. TD-020 RESOLVED.
- **Phase 9 — release & publishing (owner-gated).** Per-engine zips (packaging stamps
  `EngineVersion` — release-process skill), Fab listing + upload (identity verification has
  lead time), the demo video (AI storyboards, owner records), Discord announcement, and the
  packaged-fidelity field test (fresh project, packaged plugin, gallery renders — needs UE 5.7
  reinstalled for its leg).

## 2. Epic-interop polish (small, user-facing — audit 2026-07-14 findings)

- ~~**TD-028 — `URuiHostWidget` props/viewmodel channel**~~ — ✅ SHIPPED 2026-07-15
  (`InitialProps` + `ViewModel` UPROPERTYs → `RUI::Umg::UseHostProp`/`UseHostViewModel`;
  suite `ReactiveUI.Umg.HostProps`). TD-028 RESOLVED.
- ~~**TD-029 — `URuiActivatableScreen::GetDesiredFocusTarget()`**~~ — ✅ SHIPPED 2026-07-15
  (`RUI::CommonUI::UseDesiredFocus` + the screen's focus-forwarding overrides; suite
  `ReactiveUI.CommonUI.DesiredFocus`). TD-029 RESOLVED — the real-gamepad PIE pass stays in §3.

## 3. Verification debt (needs eyes/hands, mostly owner)

- **Accessibility screen-reader pass** (audit §9-Q1): the "output is ordinary SWidgets" claim is
  verified for Widget Reflector/styling but the screen-reader path has never been exercised;
  docs copy is deliberately softened until it is.
- **CI engine arming decision** (archive/OWNER_ACCEPTANCE_CHECKLIST_v2 §H): set
  `RUI_CI_ENGINE_ARMED=true` + `EPIC_GHCR_PAT` and verify the armed leg, or consciously stay
  local-battery-only. Unarmed is currently the documented state.
- **PIE-only interactive spot-checks** (carried from checklist v2 — the classes of behavior
  headless can't reach): gamepad/keyboard ComboBox picks, input-method switch + owning-player
  change on a real device (CMU-1), asset-brush GC stress under `obj gc`.
- **Owner setup**: re-add the branch ruleset with THIS repo's check names (the imported one
  carried the Godot repo's and was deleted — ROADMAP status note).
- **Bench figures in README** (audit §9-Q6): 3 of 5 numbers match no committed
  `BENCH_BASELINES.md` row; re-run and commit fresh rows, or correct the README to the
  committed numbers (house rule: printed numbers come only from committed rows).

## 4. Docs build-out (Phase 8 tail)

- **Generated per-hook reference pages**: the per-widget Components catalog is generated from
  the schema; the 23 core hooks + 17 router hooks still have only guide-level coverage
  (ROADMAP row 8 "generated per-widget/per-hook references remain"). A production line
  (template + registry-driven counts) mirrors the widgets one.
- **Demo/gallery decisions** (audit §9-Q5, MASTER_PLAN Phase 8): which of
  todo / 5k-virtualized-inventory / world-space / HUD-over-game are v1-blocking, and the
  marquee-demo ship-or-defer (family precedent: the Godot Doom clone drove perf work AND
  became the demo video).

## 5. Widget & markup surface (v1.x production lines)

- **Batch-3 widgets**: `SSearchableComboBox` (new in the 5.6→5.8 diff) is the seeded candidate;
  WIDGET_INVENTORY.md governs — "a widget with no row is a sweep bug". The UE 5.7/5.8 additive
  FArguments surface recorded there is the same line's feedstock.
- **TD-012 remainder**: runtime setters found by the header sweep and deferred by decision —
  WIDGET_INVENTORY tracks per-widget.
- **Markup `Ref` attribute gap** (found 2026-07-15 during the docs sweep): capturing a host
  handle (portals, focus) requires the C++ props-level `Props.Ref = …` — markup has no `Ref`
  attr, so a pure-markup component cannot target a portal at its own subtree. Decide: grow a
  `Ref={ … }` markup attr (codegen assigns the props field) or keep it a C++-level surface.
- **TD-011 tail**: construct-only prop CHANGES rebuild via `ReplaceWidget`; the reconstruct-mask
  audit for future construct-only props stays a checklist item on the component pipeline.

## 6. Post-v1 by locked decision (tracked, not forgotten — full entries in TECH_DEBT)

- **TD-005** Rider support (skipped entirely for v1 — owner 2026-07-11).
- **TD-007** on-device remote reload (Phase-4-style, over TCP).
- **TD-008** scripting adapters (UnrealSharp/AngelScript — plugin-level, opt-in, never an
  engine fork).
- **TD-HMR-XPLAT** live HMR is Windows-only (Live Coding); Hot Reload as the potential
  cross-platform path.
- **TD-HMR-DEMOS** the `ReactiveUetkx ▸ Demos` launcher submenu.
- **TD-009 / TD-018** cross-repo corpus mirroring PRs into the Godot/Unity repos (process
  items; the corpus itself is hash-gated here).
- **TD-013 tail** typed authoring API follow-ons; **TD-015** deliberate v1 grammar cuts;
  **TD-016** event payload single-`Value` surface; **TD-019** hook-state value migration
  (superseded in practice by HMR v2's whole-project recompile); **TD-026** accepted
  interp-era divergences (record only).

## 7. Open product questions (owner call, from the audit's §9)

- **Q3**: `URuiWorldSubsystem : UWorldSubsystem` vs locked D-17's `UGameInstanceSubsystem` —
  bless the as-built design (banner the decision) or treat as a defect.
- **Q4 remainder**: research-promised APIs never locked — `UseOwnedViewModel`, stale-VM policy,
  a marshaling helper. Backlog or strike from the record. (The two concrete halves became
  TD-028/TD-029 — §2 above.)
