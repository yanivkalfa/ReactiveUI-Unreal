# Owner acceptance checklist v2 — verifying the round-2 bug-hunt fixes

> **Scope.** This checklist covers the fixes from [BUGHUNT_2026-07-12_round2.md](BUGHUNT_2026-07-12_round2.md)
> (commit `4ff1ab8`, branch `feat/uetkx-imports`). It is deliberately SMALL: **most of the 51 fixes are
> already proven headless** and you do not need to re-verify them. This file is the short list of things
> that genuinely need eyes/hands (PIE, the editor loop, the armed CI leg, the IDE), plus optional visual
> spot-checks. Check items off as you go.
>
> **Do NOT re-verify by hand what CI already gates** — see §0 first.

## §0. Already proven headless — DON'T re-do these (~2 min to confirm)

The full pass is committed and green. Confirm the baseline once, then skip everything it covers:

```bat
:: from the repo root
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUICompile -check -unattended -nopause -nullrhi
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -ExecCmds="Automation RunTests ReactiveUI; Quit" -unattended -nopause -nosplash -nullrhi -log -stdout -ReportExportPath=<scratch>\report
cd ide-extensions\lsp-server && npm ci && npm test
node scripts\corpus-hash.mjs --check & node scripts\check-headers.mjs & node scripts\verify-mirror.mjs
```

- [ ] `RUICompile -check` → `23 file(s), 0 drifted, 0 error(s)`.
- [ ] Automation suite → **101/101** (parse `report\index.json`, not the exit code).
- [ ] LSP → **29/29**. Node gates → all ✓.

**What that already locks (no manual check needed):** CG-1/2/3/4 (codegen banner/indent/tag/ordering),
DRV-1..4 + CHECK-1 + IMPORT-1..4 + FMT-1 + RESOLVE-1 (the whole import/staleness/gate cluster),
SCAN-1/2/3 (scanner), STYLE-1/2/3 (theme tokens), RECON-1/3/4 (router matching/search/docs), the LSP
findings LSP-1..5 + LSW-1..3, and BUILD-1 (corpus hash). The explicit assertions live in
`ReactiveUI.Bugfix2.*` (10 tests) + the LSP suite. **The rest of this file is what those can't reach.**

---

## §A. HMR — the two dev-loop fixes (~8 min; editor + VS Code, PIE running)

Setup: editor open, PIE running, a gallery `.uetkx` (e.g. `Source/RuiDemo/Screens/SimpleCounter/SimpleCounter.uetkx`)
open in your editor. This is the one place HMR-1/HMR-2 actually execute (the swap + the Live Coding patch
are runtime events no headless test forces).

- [ ] **HMR-1 (state migration ordinal).** In a component, put a `UseRef` or `UseMemo` **before** a
      `UseState` (natural React ordering), e.g. `auto R = Ctx.UseRef<int>(0);` then
      `auto [A, SetA] = Ctx.UseState(1);` `auto [B, SetB] = Ctx.UseState(2);`. Drive A and B to new values
      in PIE, then **save** the file (first compiled→interp swap). ✅ Expect: A and B keep their driven
      values (the note reads `state migrated: first interp swap`, `reset 0`). Before the fix, B would have
      been seeded with A's value (or state silently reset).
- [ ] **HMR-2 (Live Coding clears overrides).** Save a `.uetkx` (an interp override is now active for that
      component — behavior comes from the interpreter). Then trigger **Live Coding** (Ctrl+Alt+F11) to
      recompile. ✅ Expect: after the patch, the component behaves per the **recompiled** compiled code —
      "rebuild for full behavior" actually takes effect. Before the fix, the interp override permanently
      shadowed the rebuild for the rest of the session. (MessageLog "ReactiveUI" is where the HMR status
      lines print.)

## §B. CommonUI input-method — CMU-1 (~5 min; PIE with a gamepad, or split-screen)

The B12 subscribe was headless-proven (`ReactiveUI.CommonUI.InputMethod`); CMU-1 is the *live re-point*
that only matters with a real player/device.

