# Changelog

All notable changes to the **ReactiveUI plugin** (`Plugins/ReactiveUI/`) are documented here,
keep-a-changelog style; versions are the plugin's `VersionName`. This file is mirrored
**byte-identically** into `Plugins/ReactiveUI/CHANGELOG.md` — edit only this root file, then
resync with `cp CHANGELOG.md Plugins/ReactiveUI/CHANGELOG.md` (CI byte-compares them via
`scripts/verify-mirror.mjs`). The IDE extensions are NOT covered here — they use
`ide-extensions/changelog.json` (Lane B; see the release-process skill).

## [0.7.0] — 2026-07-16

Widget-completion wave 3: the protocol widgets — popups, toasts, anchors, splitters — and
the first engine-version-gated widget.

### Added

- **10 new widgets** (60 markup tags total): `ConstraintCanvas` (P5a anchor slot model —
  `Slot.Anchors/Offset/Alignment/AutoSize/ZOrder`, all live), `Splitter` (P5b fraction
  slots — `Slot.SizeRule/SizeValue/MinSize/Resizable` + `OnSplitterFinishedResizing`),
  `MenuAnchor` (P3, THE popup primitive: anchor child + `Slot.Role="menu"` popup content;
  controlled `bIsOpen`), `NotificationList` (P4 toast mount +
  `RUI::Slate::PushNotification` command), `NumericDropDown`, `BreadcrumbTrail`
  (declarative crumbs), `LinkedBox` (uniform-size sibling groups by `GroupKey`),
  `WindowTitleBarArea`, `VirtualJoystick`, and `SearchableComboBox` — **the first
  `sinceUE: 5.7` widget** (compiled out on 5.6; the schema carries the annotation).
- **16 slot keys** (anchor + splitter + role families) with `RUI::Slot()` builder parity
  (gate-enforced).

## [0.6.0] — 2026-07-16

Widget-completion wave 2: twelve interactive composites, the imperative-handle rails, and
the TD-012 runtime-setter sweep.

### Added

- **12 new widgets** (50 markup tags total): `EditableText`, `InlineEditableTextBlock`,
  `VirtualKeyboardEntry`, `ColorWheel`, `ColorSpectrum`, `ColorGradingWheel` (the LIVE
  AdvancedWidgets one — new module dependency; the Slate twin is deprecated 5.5),
  `InputKeySelector`, `VolumeControl`, `RadialBox`, `TextScroller`, `LayeredImage`,
  `ExpandableButton` (three role slots via `slot.role`). Controlled-color and
  attribute-only surfaces ride the reconstruct mask; text edits follow the D-16 caret rule.
- **`UseImperativeHandle` ref-publishing overload** (P2): a child publishes a handle object
  into a parent-owned ref box, rebuilt on deps change and CLEARED on unmount; plus
  `RUI::Slate::WidgetFromHandle<T>()` — the typed escape hatch for ScrollToEnd-class
  imperative calls on any captured `Ref`.
- **TD-012 sweep**: Slider `Orientation/bLocked/bIndentHandle/SliderBarColor/
  SliderHandleColor`; ScrollBox `bAllowOverscroll/bAnimateWheelScrolling/
  WheelScrollMultiplier`; TextBlock style keys `LineHeightPercentage` + `OverflowPolicy`
  (16 style keys total); proxy payload handlers for bool/color/name events.

## [0.5.0] — 2026-07-16

Widget-completion wave 1 (the road to 1.0 = ALL official widgets —
`plans/WIDGET_COMPLETION_PLAN.md`): eight new wrapped widgets, the universal tooltip, and two
new anti-drift gates.

### Added

- **8 new widgets** (38 markup tags total): `ColorBlock`, `SimpleGradient`, `ComplexGradient`,
  `Hyperlink` (with `OnNavigate`), `EnableBox`, `ScissorRectBox`, `BackgroundBlur`
  (live `BlurStrength`/`BlurRadius`), `InvalidationPanel` (opt-in paint cache via `bCanCache`).
  The color/gradient/hyperlink leaves are the first widgets whose ENTIRE prop surface is
  construct-only — any prop change replaces the widget in place through the reconstruct-mask
  machinery (cheap leaves; no state to lose).
- **Universal `ToolTipText` style key** — a tooltip on ANY element (the family contract: a
  prop, never a wrapper widget), via `SWidget::SetToolTipText`; removal resets to no tooltip.
  14 style keys total.
- **`RUI::Slot()` gains `ZOrder`/`Position`/`Size`**; `RUI::Style()` gains `Clipping`/
  `ToolTipText` — and a new CI gate (`scripts/check-style-builders.mjs`) fails the build if a
  schema style/slot key ever lacks a builder method again (found 3 missing methods on its
  first run).
- **Button `bIsFocusable`** (construct-only; reconstruct-masked) and **ProgressBar
  `BarFillType`** (live; loyal lowerCamel enum names) — TD-012 riders.
- **`ReactiveUI.Contract.AdapterMasks`** — a registry-wide meta-gate: every adapter declaring
  a reconstruct mask must implement a side-effect-free, Has-gated `ConstructOnlyChanged`
  (caught one violating test fixture on its first run). Powered by a new public
  `RUI::Slate::ForEachAdapter` enumeration seam.

## [0.4.0] — 2026-07-16

### Added

- **Localization** — the last v1-gate production gap closes. Markup text (emitted as
  self-namespaced `NSLOCTEXT` since the first compiler release) now has a verified gather
  pipeline: add `*.inl` to a localization target's `GatherTextFromSource` masks (the demo
  project ships a complete reference target, `Config/Localization/RuiDemo_Gather.ini`) and
  the Localization Dashboard sees markup text like any C++. A culture switch re-renders every
  mounted root (`RuiCultureSync` subscribes the reconciler to the engine's text-revision
  event), healing values components baked under the previous culture. New suite:
  `ReactiveUI.Loc` (mechanism, live culture switch, gather-manifest tripwire).
