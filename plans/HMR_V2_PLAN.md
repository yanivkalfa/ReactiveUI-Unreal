# HMR v2 + ReactiveUetkx editor menu/window — implementation plan

> **Status:** FINALIZED for build (owner decisions locked, §2 + §7). Replaces the interpreter-based
> hot-swap (deleted) with a Live-Coding-driven, whole-project HMR, and adds a `ReactiveUetkx` editor
> menu + a Start/Stop **HMR window** + two rebindable shortcuts — **modeled 1:1 on the Unity sibling**
> (`ReactiveUIToolKit`, `Editor/HMR/`) so the three-engine family stays UX-parallel.
>
> **Why:** the shipped HMR made a single-file *interpreter* the default. It can't follow an `import`,
> can't run user hooks/effects, and silently replaced working compiled components with dead ones — not
> React parity. For a compiled-C++ library the correct engine is the real compiler, hot-patched:
> **Unreal Live Coding**, automated. This makes that the only path.

---

## 1. The model (locked with the owner)

**HMR is a mode you turn ON/OFF. On = the Live Coding session is enabled. While on, ANY `.uetkx`
event — save, new, delete, rename, copy, move — triggers a recompile; Live Coding patches it into the
running editor; the reconciler refreshes. Off = normal compilation.** That's the whole thing. No
interpreter, no per-edit "can we patch this?" branching — we just recompile on every change and let
Live Coding do its job.

```
[ Start HMR ]  → EnableForSession(true)   (Live Coding mode ON; external builds pause)
   while ON, any watcher event (save / new / delete / rename / copy):
      debounce+coalesce → incremental RUICompile (regen .inl + aggregator + importers)
                        → ILiveCodingModule::Compile()   (no keystroke)
                        → on patch-complete → refresh reconciler roots (+ state preserved)
[ Stop HMR ]   → mode OFF; compile normally.
```

### 1.1 The Unity-sibling reference we mirror (UX, not engine)
From `ReactiveUIToolKit/Editor/HMR/`:
- **Menu** — one top-level `ReactiveUITK` menu (`HMR Mode`, `UI Toolkit Debugger`, `Debug/…`,
  `Demos/…`). → ours: top-level **`ReactiveUetkx`**.
- **Window** `UitkxHmrWindow.cs` (`"UITKX Hot Reload"`) — a **Start/Stop** window: big green
  `● Start HMR` / red `■ Stop HMR`, `● ACTIVE`/`Idle` status, stats (`Watched` / `Swaps` / `Errors` /
  `Last: <Component> (Nms)`), RAM readout, settings toggles, **shortcuts rebindable in-window** (a
  key recorder), a `"compilation is locked while active"` warning, and a recent-errors scroll. Closing
  the window auto-stops; the controller persists across Start/Stop.
- **Controller** `UitkxHmrController.cs` — `Start(out error)`/`Stop()`, `Active`, `SwapCount`,
  `ErrorCount`, `Last…`. Start enables the fast path + locks normal compilation; Stop releases it.
- **Shortcuts** `UitkxHmrKeybinds.cs` — two chords (Toggle-HMR, Toggle-Window) in prefs, **default
  unbound**, set from the window.

The engine differs and that's expected: Unity uses Roslyn + in-place method-swap with the domain
reload suppressed; we use Live Coding. Same *shape* (a mode that locks normal compilation while
active), so the whole window/warning/UX ports directly. We do **not** import Unity's "rude edit"
concept — for us every change is just "recompile," and Live Coding decides internally how much to
rebuild.

---

## 2. Locked decisions

- **D-HMR-1 — Delete the interpreter executor.** Remove `FUetkxInterpDef`, the interp element builders,
  and the interp-swap in `FRuiHmr`. Keep the lexer/parser/scanner (the compiler needs them).
- **D-HMR-2 — Live Coding is the HMR engine.** Any `.uetkx` change while active → incremental recompile
  → `ILiveCodingModule::Compile()` → patch → refresh. Full fidelity (imports, hooks, effects, new/
  deleted/renamed/copied files) because it's the real compiler.
