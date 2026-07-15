# PR: v1-gate closeout — localization ships, TD-020/028/029 resolved, docs sweep, plans triage (0.4.0)

> Branch `feat/v1-gate-closeout` → `dev`. Paste this file as the PR body (it archives with the
> campaign afterwards, like its predecessors in `plans/archive/`).

The five-task campaign, one branch as always:

## 1 · Docs sweep

- Code samples render tab-indented at **2-wide** (`tab-size: 2` on the CodeBlock — samples stay
  byte-identical to the golden formatter's output)
- **Zero sibling-project mentions** on the rendered site (Introduction family section,
  GDExtension comparisons, Godot/Unity divergence notes, search index)
- Router page samples rewritten as **compilable `.uetkx`** — and proven: a new **RouterDemo
  gallery screen** uses the exact pattern (route table in setup, Router as an expression child,
  bare `Use*` router hooks). Found+fixed on the way: `FRuiRoute` must be `RUI::`-qualified
  (the old C++ sample was silently uncompilable)

## 2 · Plans triage

- 12 finished campaign docs → `plans/archive/` (each with an outcome row in archive/README.md;
  references rewritten repo-wide)
- **`plans/REMAINING.md`** — every open item in one categorized file (v1 gate · interop polish ·
  verification debt · docs build-out · widget/markup surface · post-v1 by decision · owner
  questions)

## 3 · Localization — the last v1-gate production gap CLOSES

- Verified gather pipeline: `*.inl` in the `GatherTextFromSource` masks; the demo ships a
  complete reference target (`Config/Localization/RuiDemo_Gather.ini`) with committed output
- Culture change → every mounted root re-renders (`RuiCultureSync` on the engine's
  text-revision event)
- New suite `ReactiveUI.Loc` (mechanism / real `SetCurrentCulture` switch / gather-manifest
  tripwire); docs Localization guide replaces the "deferred" callout everywhere

## 4 · TD-020 RESOLVED — embedded-C++ completion

- `onCompletion` forwards to clangd over the virtual doc (replace-range translation back to
  `.uetkx`; auto-include edits stripped; markup baseline without clangd).
  Hover/definition/completion now all clangd-backed. LSP suite 33/33 + smoke

## 5 · TD-028 + TD-029 RESOLVED — interop polish

- `URuiHostWidget`: `InitialProps` + `ViewModel` designer/BP channel →
  `RUI::Umg::UseHostProp`/`UseHostViewModel`; `SynchronizeProperties` forwards edits live
  (suite `ReactiveUI.Umg.HostProps`)
- `URuiActivatableScreen`: `RUI::CommonUI::UseDesiredFocus` + the `GetDesiredFocusTarget()`
  contract honored via focus forwarding (suite `ReactiveUI.CommonUI.DesiredFocus`)

## 6 · Second batch (owner-directed, 2026-07-15)

- **`Ref={ expr }` markup attribute** — universal reserved prop (joins `key`/`classes`):
  host-handle capture from pure markup (portal targets, focus designation). Contract fixture
  `RefCapture` + AcceptanceLab §9 live proof + LSP completion/hover + docs.
- **`RUI::Umg::UseOwnedViewModel<T>`** + **`MarshalToProperty/FromProperty`** — the audit's
  Q4 research-promised APIs, built (suites `ReactiveUI.Mvvm.OwnedViewModel`,
  `ReactiveUI.Umg.Marshal`); the prop-map now routes through the shared marshal table.
- **Generated per-hook reference docs** — 23 core + 17 router pages from a header-authored
  catalog; docs-drift now 8 checks (catalog counts vs `RuiContext.h`/`RuiRouter.h`).
- **Bench re-run** — 5 fresh `BENCH_BASELINES.md` rows (M1 / UE 5.6.1); README cites them.
- **`plans/DOOM_DEMO_PLAN.md`** — the Unreal Doom-demo port plan from Godot+Unity research
  (owner-decided; own campaign next). Screen-reader pass DEFERRED by owner (REMAINING §3).

## Gates (all green)

- UE 5.6: build · `RUICompile -check` 33 files / 0 drifted / 0 errors · GatherText · contract goldens 10/10 ·
  **battery 110/110, 0 failed** (incl. the boot check + Demos mounting all 17 screens)
- Engine-free: changelog verify · mirror byte-check · headers · skills lint · docs-drift (8) ·
  corpus hash · LSP 33/33 + smoke · docs build + eslint · clang-format full set

## Versions staged

- Plugin **0.4.0** (Version 6) + `[0.4.0]` changelog (mirror byte-identical)
- Extensions **0.2.0** both (lsp-server change ⇒ both bump) + Lane B entry
- ROADMAP row 7 **COMPLETE**; MASTER_PLAN Phase-7 banner + the gate's "Production gaps closed"
  line ticked; PENDING_CHANGELOG staged for release-process
