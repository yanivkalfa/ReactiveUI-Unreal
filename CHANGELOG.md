# Changelog

All notable changes to the **ReactiveUI plugin** (`Plugins/ReactiveUI/`) are documented here,
keep-a-changelog style; versions are the plugin's `VersionName`. This file is mirrored
**byte-identically** into `Plugins/ReactiveUI/CHANGELOG.md` — edit only this root file, then
resync with `cp CHANGELOG.md Plugins/ReactiveUI/CHANGELOG.md` (CI byte-compares them via
`scripts/verify-mirror.mjs`). The IDE extensions are NOT covered here — they use
`ide-extensions/changelog.json` (Lane B; see the release-process skill).

## [0.3.0] — 2026-07-14

### Added

- **Unreal Engine 5.8 support** — the full 103-test battery is green on 5.6, 5.7, and 5.8.
  The 5.6→5.8 API diff found NO removals: +3 Slate widgets (SSearchableComboBox recorded as
  a wrap candidate; two editor docking widgets out of scope) and additive-only FArguments
  surface across 26 widgets (plans/WIDGET_INVENTORY.md).

### Fixed

- UE 5.8 compat: FJsonObject::Values' key type changed (FString → UE::FSharedString) — the
  .uetkx sidecar reader now iterates keys engine-agnostically; the .uetkx watcher's ticker
  delay uses the API's float type.
- The engine-api-diff tool hard-fails when an engine install is partial (a scan finding
  fewer than 50 widgets aborts instead of emitting a wrong diff).

## [0.2.0] — 2026-07-14

### Added

- **Unreal Engine 5.7 support** — the full 103-test battery is green on 5.6 AND 5.7. The
  5.6→5.7 API diff (scripts/engine-api-diff.ps1) found zero added/removed widgets and only
  additive FArguments surface (recorded in plans/WIDGET_INVENTORY.md as v1.x candidates).
  The dev .uplugin no longer pins EngineVersion — the pin is a hard per-version compat gate
  (5.7 silently skipped loading the whole plugin in headless runs); per-engine packages
  stamp it at release. DocsURL now points at the docs site.

### Fixed

- UE 5.7 deprecation sweep: FReferenceCollector::AddReferencedObject now takes TObjectPtr
  (the raw-pointer overload is unsafe under incremental GC and removed next release); the
  deprecated FieldNotification include path replaced; a C5038 initializer-order warning in
  TRuiProvidedValue silenced at the source.

- **Portal content no longer leaks on unmount** — deleting a subtree containing a portal (or
  unmounting a whole root) now explicitly removes the portal's children from their target panel;
  previously the release path assumed they lived under the released subtree, leaking them in
  external targets. Found by the feature's first automated test (`ReactiveUI.Core.Portal`).
- **`URuiHostWidget::Remount()` actually remounts** — UMG caches the widget returned by
  `RebuildWidget`, so the old implementation unmounted and never rebuilt. The host now returns a
  stable wrapper and swaps its content in place, making runtime `ComponentName` changes +
  `Remount()` work (covered by `ReactiveUI.Umg.Lifecycle`).
- **Packaged README rewritten** — the README inside the plugin package still described a
  "pre-alpha scaffold — not yet functional"; it now states the beta reality (23 hooks, 35+
  widgets, compiler + drift gate, hot reload, router, stylesheets, Epic interop).
- Softened the unverified accessibility claim in the interop copy to what is verified (Widget
  Reflector + the Slate toolchain see ordinary widgets); the screen-reader path has not been
  exercised yet.

## [0.1.0] — 2026-07-13

First beta. A complete React-style reactive UI stack for Unreal in pure C++ — function components
return a virtual tree, a fiber reconciler patches real Slate widgets, and state lives in hooks.

### Added

- **Reactive core** — fiber reconciler with keyed diffing/bailout, 23 hooks (`UseState`,
  `UseEffect`, `UseMemo`, `UseRef`, `UseContext`, `UseReducer`, signals, `UseTween`/`UseAnimate`,
  `UseSfx`, `UseFocus`, …), structural error boundaries, and `<Presence>` exit animations. No
  UObject/CoreUObject dependency in the core.
- **Slate host** — 25+ wrapped Slate widgets (boxes, `Button`, `Border`, `Image`, `ScrollBox`,
  text inputs, `CheckBox`, `Slider`, `ProgressBar`, `GridPanel`, virtualized `ListView`/`TileView`,
  plus specials: `ExpandableArea`, `SegmentedControl`, `NumericEntryBox`, `ComboBox`,
  `SuggestionTextBox`) with setter-based styling (a style tweak never rebuilds a widget), typed
  events, drag-and-drop, and keyboard shortcuts.
- **`.uetkx` markup language** — JSX-like, compiles to native C++ (committed `.inl`); `RUICompile`
  commandlet with a content-based drift gate + a golden formatter; **static imports/exports** (strict
  from day one) with the `RUIMigrateImports` codemod.
- **Epic interop** — Slate is the render target; UMG is a two-way door (`URuiHostWidget` /
  `RUI::Umg::UserWidget`); CommonUI hosts our screens (`URuiActivatableScreen` + activation hooks);
  MVVM/FieldNotify viewmodels feed us (`UseField`). A gallery **Interop Showcase** demonstrates all
  four in one screen.
- **HMR** — Unreal Live-Coding-driven hot reload: save a `.uetkx` and the running UI patches with
  state preserved, via the `ReactiveUetkx` editor menu + Hot Reload window (Windows; Live Coding is).
- **`@theme`/`@uss` stylesheets**, the **router** subsystem (17 hooks), a read-only `.uetkx` preview
  tab, and the example gallery (15 screens, including the interop demos).
- **IDE tooling** — a `.uetkx` language server with VS Code and VS2022 clients (completions, hover,
  go-to-definition, import intelligence, formatting, embedded-C++ intelligence).
- **`RUI::Fmt`** — `{}`-placeholder `FText` interpolation for clean bindings.

### Notes

- Beta: API may still shift before 1.0. Cross-platform for build/run; the live-reload DX is Windows.
