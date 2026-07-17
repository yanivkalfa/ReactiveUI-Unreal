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
