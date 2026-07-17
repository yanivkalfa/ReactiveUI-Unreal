# Reactive UI - Unreal Engine - VS Code (UETKX)

Editor support for **`.uetkx`**, the JSX-like markup of [ReactiveUI for Unreal](https://github.com/yanivkalfa/ReactiveUI-Unreal) — a React-style reactive UI library for Unreal Engine 5.6+, in pure C++.

## Features

- **Syntax highlighting** for the JSX-like markup *and* the embedded C++, via a self-contained TextMate grammar.
- **Markup IntelliSense** — completion and hover for host-element tags, structural/common attributes, style/slot keys, and per-element event handlers, driven by the compiler-exported schema.
- **Import intelligence** — specifier and exported-name completions, go-to-definition across `.uetkx` files, and strict resolution diagnostics.
- **Diagnostics** — live parse diagnostics plus the compiler's own diagnostics surfaced from committed sidecars — **fully offline, no running Unreal editor required**.
- **Formatting** — canonical `.uetkx` formatting (golden-corpus locked to the C++ compiler); **Format on Save is enabled by default** for `.uetkx` files (the standard `editor.formatOnSave` setting, scoped to the language).
- **Clean Explorer** — the generated companions a `.uetkx` file drags along (`*.uetkx.inl`, `*.uetkx.diags.json`, `*.Uetkx.gen.cpp`) are **hidden from the Explorer by default**. They still exist (the `.inl`/`.gen.cpp` are committed, compiled C++ — only the view is filtered). To see them again, override the corresponding `files.exclude` entries in your settings:

  ```json
  "files.exclude": {
    "**/*.uetkx.inl": false,
    "**/*.uetkx.diags.json": false,
    "**/*.Uetkx.gen.cpp": false
  }
  ```

## Requirements

- Everything is bundled — the language server ships with the extension (a Node runtime is included), so no Node on your PATH and no running Unreal editor are required.
- **Embedded C++ intelligence** (completion/hover/diagnostics *inside* setup/hook/module bodies) needs two things:
  1. **clangd** — **bundled on Windows x64** (clangd 22.1.6, Apache-2.0 w/ LLVM exceptions — nothing to install). On other platforms it is auto-discovered from PATH, `%LOCALAPPDATA%\uetkx\clangd\bin`, `C:\Program Files\LLVM`, or the VS 2022 "C++ Clang tools" component; or point `uetkx.clangd.path` at one (an explicit setting always wins, including over the bundle). Without any clangd the extension degrades gracefully to full markup intelligence (a one-time notice tells you).
  2. **A compile database** — generate once per project with UBT's VS Code generator; the language server finds the `.vscode/compileCommands_<Project>.json` it writes and mirrors it to the canonical `compile_commands.json` automatically (add that name to your `.gitignore` — it holds machine-local paths):

     ```
     dotnet "<Engine>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" -projectfiles -project="<your>.uproject" -game -vscode
     ```

## Changelog

### [0.4.2] - 2026-07-16
- Embedded-C++ intelligence now sees the full auto-included prelude: the virtual document's header list gains `CoreMinimal.h`, `RuiSignal.h`, `RuiSlateElements.h`, and `RuiStyle.h`, matching the compiler's 18-header list exactly — clangd-backed completion/hover inside setup bodies now resolves Slate element factories, signals, and style types.

### [0.4.1] - 2026-07-16
- The marketplace description now credits the family Discord (discord.gg/Knedqu4Wyv) and the GitHub repo, kept under 280 characters for both listings.

### [0.4.0] - 2026-07-16
- `.uetkx` preambles are imports-only now: an `import "@Header.h"` host-include form (family shape, ported from Unity's `import "@Namespace"`; our payload is a header path) compiles to `#include "Header.h"` — nameless by design, since the C++ compiler resolves the header's symbols. A raw `#include` line still works too, but naming a header the generated file already auto-includes now gets a redundancy hint (`UETKX2317`). Syntax highlighting, live diagnostics, and a discoverable `import "@"` completion snippet ship for the new form.

### [0.3.1] - 2026-07-16
- Marketplace listing overhaul: distinguishable display names — `UETKX (Unreal - VS Code)` / `UETKX (Unreal - VS2022)` — and a structured page body (Title / Description / Features / Requirements / Changelog) on both marketplaces.
- The VS Code marketplace page now has a body at all — README.md is generated from the centralized changelog (it was missing entirely).

### [0.3.0] - 2026-07-16
- Schema catch-up to plugin 0.9.0: the bundled .uetkx vocabulary grows 38 → 63 tags (the widget-completion campaign — ConstraintCanvas, Splitter, MenuAnchor, NotificationList, color pickers, input boxes, and 20+ more), 16 style keys, 16 slot keys.
- First engine-gated tag: SearchableComboBox carries sinceUE: 5.7 in the schema — completions/hover know it does not exist on UE 5.6.
- Grammar wave G: early component-level markup returns are accepted (multi-return collector, byte-identical to the C++ compiler per the grammar contract); the final return must be top-level (new UETKX3007); short-circuit &&/|| markup is no longer a diagnostic (UETKX3002 retired). Scanner corpus +4 cases.

### [0.2.0] - 2026-07-15
- Embedded C++ completion: inside a component setup / hook / module body, completion now forwards to clangd over the virtual document (locals, engine symbols, members) — hover, go-to-definition, and completion are all clangd-backed there. Replace ranges map back to .uetkx coordinates; without clangd on PATH everything degrades to the markup baseline as before.

### [0.1.2] - 2026-07-14
- Refreshed the ReactiveUI brand icon.

### [0.1.1] - 2026-07-13
- Added the ReactiveUI brand icon to the marketplace listing.

### [0.1.0] - 2026-07-13
- First beta — .uetkx language support for VS Code and Visual Studio 2022: schema-driven tag/attribute/event completions and hover, live parse diagnostics, import intelligence (specifier + name completions, go-to-definition, and strict resolution diagnostics), golden-corpus formatting, and embedded-C++ awareness. Visual Studio 2022 adds a Tools > Options Format-on-Save setting.
