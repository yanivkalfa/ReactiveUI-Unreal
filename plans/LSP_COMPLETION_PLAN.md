# LSP feature completion + scope-aware rewrites — EXECUTION PLAN (TD-033 + TD-034)

> **✅ COMPLETE (2026-07-18):** N0–N7 all landed on `feat/es-modules` (commits `0030779`
> N0–N3, `3f4dc57` N4, `792d227` N5, `f07a7d0` N6 incl. the clangd-rename stretch, plus the
> N7 close-out). TD-033 RESOLVED; TD-034 RESOLVED-with-residual (`plans/TECH_DEBT.md`).
> Corpus hash re-verified `f8ae9961…` — unmoved.
>
> **Status:** EXECUTION-GRADE, authored 2026-07-18 against the live tree at `feat/es-modules`
> HEAD (post-ES-modules M0–M9; plugin **0.12.0** / extensions **0.6.0**, both UNRELEASED).
> Every file:line anchor below was verified against the working tree on 2026-07-18.
> **Branch:** CONTINUE ON `feat/es-modules` (owner decision 2026-07-18 — no new branch; this
> work rides the same PR). **Never merge; push commits to the branch only.**
>
> **What this closes:** **TD-033** (the LSP has NO rename handler — and the audit found four
> more missing workspace features that share its root cause: no find-all-references, no
> document symbols, no workspace symbols, no code actions) and **TD-034** (three accepted
> caveats in the rewrite/reference planes: local-variable shadowing, false 2305 on bare names
> that shadow exported values, and 2304/2305 blindness to markup attr expressions).
>
> **Audience:** an executing agent following research → develop → test → bughunt → fix →
> commit per milestone (`dev-process` skill); never weaken a gate; production-grade only.

---

## §1 — Locked decisions (N-01..N-10; do not re-litigate)

- **N-01 — Same branch, same release.** 0.12.0 / 0.6.0 have NOT shipped (the owner has not
  merged or published). This work folds INTO those versions: EXTEND the existing Lane A
  `## [0.12.0]` section and Lane B 0.6.0 entries — **no additional version bump**, no new
  changelog sections. CodegenVersion stays 3 (it never shipped; the fingerprint already
  re-stales everything).
- **N-02 — The workspace reference index is the foundation** and is built FIRST. Every new
  feature (references, rename, symbols, code actions) is a query over it. It is the family
  reference design (Godot/Unity servers adopt the same architecture in their legs; per-engine
  lexis only).
