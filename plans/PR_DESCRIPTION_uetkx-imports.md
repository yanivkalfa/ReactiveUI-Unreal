# feat(uetkx): import/export + strict resolution + codemod (imports leg 1 of 3)

> Paste into the GitHub compare page. **Base:** `feat/uetkx-compiler` (this stacks on PR #14 —
> rebase onto `dev` once #14 merges; `dev` is a clean ancestor). Delete this file after merge.
> Compare: https://github.com/yanivkalfa/ReactiveUI-Unreal/pull/new/feat/uetkx-imports

## What this is

The Unreal leg of the family import/export feature (`plans/IMPORT_EXPORT_PLAN.md`). `.uetkx` gains
**static imports/exports, strict from day one**, **full mixed-decl files**, **privacy-by-default**,
and a **one-command codemod** that migrates an existing project in place — with the whole gallery
migrated strict-clean in this PR.

## Highlights

- **Grammar (M2):** `import { A, B } from "./x"` / `~/root-alias`, extensionless, named-only,
  preamble-only. `export` on any `component`/`hook`/`module` makes it cross-file addressable. A
  file is now a free SEQUENCE of components + hooks + modules (one-per-file is a lint, not an
  error). Scanner rewritten and mirrored 1:1 in the TS LSP; +17 corpus cases run in both languages.
- **Strict resolution (M4):** a new `IUetkxImportResolver` + `FUetkxFsResolver` + `FUetkxResolve`
  emit the family diagnostics **UETKX2300–2309** (unknown specifier, not-exported, not-declared,
  duplicate/unused/misplaced import, used-not-imported *with a fix-it import line*, module-boundary).
  Hand-written C++ namespaces/hooks stay ambient (A4). Run inside `CompileSource`; **enforced by the
  driver** (`RUICompile -check` is the CI strict gate).
- **Codemod (M10a):** `-run=RUIMigrateImports` — idempotent, re-runnable. Adds `export` to every
  decl (export-everything, A3) + inserts the imports each file needs from a fresh reference scan,
  then requires a zero-diagnostic compile. Migrated the 15-file gallery: 15 exports, 4 imports, 0
  cross-module, 0 errors; re-run is a no-op.
- **Privacy (M5):** non-exported decls emit into a per-file `RuiPriv_<Basename>` detail namespace
  and are tree-shaken (no named factory); same-file references qualify. `UETKX2106` keys exported
  names only; `CompileAllRoots` is a single ledger across Source + Plugins.
- **Staleness (M8):** sidecar v3 (`export_hash` + `dep_hashes`); reverse-edge staleness re-stales
  importers when an exported decl changes; the A5d verdict-poisoning bug is fixed and test-pinned.
- **HMR (M9):** mixed files swap their components AND note the rebuild, naming the import blast
  radius (`FUetkxDriver::ImportersOf`).
- **Config (M1):** shared `FUetkxConfig` walk-up (formatter + resolver); new `"root"` key for `~/`.
- **IDE (M11):** formatter canonicalizes the import block + preserves `export` (C++ + TS mirror);
  TextMate `import`/`export`/`from` keywords (vscode + VS2022). vscode/lsp `0.1.0 → 0.2.0`.
- **Family gate (M0):** `scripts/corpus-hash.mjs` + `plans/family-corpus.hash` + CI job — the
  import corpus is the first mirrored set (A4/TD-009). Diagnostic-block audit: 2300–2315 free.

## Verification (all green)

- `RUICompile -full` (15/15 compiled, 0 err) and `RUICompile -check` (0 drift, 0 err, strict).
- Full headless automation battery: **54 / 54**.
- LSP `node --test` **9/9** + `scripts/smoke.js` PASSED.
- Engine-free gates (mirror, headers, skills, docs-drift, **corpus-hash**, changelog) green.
- Docs site build + lint green.

## Deferred (tracked, not dropped)

Self-contained follow-ups documented in `plans/TECH_DEBT.md`:
- **TD-023** — two-phase fwd-decl aggregator + `#line` (cross-file *component*-cycle parity +
  debugger stepping; CodegenVersion-2 golden re-pin). The gallery has no cycles, so today's
  single-phase emit ships correct.
- **TD-024** — LSP import completions / go-to-def / workspace resolution diags, `FmtHook`/
  `FmtModule`, VS2022 vsix rebuild.
- **TD-025** — UETKX2306 value cycles, source-truth aggregator ordering, one-call staleness fixpoint.
- **TD-026** — accepted v1 caveats (interp global-name scoping; private-FName last-swap-wins).

## Notes

- Sidecars (`*.uetkx.diags.json`) are git-ignored per-machine cache (A2) — not in the diff.
- Sequencing: leg 2 (Godot) branches after this merges AND `feat/doom-guitkx-port` merges; leg 3
  (Unity) after leg 2.
