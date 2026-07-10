---
name: test-run
description: The exact incantations to build and run this repo's test suites headlessly — UE's CLI is arcane and these commands are load-bearing, in this order. Use whenever building or running ReactiveUI.* automation suites, benches, or the markup drift gate.
---

# Test run — the canonical commands, in order

Environment facts first (verify, don't assume): engine discovered via
`Get-ChildItem "C:\Program Files\Epic Games\UE_5.*"` — **UE 5.6 install pending on the owner's
machine as of Phase 0; record the exact path here when it lands.** `<abs>` below = this repo's
absolute path. ALWAYS redirect engine output to a file — piping block-buffers and hides
everything (family scar).

```bat
:: 0. Build (compile step — Development Editor)
<Engine>\Engine\Build\BatchFiles\Build.bat ReactiveUIUnrealDemoEditor Win64 Development -Project=<abs>\ReactiveUIUnrealDemo.uproject -WaitMutex

:: 1. Markup compile sweep + drift gate (Phase 3+; skip while no .uetkx exist)
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUICompile -check

:: 2. Suites (headless, no GPU)
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject ^
  -ExecCmds="Automation RunTests ReactiveUI; Quit" ^
  -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput ^
  -ReportExportPath=<scratch>\report > <scratch>\run.log 2>&1
```

- **Pass/fail comes from `<scratch>\report\index.json`** (`succeeded`/`failed` counts) — the
  exit code alone is unreliable.
- **Filters are prefix-matched**: `Automation RunTests ReactiveUI.Core` runs that suite;
  `ReactiveUI.Widgets.Button` a single widget's tests. Suite map: `Boot` (module startup + root
  mount — the gate ladder's boot check, NEVER optional), `Core`, `Update`, `Style`, `Widgets.*`,
  `Demos` (mounts every demo — the real "generated code runs" check), `Uetkx`, `Contract`,
  `Hmr`, `Umg`, `Mvvm`, `CommonUI`.
- **`ReactiveUI.Bench` is NOT pass/fail** — numbers go to `plans/BENCH_BASELINES.md` WITH the
  machine/config context that file demands; cross-machine comparisons are invalid.
- Engine-free gates (run these anywhere, before any engine work):
  `node ide-extensions/scripts/changelog.mjs verify && node scripts/verify-mirror.mjs && node scripts/check-headers.mjs && node scripts/lint-skills.mjs && node scripts/docs-drift.mjs`
- Suites must print per-section markers so hangs name their culprit — keep that convention when
  writing new tests.

## Scar tissue (why these steps exist)

- **Output redirection to a file, always**: the family lost real errors to block-buffered pipes
  more than once; `> run.log 2>&1` is not optional style, it's how you SEE failures.
- **index.json over exit codes**: UnrealEditor-Cmd has returned 0 with failed tests in the
  report; the JSON is the truth.
- **The RUICompile pre-step mirrors `tests/guitkx_build.gd`** — suites consuming stale generated
  code produce phantom passes/failures; compile first, always, in this order.
- **Per-section markers**: a hanging suite with no markers cost an hour of bisection in the
  sibling repo; with markers, the hang names its culprit in one read.
