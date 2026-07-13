# UETKX — ReactiveUI for Unreal

Editor support for **`.uetkx`**, the JSX-like markup of [ReactiveUI for Unreal](https://github.com/yanivkalfa/ReactiveUI-Unreal) — a React-style reactive UI library for Unreal Engine 5.6+, in pure C++.

## Features

- **Syntax highlighting** for the JSX-like markup *and* the embedded C++, via a self-contained TextMate grammar.
- **Markup IntelliSense** — completion and hover for host-element tags, structural/common attributes, style/slot keys, and per-element event handlers, driven by the compiler-exported schema.
- **Import intelligence** — specifier and exported-name completions, go-to-definition across `.uetkx` files, and strict resolution diagnostics.
- **Diagnostics** — live parse diagnostics plus the compiler's own diagnostics surfaced from committed sidecars — **fully offline, no running Unreal editor required**.
- **Formatting** — canonical `.uetkx` formatting (golden-corpus locked to the C++ compiler), with an optional **Format on Save** (Tools ▸ Options ▸ ReactiveUI ▸ UETKX).

## Requirements

- Everything is bundled — the language server ships with the extension (a Node runtime is included), so no Node on your PATH and no running Unreal editor are required.
