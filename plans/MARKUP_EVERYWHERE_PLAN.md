# Markup Everywhere — the third IDE pass + grammar-parity campaign

One plan for the whole remaining batch of the 2026-07-17 owner testing session (TESTING_BUGS.md
is the bug ledger; this is the execution plan). Sections run IN ORDER; each has its Verify.
The commit checkpoint that preceded this plan (5 commits on `feat/click-counter-demo`,
unpushed) is NOT part of this plan.

## §0 — Locked decisions (owner, 2026-07-17)

- **UETKX2316 is an error** (shipped in the checkpoint).
- **clangd ships INSIDE the artifacts** — no runtime downloads, no user-side installs, no
  Node assumptions ("completely self-sustained"). VS Code: platform-specific vsix for
  `win32-x64` bundling clangd, PLUS a universal fallback vsix WITHOUT clangd (Mac/Linux keep
  full markup intelligence + discovery-based clangd). VS2022: `clangd.exe` bundled directly
  (Windows-only, no matrix). ~28 MB compressed / ~94 MB unpacked per bundle is accepted.
- **clangd version is PINNED** (22.1.6, the verified build); bumps are deliberate, with a
  changelog line. LLVM's Apache-2.0-with-exception license text ships beside the binary.
- **Family-core corpus, done right, NOW**: markup-everywhere cases go into the family-core
  tier in ALL THREE repos with the same regenerated `family-corpus.hash` — sibling-repo
  commits stay LOCAL/UNPUSHED (the owner pushes family-wide). No two-stage perLeg parking.
- **Versions**: extensions 0.5.0 (feature release), plugin 0.11.0. One combined release at
  the end of this plan, not piecemeal.

## §1 — Server crash-hardening + a REAL end-to-end test

**Why:** the owner saw a session with ZERO diagnostics (markup and C++). The scanner was
proven correct on the exact input — the failure signature is a dead server process: in Node,
one unhandled exception or promise rejection kills the whole server, and every feature goes
silently dark. The URI silent-drop bug (fixed in the checkpoint) came from the same family:
works in the harness, dies in the editor.

**Do:**
1. try/catch + `connection.console.error` in every entry point: `validate`,
   `publishEmbeddedDiagnostics`, the clangd stdout pump (`onData` dispatch), `syncEmbeddedDoc`
   (await + catch — a `void`-ed rejection is a process kill), each request handler's body.
2. Process-level `unhandledRejection`/`uncaughtException` traps → log, never exit.
3. **stdio E2E test** (new test file): spawn the ACTUAL bundled `out/server.js` over stdio,
   drive the LSP handshake, `didOpen` a mangled CommonUiDemo replica, assert a
   `publishDiagnostics` with UETKX0302 arrives; with clangd present, also assert a clangd-
   sourced diagnostic arrives (skip-not-fail when clangd is absent, so CI stays green).

**Verify:** LSP suite green incl. the new E2E; manually mangle markup + C++ in the editor —
both squiggle families appear; Output → "UETKX Language Server" shows logs, not silence.

## §2 — Markup-expression lifting (C++ intelligence inside `{ … }`)

**Why:** TD-020 lifts only setup/hook/module bodies. Every markup expression —
`OnClicked={ SetActive(!bActive) }`, `{ RUI::Umg::UserWidget(...) }` — is invisible to
clangd: no hover, no completion, no diagnostics. The parser already records offsets
(`UetkxAttr.vat/end`, expr-child `vat`, `UetkxIfBranch.bodyAt`).

**Do (virtualDoc.ts):** per component, after the setup region and INSIDE the same synthetic
function (so setup locals are in scope), emit each markup expression as a mapped region:
- plain attr exprs / expr children / spreads / `key={}`: `{ (void)( <expr> ); }`
- event handlers (`On[A-Z]*={ … }` — D-33 makes the name test exact): wrapped in a lambda
  whose `Value` parameter uses the SAME type codegen emits (read it from a committed `.inl`,
  do not guess) — `Value.TextValue`-style member access then types for real.
- directive interiors: recurse via each branch's `bodyAt` re-parse; directive conditions
  (`@if (cond)`) lift as `(void)(cond);` regions.
- CONSISTENCY RULE (already load-bearing): every `buildEmbeddedView` for a doc must produce
  byte-identical text to what clangd holds — the resolver/fsPath threading pattern from the
  imported-surfaces work applies unchanged.

**Verify:** unit tests (lift shapes + round-trip mapping); live clangd smoke: hover on
`SetActive` inside `OnClicked` resolves with a real type; a typo inside an attr expr
squiggles at the right offset; battery + LSP suites stay green.

## §3 — Tools → "ReactiveUI Message Log" menu entry

Add a third entry beside `ReactiveUetkx Hot Reload` / `ReactiveUI Preview` (same
registration site in `ReactiveUIEditor`) that calls `FMessageLog("ReactiveUI").Open()`.
Engine-menu archaeology ends; the red failure toast remains the shortcut.

**Verify:** editor boot → Tools menu shows the entry → opens the ReactiveUI page.

## §4 — Markup everywhere (the parity port — the campaign's namesake)

