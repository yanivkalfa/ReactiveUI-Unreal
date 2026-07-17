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
  matching codegen's emitted shape — VERIFIED from SimpleTextField.uetkx.inl:
  `[=](const FRuiValue& Value) { … }` — so `Value.TextValue`-style access types for real.
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

**Research (RESOLVED 2026-07-17 — the family algorithm exists and is documented):**
`ReactiveUI-Gadot/addons/reactive_ui/guitkx/guitkx_jsx_scan.gd` is the reference
implementation: markup inside an expression is detected via a **POSITION-GATED WHITELIST** —
a `<` begins markup ONLY when it follows (whitespace-skipped) a boundary token that can only
be followed by an expression (assignment, `return`, `(`, `,`, ternary/short-circuit
operators), AND the char after `<` is a tag-name start or `>` (fragment). Comparisons
(`a < b`, `i < n`) can never match because no boundary token precedes them. Its header says
"**Like uitkx**" — Unity uses the same gate. It also specifies: unbalanced markup after a
boundary token is a DIAGNOSTIC, never verbatim emit (their T1.2), and short-circuit
`and`/`&&` markup records an `op` for ternary desugar at emit (our wave G already retired
UETKX3002 for this — partial overlap to reconcile, not duplicate). Port THIS gate into both
Unreal scanners; remaining research during execution: Unity's hook-placement code numbers +
their corpus cases for these shapes (family-core cases must MATCH, not approximate).

**Do (Unreal leg, C++ + TS in lockstep):**
1. **Scanner** (`FUetkxFileScan` + `uetkxFileScan.ts`): generalize the markup-return
   collector into a body-walker that also collects markup at VALUE positions (`= (<…>)`,
   `= <…>`) and treats directive bodies as nested bodies (recursive). `UETKX0114` narrows to
   the positions that stay illegal (markup outside any body/value/return context). New
   hook-placement diagnostic (family-numbered per Unity) for `Use*` below top level.
