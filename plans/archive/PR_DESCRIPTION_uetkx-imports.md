# feat: `.uetkx` compiler + imports, IDE/LSP, Epic interop, HMR v2 — the full authoring stack

> Paste into the GitHub compare page. **Base:** `dev` (clean ancestor — 97 commits ahead, 0 behind).
> Compare: https://github.com/yanivkalfa/ReactiveUI-Unreal/pull/new/feat/uetkx-imports
> Delete this file after merge.

## What this is

The branch that takes ReactiveUI-Unreal from "runtime + reconciler" to a **complete authoring stack**:
the `.uetkx` markup language (compile-to-C++), static imports/exports, the IDE tooling, the Epic-interop
layer, the full runtime feature set (router, stylesheets, animation, widgets, DnD), and **HMR v2** — a
Live-Coding-driven hot reload with its own editor menu/window. 97 commits, 313 files
(+38.3k / −1.0k), all against `dev`.

## The `.uetkx` compiler (M3.x)

- **Lexer + parser + codegen:** `FUetkxLexer` (C++ lexis), `FUetkxMarkup` (byte-compatible family
  grammar), `FUetkxJsxScan` (markup-inside-expression), and the C++ **codegen** (`source → .uetkx.inl`).
- **Driver + commandlets:** `FUetkxDriver` (sidecars, staleness graph, aggregators, fingerprint);
  `RUICompile` (`-full`/`-check` drift gate), `RUIExportSchema`; the canonical **formatter** + golden
  corpus; the contract harness (57 cases). The demo gallery runs on compiled `.uetkx` with committed
  `.inl`.

## Static imports / exports (M1–M11)

- **Grammar:** `import { A, B } from "./x"` / `~/root-alias`, extensionless, named-only, preamble-only;
  `export` makes a `component`/`hook`/`module` cross-file addressable; a file is a free sequence of
  decls. Scanner mirrored 1:1 in the TS LSP.
- **Strict resolution (day one):** `IUetkxImportResolver` + diagnostics **UETKX2300–2309** (unknown/
  not-exported/not-declared/duplicate/unused/misplaced/used-not-imported with a fix-it), enforced by
  `RUICompile -check`.
- **Codemod:** `-run=RUIMigrateImports` — idempotent; adds `export` + inserts needed imports, requires a
  zero-diagnostic compile. Migrated the gallery strict-clean.
- **Privacy + staleness:** non-exported decls tree-shaken into `RuiPriv_<Basename>`; sidecar v3 with
  reverse-edge staleness (an importer re-stales when its dep's export changes).

## IDE / LSP (Phase 5, TD-016, TD-020, TD-024)

- `.uetkx` **language server** + **VS Code** and **VS2022** clients: schema-driven completions/hover,
  hash-gated sidecar diagnostics, golden-corpus formatting, TextMate grammar with embedded `source.cpp`.
- **Import intelligence:** specifier/name completions, go-to-def, live resolution diagnostics; typed
  event-payload completion (`Value.<Field>`); the embedded-C++ intelligence core (virtualDoc + sourceMap
  + clangd proxy). VS2022 options page + format-on-save.

## Runtime features

- **Router** (TD-001): 17 hooks + matching engine. **Stylesheets** (TD-002): `@theme` tokens + `@uss`
  third layer. **Exit animations** (TD-003): `<Presence>` delayed-unmount. **Drag & drop** (TD-004):
  typed DnD over `FDragDropOperation` + `UseShortcut`.
- **Widgets:** batch-2 (WidgetSwitcher/ScaleBox/Throbber/WrapBox, MultiLine/Search text, SafeZone/
  DPIScaler/Separator, SpinBox/UniformWrapPanel/RichText/Grid) + specials (ExpandableArea,
  SegmentedControl, NumericEntryBox, ComboBox, SuggestionTextBox); typed fluent builders (TD-013);
  `{children}` forwarding (TD-015).
- **Item-model views** (TD-022): virtualized `ListView`/`TileView` with per-row sub-roots; focus
  extensions (`UseFocus`); GC-rooted asset brushes.
- **Hooks Phase 7:** `UseTween`/`UseAnimate`/`UseTweenValue` (host-clock, easing, retarget-from-current)
  + `UseSfx` — every hook is now stub-free.
- **Read-only `.uetkx` preview** (TD-006): in-editor tab + panel + `FUetkxPreview`.

## Epic interop (Phase 6, TD-021)

- **UMG:** `URuiHostWidget` (ours-inside-theirs), `RUI::Umg::UserWidget` (theirs-inside-ours),
  `URuiWorldSubsystem` teardown contract, `UseField` over FieldNotify.
- **CommonUI + MVVM:** activatables, MVVM global collection, UMG prop-map bridge, `URuiSignalViewModel`
  (ours feeding theirs). Enabled as optional plugin refs.

## HMR v2 — Live-Coding-driven hot reload

- **Engine:** deleted the single-file interpreter (couldn't resolve imports or run user hooks);
  `FUetkxHmrController` drives **Unreal Live Coding** — save → recompile → patch → refresh, state
  preserved (hook cells are heap-resident). Watcher debounce/coalesce, all event kinds.
- **UI:** top-level **`ReactiveUetkx`** menu + a **Hot Reload window** (Start/Stop, ACTIVE/Idle, swaps/
  errors/last/RAM, recent errors); read-only preview reworked to the compiled component.
- **Shortcuts + settings:** two rebindable, default-unbound commands + a global key processor +
  `UDeveloperSettings` + an in-window chord recorder; **Follow Play** (start HMR on Play, release the
  Live Coding session on Stop so external builds work); event-driven hiding of Epic's Live Coding
  console while HMR is active.
- **Platform:** live HMR is **Windows-only** (Live Coding is), documented in README + **TD-HMR-XPLAT**,
  which also tracks a legacy-Hot-Reload cross-platform spike. The library itself builds/runs on every UE
  platform; only the live-reload DX is Windows.

## Hardening

- Two adversarial bug hunts fully resolved: round 1 (17 confirmed + 2 plausible) and round 2 (51
  findings), each with regression tests.

## Verification (all green)

- Build: `ReactiveUIUnrealDemoEditor` Development OK. Markup drift: `RUICompile -check` — 23 files,
  0 drifted, 0 errors.
- Headless automation suite: **99 / 99**. LSP `node --test` + smoke green.
- Engine-free CI gates (headers, changelog mirror, docs-drift, skills, corpus-hash) green. Docs site
  build + lint green.
- The live Live-Coding loop is owner-verified in-editor (no headless test can drive Live Coding).

## Notes

- Sidecars (`*.uetkx.diags.json`) are git-ignored per-machine cache — not in the diff.
- Deferred items are tracked in `plans/TECH_DEBT.md` (TD-HMR-DEMOS, TD-HMR-XPLAT, and the accepted v1
  caveats); nothing dropped.
