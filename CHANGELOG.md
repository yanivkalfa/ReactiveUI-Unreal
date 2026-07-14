# Changelog

All notable changes to the **ReactiveUI plugin** (`Plugins/ReactiveUI/`) are documented here,
keep-a-changelog style; versions are the plugin's `VersionName`. This file is mirrored
**byte-identically** into `Plugins/ReactiveUI/CHANGELOG.md` — edit only this root file, then
resync with `cp CHANGELOG.md Plugins/ReactiveUI/CHANGELOG.md` (CI byte-compares them via
`scripts/verify-mirror.mjs`). The IDE extensions are NOT covered here — they use
`ide-extensions/changelog.json` (Lane B; see the release-process skill).

## [0.1.1] — 2026-07-14

### Fixed

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
