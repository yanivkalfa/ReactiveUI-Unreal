---
name: rebuild-ide-extensions
description: Rebuild the VS Code and/or VS2022 .uetkx extensions locally for F5 / Extension-Development-Host testing. Use when the user says "rebuild for F5", "test the extension", "build the language server locally", or after editing ide-extensions/lsp-server/**, ide-extensions/vscode-uetkx/**, or the shared grammar/schema. Covers the npm pipelines, server bundling, artifact verification, and the VS2022 experimental-instance flow. Does NOT cover marketplace releases — publish.yml owns those.
---

# Rebuild IDE extensions for F5

Repo root (Windows): `C:\Yanivs\GameDev\ReactiveUI\ReactiveUI-Unreal`. Both IDE clients load
the **same bundled Node language server** (`lsp-server/out` + runtime deps copied to a
`server/` dir); there is no native/.NET piece, so one `.vsix` serves every platform.

## One-time prep (fresh clone)

```powershell
cmd /d /c "cd /d ide-extensions\lsp-server && npm ci && npm run build"
cmd /d /c "cd /d ide-extensions\vscode-uetkx && npm ci"
```

## Decide which pipelines to rebuild

- `ide-extensions/lsp-server/src/**` or its `src/uetkx-schema.json` → **server** rebuild,
  then the client bundle (root F5 does both automatically, in that order).
- `ide-extensions/vscode-uetkx/src/**` or `package.json` → **client** rebuild.
- `ide-extensions/vscode-uetkx/syntaxes/**` or `language-configuration.json` → no compile;
  reload the dev host (VS2022: repackage — it carries its own copies under `UetkxVsix\`).
- Codegen vocabulary changed (`UetkxCodegen.cpp`) → the shipped schema must be re-synced from
  `-run=RUIExportSchema` output and `ReactiveUI.Uetkx.Acceptance` re-run; that's a runtime
  change, not an IDE rebuild — see the test-run skill.

## VS Code — the F5 flow (canonical)

Open **the repo root** in VS Code and press **F5** ("Run UETKX Extension"). The
`preLaunchTask` chain in `.vscode/tasks.json` builds the server, then the client —
`vscode-uetkx`'s build runs `tsc` **and** `scripts/bundle-server.mjs`, which copies
`lsp-server/out` + its runtime deps into `vscode-uetkx/server/`. An Extension Development
Host then opens on this repo; poke any `Source/RuiDemo/Screens/*.uetkx` for colors,
completion, hover, diagnostics, formatting.

- **Client breakpoints** (`src/extension.ts`) bind directly in the F5 session.
- **Server breakpoints**: with the host up, run **"Attach to UETKX Language Server"** —
  the client starts the server with `--inspect=6010` in debug mode.
- **Server logs**: in the dev host, Output tab → dropdown → **"UETKX Language Server"**
  (the server's stdout/stderr; an unhandled exception there = broken bundle, not markup).
- Opening `ide-extensions/vscode-uetkx` as its own workspace also works (it keeps a nested
  `.vscode/`), but root-F5 is the supported flow.

**Verify artifacts** if language features look dead:

```powershell
Get-Item ide-extensions/vscode-uetkx/out/extension.js,
         ide-extensions/vscode-uetkx/server/server.js,
         ide-extensions/vscode-uetkx/server/uetkx-schema.json
```

Expected: `extension.js` ~3 KB, `server/server.js` ~10 KB, schema ~4 KB, plus
`server/node_modules/vscode-languageserver*`. **Small files are correct here** — this is
plain `tsc` output, not an esbuild bundle; do not apply the Unity sibling's
"<50 KB = broken" rule.

## VS2022 — experimental instance

Requires VS2022 + the **"Visual Studio extension development"** workload. One command does
the whole chain (npm builds → mirror the server bundle into `UetkxVsix\server` → msbuild):

```powershell
cd ide-extensions\visual-studio
.\build-local.ps1           # build only;  -Debug launches the Exp instance;  -Install = main hive
```

Or in-IDE: run at least step 1 of the script once (the server mirror), then open
`ide-extensions/visual-studio/UetkxVsix/UetkxVsix.csproj` in VS2022 and **F5** — the csproj
commits `StartAction/StartProgram/StartArguments` (devenv `/rootsuffix Exp`), so F5 deploys
to the experimental hive and launches it, Unity-sibling style. The client prefers a bundled
`server\node.exe` and falls back to `node` on PATH; for a dev box, PATH is fine.

## Out of scope — marketplace releases

Marketplace/OpenVSX publishing is `publish.yml`'s `publish-vscode` job (currently a guard
stub). Never run `vsce publish` / `ovsx publish` from a developer machine — this skill stops
at F5-ready local artifacts. Local `.vsix` packaging for sideloading is
`npm run package` in `vscode-uetkx` (uses `npx @vscode/vsce`, not a dep).

## Scar tissue

- **npm runs through `cmd /d /c`, never bare PowerShell** — stock Windows resolves `npm` to
  `npm.ps1`, which ExecutionPolicy blocks (Unity-sibling scar; its tasks.json wraps
  identically, and ours does too).
- **`bundle-server.mjs` hard-fails when `lsp-server/out/server.js` is missing** — deliberate:
  silently packaging without a server yields "extension loads, zero language features"
  confusion. The root build task rebuilds the server *first*, so a plain F5 can't hit this;
  only a fresh clone that skipped one-time prep can.
- **The launch config MUST live in the root `.vscode/`** — VS Code only reads the open
  folder's `.vscode/`. The config originally lived only in `vscode-uetkx/`, so F5 from the
  repo root did nothing (owner-reported, 2026-07-11). Don't consolidate it back down.
- **VS Code never hot-swaps a running server** — after a rebuild with the dev host still
  open, run "Developer: Reload Window" *in the dev host* (or stop and F5 again); the old
  node process keeps serving otherwise.
- **VS2022 "sees an old server"** — the static mirror into `UetkxVsix\server` was skipped.
  Same failure shape as the Unity sibling's VSIX, same fix: re-run build-local.ps1.
- **Red Debug Console noise ≠ our extension failing** — before `--disable-extensions` was in
  the launch args, installed extensions (GitLens' ConfigCat telemetry) spammed red stderr in
  the dev host and read as extension errors (owner-reported, 2026-07-11). Our extension's
  real signal lives in Output → "UETKX Language Server".
- **VSIX sometimes not regenerated on incremental build** — build-local.ps1 deletes `obj\`
  and uses `/t:Rebuild` deliberately (Unity-sibling scar); don't "optimize" that away.