- **`URuiHostWidget` designer props & viewmodel channel** — `InitialProps` (name → string map)
  and `ViewModel` (any FieldNotify object) are designer/Blueprint-editable and publish into the
  hosted tree as context; components read them with `RUI::Umg::UseHostProp` /
  `UseHostViewModel` (the viewmodel feeds `UseField` directly). `SynchronizeProperties`
  forwards edits live — no remount, hook state preserved. Suite: `ReactiveUI.Umg.HostProps`.
- **CommonUI desired focus** — `RUI::CommonUI::UseDesiredFocus` lets the hosted tree designate
  its initial gamepad-focus widget (pair with `RUI::Slate::UseFocus`); `URuiActivatableScreen`
  now honors CommonUI's `GetDesiredFocusTarget()` contract by returning itself and forwarding
  the arriving focus to the designated widget. Suite: `ReactiveUI.CommonUI.DesiredFocus`.
- **Embedded-C++ completion in the IDE extensions** (ships as vscode/vs2022 0.2.0): completion
  inside `setup`/`hook`/`module` bodies now forwards to clangd over the virtual document —
  hover, definition, and completion are all wired; markup-only remains the no-clangd baseline.
- **Router gallery demo** — the docs' router samples, live: an in-memory two-route screen
  (`/` + `/users/:id`) with `UseNavigate`/`UseParams`, authored in `.uetkx` with the route
  table in the setup section and the Router as an expression child (gallery: 17 screens).
- **`Ref={ expr }` markup attribute** — the universal reserved prop joins `key` and `classes`:
  capture a mounted widget's host handle straight from markup (React ref lifecycle — called on
  attach, cleared on detach). Powers pure-markup portal targeting and focus designation
  (`<Button Ref={ Focus.Ref } …>`); expr-only (a string form is diagnosed). Pinned by the
  `RefCapture` contract fixture and live in the Acceptance Lab's §9.
- **`RUI::Umg::UseOwnedViewModel<T>`** — create-and-own a viewmodel for the component's
  lifetime (constructed on first render, GC-rooted in a hook slot, released on unmount) — the
  `UseMemo` + `TStrongObjectPtr` pattern, packaged. Suite: `ReactiveUI.Mvvm.OwnedViewModel`.
- **`RUI::Umg::MarshalToProperty` / `MarshalFromProperty`** — the FRuiValue ↔ reflected-
  UPROPERTY conversion table, promoted from the prop-map bridge's internals to a public,
  shared helper (bool, int32/64, float/double, FString, FText, FName; kind mismatches skip,
  never mangle). The prop-map now routes through it. Suite: `ReactiveUI.Umg.Marshal`.
- **Generated per-hook reference docs** — every hook now has its own reference page (23 core +
  17 router), generated from a header-authored catalog the docs-drift gate compares against
  the code registries — the docs can never claim hooks the code doesn't have.
- **`Canvas` widget** — absolute placement over `SCanvas`: children position/size themselves
  via `Slot.Position`/`Slot.Size` (expr or "x,y" literals); paint order = child order; a
  slot-prop update mutates the LIVE slot in place, so per-frame quad movement costs no slot
  churn. Built as the Doom demo's framebuffer container. Suite: `ReactiveUI.Widgets.Canvas`.
- **`<Image Image={ Brush } />`** — the asset-brush prop is now a markup attribute (and the
  props field is renamed `Brush` → `Image`, the loyal Unreal name — D-33; pre-1.0 rename).
- **`Clipping` style key** (universal) — `SWidget::SetClipping` with the loyal
  `EWidgetClipping` names (`clipToBounds` etc.; removal resets to `inherit`). Slate never
  clips children to bounds by default — absolute-placed content (Canvas framebuffers)
  needs this. 13 style keys total.
- **`ScaleBox` `HAlign`/`VAlign`** — place the scaled content inside the box (live
  `SScaleBox` setters; default center|center). Lets fixed-resolution content pin to an
  edge while scaling to fit.
- **The Doom demo** — the family's marquee stress showcase, ported third: a fully playable
  software-raycast FPS whose ENTIRE framebuffer is the widget tree — every wall strip, floor/
  ceiling band, sprite, and tracer is a real keyed `<Image>` diffed per tick (~7k LOC; six
  levels; 100% procedural textures, zero assets). Gallery: 18 screens. Determinism suite
  `ReactiveUI.Doom` (182 checks, ported from the family); `ReactiveUI.Bench.Doom` measures
  the WHOLE frame — sim + geometry + reconcile + Slate apply — at **~197 µs median**.

### Fixed

- `UseStableFunc`/`UseStableAction` were missing from the markup compiler's built-in hook
  table — calling them from `.uetkx` emitted uncompilable code. Added in both
  implementations (C++ scanner + TS language-server mirror); schema re-exported (22 hooks).
- The `ColorAndOpacity` style key was silently dropped on `Image` and `Separator` — markup
  routes it through the style dict (like TextBlock's), but only the C++ typed-props path had
  a handler, so markup tints never reached the widget (the Doom viewport rendered as a solid
  white sheet: its alpha-0 full-screen flash quads painted opaque). Both adapters now handle
  the style key, with removal-reset to opaque white. Suites: `ReactiveUI.Style.WidgetColorKeys`
  (apply/update/reset on both widgets) + `ReactiveUI.Doom.MountedFrame` (the mounted game
  screen's flash quads are asserted transparent).

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
