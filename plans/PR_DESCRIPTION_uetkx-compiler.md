## What this is

Phases 3–8 of the MASTER_PLAN in one campaign branch: the `.uetkx` toolchain end to end — compiler, live hot reload, IDE tooling, Epic interop core, real animation/media hooks — with the demo gallery fully converted to the compiled path and a coherence gate over the whole chain. **Battery: 53/53 headless suites** (was 38 at branch start).

## Phase by phase

- **Phase 3 — compiler (M3.1–M3.9):** C++ lexer/markup parser/jsx scan/file scan (in `ReactiveUIInterp`, D-27) → codegen to **committed `Foo.uetkx.inl`** + stable per-module aggregators (D-19.1) → schema-v2 diagnostics sidecars, full staleness machinery (error verdicts, mtime-tie hash break, env hold, orphan sweep, UETKX2106 duplicate binding, compiler fingerprint) → `-run=RUICompile [-full|-check]` (content-based CI drift gate), `RUIExportSchema`, `RUIContractDump [-check]` → canonical formatter + `uetkx.config.json` walk-up. Contract mechanism: 57-case scanner corpus + 16-case formatter goldens + 4 full-file contract fixtures. **All 11 gallery screens now compile from `Source/RuiDemo/Screens/*.uetkx`** and the Demos battery drives the compiled path (real clicks, full TicTacToe flow).
- **Phase 4 — hot reload:** the D-20 expression VM (whitelist registry; Printf/FText/FMath/ctor builtins), the markup interpreter (interpreted clicks drive interpreted state; setter-call events with the `Value` payload; C-style `@for`/`@while`/`@if`/`@match`), `FRuiHmr` (swap/link/reset + the family `reloaded|refreshed|reset|linked|global|ms` status line, per-file isolation), reconciler override seam + `HmrRefreshAll`, the three-trigger editor watcher (deadman, MessageLog dedup + resolved line), `rui.Hmr.AutoLiveCoding`.
- **Phase 5 — IDE tooling:** `uetkx-language-server` with the TS front-end **corpus-locked to the C++ compiler** (both shared corpora + FNV hash pins replay byte-identically), schema-driven completions/hover, hash-gated sidecar diagnostics, golden-corpus formatting; VS Code extension (TextMate grammar with embedded `source.cpp`, self-contained JS server bundle — one vsix, all platforms); VS2022 MEF `ILanguageClient` host. clangd enhancement layer recorded (TD-020); Rider cut per owner.
- **Phase 6 — Epic interop core:** `URuiHostWidget` (our UI inside UMG, design-time placeholder, teardown on `ReleaseSlateResources`), `RUI::Umg::UserWidget` (UMG inside our tree via `SObjectWidget`), `URuiWorldSubsystem` (PIE-end/level-travel teardown contract), `RUI::Umg::UseField` over FieldNotify (MVVM-plugin-independent). CommonUI/MVVM-plugin layers → TD-021.
- **Phase 7:** `UseTween`/`UseAnimate`/`UseTweenValue` real (host-clock, retarget-from-current, easing, self-re-arming frame chain) + `UseSfx` sink — **every hook now stub-free**; exit-animation Presence protocol designed (`plans/EXIT_ANIMATION_DESIGN.md`). Asset brushes/focus/item-model → TD-022.
- **Phase 8 (partial by design):** bench refreshed + printed in the README (mount-1000 ≈ 192 µs med; no-op ≈ 0 µs; 1-of-1000 ≈ 149 µs); README → alpha with a quick-taste sample. Docs site + Inspector tab ride the owner-review cycle.
- **Finalization:** `ReactiveUI.Acceptance` (the coherence gate: drift/contract/registration/format-round-trip/HMR-swap/schema-parity in one suite) + `plans/OWNER_ACCEPTANCE_CHECKLIST.md` (the interactive half).

## Honest deferrals (each with a TD entry + rationale)

TD-014 Content-Browser presence · TD-015 v1 grammar limits · TD-016 typed event payloads · TD-017 hook/module companion decls · TD-018 Godot-repo corpus mirror PR · TD-019 compiled→interp state-value migration · TD-020 clangd proxy + VS2022 polish · TD-021 CommonUI/MVVM-plugin layers + UMG prop maps · TD-022 asset brushes/focus/item-model.

## Gates

- 53/53 headless battery (`Automation RunTests ReactiveUI`, nullrhi), including the new Vm/Hmr/Umg/Mvvm/Tween/Sfx/Contract/Acceptance suites
- `RUICompile -check` exit 0 (11 files, 0 drifted) · `RUIContractDump -check` exit 0
- lsp-server `node --test` 7/7 (both shared corpora byte-identical) · vscode-uetkx builds + bundles
- clang-format gate green over Source + Plugins on every commit

🤖 Generated with [Claude Code](https://claude.com/claude-code)