- **N-03 — Index shape.** Per swept file (the `sweptUetkxFiles` universe), mtime-cached like
  `cachedScan`: the decl list (name/kind/exported/nameAt — already exists) PLUS a reference
  list, each entry `{ name, kind, start, len }` with kind ∈ `tag` | `code` (bare/call/qual
  refs in verbatim regions incl. markup attr/event/directive expressions) | `import-binding`
  (the TARGET-name spelling in an import) | `import-alias` (the LOCAL alias spelling: `as`
  names, `* as` alias, default alias) | `export-list` | `export-default` | `decl-name` (the
  declaration's own name token). Alias links resolve at query time via `resolveSpecifier` +
  the import records (never baked into the index — files change independently).
- **N-04 — Rename semantics (alias-aware, ES parity).**
  - Rename at a **declaration name** (or any reference that resolves TO that declaration
    through imports): renames the decl token, every importer's TARGET-name spelling
    (`import { Old ... }` → `import { New ... }`), every direct reference in every file, every
    `export { Old }` / `export default Old;` occurrence. A RENAMED import (`{ Old as B }`)
    changes only its target spelling — local `B` refs stay untouched.
  - Rename at a **local alias** (`B` of `{ A as B }`, a `* as X` alias, a default alias):
    edits ONLY the importer file — the alias token + that file's references. Renaming a
    plain named binding's local spelling (no `as`) rewrites it to `{ A as NewLocal }` + local
    refs (the ES rename-at-use-site behavior).
  - `prepareRename` rejects: positions outside any reference/decl token, host-include
    specifiers, markup text, keywords, and anything inside embedded C++ that is NOT a
    `.uetkx`-level symbol (clangd forwarding for C++ locals is N-10 stretch scope).
  - The result is ONE `WorkspaceEdit`; files are read at query time (never stale index text);
    any mtime mismatch between index and disk → re-scan before producing edits.
- **N-05 — Validation collision rule.** Rename REFUSES (returns null + a `window/showMessage`
  warning) when the new name: is not an identifier; collides with a declaration or import
  binding in ANY edited file (the 2325/2106 shapes); or (component renames) collides with a
  host tag name. Never produce an edit that the compiler would immediately reject.
- **N-06 — Code actions (the fix-it set, quickfix kind), derived from existing diagnostics —
  NO new diagnostic codes anywhere in this plan:**
  - `UETKX2305` → "Add import { X } from "<spec>"" (the message already carries the exact
    line; insert at `ScanPreamble().FirstDeclAt`, merging into an existing same-spec import).
  - `UETKX2304` → "Remove unused import binding" (drop the binding; drop the whole line when
    it was the only one).
  - `UETKX2301` → "Add `export` to the declaration in <target>" (cross-file edit).
  - `UETKX2320` → "Migrate this declaration to the plain grammar" (single-decl rewrite using
    the SAME record-driven forms as the codemod — never regex).
- **N-07 — Scope-aware local-declaration tracker (TD-034 #1/#2), C++ side.** One shared
  detector over verbatim-C++ code points, brace-scoped, recognizing the pragmatic subset:
  `Type Name [=;,)]`, `auto Name`, `auto [A, B]` structured bindings, function/lambda
  parameter lists, for-init declarations, `const`/`&`/`*` decorations. It is a HEURISTIC by
  design (no clang; the family model) — every recognized pattern gets a test pin; an
  unrecognized shadow remains a documented residual (TD-034 narrows, the entry stays with a
  "residual" note rather than closing as perfect).
  - Consumers: `RewriteAliases` + the `Qualified` branch of `PrefixHookCalls` (skip rewrite
    when a local declaration of that name is in scope at the reference) and
    `CollectExternalRefs` (suppress `Bare`/`Call` refs of locally-declared names — fixes the
    false 2305).
  - The TS LSP mirror gets the same tracker for its reference collector (N-03 `code` refs) so
    find-references/rename don't hit locals either.
- **N-08 — Attr-expression usage scanning (TD-034 #3).** `FUetkxResolve::Apply` and
  `MissingImports` additionally walk each component's parsed markup windows
  (`Returns[..].Root` / `WindowNodes`) — attr exprs, event exprs, directive headers/bodies,
  expr children — through `CollectExternalRefs`, using the EXACT walking shape
  `ValidateMarkupNodeHooks`/`ScanMarkupCodeForHooks` already pin (UetkxFileScan.cpp:712-846).
  TEXT children are NEVER scanned (prose can't reference). This both kills the false 2304
  warns and gains real 2305 policing where value references actually live. Same walk in the
  TS reference collector.
- **N-09 — Behavior-change accounting.** #1 changes EMISSION only where a shadow exists
  (none in the real tree — the migrated 42 compile green today, so the fix is byte-neutral
  there; a NEW ContractFixture pins the shadow-skip). #2/#3 change RESOLVER diagnostics only
  (not scanner output) → **the family corpus hash does NOT move** (`fileScan` cases are
  scanner-level); pin the new behavior in `ReactiveUIUetkxResolveTest.cpp` + LSP tests
  instead. If ANY change would move `corpus-hash`, STOP — that means scanner behavior
  drifted, which this plan must not do.
- **N-10 — Stretch (build LAST, skippable without failing the plan):** embedded-C++ rename
  for locals inside one body — forward `textDocument/rename` to clangd over the virtual doc,
  translate the WorkspaceEdit back through the source map (the completions pipeline's shape,
  single-file only). If skipped: note in TD-033's closing entry.

## §2 — Verified anchor table (2026-07-18)

| Surface | File | Anchors |
|---|---|---|
| Server capabilities + handlers | `ide-extensions/lsp-server/src/server.ts` | `onInitialize` capabilities L113-125 (add `renameProvider: { prepareProvider: true }`, `referencesProvider`, `documentSymbolProvider`, `workspaceSymbolProvider`, `codeActionProvider`); `validate` L208; `onDidChangeContent` L253 (index invalidation hook); `onCompletion` L269; `onHover` L438; `onDefinition` L560 + `markupDefinition` L626 (the alias-resolution logic to REUSE for "which decl does this position mean"); `onDocumentFormatting` L744; `fsPathOf` L127 |
| Workspace layer | `ide-extensions/lsp-server/src/uetkxWorkspace.ts` | `cachedScan` L42 (mtime cache the index extends — add the reference list to `CacheEntry`); `getDecls` L75; `defaultExportOf` L81; `workspaceRootFor` L107; `resolveSpecifier` L128; `suggestSpecifier` L147; `importedSurfaceFor` L210; `sweptUetkxFiles` L298; `findExporter` L309; `resolveDiagnostics` L326 |
| TS scanner (reference sources) | `ide-extensions/lsp-server/src/uetkxFileScan.ts` | `scanFile` result: imports (names/nameAts/localNames + namespace/default aliases + Ats), components (nameAt, setup/setupAt, returns[].root, windowNodes), hooks/modules/values/utils (nameAt, body/bodyAt or init/initAt), defaultExportName/At. The markup node type (`uetkxMarkup.ts` `UetkxNode`) carries attrs with value offsets — the attr-expr walk source |
| C++ rewrite planes (TD-034 #1) | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp` | `FUetkxAliasPlane` :539; `RewriteAliases` :554 (scan-back :577-592 — insert the scope check here); `PrefixHookCalls` :634 (`ScanBack` :652-668; the generalized `Qualified` branch below it); `FEmitter::EmitExpr` :1027 (attr/expr path — all routed through `PrefixHooks`) |
| C++ reference collection (TD-034 #2/#3) | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxResolve.cpp` | `FExtRef` :61 (Bare :160 / Call :156); `CollectExternalRefs` :81; `Apply` :398 (region list :578-598 — ADD the markup-window walk; ref loop :600; kind switch :645); `MissingImports` :701 (regions :778; kind dispatch :796-806) |
| Markup-walk precedent (N-08) | `Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp` | `ScanMarkupCodeForHooks` :719; `ValidateMarkupNodeHooks` :712/:814 (attr walk with `Attr.Vat` :837) — copy this SHAPE for the usage walk; `WindowNodes`/`Returns` in `UetkxFileScan.h` :92-106 |
| Codemod single-decl rewrite (N-06 2320 action) | `Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/RUIMigrateImportsCommandlet.cpp` | pass-3 rewrite forms (component/hook header flip + `HoistModuleMembers`) — the LSP action mirrors ONE decl's rewrite in TS; keep the two spellings identical (a corpus-style pin: the action's output must equal the codemod's for the same input) |
| Tests | `Source/RuiHostTests/Private/ReactiveUIUetkxResolveTest.cpp` (M4 block — extend); `...CodegenTest.cpp` (alias/ternary block — extend with shadow pins); `ide-extensions/lsp-server/src/test/` (`server.test.ts`, `contract.test.ts`, `embeddedCpp.test.ts`; NEW `index.test.ts` + `rename.test.ts`) |
| Fixtures | `Source/RuiHostTests/ContractFixtures/` — NEW `ShadowedNames.uetkx` (+ golden) pinning N-07's skip behavior |
| Ledger / changelogs | `plans/TECH_DEBT.md` TD-033 + TD-034 (close with evidence; TD-034 keeps a "residual: unrecognized-shadow shapes" note); root `CHANGELOG.md` `## [0.12.0]` (EXTEND); `ide-extensions/changelog.json` 0.6.0 entries (ADD, same versions); Discord `[0.12.0]` entry (EXTEND — recount ≤2000) |

## §3 — Milestones (each ends green on its verify block; commit per milestone)

### N0 — Index + find-all-references (TS)

- `uetkxWorkspace.ts`: extend `CacheEntry` with `refs: FileReference[]` (N-03 shape) built by
  a new `collectFileReferences(scan, source)` — tags from every component's window trees
  (walk `windowNodes`/`returns[].root`: element tags, attr exprs, event exprs, directive
  code), code refs from setup/hook/module/util bodies + value inits (a TS
  `collectExternalRefs` with the SAME member/scope/ternary rules as the C++ — the IMPORT-3
  colon rule included), import bindings + aliases, export lists, default markers, decl-name
  tokens. New exports: `getFileIndex(fsPath)`, `findReferences(name, originFsPath)` →
  resolves the symbol's DECLARATION first (reusing `markupDefinition`'s alias logic, lifted
  out of server.ts into the workspace layer so both F12 and references share it), then
  queries every swept file's index for entries that RESOLVE to that declaration (alias-aware:
  an importer's local spelling counts when its import points at the decl's file+name).
- `server.ts`: capability `referencesProvider: true` + `connection.onReferences` (include the
  declaration when `context.includeDeclaration`).
- Index invalidation: mtime does it for closed files; for OPEN dirty docs, references/rename
  read the live `documents` text (parse on demand) — never stale disk text.
- Tests (`index.test.ts`): a 3-file workspace (exporter + renamed importer + `* as` importer)
  — references for the decl finds tag uses, code uses, the `as`-target spelling, and the
  export-list entry; a local alias's references stay within its file; attr-expr refs found.

Verify:
```bat
cd ide-extensions\lsp-server && npm run build && node --test out/test/*.test.js && node scripts\smoke.js
```

### N1 — Rename + prepareRename (TS)

- `uetkxWorkspace.ts`: `renameSymbol(fsPath, offset, newName)` → N-04 semantics over
  `findReferences`, N-05 validation, returns `{ edits: WorkspaceEdit } | { error: string }`.
- `server.ts`: capabilities `renameProvider: { prepareProvider: true }`;
  `onPrepareRename` (token range or null per N-04); `onRenameRequest` (apply or
  `showMessage` the refusal reason).
- Tests (`rename.test.ts`): rename at decl updates importer bindings + tags + export list;
  rename at a local alias edits one file; plain-binding local rename produces `{ A as New }`;
  refusals (non-ident, 2325-shape collision, host-tag collision); dirty-doc rename uses live
  text.

Verify: same command block as N0.

### N2 — Document + workspace symbols (TS)

- `server.ts`: `documentSymbolProvider` (per-file decls as `DocumentSymbol`s — kind mapping:
  component→Class, hook→Function, value→Constant, util→Function, module→Namespace) and
  `workspaceSymbolProvider` (query over every swept file's decl index, prefix/fuzzy contains
  match, capped at 200).
- Tests: outline for a mixed 5-kind file; workspace query finds an exporter by partial name.

Verify: same command block.

### N3 — Code actions (TS)

- `server.ts`: `codeActionProvider: { codeActionKinds: [CodeActionKind.QuickFix] }` +
  `onCodeAction` producing N-06's four actions from the diagnostics already published at that
  range. The 2320 migration action reuses a new `migrateDeclText(scan, declRef)` in a shared
  module whose output is pinned equal to the codemod's rewrite for the same declaration
  (fixture-compared in a test — never two drifting spellings of the migration).
- Tests: each action's edit applies to a scratch workspace and the re-scan shows the
  diagnostic gone; the 2320 action's output byte-equals the codemod's for the same decl.

Verify: same command block.

### N4 — Scope-aware tracker (C++) — TD-034 #1/#2

- New shared detector in `UetkxCodegen.cpp`'s anon namespace (and a small TS mirror in the
  LSP's reference collector): `FScopedLocals` — walk a verbatim region ONCE, produce per-
  offset "is `Name` locally declared here?" (a stack of brace scopes, each a TSet<FString>;
  declarations recognized per N-07's subset, params seeded from the decl's param records).
- Wire: `RewriteAliases` (:554) and `PrefixHookCalls`'s `Qualified` branch skip when shadowed;
  `CollectExternalRefs` (:81) suppresses shadowed `Bare`/`Call` refs (callers pass the
  region's param seed where applicable — hook/util params).
- Tests: CodegenTest — a local `Primary` shadowing a rename alias is NOT rewritten; a local
  shadowing a private value is NOT qualified; params shadow; structured bindings shadow;
  inner-scope shadow ends at `}`. ResolveTest — a local named like an exported value no
  longer 2305s; the un-shadowed case still does. NEW `ShadowedNames.uetkx` fixture + golden.
- **Byte-neutrality check:** `RUICompile -check` after the emitter change MUST stay 0-drift
  on the real 42 (no real file shadows) — if it drifts, a detector false-positive is
  swallowing a legitimate rewrite: STOP and fix the detector, never re-pin over it.

Verify:
```bat
<Engine>\...\Build.bat ReactiveUIUnrealDemoEditor Win64 Development -Project=<abs>\ReactiveUIUnrealDemo.uproject -WaitMutex
<Engine>\...\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUICompile -check
<Engine>\...\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUIContractDump -write   :: ShadowedNames golden only — any OTHER golden moving is a bug
<Engine>\...\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -ExecCmds="Automation RunTests ReactiveUI.Uetkx; Quit" ... -ReportExportPath=<scratch>\report
```

### N5 — Attr-expression usage walk (C++ + TS) — TD-034 #3

- `UetkxResolve.cpp`: a `CollectMarkupRefs(const FUetkxComponentDecl&, TArray<FExtRef>&)`
  walking window trees per N-08 (attr exprs / event exprs / directive headers+bodies / expr
  children; text children NEVER), feeding the same ref loop in `Apply` (:600) and
  `MissingImports`; scope-tracker-filtered (N4) so markup locals (`@for` loop vars) don't
  police.
- TS: the N0 collector already walks windows — assert parity via a shared test case.
- Tests: ResolveTest — a value imported and used ONLY in `Text={ Cool }`: no 2304, and
  removing the import now yields 2305; an `@for` loop variable matching an exported value
  name does not diagnose. LSP mirror case in `server.test.ts`.
- Ripple check: the real tree's `-check` must stay 0-drift (usage scanning changes diags,
  not emission) and the full battery green (AcceptanceLab/ContextDemo use values in attr
  exprs — their imports must not regress).

Verify: N4's block + full battery `Automation RunTests ReactiveUI`.

### N6 — Stretch (skippable): embedded-C++ local rename via clangd

- Forward rename at embedded offsets to clangd over the virtual doc; translate edits back
  through the source map; single-file only; refuse if any edit lands in unmapped glue.
- If skipped: record in the TD-033 close-out note. Never let this block the campaign.

### N7 — Ledger, changelogs, close-out

- TECH_DEBT: TD-033 → RESOLVED (feature list + anchors); TD-034 → RESOLVED with the
  residual note (unrecognized-shadow shapes under the N-07 heuristic).
- Lane A: EXTEND `## [0.12.0]` (Added: references/rename/symbols/code actions; Fixed: the
  three TD-034 items) + `cp` mirror + `verify-mirror`. Lane B: ADD 0.6.0 entries
  (`changelog.mjs add` + extracts + verify). Lane C: EXTEND the `[0.12.0]` Discord entry —
  RECOUNT ≤2000 chars (it sits at 1895; trimming will be needed — trim, don't overflow).
- Docs: ImportsPage/migration page gain a one-line "IDE: rename + find-references are
  alias-aware" note only if wording changed elsewhere requires it (no new page).
- Final: the ES-modules M9 ordered verify block end-to-end + repackage both vscode vsix
  flavors + the vs2022 vsix at 0.6.0 (the shipped server bytes changed).

## §4 — Sync-surface checklist (all gating)

- [x] Workspace index + references + rename + prepareRename + symbols + code actions (TS) — N0–N3 (commit 0030779)
- [x] Scope tracker C++ + TS mirror; rewrite planes + ref collection wired — N4 (commit 3f4dc57)
- [x] Markup-window usage walk C++ (TS index already walked markup; 2304/2305 flow from the C++ sidecar) — N5 (commit 792d227)
- [x] ShadowedNames fixture + golden; ResolveTest/CodegenTest/LSP test extensions — N4/N5
- [x] Family corpus hash UNCHANGED (`f8ae9961…`) — re-verified at close-out 2026-07-18 (N-09)
- [x] TECH_DEBT TD-033/TD-034 closed (TD-034 with residual note) — N7
- [x] Lane A/B/C extended in place (no version bump — N-01); mirrors + verify green — N7
- [x] vsix rebuilds (both vscode flavors + vs2022; all three verified to embed the new server) — N7
- [x] Full M9-style ordered verify block green — N7 (2026-07-18: node gates ✓, corpus f8ae9961 ✓, LSP 71/71 + smoke ✓, RUICompile 42/0 ✓, Contract 32/0 ✓, battery 128/128 ✓)

## §5 — Guardrails + STOP-AND-ASK

**Never:**
- Bump versions or open new changelog sections (N-01 — 0.12.0/0.6.0 are unreleased).
- Move the family corpus hash (N-09) — scanner behavior is FROZEN in this campaign.
- Produce a WorkspaceEdit from stale text (mtime/live-doc rule, N-04) or one that fails
  N-05 validation.
- Let the 2320 code action and the codemod drift into two migration spellings (N3's pin).
- Re-pin an existing golden in N4/N5 — only `ShadowedNames` is new; any other diff is a
  detector bug.
- Regex over params/decls anywhere — scanned records only (the codemod's standing rule).

**STOP AND ASK the owner:**
1. Any change that would move `corpus-hash` (means the scanner drifted — out of contract).
2. The N-07 heuristic proving insufficient for a REAL demo-tree pattern (i.e. `-check`
   drift on the 42) that can't be fixed by extending the subset cleanly.
3. Discord entry cannot be kept ≤2000 chars without dropping something substantive.
4. Whether N6 (clangd rename forwarding) should block the campaign if it turns out flaky.

**Watch-list:** rename across DIRTY open documents (live-text rule); `* as` alias rename
touching `X::` quals (the qual's `X` token is an `import-alias` ref — the member is NOT
renamed); workspace-symbol scan cost on big trees (the mtime cache bounds it; cap results);
the TS/C++ ternary-colon rule staying identical in the new TS collector (IMPORT-3 —
regression-pinned on both sides).