- **D-HMR-3 — Start/Stop MODE.** Toggle turns the Live Coding session on/off (family parity + honest
  about the "external builds pause while on" tradeoff — you explicitly start the mode, we never flip it
  silently). While on, reloads are automatic; no keystroke, no manual Compile.
- **D-HMR-4 — Trigger on ANY file event, debounced + coalesced.** save / new / delete / rename / copy /
  move under the watched roots. Coalesce bursts; never overlap compiles; always land the latest edit.
- **D-HMR-5 — Fail safe.** A compile error (ours or the user's own C++, since Live Coding is global)
  keeps the old UI running, surfaces it in the window + Message Log, retries next change. Never a
  half-patched tree.
- **D-HMR-6 — Two rebindable shortcuts, default unbound**, set in the window (and mirrored in Editor
  Preferences ▸ Keyboard Shortcuts).
- **D-HMR-7 — Menu is `ReactiveUetkx`.** Top-level main-menu holding HMR + Preview + Debug (+ Demos
  later, TD).
- **D-HMR-8 — Preview reworked to the COMPILED component** (interpreter gone). Renders the real
  component under the P2 design-time guard (children stubbed so their effects don't run at edit time).
- **D-HMR-9 — HMR stays active during PIE.** Reloading while playing is the entire point, so no
  auto-stop-on-Play (the sibling's default is auto-stop; we deliberately diverge). No such setting.

---

## 3. Phase 1 — the engine (`FRuiHmr` / `UetkxWatcher`, Live-Coding-driven)

- **Start/Stop:** `FRuiHmr::Start()` → if Live Coding available + `CanEnableForSession` →
  `EnableForSession(true)`, arm the watcher, `Active=true`, fire `OnStatus`; else fail with a clear
  reason shown in the window. `Stop()` → disarm the watcher, `Active=false` (leave/disable the session
  per a setting). Auto-stop when the last window closes (sibling parity).
- **Watcher:** whole project (`WatchedRoots`, default `Source/` + `Plugins/`), all event kinds. Added →
  scan+compile+register; Removed → orphan sweep + unregister; Rename/Copy/Move → the corresponding
  Added/Removed pair. Debounce ~250–300 ms; coalesce; `IsCompiling()` → set `bDirtyAgain`, re-fire on
  patch-complete. No overlap, no dropped edits.
- **Compile → patch → refresh:** incremental `FUetkxDriver` regen (the round-2 reverse-edge staleness
  graph recompiles every importer of a changed dep) → `LiveCoding->Compile()` → the already-wired
  `GetOnPatchCompleteDelegate` → `ForEachLive` → `HmrRefreshAll` + `FlushSync`. Hook state is
  heap-resident → preserved on stable hook shape, reset on a real shape change (TD-019, compiled→
  compiled). Bump `SwapCount`, emit `FRuiHmrStatus` via `OnStatus`.
- **Preview rework (D-HMR-8):** `FUetkxPreview` renders the compiled component (`RUI::Named` under the
  design-time guard) instead of the interpreter; it reflects the latest edit after an HMR recompile.
- **Tests (headless):** driver added/removed-file suites; the debounce/coalesce state machine with a
  mock clock + mock compiler; `OnStatus` emission. Live Coding itself is owner-verified in-editor.

## 4. Phase 2 — the `ReactiveUetkx` menu + the HMR window

- **Menu** (`FReactiveUetkxMenu`, UToolMenus on `MainFrame.MainMenu`): `HMR Mode` (opens the window),
  `Preview` (existing tab), `Debug/Check Registry`. (`Demos/…` deferred — TD.)
- **Window** `SReactiveUetkxHmrPanel` (nomad `SDockTab`, "ReactiveUetkx Hot Reload"), 1:1 with the
  sibling:
```
┌ ReactiveUetkx Hot Reload ────────────────────────────┐
│            [ ● Start HMR ] / [ ■ Stop HMR ]           │
│                     ● ACTIVE  |  Idle                 │
│ Watched   Source/**/*.uetkx, Plugins/**/*.uetkx       │
│ Swaps 7   Errors 0   Last  SimpleCounter (1180 ms)    │
│ RAM   2410 MB (+120 since open)                       │
│ [x] Show swap notifications                           │
│ [ ] Verbose watcher trace                             │
│ Toggle HMR   [ None ] [×]   (click to record)         │
│ Open Window  [ None ] [×]                             │
│ ⚠ External builds pause while HMR is active. Stop to   │
│   build normally.                                     │
│ Recent Errors: 18:47 Palette.uetkx UETKX0105 …         │
└──────────────────────────────────────────────────────┘
```
  Start/Stop → `FRuiHmr::Start/Stop`; stats bind via `OnStatus`; `Tick` refreshes `IsCompiling()` + RAM
  on an interval (no per-frame repaint). Settings trimmed to what our model needs (no "rude edit", no
  "auto-stop on Play").

## 5. Phase 3 — shortcuts + settings

- **`FReactiveUetkxCommands : TCommands`** — `ToggleHmr`, `ToggleHmrWindow`, default **unbound**; bound
  via `FInputBindingManager` (Editor Preferences ▸ Keyboard Shortcuts) AND the in-window key recorder
  (port of the sibling's `KeyCombo` + capture UI); the two stay in sync.
- **`UReactiveUetkxEditorSettings : UDeveloperSettings`** (EditorPerProjectUser, Project Settings ▸
  Plugins ▸ ReactiveUetkx): `bShowNotifications`, `bVerboseWatcher`, `bDisableSessionOnStop`,
  `DebounceMs`, `WatchedRoots`, the two shortcut chords.

## 6. Files

**New:** `SReactiveUetkxHmrPanel.h/.cpp`, `ReactiveUetkxMenu.h/.cpp`, `ReactiveUetkxCommands.h/.cpp`,
`ReactiveUetkxEditorSettings.h/.cpp`, `RuiKeyCombo.h`.
**Modified:** `ReactiveUIEditorModule.cpp` (register menu/window/commands/settings),
`UetkxWatcher.cpp` (debounce/coalesce, all event kinds, Live-Coding-primary), `RuiHmr.h/.cpp`
(Start/Stop mode, Live-Coding refresh/migrate, `OnStatus`, `SwapCount`; remove interp swap),
`UetkxPreview.*` (compiled-component rework).
**Deleted:** `UetkxInterpComponent.h/.cpp`, `UetkxInterpElements.*`. `ReactiveUIInterp` shrinks to
**parser-only** (keep `UetkxFileScan`/`UetkxLexer`; the toolchain still consumes them).

## 7. Owner decisions — RESOLVED
1. **Menu name** → `ReactiveUetkx`. ✔
2. **Interpreter** → deleted. ✔ (D-HMR-1)
3. **`ReactiveUIInterp` module** → kept **parser-only** (avoids include-graph churn). ✔
4. **Auto-stop on PIE** → **no** — HMR stays active during PIE (reloading while playing is the point).
   ✔ (D-HMR-9)
5. **Shortcuts** → **unbound**, bound in the window. ✔ (D-HMR-6)
6. **Demos submenu** → **deferred to TECH_DEBT** (TD-HMR-DEMOS). ✔

## 8. Phasing
1. **Phase 1 — engine** (the substance): delete interpreter, rework preview to compiled, watcher
   debounce + all event kinds, Live-Coding Start/Stop + refresh + fail-safe + `OnStatus`. Headless tests.
2. **Phase 2 — menu + window**: `ReactiveUetkx` menu; `SReactiveUetkxHmrPanel`.
3. **Phase 3 — shortcuts + settings**: commands + settings + in-window rebinding.

## 9. Verify (when built)
- Headless: driver added/removed-file suites + debounce/coalesce unit test + `OnStatus`.
- In-editor (owner): **Start HMR** → edit a component with an imported hook + effects → save → reloads
  with state preserved, **no keystroke**; **create/copy/rename/delete** a `.uetkx` → each handled with no
  restart; broken save → old UI survives + error in the window; Swaps/Last/Errors/RAM update; the
  "builds locked" warning shows while active; reload works **during PIE**; **Stop HMR** → builds
  normally; both shortcuts + in-window rebinding work.