- [ ] Push a `URuiActivatableScreen` whose component uses `UseInputMethod` onto an activatable stack in
      PIE. Switch input mouse↔gamepad mid-session. ✅ Expect: the screen re-renders to the new input
      method each switch (B12), and — if you can force an **owning-player change** (split-screen add/remove
      a local player) — the subscription follows the new player rather than tracking a dead one (CMU-1).
- [ ] End PIE. ✅ Expect: no "delegate still bound" / dangling-subsystem warnings (teardown unbinds from
      the subsystem it actually bound to).

## §C. ComboBox controlled selection — IW-3 / B6-1 (~5 min; PIE, gamepad + keyboard)

The mouse path is headless-covered; the fix is about the paths a mouse click doesn't take.

- [ ] Give a ComboBox an `OnSelectionChanged` that logs. **Keyboard-navigate + Enter**, and **gamepad
      Accept**, to pick items. ✅ Expect: `OnSelectionChanged` fires **once per real pick** with the right
      index — never a phantom `-1`.
- [ ] Bind the ComboBox's Options to state and **remove the currently-selected option**. ✅ Expect: the
      engine prunes the selection but your `OnSelectionChanged` is **not** called with `-1` (that prune is
      engine-originated, not a user pick).

## §D. Live-widget visual spot-checks (~8 min; PIE, optional but reassuring)

The logic is build+drift-locked, but these confirm the pixels. Quickest path: drop the snippets into a
scratch screen (or the AcceptanceLab) and Play.

- [ ] **SLOT-1 / SLOT-2 (literal string slot values).** A grid with **string** slot values —
      `<GridPanel><Box Slot.Column="1" Slot.Row="0">A</Box><Box Slot.Column="0" Slot.Row="0">B</Box></GridPanel>`
      — and a `<WrapBox>` with `Slot.Padding="8"`. ✅ Expect: A and B land in **different cells** (not stacked
      in 0,0), and the wrap children show the 8px gap. Before the fix these string forms collapsed to 0.
- [ ] **GRID-1 (dynamic cell move).** Bind a grid child's `Slot.Column={col}` to state; change `col`. ✅
      Expect: the child **moves** to the new cell (previously it stayed put).
- [ ] **WRAP-1 (no reorder on slot change).** A WrapBox `[A, B, C]`; change **B's** `slot.padding` via
      state. ✅ Expect: order stays `[A, B, C]` (B does **not** jump to the end).
- [ ] **IW-1 (NumericEntryBox clamp).** A NumericEntryBox with `MinValue=0, MaxValue=100`. Type `999`,
      Enter. ✅ Expect: the committed value clamps to `100` (and `-50` → `0`).
- [ ] **STYLE-1 (theme tokens).** Load a stylesheet with `@theme dark { accent: #4f8cff; }`, reference
      `$accent` from a widget style, `SetActiveTheme("dark")`. ✅ Expect: the color resolves (both `accent:`
      and `$accent:` declaration forms work now).
- [ ] **POOL-1 (Separator pooling).** Render an `SSeparator` with a non-default `Thickness`, unmount it,
      then mount a **fresh** Separator with default thickness in the same frame. ✅ Expect: the new one shows
      the **default** thickness (no stale value leaked from the pool).

## §E. Router blocker on POP — RECON-2 (~3 min; PIE or the headless test)

Headless-proven by `ReactiveUI.Bugfix2.BlockerPop`, but a PIE sanity check is nice:

- [ ] In a routed screen, register a `UseBlocker` (active), navigate forward, then press the **back**
      button / call `UseGo(-1)`. ✅ Expect: the blocker fires and navigation is **prevented** (before the
      fix, back bypassed the guard that `navigate()` honored).

## §F. Memory safety under GC — B11-class asset brushes (~5 min; PIE, best-effort)

No headless test forces the GC timing; this is the one to eyeball under stress.

