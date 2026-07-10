# ReactiveUI for Unreal

React-style reactive UI for Unreal Engine 5.6+, in pure C++. Function components return a
virtual tree; a fiber reconciler patches only what changed on real **Slate** widgets; state
lives in hooks. `.uetkx` markup (grammar-compatible with the family's `.guitkx`/`.uitkx`)
compiles to native C++ for shipping builds and hot-reloads live in PIE during development —
no script VM in your shipped game.

The Unreal sibling of [ReactiveUIToolKit](https://github.com/yanivkalfa/ReactiveUIToolKit)
(Unity) and [ReactiveUI-Godot](https://github.com/yanivkalfa/ReactiveUI-Godot) (Godot).

> **Status: pre-alpha scaffold — not yet functional.** This plugin currently contains the
> module skeletons only. Progress, plans, and the v1 ship gate live in the repository:
> <https://github.com/yanivkalfa/ReactiveUI-Unreal>

## What it will be

- **Slate is the render target** — output is ordinary `SWidget`s; Widget Reflector and the
  accessibility layer see normal widgets.
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

MIT (see `LICENSE`). Code generated from **your** `.uetkx` files belongs to **your** project.
