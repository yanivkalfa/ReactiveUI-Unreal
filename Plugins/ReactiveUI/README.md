# ReactiveUI for Unreal

React-style reactive UI for Unreal Engine 5.6+, in pure C++. Function components return a
virtual tree; a fiber reconciler patches only what changed on real **Slate** widgets; state
lives in hooks. `.uetkx` markup (grammar-compatible with the family's `.guitkx`/`.uitkx`)
compiles to native C++ for shipping builds and hot-reloads live in PIE during development —
no script VM in your shipped game.

The Unreal sibling of [ReactiveUIToolKit](https://github.com/yanivkalfa/ReactiveUIToolKit)
(Unity) and [ReactiveUI-Godot](https://github.com/yanivkalfa/ReactiveUI-Godot) (Godot).

> **Status: beta — built end to end.** 23 core hooks, 65+ wrapped Slate widgets (including
> virtualized `ListView`/`TileView`), the `.uetkx` compiler with committed codegen and a CI
> drift gate, live hot reload in PIE (Unreal Live Coding — Windows editor; the library itself
> builds and runs on every UE platform), the 17-hook router, `@theme`/`.uss` stylesheets,
> drag-and-drop, exit animations, localization (markup text gathers through the stock
> Localization Dashboard; culture switches re-render live), and first-class UMG/CommonUI/MVVM
> interop — all green under the repository's headless automation battery. Remaining before v1:
> docs-site build-out. Progress and the v1 ship gate:
> <https://github.com/yanivkalfa/ReactiveUI-Unreal>

## What it is

- **Slate is the render target** — output is ordinary `SWidget`s; Widget Reflector, styling,
  and the rest of the Slate toolchain see normal widgets.
- **UMG both ways** — host our UI inside UserWidgets (`URuiHostWidget`), or use UMG widgets
  inside our tree (`RUI::Umg`).
- **CommonUI stays in charge** of menus, input routing, and gamepad focus — our screens push
  onto existing activatable stacks (`URuiActivatableScreen`).
- **MVVM viewmodels feed us data** — `UseField(VM, "Health")` re-renders on FieldNotify
  broadcasts.

## Support

Issues: <https://github.com/yanivkalfa/ReactiveUI-Unreal/issues> ·
Discord: <https://discord.gg/Knedqu4Wyv> (`#unreal`)

## License

ReactiveUI Community License 1.0 (see `LICENSE`) — free to use, modify, and ship inside your
games and projects (commercial included) if your company earned under US $250k in the last
12 months; above that, shipping needs a commercial license ($2,000/title or
$2,500/studio/year — see the repo's `LICENSE-COMMERCIAL.md`). Credit "Made with ReactiveUI";
not to be redistributed or sold as a competing product. Code generated from **your** `.uetkx`
files belongs to **your** project.
