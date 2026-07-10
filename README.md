Questions, feedback, showcase → **[Discord](https://discord.gg/Knedqu4Wyv)** (family server — `#unreal`)

# ReactiveUI for Unreal

A **React-style reactive UI library for Unreal Engine 5.6+, in pure C++** — the Unreal sibling
of [ReactiveUIToolKit](https://github.com/yanivkalfa/ReactiveUIToolKit) (Unity/C#) and
[ReactiveUI-Godot](https://github.com/yanivkalfa/ReactiveUI-Godot) (Godot/GDScript).

Function components return a virtual tree; a fiber reconciler diffs each render and patches only
what changed on real **Slate** widgets. State lives in hooks. On top sits `.uetkx` — a JSX-like
markup (same grammar as the siblings' `.guitkx`/`.uitkx`) that **compiles to native C++ for
shipping builds** and **hot-reloads live in PIE** during development: save the file, see the UI
update in under a second, no C++ recompile, no script VM in your shipped game.

> **Status: pre-alpha — nothing to install yet.** The repo currently contains the plans, the
> research corpus, and the Phase 0 production scaffolding (CI, skills, templates, plugin
> skeleton). Follow progress in [plans/ROADMAP.md](plans/ROADMAP.md) — the living status table —
> and the detailed execution plan in [plans/MASTER_PLAN.md](plans/MASTER_PLAN.md).

## The one-sentence thesis

**We are the layer that decides which Slate widgets exist each frame — everything of Epic's
(UMG, CommonUI, MVVM) stays in place, either feeding us data or hosting our output.**

- **Slate** is the render target: our output is ordinary `SWidget`s — Widget Reflector,
  accessibility, and styling all see normal widgets.
- **UMG** is a door in both directions: designers drop our UI inside their UserWidgets
  (`URuiHostWidget`), and their widgets work inside our tree (`RUI::Umg`).
- **CommonUI** keeps owning menus, input routing, gamepad focus, platform glyphs — our screens
  are pushed onto *their* stacks (`URuiActivatableScreen`).
- **MVVM / FieldNotify** viewmodels feed us data: `UseField(VM, "Health")` re-renders the
  component when the field changes. They own values; we own structure.

## What v1 will ship (the gate — "no half product")

The reconciler with all 23 family hooks · 25 wrapped Slate widgets plus a virtualized list ·
setter-based styling (a style tweak never rebuilds a widget) · the `.uetkx` compiler
(compile-to-C++ for shipping, interpret-live for dev) · VS Code + VS2022 extensions on the shared
family language server · the UMG/CommonUI/MVVM interop above · localization, focus preservation,
portals, asset-safe brushes · a demo project (this repo — open and press Play) and a docs site,
with every performance claim measured before it's printed.

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