2. **Codegen** (`UetkxCodegen`/`UetkxDriver`): REUSE the wave-G cursor-walk (the multi-
   return path at the tail of the component emitter — verbatim segments spliced between
   spans, each span lowered in place; the comment names Unity's `SpliceSetupCodeMarkup`) with
   a second span kind: a VALUE span lowers to its `EmitNodeExpr` in initializer position
   instead of `return { … };`. `EmitBody` already gives directive bodies the same treatment.
   RUNTIME IS UNTOUCHED — `{ X }` children already splice `FRuiNode` values.
3. **HMR PROTECTION (research finding, 2026-07-17 — MANDATORY):** the hook signature
   (`ScanHookCalls` over `Decl.Setup`) currently scans markup spans too — with markup values
   in setup, an edit INSIDE markup could perturb the signature and cause spurious
   Live-Coding state resets. Fix in the same change: hook-signature scanning AND
   `PrefixHookCalls` treat ALL collected markup spans (returns + values) as excluded
   regions — signatures derive from CODE only. Note: this corrects a latent wart (early-
   return markup already leaks into the signature domain today), so components with early
   returns get a ONE-TIME signature change at upgrade — one HMR state reset, changelogged.
   Confirm the signature's runtime consumer during execution (grep HookSignature uses) and
   pin the new exclusion with a scanner corpus case.
4. **Formatter** (both): markup in initializer position formats with the same rules as
   return markup (indent anchored at the statement); corpus cases pin it.
5. **LSP**: virtual-doc lifting from §2 extends naturally (a markup value's expressions are
   body content); grammar/TextMate already highlights `= (<Tag` correctly (verified).
6. **Contract + compiled proof**: a ContractFixtures golden pinning the value-lowering
   codegen shape, and a GrammarProof case (compiled through MSVC and mounted at runtime,
   the MultiReturnProof pattern) — sweep pins bump accordingly.
7. **Corpus, family-core, ALL THREE repos** (paths verified: Godot has
   `scripts/corpus-hash.mjs` + `plans/family-corpus.hash`; Unity's corpus is
   `ide-extensions~/lsp-server/test-fixtures/uitkx-scanner-cases.json`): author the shared
   cases (value-position markup,
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

**DONE (2026-07-17) — execution notes, all Verify items green:**
- **The port was smaller than planned**: `FUetkxJsxScan` (the guitkx_jsx_scan.gd port) already
  existed with `=` in its boundary set — codegen only needed the five verbatim-splice sites
  (single-return setup, multi-return segments + tail, EmitBody lead + no-return) routed through
  `EmitExpr`, which degenerates to `PrefixHooks` byte-identically when no markup is present.
  **All 14 pre-existing contract goldens stayed byte-identical**; the new `ValueMarkup` fixture
  pins the IIFE value-lowering (`auto Card = ([&]() -> FRuiNode { … }());`).
- **Hook placement = the family codes**, not a new number: UETKX0013 conditional / 0014 loop /
  0015 match / 0016 callback (Unity UITKX0013-0016, Godot GUITKX T2.5), C++-shaped as a
  brace-stack walk (if/else/for/while/do/switch + lambda intro detection) plus a parsed-tree
  walk over every markup region (event attrs → 0016; plain attr/child exprs → 0013 "inside
  markup"; directive bodies → the directive's kind, recursively via the EmitBody split).
- **0114 narrowed**, not deleted: the one still-illegal spelling is a paren-less statement-level
  markup return (`return <Tag/>;` → "must be parenthesized"); it would otherwise die as an
  FRuiNode→FRuiNodeArray conversion error inside the .inl. Lambda-nested bare returns (deduced
  return type) lower fine and stay legal.
- **HMR protection (§4.3)**: `ScanHookCalls` skips all jsx ranges (C++ + TS), corpus-pinned via
  the new `hookCalls` expectation field in both harnesses. 0107's walker skips ranges too (its
  depth tracking would otherwise lex markup text as C++).
- **Virtual doc**: every code emission is jsx-aware (`emitCode`) — ranges neutralize to the
  prelude-declared `extern FRuiNode __rui_rn;` placeholder (`? __rui_rn : __rui_rn` for
  short-circuit ops), code segments stay exact mapped substrings, and each range's parsed tree
  lifts at the nearest statement context. Statement-level early-return windows in setup (incl.
  directive-form `return ( @if … )`, a pre-existing gap) neutralize whole. 7-file clangd sweep
  CLEAN (AcceptanceLab now 88 regions).
- **Formatter = family parity**: Godot's formatter does NOT restructure value markup — setup is
  reanchored code in the whole family. Pinned with an idempotence corpus case rather than
  inventing a divergent restructuring. (A family-wide value-markup formatter is future work.)
- **Corpus ×3 reconciled INCLUDING the wave-G drift**: the two wave-G family cases had never
  been mirrored (Unreal hashed 499fe2e4…, siblings 657e5f4e…). Mirrored those + the new
  S4 value-markup case into both siblings; all three repos now hash
  `917dd8cd…` and their suites pass unchanged scanners (Unreal battery 128/128, Godot TS
  182/182, Unity dotnet 123/123) — both sibling scanners already implemented the semantics.
  Godot has no GDScript-side consumer of the fileScan corpus (TS suite is its runner there).
- **AcceptanceLab §10**: Badge/Chip FRuiNode locals (paren + bare spellings), the same Badge
  spliced twice, short-circuit + ternary value markup inline — compiled by the sweep
  (`RUICompile`: 1 compiled, 0 errors) and rendered by the Demos suite.

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

**DONE (2026-07-17) — execution notes:**
- `fetch-clangd.mjs`: pin = 22.1.6, sha256 `ce54f16e…`, cached zip; stages both layouts;
  upstream `LICENSE.TXT` preserved (a same-named note file CLOBBERED it on the
  case-insensitive FS — the note is `BUNDLED-CLANGD.txt`). Staging dirs gitignored; the
  header lint's EXCLUDE_DIRS gained `clangd` (LLVM's headers are not ours to stamp).
- Discovery: bundled dir wins over all MACHINE discovery, but an explicit
  `uetkx.clangd.path` stays authoritative (the plan's literal "bundled first, then setting"
  would let the bundle override a user's explicit choice — intent over letter).
- Packaging scar: vsce packages ANY in-tree dir, dot-named or not — the first universal
  vsix shipped all 28MB of clangd because the aside dir sat inside the extension root. The
  gate is physical and the aside dir lives at `ide-extensions/.clangd-staged-aside`.
- Sizes: win32-x64 28.67MB (555 files) / universal 423KB (224 files). Verified from the
  extracted vsix: the packaged server discovers ITS OWN clangd (over the machine's
  LOCALAPPDATA install) and the binary runs.
- XML scar: the csproj comment naming the fetch flag broke msbuild — `--` is illegal inside
  XML comments (MSB4025), and the failed build still left a STALE bin/Release vsix behind
  for the locate step to find; the script's exit code is what catches it.

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
