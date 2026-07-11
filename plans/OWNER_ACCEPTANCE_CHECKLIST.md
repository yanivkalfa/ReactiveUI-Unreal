# Owner acceptance checklist — the interactive half of `ReactiveUI.Acceptance`

> One sitting per section. The headless half already gates CI (52-suite battery + the
> `ReactiveUI.Acceptance` coherence suite). Everything here needs eyes/hands: PIE, the
> editor loop, and the IDE extensions. Check items off in this file as you go.

## A. PIE gallery regression (~10 min)

- [ ] Open `ReactiveUIUnrealDemo.uproject` (UE 5.6.1), press **Play**.
- [ ] Walk ALL 11 menu entries. Every screen renders; no log errors in Output.
      (These are now COMPILED `.uetkx` — same visuals as the Phase-2 playtest.)
- [ ] Counter: `+` increments. Text Field: typing mirrors, caret stays put.
- [ ] Signals: two panels track together across Increment/Decrement/Reset.
- [ ] Context: theme toggle recolors both consumer panels.
- [ ] Keyed Diff: Reverse/Shuffle/Insert/Remove keep rows stable (no flicker/rebuild).
- [ ] Tic Tac Toe: full game to a win line; marks visible; status line correct.
- [ ] Custom Draw: polygon sides +/-, tint toggle, RedrawKey shuffle all repaint.
- [ ] Stress Test: 300 boxes/10s run completes; `stat ReactiveUI` shows live counters;
      note the FPS line (was ~112fps @ 1000 keyed boxes in Phase 2).

## B. The HMR headline — save → screen, mid-PIE (~10 min)

Setup: editor open, PIE running, `Source/RuiDemo/Screens/SimpleCounter.uetkx` open in VS Code.

- [ ] **Style edit** (change the Button `ContentPadding`, save): screen updates **< 1 s**,
      counter VALUE survives. MessageLog "ReactiveUI" shows the
      `reloaded 1 | refreshed … | reset 0 | … ms` status line.
- [ ] **Structure edit** (add a `<TextBlock Text="hi" />` row): updates < 1 s, state kept.
      (Note: the FIRST save after PIE start reports `reset … (representation)` — expected,
      recorded as TD-019; subsequent saves preserve.)
- [ ] **Hook-shape edit** (add `auto [X, SetX] = UseState<int32>(0);`): that screen resets,
      status line says `reset 1`, other screens untouched.
- [ ] **Parse error** (delete a `>` and save): old UI keeps running; MessageLog shows the
      UETKX error once (no spam on re-saves); fix it → green "compile errors resolved" line.
- [ ] **External-editor sleep test**: save while the UE window is minimized; tab back in —
      the activation trigger sweeps within a second.
- [ ] Optional: `rui.Hmr.AutoLiveCoding 1` then save a `.uetkx` whose edit reports
      "rebuild required" → Live Coding fires.

## C. Commandlets / CI lanes (~5 min, terminal)

- [ ] `UnrealEditor-Cmd <proj>.uproject -run=RUICompile -check` → exit 0, `11 file(s), 0 drifted`.
- [ ] Touch a `.uetkx` (whitespace edit), rerun `-check` → **non-zero**; run `-run=RUICompile`
      then `-check` again → 0. Revert the touch (git).
- [ ] `-run=RUIContractDump -check` → exit 0.
- [ ] `-run=RUIExportSchema` → `Saved/ReactiveUI/schema.json` regenerates.

## D. VS Code extension (~10 min)

- [ ] `cd ide-extensions/lsp-server && npm ci && npm test` → 7/7 (both shared corpora replay).
- [ ] `cd ../vscode-uetkx && npm ci && npm run build`, then F5 (Extension Development Host)
      or `npx @vscode/vsce package` + install the `.vsix`.
- [ ] Open a gallery `.uetkx`: syntax colors (markup + embedded C++ tint), `<` completes
      element names, attr position completes typed attrs + style/slot keys, hover on
      `Button`/`UseState`/`RenderOpacity` shows docs.
- [ ] Break the markup → squiggle with the `UETKX####` code; fix → clears.
- [ ] Format Document (or format-on-save): canonical layout; format twice = no further change.
- [ ] `uetkx.config.json` with `{"indentStyle":"space","indentSize":2}` at repo root changes
      the formatting; delete it after.

## E. VS2022 extension (~15 min, needs the VSSDK workload)

- [ ] Open `ide-extensions/visual-studio/UetkxVsix/UetkxVsix.csproj` in VS2022 (17.x) with the
      "Visual Studio extension development" workload; restore + build.
- [ ] Copy the built lsp-server bundle into `UetkxVsix/server/` (the vscode sibling's
      `scripts/bundle-server.mjs` output layout) — optionally a `node.exe` beside it.
- [ ] F5 (experimental instance) → open a `.uetkx`: colors + completions + diagnostics work.

## F. Interop spot checks (~10 min)

- [ ] In a Blueprint UserWidget, add **ReactiveUI Host** (palette category "ReactiveUI"),
      set `ComponentName = HelloWorld`: designer shows the placeholder, PIE shows the screen.
- [ ] End PIE → no warnings about lingering ReactiveUI roots (the subsystem teardown log
      line appears if anything was mounted via `MountNamed`).
- [ ] (If you have a FieldNotify VM handy) `RUI::Umg::UseField` against it re-renders on
      broadcasts — the `ReactiveUI.Mvvm` suite is the headless proof if not.

## G. Cross-machine sanity (~5 min)

- [ ] Fresh clone → generate project files → build → run the battery:
      `UnrealEditor-Cmd … -ExecCmds="Automation RunTests ReactiveUI; Quit" -nullrhi -unattended`
      → **53/53** (52 + Acceptance). This proves the committed `.inl` story: no generation
      step needed before first build.