**The model (owner, 2026-07-17):** a BODY is a BODY, recursively. A component body is
{ C++ statements + markup returns + markup VALUES }; a directive body (`@if/@elif/@else/
@for/@foreach/@while/@match` arm) is EXACTLY the same construct — the directive header
plays the role of the component signature. Markup behaves identically at every position:
same directives, same components, same diagnostics. **One exception: hooks are TOP-LEVEL-
BODY-ONLY** — no `Use*` calls inside directive bodies or markup expressions (rules-of-hooks:
directives are conditional/looping contexts). Unity's analyzer enforces hook placement via a
HookContext walk; our leg must diagnose the same way (research the exact UITKX code + message
and mirror it with the family number).

**Research first (both siblings, exact semantics — no more single-line-grep conclusions):**
- Unity `language-lib` parser/analyzer + `DeepNestedSection.uitkx`: accepted positions
  (parenthesized `= ( <JSX> )`, bare `= <JSX>`, bare `return <JSX>` inside directives,
  `{variable}` splice), how their codegen lowers a markup value, their hook-placement codes.
- Godot `addons/reactive_ui` parser: same checklist (owner states support is 100% there).
- Unity's corpus/test cases for these shapes — the family-core cases should MATCH their
  semantics, not approximate them.

**Do (Unreal leg, C++ + TS in lockstep):**
1. **Scanner** (`FUetkxFileScan` + `uetkxFileScan.ts`): generalize the markup-return
   collector into a body-walker that also collects markup at VALUE positions (`= (<…>)`,
   `= <…>`) and treats directive bodies as nested bodies (recursive). `UETKX0114` narrows to
   the positions that stay illegal (markup outside any body/value/return context). New
   hook-placement diagnostic (family-numbered per Unity) for `Use*` below top level.
2. **Codegen** (`UetkxCodegen`/`UetkxDriver`): lower a markup value in place to an
   `FRuiNode` local (the same node-building emission as return-position markup, minus the
   `return`). RUNTIME IS UNTOUCHED — `{ X }` children already splice `FRuiNode` values.
3. **Formatter** (both): markup in initializer position formats with the same rules as
   return markup (indent anchored at the statement); corpus cases pin it.
4. **LSP**: virtual-doc lifting from §2 extends naturally (a markup value's expressions are
   body content); grammar/TextMate already highlights `= (<Tag` correctly (verified).
5. **Corpus, family-core, ALL THREE repos**: author the shared cases (value-position markup,
   bare forms, directive-body recursion, hook-placement violations, 0114-remainder), add to
   each repo's corpus with the code-prefix substitution, regenerate the SAME
   `family-corpus.hash` in all three, run each leg's suite (Unity: dotnet test on
   language-lib; Godot: its runner if the local engine binary cooperates — otherwise record
   the gap explicitly for the owner). Sibling commits local, unpushed.
6. **Demo**: AcceptanceLab (the every-feature screen) gains a markup-value section — a node
   local built in setup, spliced via `{ … }`, plus one inside a directive body; acceptance
   pins update accordingly.

**Verify:** battery green with the new AcceptanceLab section; `RUICompile -check` 0 drift;
corpus hash identical across the three repos; LSP suite green; the owner's original
ClickCounter experiment (`auto X = (<VerticalBox>…); … { X }`) COMPILES AND RENDERS.

## §5 — clangd bundling (per §0 decisions)

1. Layout: `ide-extensions/vscode-uetkx/clangd/<platform>/` staging (gitignored — binaries
   never enter git; a fetch script downloads the PINNED release + verifies + stages, run at
   package time and in CI).
2. Packaging: `vsce package --target win32-x64` (clangd staged in) + a universal package
   (clangd absent) — script drives both; `.vscodeignore` gates the binary per flavor.
3. Discovery order gains "extension-bundled dir" FIRST (then setting → PATH → managed →
   clangd-extension storage → LLVM → VS). The no-clangd notice only fires when nothing at
   all is found (universal build on a machine with no clangd anywhere).
4. VS2022: stage `clangd.exe` into the VSIX (single platform); the vendored server's
   discovery learns the VSIX install dir.
5. publish.yml: two vsce publishes (win32-x64 + universal) — marketplace serves per
   platform automatically; licenses (LLVM Apache-2.0 w/ exception) ship beside the binary;
   listing notes the bundled clangd version.

**Verify:** both vsix flavors install & run — win32-x64 lights C++ intel with ZERO setup on
a clean machine (no PATH clangd, no LLVM); universal degrades to markup-only with the
notice; VS2022 VSIX carries clangd and its server finds it.

## §6 — Release 0.5.0 / 0.11.0

- `bump.mjs vscode 0.5.0` (+ vs2022, lsp lockstep), `bump.mjs plugin 0.11.0`.
- Lane A `[0.11.0]`: 0107/0114/2316 + hook-placement diagnostics, markup-everywhere grammar
  + codegen, watcher click-tokens, Tools menu entry. Mirror + verify.
- Lane B (one `changelog.mjs add` per change): embedded-C++ channel (discovery/diagnostics/
  imported surfaces/expression lifting), hover rebuild, new diagnostics, bundled clangd,
  crash-hardening. Extract + extract-overview + verify.
- Discord entry (≤2000 chars), TESTING_BUGS statuses closed, DiagnosticsPage/docs sync
  (hook-placement code, markup-everywhere docs page section), README claims re-checked
  against docs-drift.
- Full ladder: LSP suites, corpus hash ×3 repos, `RUICompile -check`, battery, engine-free
  gates, package + install both flavors.

## Resume protocol

Sections are ordered and independent enough to resume between them; establish ground truth
by RUNNING each section's Verify, never by trusting this prose. The bug ledger with root
causes is `plans/TESTING_BUGS.md`; decisions above override anything older.