- [ ] Render an `<Image>`/`<Border>` with an **asset** brush (`RUI::Umg::MakeAssetBrush(Texture)`) behind a
      state flag, then **remove** the brush on a re-render (`bShow=false`), and keep the screen painting for
      a few seconds / force a GC (`obj gc` console). ✅ Expect: no crash, no flicker to freed memory — the
      widget resets to `GetNoBrush()` on removal (B11 fix) rather than dereferencing a dangling brush.

## §G. The import codemod — MIGRATE-1 (~5 min; terminal, on a scratch copy)

- [ ] On a **throwaway branch/copy**, create two `.uetkx` files that each hold a same-named **private**
      component (legal today). Run `<Engine>\UnrealEditor-Cmd <proj>.uproject -run=RUIMigrateImports`.
      ✅ Expect: the codemod **reports `UETKX2106`** (export-everything collided the two names) and exits
      non-zero — matching what `RUICompile -check` would say. Before the fix it green-lit a tree `-check`
      then rejected. Discard the scratch copy.
- [ ] Sanity: on a clean tree, `-run=RUIMigrateImports` twice in a row is a **no-op** the second time
      (idempotent), and `-run=RUICompile -check` stays 0.

## §H. The CI drift gate + suites — BUILD-2 (owner-only; arms the engine leg)

This is the meta-fix: `engine-tests.yml` steps 3 (drift) + 4 (suites) were commented out; they are now
live. They only *run* on an **armed** engine leg.

- [ ] Set the repo variable `RUI_CI_ENGINE_ARMED=true` (and confirm `EPIC_GHCR_PAT` is present) and push a
      commit / open a PR. ✅ Expect: the `UE 5.6` job now runs **`RUICompile -check`** (fails on drift) and
      **the ReactiveUI.* automation suites** (parses `report/index.json`), not just the two compile legs.
- [ ] Negative check (optional): hand-edit a committed `*.uetkx.inl` on a scratch branch and push armed →
      the drift gate should **fail** (proving it now guards the 25 generated files). Revert.
- [ ] If you keep CI unarmed for now: this stays a documented, ready-to-arm gate — the node `gates` job is
      unchanged, so nothing regresses meanwhile.

## §I. IDE interactive re-checks (~8 min; VS Code, only if you touched the LSP)

The LSP unit suite is 29/29; these confirm the interactive surface of the round-2 LSP fixes.

- [ ] **LSW LSP-3 (multi-line import).** Type a multi-line `import {\n  A,\n  B\n} from "./x"` and invoke
      completion **on a continuation line** (inside the `{ }`). ✅ Expect: exported-name completions appear
      (before the fix, a cursor off the first line saw no import and offered nothing).
- [ ] **LSP-3 / FMT-1 (formatter keeps import comments).** Add a trailing comment to an import line
      (`import { A } from "./x" // keep me`) and **Format Document**. ✅ Expect: the comment survives (the
      preamble is emitted verbatim when it carries a comment).
- [ ] **LSP-1 (embedded C++ intel, non-ASCII).** With `clangd` on PATH, put a non-ASCII char (emoji/CJK) in
      a `setup`/`hook` body, then hover/go-to-def a C++ symbol after it. ✅ Expect: embedded intel keeps
      working and lands on the right token (the frame decoder now frames by UTF-8 bytes; before, one
      non-ASCII clangd message silently killed the stream). *(Skips gracefully if clangd is absent.)*

---

## Sign-off

- [ ] §0 baseline confirmed green.
- [ ] §A–§C interactive dev-loop / input / combo verified in PIE.
- [ ] §D–§F visual + memory spot-checks acceptable.
- [ ] §G codemod gate behaves.
- [ ] §H CI arming decision made (armed & verified, or consciously deferred).
- [ ] §I IDE surface (only if LSP is in scope for this acceptance).

When §0 is green and §A–§C pass, the round-2 fixes are accepted for merge. §D–§I are confidence layers.
Then the owner-gated release actions remain: **push `feat/uetkx-imports`**, merge → `dev` → `master`, and
any Fab/Discord — none of which this assistant performs.
