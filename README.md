Questions, feedback, showcase → **[Discord](https://discord.gg/Knedqu4Wyv)** (family server — `#unreal`)

# ReactiveUI for Unreal

A **React-style reactive UI library for Unreal Engine 5.6+, in pure C++** — the Unreal sibling
of [ReactiveUIToolKit](https://github.com/yanivkalfa/ReactiveUIToolKit) (Unity/C#) and
[ReactiveUI-Godot](https://github.com/yanivkalfa/ReactiveUI-Godot) (Godot/GDScript).

Function components return a virtual tree; a fiber reconciler diffs each render and patches only
what changed on real **Slate** widgets. State lives in hooks. On top sits `.uetkx` — a JSX-like
markup (same grammar as the siblings' `.guitkx`/`.uitkx`) that **compiles to native C++ for
shipping builds** and **hot-reloads live in PIE** during development (Windows): save the file, see
the UI update in under a second, no C++ recompile, no script VM in your shipped game.

> **Status: beta — the product is built end to end.** The reconciler (23 core hooks, all
> stub-free), 65+ wrapped Slate widgets with setter styling (the core set + the batch-2 everyday
> widgets + specials — ExpandableArea/SegmentedControl/NumericEntryBox/ComboBox/SuggestionTextBox)
> plus **virtualized `ListView`/`TileView`**, the `.uetkx` compiler (committed codegen +
> `RUICompile`/drift-gate/contract commandlets), live hot reload mid-session (Unreal Live Coding +
> editor watcher, whole-project, state preserved — **Windows only**, since Live Coding is; the library
> itself builds and runs on every UE platform), the full **router** subsystem (17 hooks), **`@theme`/`@uss`
> stylesheets**, **exit animations** (`<Presence>`), **drag-and-drop** + keyboard shortcuts,
> first-class **CommonUI/MVVM citizenship** (activatable screens, MVVM global collection, UMG
> prop-map bridge), an **in-editor `.uetkx` live preview**, and VS Code/VS2022 language tooling
> (with embedded-C++ clangd intelligence) are implemented and green under a **100+-test headless
> automation battery**. The demo gallery's 19 screens all compile from `.uetkx`, and markup text
> is **localizable through the stock Localization Dashboard** (with live culture switching). Open
> `ReactiveUIUnrealDemo.uproject` (UE 5.6+; the battery is verified green on 5.6, 5.7, AND 5.8)
> and press Play. Remaining before v1: the **docs-site content build-out** — tracked in
> [plans/ROADMAP.md](plans/ROADMAP.md) and [plans/REMAINING.md](plans/REMAINING.md).

**Quick taste** — `Source/RuiDemo/Screens/SimpleCounter/SimpleCounter.uetkx` (compiles to the
committed sibling `.inl`; edit it while the editor runs and the screen hot-swaps in place):

```jsx
export FRuiNode SimpleCounter() {
	auto [Count, SetCount] = UseState<int32>(0);

	return (
		<VerticalBox>
			<TextBlock Text={ RUI::Fmt(TEXT("Count: {}"), Count) } />
			<Button OnClicked={ SetCount(Count + 1) } ContentPadding="12,4">+</Button>
		</VerticalBox>
	);
}
```

Reconciler numbers (headless bench, Win64 dev build, 2026-07-15 — medians from the committed
[plans/BENCH_BASELINES.md](plans/BENCH_BASELINES.md)): mount 1000 leaves ≈ **193 µs**; no-op
re-render ≈ **0 µs**; 1-of-1000 targeted update ≈ **143 µs**; 500 keyed rows fully reversed ≈
**188 µs**; minimal-move single reorder ≈ **3 µs**. And the stress proof: the gallery's **Doom
demo** — a playable software-raycast FPS whose entire framebuffer is the widget tree — spends
≈ **197 µs** of CPU per WHOLE game frame (sim + geometry + reconcile + Slate apply,
`Bench.Doom`).

## The one-sentence thesis

**We are the layer that decides which Slate widgets exist each frame — everything of Epic's
(UMG, CommonUI, MVVM) stays in place, either feeding us data or hosting our output.**

- **Slate** is the render target: our output is ordinary `SWidget`s — Widget Reflector,
  styling, and the rest of the Slate toolchain see normal widgets.
- **UMG** is a door in both directions: designers drop our UI inside their UserWidgets
  (`URuiHostWidget`), and their widgets work inside our tree (`RUI::Umg`).
- **CommonUI** keeps owning menus, input routing, gamepad focus, platform glyphs — our screens
  are pushed onto *their* stacks (`URuiActivatableScreen`).
- **MVVM / FieldNotify** viewmodels feed us data: `UseField(VM, "Health")` re-renders the
  component when the field changes. They own values; we own structure.

## What v1 will ship (the gate — "no half product")

The reconciler with all 23 family hooks · 65+ wrapped Slate widgets including virtualized
`ListView`/`TileView` ·
setter-based styling (a style tweak never rebuilds a widget) · the `.uetkx` compiler
(compile-to-C++ for shipping, Live-Coding hot reload for dev) · **true ES modules** (a file IS a module: named / `as`-renamed / `* as` / default imports,
value + util exports, `export { … };` lists + `export default`, privacy-by-default with
file-qualified runtime identity, strict resolution enforced by the compiler, and a one-command
`-run=RUIMigrateEsModules` codemod to upgrade an existing project) · VS Code + VS2022 extensions on
the shared family language server (the VS Code one also hides a `.uetkx` file's generated
companions — `*.uetkx.inl`/`*.uetkx.diags.json`/`*.Uetkx.gen.cpp` — from the Explorer by
default; flip the `files.exclude` entries to see them) · the UMG/CommonUI/MVVM interop above · localization, focus
preservation, portals, asset-safe brushes · a demo project (this repo — open and press Play) and
a docs site, with every performance claim measured before it's printed.

## Repository layout

| Path | What it is |
|---|---|
| `Plugins/ReactiveUI/` | The plugin — the deliverable (self-contained: own README/CHANGELOG/LICENSE) |
| `Source/`, `Content/` | The demo host project (`ReactiveUIUnrealDemo.uproject` at root) + automation tests |
| `ide-extensions/` | `.uetkx` IDE tooling (shared family language server; VS Code + VS2022) |
| `ReactiveUIUnrealDocs~/` | Docs site (React + Vite, the family shell) |
| `plans/` | ROADMAP (status source of truth), MASTER_PLAN, ledgers |
| `research/` | The two-round research corpus the plans rest on |
| `.claude/skills/` | The house process, encoded (dev/release/testing/production-line runbooks) |

## License

[MIT](LICENSE) on GitHub. Fab downloads (when the listing exists) are additionally governed by
the Fab EULA. Code generated from **your** `.uetkx` files belongs to **your** project.
