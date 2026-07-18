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
- ~~**Widget completion (owner re-decision 2026-07-16)**~~ — ✅ **COMPLETE 2026-07-16**
  (waves 0–4 + G, 0.5.0 → 0.9.0); see §5 and
  [WIDGET_COMPLETION_PLAN.md](archive/WIDGET_COMPLETION_PLAN.md).
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

- **Accessibility screen-reader pass** (audit §9-Q1) — **DEFERRED by owner 2026-07-15** (no
  screen reader available). Docs copy stays softened to what is verified until a pass runs;
  revisit at/after v1.
- **CI engine arming decision** (archive/OWNER_ACCEPTANCE_CHECKLIST_v2 §H): set
  `RUI_CI_ENGINE_ARMED=true` + `EPIC_GHCR_PAT` and verify the armed leg, or consciously stay
  local-battery-only. Unarmed is currently the documented state.
- **PIE-only interactive spot-checks** (carried from checklist v2 — the classes of behavior
  headless can't reach): gamepad/keyboard ComboBox picks, input-method switch + owning-player
  change on a real device (CMU-1), asset-brush GC stress under `obj gc`.
- **Owner setup**: re-add the branch ruleset with THIS repo's check names (the imported one
  carried the Godot repo's and was deleted — ROADMAP status note).
- ~~**Bench figures in README**~~ (audit §9-Q6) — ✅ RESOLVED: the audit campaign had already
  re-pointed the README at committed rows; 2026-07-15 re-ran the bench, committed fresh
  baseline rows, and the README cites them.

## 4. Docs build-out (Phase 8 tail)

- ~~**Generated per-hook reference pages**~~ — ✅ SHIPPED 2026-07-15: hooksCatalog.ts
  (23 core + 17 router entries authored from the real headers) renders one reference page
  per hook (sidebar sections "Hooks" / "Router Hooks"); docs-drift compares the catalog
  counts against RuiContext.h/RuiRouter.h (8 checks armed).
- ~~**Doom demo**~~ — ✅ BUILT 2026-07-15 (`feat/doom-demo`): playable, 182-check
  determinism suite green, whole-frame bench ~197 µs (archive/DOOM_DEMO_PLAN.md has the record).
  PIE playtest DONE 2026-07-16 (3 rounds; surfaced + fixed the ColorAndOpacity/Clipping/
  ScaleBox-alignment library gaps and the raw-mouse input feel). Remaining: the demo video +
  showcase copy; optional BSP upgrade stays deferred (plan Phase 7).
- **Remaining demo decisions** (audit §9-Q5): which of todo / 5k-virtualized-inventory /
  world-space are v1-blocking vs v1.x gallery additions.

## 5. Widget & markup surface — **NOW v1-BLOCKING (owner re-decision 2026-07-16)**

- ~~**Full widget coverage + the TD-012 setter remainder moved INTO the v1 gate**~~ —
  ✅ **PLAN COMPLETE 2026-07-16**: waves 0–4 + G all shipped (0.5.0 → 0.9.0) — 63 markup
  tags / 65+ wrapped widgets, protocols P1–P5, TD-011/TD-013 gates in CI, TD-012 drained,
  TD-022 closed (TreeView/HeaderRow), wave G grammar (early returns + short-circuit;
  UETKX3002 retired). The 1.0 gate's widget-completion clauses are MET; see
  [WIDGET_COMPLETION_PLAN.md](archive/WIDGET_COMPLETION_PLAN.md) + WIDGET_INVENTORY.md Shipped tables.
  Follow-on process item: outbound corpus-sync PRs to the Godot/Unity repos (TD-009/TD-018).
- ~~**Markup `Ref` attribute gap**~~ — ✅ SHIPPED 2026-07-15 (owner decision): `Ref={ expr }`
  is a universal reserved attribute (codegen assigns `Props.Ref`; expr-only, string form
  diagnosed); LSP completion/hover, contract fixture `RefCapture`, AcceptanceLab §9 live
  proof, Portals/CommonUI docs now show the markup form.
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
- **Combined imports vs the LEGACY hoist codemod**: `-run=RUIMigrateImports`'s hoisted-module
  pass (RUIMigrateImportsCommandlet ~L877) deliberately skips a COMBINED import's named part
  when converting `{ M }` -> `import * as M` — combined syntax postdates every legacy `module`
  tree, and the ES codemod's zero-diagnostics compile gate catches the unconverted import
  rather than silently corrupting it. Revisit only if a field report shows a combined import
  naming a legacy hoisted module (then: reprint the combined head in the conversion).

## 7. Open product questions (owner call, from the audit's §9)

- **Q3**: `URuiWorldSubsystem : UWorldSubsystem` vs locked D-17's `UGameInstanceSubsystem` —
  bless the as-built design (banner the decision) or treat as a defect.
- ~~**Q4 remainder**~~ — ✅ RESOLVED 2026-07-15 (owner decision: build them):
  `RUI::Umg::UseOwnedViewModel<T>` (create-and-own for the component lifetime; suite
  `ReactiveUI.Mvvm.OwnedViewModel`) and the marshaling helper
  `RUI::Umg::MarshalToProperty/MarshalFromProperty` (the prop-map's conversion table made
  public + shared; suite `ReactiveUI.Umg.Marshal`). Stale-VM policy was already recorded
  (quiet defaults — RuiFieldHooks.h).
