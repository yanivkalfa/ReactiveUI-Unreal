# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this
repository.

## What this repo is

A **React-style reactive UI library for Unreal Engine 5.6+, in pure C++** — the third sibling of
ReactiveUIToolKit (Unity/C#) and ReactiveUI-Godot (GDScript). Function components return a
virtual tree; a fiber reconciler patches real **Slate** widgets. `.uetkx` markup (grammar
byte-compatible with the family's `.guitkx`/`.uitkx`) compiles to C++ for shipping and
hot-reloads live in dev via **Unreal Live Coding** (whole-project, state preserved; HMR v2 —
the old dev-loop interpreter was deleted).

`.uetkx` files use **static imports** (strict from day one): `import { A, B } from "./x"` /
`~/root-alias`, extensionless, named-only, preamble-only; `export` on a `component`/`hook`/
`module` makes it cross-file addressable (non-exported = private, tree-shaken). A file is a free
sequence of components + hooks + modules. `-run=RUIMigrateImports` is the idempotent codemod that
adds `export` + the needed imports to an existing tree (`RUICompile -check` enforces resolution).

**The plans are the source of truth**: [plans/ROADMAP.md](plans/ROADMAP.md) (living status
table) and [plans/MASTER_PLAN.md](plans/MASTER_PLAN.md) (locked decisions D-01..D-32, the 10
phases, the v1 ship gate, the delegation matrix). Its §Resume-protocol says how to join
mid-project: ROADMAP table → MASTER_PLAN banners → §1 decisions → first unchecked phase only —
then establish ground truth by RUNNING the phase's Verify commands, never by trusting prose.
Full research corpus in `research/` (two rounds + audit ledgers).

## Deliverables & versions

| Deliverable | Location | Version source | Tag |
|---|---|---|---|
| Runtime plugin | `Plugins/ReactiveUI/` | `ReactiveUI.uplugin` → `VersionName` (+ integer `Version`) | `v*` (one tag; per-engine zips as assets) |
| VS Code extension (Phase 5) | `ide-extensions/vscode-uetkx/` | `package.json` | `vscode-v*` |
| VS2022 extension (Phase 5) | `ide-extensions/visual-studio/` | `source.extension.vsixmanifest` | `vs2022-v*` |
| lsp-server (Phase 5) | `ide-extensions/lsp-server/` | vendored; version-locked to vscode-uetkx (no own tag) | — |
| Docs site | `ReactiveUIUnrealDocs~/` | not version-gated | — |

The demo host project (`ReactiveUIUnrealDemo.uproject` at repo root, modules under `Source/`)
is **not shipped** — it hosts the automation tests, the demo gallery, and the per-engine example
projects Fab requires.

## Commands

Node scripts are engine-free and run anywhere:

```bash
node ide-extensions/scripts/changelog.mjs verify      # Lane B changelogs in sync (CI gate)
node scripts/verify-mirror.mjs                        # root CHANGELOG.md == plugin mirror (CI gate)
node scripts/check-headers.mjs                        # copyright header lint (CI gate)
node scripts/lint-skills.mjs                          # skills frontmatter + scar-tissue lint (CI gate)
node scripts/docs-drift.mjs                           # claimed counts vs registries (CI gate)
node scripts/bump.mjs <artifact> <version>            # version bump + lockstep partners + changelog scaffold
```

Engine commands (require UE 5.6+ installed; paths per the `test-run` skill's environment facts):

```bat
:: 0. Build (there IS a compile step — unlike Godot)
<Engine>\Engine\Build\BatchFiles\Build.bat ReactiveUIUnrealDemoEditor Win64 Development -Project=<abs>\ReactiveUIUnrealDemo.uproject -WaitMutex

:: 1. Markup compile sweep + drift gate (Phase 3+)
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUICompile -check

:: 2. Suites — headless; ALWAYS redirect output to a file; parse report\index.json, not the exit code
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -ExecCmds="Automation RunTests ReactiveUI; Quit" -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput -ReportExportPath=<scratch>\report
```

Suite filters are prefix-matched: `ReactiveUI.Boot` (the boot check — unit suites do NOT run
`StartupModule`, so it is never optional), `.Core`, `.Update`, `.Style`, `.Widgets.*`, `.Demos`,
`.Uetkx`, `.Contract`, `.Umg`, `.Mvvm`, `.CommonUI` (plus `.Slate`, `.Router`, `.Hooks`,
`.Bugfix*`, `.Acceptance`, `.Editor` — there is no `.Hmr` suite); `ReactiveUI.Bench` is NOT
pass/fail (numbers go to `plans/BENCH_BASELINES.md` with machine/config context).

Docs site: `cd "ReactiveUIUnrealDocs~" && npm ci && npm run dev` (or `npm run build && npm run lint`).

## Architecture (one paragraph + pointers)

`ReactiveUICore` (Runtime, **no UObject/CoreUObject**) holds vnodes/fibers/reconciler/hooks and
talks to engines only through `IRuiHostConfig`. `ReactiveUISlate` implements the host with
per-widget adapters (typed props structs + set-bitmask, setter tables, reconstruct masks, event
proxies). `ReactiveUIUMG`/`ReactiveUICommonUI`/`ReactiveUIMVVMBridge` are the Epic-interop
modules. `ReactiveUIInterp` (Runtime, `TargetConfigurationDenyList: ["Shipping"]`) owns the
markup lexer/parser (parser-only since HMR v2 — the dev-loop interpreter was deleted);
`ReactiveUIToolchain` (UncookedOnly) consumes the parser for codegen; `ReactiveUIEditor` (Editor)
hosts the watcher/commandlets + the Live-Coding HMR controller (`FUetkxHmrController`) and the
`ReactiveUetkx` menu/window. Full reasoning:
MASTER_PLAN §1; module table: D-27.

## Conventions (enforce; don't re-litigate)

- **Naming:** `FRui*`/`SRui*`/`URui*`/`IRui*`/`TRui*`; factories in namespace `RUI::`
  (`RUI::Slate::VerticalBox()`, `RUI::FC`, `RUI::Umg`). Markup extension `.uetkx`.
- **Element/prop/style/event naming is 1:1 loyal to Unreal (D-33, MASTER_PLAN):** tag = Slate
  class minus `S` (`VerticalBox`, `TextBlock`, `Slider`); props/style keys/events = the Unreal
  setter/property/delegate name (`WidthOverride`, `RenderOpacity`, `OnCheckStateChanged`); our
  custom widgets carry the `Rui` mark (`RuiCanvas`). No shorthands, no React aliases.
- **Copyright header (Fab requirement, CI-linted):** first line of every `.h/.cpp/.inl/.cs/.mjs`
  under `Source/`, `Plugins/`, `templates/`, `scripts/`, `ide-extensions/` (own code, not
  node_modules):
  `// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.` (`#` form for `.ps1`).
  Codegen emits it only for THIS repo's files; user-project output gets the neutral
  "belongs to your project" banner (D-32).
- **Logs:** per-module categories `LogRuiCore`/`LogRuiSlate`/`LogRuiUmg`/`LogRuiInterp`/
  `LogRuiEditor`. **CVars:** `rui.` + dotted PascalCase — the shipped set: `rui.TimeSlicing`,
  `rui.FrameBudgetMs`, `rui.HostNodePool`, `rui.HookValidation`, `rui.StrictDiagnostics`,
  `rui.StrictMode` (runtime stats via `stat ReactiveUI`, not a CVar). **MessageLog page:**
  `"ReactiveUI"`.
- **Line endings:** LF everywhere (`.gitattributes` pins it; two CI gates byte-compare files).
- **Generated code is COMMITTED** (`*.uetkx.inl`, `<Module>.Uetkx.gen.cpp`) and reflection-free;
  sidecars (`*.uetkx.diags.json`) are machine-local and gitignored.
- **Preserve family semantics** (they are decisions, not bugs — MASTER_PLAN §5): removed plain
  props don't reset (style/events/refs/draw do); structural error boundaries; synchronous
  `UseTransition`; the documented divergences FROM Godot (React ref lifecycle,
  subscribe-in-effect signals, registry-FName component identity).

## Process

- The loop: **research → develop → test → bughunt → fix → commit → repeat** (`dev-process`
  skill). Production-grade only; never weaken a gate; never re-try the same theory twice.
- **1 branch, 1 PR** per campaign: `feat/<name>` → `dev` (PR; owner merges); `master` is
  release-only, fast-forwarded via `git push origin origin/dev:master`.
- **Never auto-commit** beyond what the task established; no `Co-Authored-By`; the owner presses
  the Publish button and performs Fab uploads/Discord posts.
- Phase done ⇒ run the `plan-progress` skill (checkbox + evidence → ROADMAP row →
  `plans/PENDING_CHANGELOG.md`). Feature merged ⇒ `docs-sync` skill.
- Repetitive work (widget wrappers, style keys, docs pages, changelog entries, corpus cases)
  runs on lesser models per MASTER_PLAN §8's delegation matrix — hand them the skill + templates
  + evidence packet, never whole-repo context.

## Secrets inventory (all owner-created; none block unarmed CI)

| Name | Kind | Consumed by | Local mirror key (`publisher-secrets.json`) |
|---|---|---|---|
| `RUI_CI_ENGINE_ARMED` | repo variable | test.yml/publish.yml engine legs (gate) | — |
| `EPIC_GHCR_PAT` | secret | engine-container pulls (Linux CI legs) | — |
| `VSCE_PAT` | secret | publish.yml vscode leg (Phase 5+) | `vscePatToken` |
| `OVSX_TOKEN` | secret | publish.yml vscode leg, Open VSX (Phase 5+) | `ovsxToken` |
| `VS_MARKETPLACE_TOKEN` | secret | publish.yml vs2022 leg (Phase 5+) | `vsMarketplaceToken` |

## Environment facts (owner machine — fill/verify at first engine use)

- Engine: **`C:\Program Files\Epic Games\UE_5.6`** (Launcher install; verified 2026-07-10 —
  the empty-plugin Development Editor build succeeded, 45/45 actions, 51 s).
- VS2022 Community with the C++ workload; MSVC toolchain **14.44.35223** — UE 5.6 warns it
  "is not a preferred version (prefers 14.38)" but accepts it. If strict toolchain matching is
  ever needed (e.g. chasing a compiler-specific bug), install the 14.38 toolset via the VS
  Installer's Individual Components.
- clang-format 19.1.5 at `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-format.exe`.
- Always redirect engine console output to a file — piping block-buffers and hides everything.
- The editor WRITES to `Config/` on close: first boot normalized `DefaultInput.ini` (committed
  once, stable after) and tried to add an AndroidFileServer section **with a generated
  SecurityToken** — that plugin is disabled in the `.uproject` so generated tokens never enter
  the repo. If a boot dirties `Config/` again, inspect before committing; never commit tokens.
