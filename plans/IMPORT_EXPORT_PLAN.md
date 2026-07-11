# uetkx Import/Export — Unreal leg EXECUTION PLAN (leg 1 of 3)

> **Status:** EXECUTION-GRADE, derived from plans/IMPORT_EXPORT_MASTER_PLAN.md (locked rounds 1–3,
> amendments A1–A8) + the 7-agent review round (2026-07-11). Self-contained: do NOT re-research.
> **Branch:** `feat/uetkx-imports` off `dev`, ONLY after PR #14 (feat/uetkx-compiler) merges.
> **Vehicle:** one branch, one PR. Codemod correctness = first milestone gate (strict-day-one).
> **Never commit unless the owner explicitly asks.**
>
> **M0.2 diagnostic-block AUDIT VERDICT (recorded 2026-07-11):** family block **2300–2315 is FREE**
> — NO shift to 26xx. Evidence: Unreal grep (`Plugins` + `ide-extensions`) finds no `UETKX23xx`
> (highest occupied here: 21xx/25xx/30xx). Godot sibling `../ReactiveUI-Gadot/addons/reactive_ui/
> guitkx/vocabulary.json` has no `GUITKX23xx` (occupied: 00xx/01xx/03xx/21xx/2504–2508). Unity repo
> is not present in this workspace; its `UITKX23xx` re-grep is deferred to leg 3 per §A5 sequencing
> (leg 3 branches only after this leg merges). Family codes 2300–2309 used as specified in §A3.
>
> **Branch NOTE (recorded 2026-07-11):** PR #14 (feat/uetkx-compiler) is NOT yet merged to `dev`, so
> `feat/uetkx-imports` was cut from `feat/uetkx-compiler` HEAD (which carries all of PR #14's work;
> `dev` is a clean ancestor). Rebase onto `dev` after #14 merges — no content divergence expected.

---

## PART A — FAMILY-SHARED CONTRACT (duplicated verbatim-in-substance into all three legs)

### A1. Locked decisions (owner, rounds 1–3)

- **Strict from day one.** Implicit cross-file resolution = ERROR the moment the feature lands;
  the migration codemod runs inside the same PR so every file gains imports before strictness on.
- **Named exports only.** No `default`, no `import *`. Re-export = fast-follow, not v1.
- **Specifiers:** relative (`./`, `../`) AND family root alias `~/`, both v1. Engine-native forms
  (`res://`, `Assets/`, `Source/`) FORBIDDEN in import specifiers. Extensionless (`.uetkx` implied).
  `~/` root defined per repo by the existing config walk-up file (`uetkx.config.json`).
- **Full ESM cycle parity v1:** cross-file COMPONENT cycles legal (fwd-decl pass, function-hoisting
  parity). VALUE cycles (hooks/modules consumed at load) = compile ERROR printing the chain (TDZ parity).
- **Order: Unreal → Godot → Unity.** Godot leg waits for feat/doom-guitkx-port merge. Unity last
  (two emitters in lockstep under HmrEmitterParityContractTests).
- **FULL MIXED-DECL v1** (round 3): a file is a true SEQUENCE of any number of components + hooks +
  modules. One-component-per-file / hooks-in-.hooks-files become LINT tier (React parity:
  eslint-plugin-react-refresh). This is a data-model + emitter + HMR + LSP rewrite in each repo (A1).
- **`~/` extends into asset references v1** (round 3): one path language. Unreal note: no asset-path
  grammar position exists in .uetkx today (brush props are registry names, not paths) — the rule is
  RECORDED here and the M1 resolver util is written to be reused the day one lands. Godot: @uss/@theme;
  Unity: Asset<T>/@uss incl. byte-for-byte HMR mirror + DiagnosticsAnalyzer.
- **Cross-boundary imports = hard error v1** (A5/A7): Unreal cross-MODULE and cross-ROOT
  (Source↔Plugins); Unity cross-asmdef. One family code (UETKX2308 below).
- **Strict mode polices markup-owned names ONLY** (A4): imports address `.uetkx/.guitkx/.uitkx`
  targets only; hand-written native code (headers, `class_name` .gd, engine namespaces) is ambient.
  Strict diagnostics fire only for names present in the family export tables.
- **Codemod exports EVERYTHING existing** (A3): zero-inbound-edge roots are consumed by name from
  hand C++/UMG/tests (RuiDemoScreens.cpp, RuiHostWidget.cpp) — privacy is opt-in going forward.
  No compile-time check can protect names consumed from native strings: permanent documented hole.
- **Graph truth = declared imports in SOURCE** (A2). Sidecars are git-ignored per-machine cache +
  verifier ONLY — never the transport (fresh-clone aggregator divergence otherwise).
- **Codemod is NOT edge-derived** (A3): recorded edges cover component tags only. Each leg's codemod
  = a NEW reference scan (tags + hook-call + module-member scan against export tables).
- **Sidecar schema bump**: records decls (name/kind/exported), imports, resolved edges, export hash.

### A2. Grammar (SHAPE-identical, not byte-identical — per-leg casing idiom)

```
#include "MyGameTypes.h"                          // engine preamble directives keep working
import { StatusChip } from "./components/StatusChip/StatusChip"
import { UseCounter, CounterStyles } from "~/Screens/SimpleCounter/SimpleCounter.hooks"

export component Screen(Title: FText) { ... }     // exported: cross-file addressable
export hook UseThing(int32 A) -> int32 { ... }    // Unreal/Unity: PascalCase Use*; Godot: use_*
export module Styles { ... }
component LocalHelper { ... }                      // no export = file-private
```

- Imports: static, string-literal specifiers, PREAMBLE ONLY (with `#include`); trivially scannable
  by all six implementations (3 compilers + 3 TS mirrors). Multiple decls per file, any order/mix.
- Godot divergence (recorded): hooks are snake_case `use_*`; its importable value unit is the file
  BINDING (module/hook container). Family corpus splits family-core vs per-leg casing cases (A4b).
- Non-exported decls: unreachable cross-file; per-backend privacy (Unreal: detail namespace +
  registration tree-shake; Godot: no class_name; Unity: uitkx strict diags — C# `internal` only
  fences asmdefs).

### A3. Family diagnostics contract

**PREREQUISITE — code-block audit (M0.2, blocking):** occupied union across the three registries as
audited 2026-07-11: `0013-0026, 0103-0151, 03xx, 2100-2107, 2200-2205, 2504-2508, 3000-3006`.
Unreal grep confirms NO `UETKX23xx` in use (Plugins + ide-extensions). Godot `vocabulary.json` and
the Unity registry MUST be re-grepped for 23xx before the first code is emitted; if occupied
anywhere, the whole block shifts to 2600-2615. Family block (provisional): **2300–2315**.

| Code | Sev | Message shape (Unreal wording; family messages identical modulo prefix) |
|---|---|---|
| 2300 | err | `unknown import specifier `%s` — no file at %s(.uetkx)` |
| 2301 | err | `` `%s` is not exported by %s — add `export` to its declaration `` |
| 2302 | err | `` `%s` is not declared in %s `` |
| 2303 | err | `` duplicate import of `%s` (already imported from %s) `` |
| 2304 | warn | `` unused import `%s` `` |
| 2305 | err | `` `%s` is defined in %s but not imported — add: import { %s } from "%s" `` |
| 2306 | err | `value-import cycle: %s -> %s -> %s (hooks/modules load eagerly — break the chain or move to component refs)` |
| 2307 | err | `` `%s` is used like a uetkx component/hook but no file exports it `` |
| 2308 | err | `import crosses a module/root boundary (%s -> %s) — imports are module-scoped in v1` |
| 2309 | err | `import must appear in the preamble, before the first declaration` |
| 2310-2315 | — | reserved (family) |

- **Severity-divergence policy (A8a):** codes + messages identical family-wide; default severities
  identical; any per-repo divergence must be listed in that repo's diagnostics doc under a
  "family divergences" table. v1 divergences: none for 2300-2309. **UETKX2105** retargets from
  scan ERROR to LINT (sev 1 warn, "one component per file — convention") in each leg AS its
  mixed-decl support lands (Godot stays error until leg 2 — recorded transient divergence).
- Registries to update in lockstep per leg: Unreal `UetkxFileScan.cpp`/`UetkxDriver.cpp` +
  `ide-extensions/lsp-server/src/uetkxFileScan.ts`; Godot `vocabulary.json` + TS mirror; Unity
  DiagnosticsAnalyzer + language-lib (+ committed analyzer DLL rebuild, A7h).

### A4. Corpus mirroring prerequisite (TD-009 / TD-018 — both OPEN)

- The import corpus is the FIRST mirrored set; the mirror mechanism does not exist yet — building
  it is a **leg-1 deliverable** (M0.3):
  - Partition `ide-extensions/lsp-server/test-fixtures/uetkx-scanner-cases.json` into
    `familyCore` (markup-mode + import-grammar cases, byte-identical family-wide after
    s/uetkx/guitkx|uitkx/ code-prefix substitution) and per-leg cases (C++ lexis, casing).
  - New `scripts/corpus-hash.mjs`: sha256 over the canonicalized `familyCore` section; prints the
    hash; CI job compares against `plans/family-corpus.hash` (committed). Sibling repos adopt the
    same script + hash file in legs 2/3; release-time hash-match = TD-009's resolution.
- Sequencing: leg-1 lands corpus + gate; the Godot mirror PR (TD-018 + import cases) opens at leg-2
  start; Unity at leg-3.

### A5. Sequencing gates

1. PR #14 merges → branch `feat/uetkx-imports`. 2. This leg's PR merges green (zero-diagnostic
tree, all suites, drift check, corpus gate). 3. feat/doom-guitkx-port merges in the Godot repo →
leg 2 branches (its codemod migrates doom in the normal sweep). 4. Leg 2 merges → leg 3.
Codemods in all legs are **idempotent and re-runnable** (rebase tool for in-flight branches, A8d).

### A6. Family HMR semantics (React Fast Refresh parity)

Component-only file edit → in-place refresh, state preserved. Hook-signature change → remount that
component. Non-component export edited → propagate up the import graph to nearest component
importers. Escapes the root → full reload/rebuild notice.

---

## PART B — UNREAL LEG (mechanical, milestone-ordered)

Legend: **[REPIN]** = step changes committed generated output (.inl / aggregators / *.inl.expected /
compiler fingerprint) — all fold into ONE CodegenVersion bump + one golden re-pin commit-set.

### M0 — Prerequisites & gates

- M0.1 Confirm PR #14 merged; branch `feat/uetkx-imports` off `dev`.
- M0.2 Diagnostic-block audit (A3): grep `UETKX23` here (done: free), `GUITKX23` in
  ReactiveUI-Gadot `addons/reactive_ui/guitkx/vocabulary.json`, `UITKX23` in the Unity repo.
  Record verdict at the top of this file; shift to 26xx if occupied.
- M0.3 Corpus partition + `scripts/corpus-hash.mjs` + `plans/family-corpus.hash` + CI job in
  `.github/workflows/test.yml` (node job, engine-free — verify-mirror.mjs precedent).
- M0.4 `#line` VS spike (see M7.1) — result gates the M7 emit shape BEFORE any golden re-pin.

### M1 — `~/` root key in uetkx.config.json

- Files: `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxFormatter.cpp:801-848`
  (walk-up loader — extract to a shared `FUetkxConfig::Load(Dir)` in new
  `ReactiveUIToolchain/Public|Private/UetkxConfig.h/.cpp`; formatter + resolver both consume it);
  TS mirror `ide-extensions/lsp-server/src/uetkxSchema.ts:54-60`.
- New key: `"root": "<relative dir>"` — path from the config file's directory to the `~/` anchor.
- **Defaults + shadowing (A5g, explicit):** no config, or config without `"root"` → anchor =
  the file's UE **module root** (nearest `*.Build.cs` ancestor dir — exact rule already in
  `FUetkxDriver::BuildAggregators`, UetkxDriver.cpp:331-349; extract to
  `FUetkxConfig::ModuleRootFor(Path)`). Walk-up stays **nearest-config-wins, NO merge**: a nearer
  formatter-only config does NOT inherit an ancestor's `"root"` — it resets `~/` to the module-root
  default. Document in the config docs + a scanner-corpus formatter case.
- Two sweep roots (`RUICompileCommandlet.cpp:11-21`: `Source/`, `Plugins/`): `~/` never escapes its
  module; a `"root"` outside the owning module = config error → UETKX2308 at resolution time.
- Tests: `ReactiveUIUetkxSchemaTest.cpp` + lsp `server.test.ts` walk-up cases (root key, shadowing,
  missing-root default).

### M2 — Scanner: import/export grammar + mixed-decl data model (C++ + TS mirror)

- Files: `Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp`,
  `Public/UetkxFileScan.h`, `ide-extensions/lsp-server/src/uetkxFileScan.ts`.
- Data model (`UetkxFileScan.h`) — add:
  ```cpp
  struct FUetkxImportDecl { TArray<FString> Names; TArray<int32> NameAts;
      FString Specifier; int32 SpecifierAt = -1; int32 At = -1; };
  // FUetkxFileScanResult gains:
  TArray<FUetkxImportDecl> Imports;
  TArray<TPair<EUetkxDeclKind,int32>> Order;   // source order over Components/Hooks/Modules
  // FUetkxComponentDecl / FUetkxHookDecl / FUetkxModuleDecl each gain: bool bExported = false;
  ```
  Mirror 1:1 in `uetkxFileScan.ts` (`UetkxFileScanResult` at uetkxFileScan.ts:68-72 already holds
  `components/hooks/modules` arrays — add `imports`, `order`, `exported` flags).
- Parse `import { A, B } from "spec"` inside the existing preamble loop
  (UetkxFileScan.cpp:268-295, where `#include` capture lives). Emit **UETKX2309** if `import`
  appears after the first decl; **UETKX2303** for a name already imported in this file.
- Parse optional `export` keyword immediately before `component|hook|module` (KeywordAt).
- **De-binarize dispatch (mixed-decl v1):** DELETE the first-keyword dispatch + early
  `ScanSupportDecls` return (UetkxFileScan.cpp:296-303). Replace with one loop: at each decl
  keyword, parse that decl kind, push to its array + `Order`, continue. `ScanSupportDecls` folds in.
- **UETKX2105 retarget:** the "invalid content after the component body" hard error becomes: sev 1
  WARN fired when a file holds >1 component decl ("one component per file — convention"); genuinely
  unparseable trailing junk stays an error (existing 2101 path).
- `IsSupportFile()` retires from decision-making; keep as `HasMarkup() == Components.IsEmpty()`
  helper during transition, delete by M9.
- Corpus (`uetkx-scanner-cases.json`, D-22 — run by BOTH ReactiveUI.Uetkx.Scanner and node --test):
  import forms (1 name / n names / `./` `../` `~/` / non-BMP specifier), 2303, 2309, `export` on
  each decl kind, mixed sequences (component+hook+module in every order), 2105-as-warn,
  export-less private decl. familyCore-tagged where markup-mode.

### M3 — CompileSource de-binarization + signature change

- File: `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp`,
  `Public/UetkxCodegen.h:31`.
- New signature (also fixes M7 + M4):
  ```cpp
  static FUetkxCompileOutput CompileSource(const FString& Source, const FString& Basename,
      const FString& ProjectRelPath,                  // "" = fixtures/tests default
      const IUetkxImportResolver* Resolver = nullptr);// null = resolution skipped (syntax-only)
  ```
- DELETE the support-file early return (UetkxCodegen.cpp:849-879 block). Emit **per decl in
  `Scan.Order`**: hook → inline free function (existing shape, :851-863); module → namespace
  (:864-877); component → struct+impl+wrapper (FEmitter::Emit, :755-830). One .inl holds all.
- `Out.ComponentNames` → rename `Out.DeclNames` conceptually; sidecar moves to `decls` (M4).
  `Out.bSupportFile` retired. `HookSig` becomes per-component (map name→sig) — HMR reads per-decl.
- Callers to update: `UetkxDriver.cpp:268` (pass project-relative path — compute via
  `FPaths::MakePathRelativeTo(Path, FPaths::ProjectDir())`, forward slashes), `UetkxContract.cpp:51`
  (fixtures: pass `Basename + ".uetkx"` as the rel path — machine-independent), `RuiHmr.cpp` scan
  usage (scan-only, unaffected), `RUIContractDumpCommandlet`.

### M4 — Resolution, strict diagnostics, source-truth graph, fixture resolver

- New files: `ReactiveUIToolchain/Public/UetkxResolve.h`, `Private/UetkxResolve.cpp`:
  ```cpp
  class IUetkxImportResolver {           // implemented by driver + fixture harness + tests
    // specifier -> absolute path ("" = unresolved); applies ./ ../ ~/ + ".uetkx"
    virtual FString Resolve(const FString& Spec, const FString& ImporterAbsPath) const = 0;
    // export table of a resolved file: name -> kind; cached by (mtime, src_hash)
    virtual bool GetExports(const FString& AbsPath, TMap<FString,EUetkxDeclKind>& Out) const = 0;
  };
  ```
- Resolution runs inside CompileSource when Resolver != null, AFTER scan, BEFORE emit. Emits into
  `Out.Diags`: 2300 (Resolve fails), 2308 (resolved path's module root ≠ importer's, or crosses
  Source↔Plugins), 2301/2302 (per name vs target exports), 2304 (imported name never referenced —
  reference set = component tags + bare `Use<Upper>` calls + `Name::` module quals, computed
  below), 2305/2307 (strict): every markup tag not in the element schema, every bare `Use<Upper>(`
  call not builtin and not same-file, every `Name::` where Name is in SOME export table — must be
  imported (2305 names the exact line) or exported by no file (2307). Reuse the classification in
  `PrefixHookCalls` (UetkxCodegen.cpp:168-244 — already exempts `::`-qualified + member access, so
  hand-header namespaces like `RuiDemo::DrawPolygon` never misfire; A4 scope) and the tag→Uses
  logic (:414-417 vicinity). Builtin hook allowlist = schema hooks (UseState, UseEffect, …).
- **2306 value cycles:** hook/module import edges only (component edges exempt — fwd-decl pass
  legalizes them). DFS over hook/module edges at driver level; message prints the chain.
- **Graph truth (A2):** `BuildAggregators` (UetkxDriver.cpp:325-445) STOPS reading sidecar
  `kind`/`uses` for ordering. New input: per-file fresh **preamble scan** (imports + decl kinds —
  cheap, no markup parse; add `FUetkxFileScan::ScanPreamble(Source)`), resolved via the driver
  resolver. Sidecar `uses` demoted to verifier cache: after compile, `declared imports ⊇ used`
  check → 2305. `CheckDrift` (:452-529) already fresh-compiles each file at :476 — feed those scan
  results into BuildAggregators; kills the fresh-clone divergence.
- **UETKX2107** (component cycle, :600-666): stays, now over DECLARED component-import edges;
  message updated: cycles are legal ONLY when every participant compiles under the two-phase
  aggregator (M6) — i.e., 2107 retires as an error and becomes the M6 topo fallback path (cycle
  participants ordered alphabetically in the BODY phase; decl phase makes it compile). Delete the
  error emit; keep the Kahn as the body-phase order.
- **ContractFixtures resolver (A5f):** harness (`UetkxContract.cpp:35-77`) constructs a
  fixture-local resolver rooted at `Source/RuiHostTests/ContractFixtures/`; `~/` anchor = fixture
  dir. Fixtures are EXCLUDED from FindAll/sweeps/codemod (UetkxDriver.cpp:143-146,304) →
  hand-edit in this PR: `Showcase.uetkx` gains `import { StatusChip } from "./StatusChip"` +
  `import { UseChipLabel/... } from "./ChipSupport"` (match actual names); `StatusChip.uetkx`/
  `ChipSupport.uetkx`/`Counter.uetkx`/`Preamble.uetkx` gain `export`. **[REPIN]** all
  `*.inl.expected`; add fixture pair `ImportError.uetkx` + `ImportError.uetkx.diags.expected`
  pinning 2300/2301/2305 text.
- Sidecar schema v2→v3 (`UetkxDriver.cpp:40-70` writer, :148-175 reader — keep v2 read tolerance):
  ```json
  { "schema": 3, "src_hash": u32, "export_hash": u32,
    "decls": [{"name":"Screen","kind":"component","exported":true}],
    "imports": [{"names":["UseCounter"],"specifier":"./X.hooks","resolved":"<proj-rel>"}],
    "dep_hashes": {"<proj-rel dep>": u32},
    "uses": ["StatusChip"], "diags": [...] }
  ```
- Tests: new `Source/RuiHostTests/Private/ReactiveUIUetkxResolveTest.cpp` (resolver forms, every
  2300-2309 code, module/root boundary); `ReactiveUIUetkxDriverTest.cpp` extended (source-truth
  aggregator on a sidecar-less tree = fresh-clone sim; verifier 2305).

### M5 — Exports/privacy + global-name registries (A5e)

- Non-exported decl emission (FEmitter + hook/module emit): wrap in a per-file detail namespace +
  qualify same-file references:
  ```cpp
  // BEFORE (every decl today):            // AFTER (non-exported only):
  struct FRowUetkxProps ... ;              namespace RuiPriv_SimpleCounter {
  inline FRuiNode Row(...) {...}             struct FRowUetkxProps ... ;
  static const bool GRowUetkxFactoryReg      inline FRuiNode Row(...) {...}
      = RUI::RegisterNamedFactory(...);    } // no RegisterNamedFactory (tree-shake)
  ```
  Emitter keeps `TMap<FString,FString> QualifiedName` (private same-file decls →
  `RuiPriv_<Basename>::Name`); `EmitNodeExpr` (component call sites, :498-505,642 region) and
  `PrefixHookCalls` consult it. `RegisterComponentId` KEPT for private components (HMR identity);
  bare-FName collision across two private `Row`s = documented last-swap-wins caveat (RuiHmr.cpp:78,
  RuiNode.cpp:74-94 — process-global registry; interp swap resolves names it never imported:
  accepted compile-time-only scoping divergence, code comment + docs).
- **UETKX2106 ledger keys EXPORTED names only** + per-root/cross-root unification (A5e): CompileAll
  ledger (UetkxDriver.cpp:552-571) currently per-root (commandlet loops roots,
  RUICompileCommandlet.cpp:58); CheckDrift's spans roots (:461-489). Fix: new
  `FUetkxDriver::CompileAllRoots(const TArray<FString>& Roots)` — single ledger over all roots,
  exported names only; commandlet calls it once. Message: ``UETKX2106: exported binding `%s` is
  already bound by %s (one exported name, one file)``. Two files with private same-name decls =
  legal (test-pinned).
- 2106 interp FName identity + named-factory registry: keyed exported names only by construction
  (private decls never RegisterNamedFactory).

### M6 — Fwd-decl / struct-hoist TWO-PHASE aggregator **[REPIN]**

- Design (adversarial-settled): hoisted DECL block = **COMPLETE props-struct definitions +
  defaulted wrapper declarations + hook fwd-decls + module bodies**; BODY block = impls +
  default-FREE wrapper definitions + registrations. (Default-free-fwd-decl alternative REFUTED —
  markup call sites construct `F<Tag>UetkxProps P;` BY VALUE, UetkxCodegen.cpp:502,561; struct must
  be complete before every caller.) Per-file emit, phase-guarded:
  ```cpp
  // Foo.uetkx.inl (AFTER)
  #if defined(RUI_UETKX_DECL_PHASE)
  #line 3 "Source/RuiDemo/.../Foo.uetkx"
  namespace FooStyles { /* module body verbatim */ }        // modules live in DECL phase
  struct FFooUetkxProps final : public FRuiPropsBase { FText Title = FooStyles::Def; ... };
  inline FRuiNode Foo(FFooUetkxProps InProps = FFooUetkxProps(),
      TArray<FRuiNode> InChildren = TArray<FRuiNode>(), FRuiKey InKey = FRuiKey()); // defaults HERE
  inline int32 UseThing(FRuiContext& Ctx, int32 A);         // hook fwd-decl
  #else
  static FRuiNodeArray Foo_UetkxImpl(FRuiContext& Ctx, const FFooUetkxProps& Props, ...) { ... }
  static const FName GFooUetkxId = RUI::RegisterComponentId(...);
  static constexpr uint32 Foo_RUI_HOOK_SIG = 0x....u;
  inline FRuiNode Foo(FFooUetkxProps InProps, TArray<FRuiNode> InChildren, FRuiKey InKey)
  { return RUI::FC(&Foo_UetkxImpl, MoveTemp(InProps), MoveTemp(InChildren), InKey); } // NO defaults
  inline int32 UseThing(FRuiContext& Ctx, int32 A) { /* body */ }
  #endif
  ```
  C++ rule pinned by test: defaults on exactly ONE declaration (the hoisted one) — redefinition of
  defaults is a compile error, absence on the definition is legal.
- Aggregator (BuildAggregators emit, :435-441) → two-phase include:
  ```cpp
  #define RUI_UETKX_DECL_PHASE
  #include "A.uetkx.inl"    // decl phase: modules topo-sorted by module->module import edges
  #include "B.uetkx.inl"    //   (cycle already erred 2306), then alphabetical
  #undef RUI_UETKX_DECL_PHASE
  #include "A.uetkx.inl"    // body phase: Kahn over component import edges (M4), alphabetical ties;
  #include "B.uetkx.inl"    //   cycle remainder alphabetical (now LEGAL — decls precede all bodies)
  ```
- Module member defaults referencing imported module constants (A5b open question): resolved by
  putting ALL module bodies in the decl phase, ordered by module import edges, BEFORE any struct.
- Support-file topo (A5c): hook fwd-decls in the decl phase make hook→hook cross-file call order
  irrelevant in the body phase — no separate support topo needed; delete the alphabetical
  supports-first special case (:369-393) in favor of the unified two-phase order.
- **CodegenVersion 1→2** (`UetkxDriver.h:52`) — fingerprint re-stales everything. **[REPIN]**:
  every committed `.uetkx.inl`, every `<Module>.Uetkx.gen.cpp`, all `ContractFixtures/*.inl.expected`.
- Tests: `ReactiveUIUetkxCodegenTest.cpp` — phase guards present, defaults-on-decl-only,
  private-decl namespace, mixed-decl file emits in source order; `ReactiveUIUetkxDriverTest.cpp` —
  two-phase aggregator content, component cycle compiles (A imports B imports A fixture in a temp
  module), module cycle errs 2306.

### M7 — `#line` project-relative + VS spike **[REPIN]**

> **M7.1 SPIKE VERDICT (recorded 2026-07-11):** path = **PROJECT-RELATIVE, forward slashes**
> (`#line 4 "Source/RuiDemo/Screens/SimpleCounter/SimpleCounter.uetkx"`), fixtures/tests use the
> stable `<Basename>.uetkx`. Verified structurally + by compilation: RUICompile -full emits the
> directives, the line mapping is correct (SimpleCounter's setup starts at source line 4 → `#line
> 4`), and the RuiDemo + RuiHostTests aggregator TUs COMPILE with the directives + `#line @@R@@`
> restore fixups. **INTERACTIVE BIND CONFIRMED BY OWNER (2026-07-11):** in VS2022 (Development
> Editor, ReactiveUIUnrealDemo.sln), a breakpoint on `SimpleCounter.uetkx` line 4 BOUND and HIT
> when the setup ran (`Automation RunTests ReactiveUI.Demos`) — VS resolved the project-relative
> path off the `.sln` directory and stopped on the `.uetkx` source (not the generated `.inl`). Emit
> scope: the top-level verbatim regions (component setup, hook bodies, module bodies). Mid-line
> attr/event exprs AND directive (`@if`/`@for`/`@while`) headers are the accepted granularity limit
> (no per-line `#line`; column drift accepted) — recorded under TD-023.

- M7.1 SPIKE (before goldens): hand-edit one committed .inl with
  `#line N "Source/RuiDemo/Screens/HelloWorld/HelloWorld.uetkx"` inside setup; build; set a VS
  breakpoint in the .uetkx; confirm bind + stepping. Also test .inl-dir-relative form. Winner =
  the emitted base (expected: project-relative — VS resolves via the .sln dir). Record verdict here.
- Path: PROJECT-RELATIVE, forward slashes (abs paths refuted — committed .inl + CheckDrift
  CRLF-normalized compare, UetkxDriver.cpp:452-529, would drift per machine). Comes from the M3
  `ProjectRelPath` param. Fixtures use `Basename + ".uetkx"` (stable).
- Line tracking: new `FUetkxLexer::BuildLineStarts(Src)` (code-point offsets of line heads);
  `LineOf(Offset)`. Emit sites + rules:
  - New `FInlWriter` wrapper accumulating the .inl and COUNTING emitted lines; `EmitUserRegion(
    Text, SrcOffset)` writes `#line <LineOf(SrcOffset)+trimAdjust> "<rel>"`, the region (re-indent
    `\n`→`\n\t` preserves line count — legal under #line), then restore
    `#line <writer.Line()+1> "<InlRelPath>"`. TrimStartAndEnd adjust = count of leading `\n` trimmed.
  - Regions: component setup (Decl.SetupAt; emit at UetkxCodegen.cpp:805 site), hook bodies
    (:856-859), module bodies (:867-870), directive leads / for-while headers (:683,734-737).
  - Attr exprs + event handlers splice MID-LINE (:437,577,579): v1 rule — wrap each in its own
    emitted statement line when (and only when) the expr text contains `\n`; single-line exprs get
    NO #line (documented granularity limit; column-drift accepted). Pin with a codegen test.
- **[REPIN]** folds into the M6 re-pin (same CodegenVersion 2). Contract goldens re-pinned once,
  after M6+M7 both land.

### M8 — Staleness: reverse edges + error-verdict retry poisoning

- File: `UetkxDriver.cpp` `IsStale` (:195-242), `MatchesErrorVerdict` (:206-221), CompileAll.
- `export_hash` = FNV-1a over sorted `name|kind|exported` lines of the file's decls (computed at
  scan, stored in sidecar v3). Importer sidecars store `dep_hashes` (resolved dep → its export_hash
  at compile time).
- **Reverse-edge staleness:** `CompileAllRoots` becomes fixpoint (max 2 passes in practice):
  pass 1 compiles mtime-stale files; collect files whose `export_hash` changed; build reverse map
  from fresh preamble scans; force-recompile importers (transitively) in pass 2.
  `IsStale(A)` additionally returns true when any `dep_hashes` entry ≠ the dep sidecar's current
  `export_hash` (cheap: two sidecar reads; missing dep sidecar → stale).
- **Verdict-poisoning fix (A5d):** concrete bug — A imports {X} from ./B before B exports X → A
  errors, .inl deleted, verdict keyed to A's src_hash; B gains X; A stays "not stale" forever.
  Fix: `MatchesErrorVerdict` returns false when any recorded `dep_hashes` entry differs from the
  dep's current export_hash. Resolution diags (2300-2309) ALWAYS record dep_hashes (unresolved
  specifier records the RESOLVED-candidate path with hash 0 → any appearance of the file flips it).
- Watcher (`ReactiveUIEditor/Private/UetkxWatcher.cpp`) sweeps via CompileAll — inherits both fixes.
- Tests: driver test — the exact A/B retry scenario above goes green in a **SINGLE sweep** with no
  touch of A (the fixpoint's pass 2 catches A even though it sorts before B); export-removal marks
  importer stale; fresh-clone (no sidecars) full compile ordering. **[LANDED TD-025]**: implemented
  as `RunPass`/`ByPath`/`OldExport`+`bExportsMoved` in `CompileAllRoots` — pass 1 compiles
  mtime/dep-stale files; if any `export_hash` moved, pass 2 re-sweeps and each importer's dep-hash
  `IsStale` (reading the now-updated dep sidecar) recompiles it. `ByPath` keeps each file's compiled
  result across passes so the tally + 2106 ledger reflect the converged state. Verified:
  `ReactiveUI.Uetkx.Driver` "A recompiled after B gained X in ONE sweep (verdict un-poisoned,
  fixpoint)".

### M9 — HMR: mixed-file per-decl + import-graph blast radius

- File: `Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/RuiHmr.cpp`.
- DELETE the whole-file support branch (:60-72 `if (Scan.IsSupportFile())`). New shape: always
  iterate `Scan.Components` for interp swap (existing loop :74-126 unchanged per component);
  additionally, when `Scan.Hooks.Num()+Scan.Modules.Num() > 0`, append the rebuild note —
  message gains the blast radius:
  `"%s.uetkx: hook/module change — compiled only; importers: %s; Live Coding (Ctrl+Alt+F11) or rebuild, then affected screens refresh"`.
- Blast radius source: reverse import edges from the driver graph (expose
  `FUetkxDriver::ImportersOf(ProjRelPath)` reading sidecar v3 `imports` as cache, source-verified);
  the watcher passes importer names into FRuiHmr status. Post-Live-Coding refresh:
  existing `HmrRefreshAll` stays the v1 action (whole-app refresh; scoping to importer roots =
  follow-up); the note NAMES importers so the user knows what changed.
- Interp swap for exported components unchanged (RUI::Named lookup is name-based —
  UetkxInterpComponent.cpp:653-657; registry process-global caveat documented in M5).
- Per-component HookSig from M3 feeds the existing reset logic (:80-115) per decl.
- Tests: `ReactiveUIHmrTest.cpp` — mixed file: component swaps AND note emitted; hook-only file
  names importers; private component swap does not hot-link a named factory.

### M10 — Codemod: `-run=RUIMigrateImports`

- New: `ReactiveUIEditor/Private/RUIMigrateImportsCommandlet.h/.cpp` (RUICompile commandlet
  precedent). **Idempotent, re-runnable** (rebase tool).
- Algorithm:
  1. Roots = `URUICompileCommandlet::DefaultRoots()` (Source + Plugins); files = `FindAll` per root
     (ContractFixtures auto-excluded — those are hand-edited in M4).
  2. Pass 1 — export tables: scan every file; table `name → (abs path, kind)`. **Add `export ` to
     EVERY decl** lacking it (export-everything default, A3 — by-name C++/UMG consumers:
     RuiDemoScreens.cpp:19-29, RuiHostWidget.cpp:18-25, RuiWorldSubsystem.cpp:11-17,
     ReactiveUIDemoTests.cpp, ReactiveUIAcceptanceTest.cpp).
  3. Pass 2 — reference scan per file (NOT sidecar-derived; A3):
     - component tags: markup scan; tag ∉ element schema ∧ tag ∈ table ∧ owner ≠ self → edge;
     - hook calls: over setup + hook bodies, bare `Use[A-Z]\w*` followed by `(`, not preceded by
       `::` or `.` or `->` (PrefixHookCalls rules, UetkxCodegen.cpp:184-198), ∉ builtin schema
       hooks, ∈ table, owner ≠ self → edge;
     - module refs: `([A-Za-z_]\w*)::` over all verbatim regions; name ∈ module table, owner ≠
       self → edge. (Hand-header namespaces are ∉ table → ignored; A4.)
  4. Emit: group edges by owner file; one line per target,
     `import { N1, N2 } from "<specifier>"`, names sorted; specifier = POSIX-relative from
     importer's dir (`./`/`../`); never engine-native; skip names already imported (idempotency).
  5. Insertion point: after the last preamble `#include` line (else at file top), one blank line
     before the first decl. LF endings, no BOM (FFileHelper, .gitattributes pins LF).
  6. Cross-module/cross-root edges found → print UETKX2308 with both paths and DO NOT write that
     import (leaves 2305/2307 at compile → surfaces the real decision to the owner). Expect zero
     in the current gallery (single RuiDemo module).
  7. Finish: run `CompileAllRoots(-full)` → REQUIRE zero diagnostics (leg gate); print summary.
- Also update `templates/` .uetkx scaffolds (grep for `component ` in templates/) to `export
  component` + import examples; `new-*` skills if any reference the grammar.

### M11 — LSP / formatter / TextMate / VS2022 mirrors

- `ide-extensions/lsp-server/src/uetkxFileScan.ts`: M2 mirror (corpus-locked — same JSON cases).
- `src/server.ts`: (a) completion inside `import { … }` from the target file's export table and
  after `from "` from workspace file paths; (b) go-to-def: imported name → target decl, specifier
  string → file; (c) local diagnostics 2303/2309/2304 + resolution 2300/2301/2302/2305/2307/2308
  from a workspace export index (new `src/uetkxWorkspace.ts`, mtime-cached scans, config walk-up via
  uetkxSchema.ts M1 change). **Version-skew tolerance (A8/critic):** strict resolution diags emit
  ONLY when the project shows sidecar schema ≥3 (post-migration signal) — an old project with the
  new extension gets syntax support, no strict noise; old extension on new project degrades to
  unknown-preamble-token passthrough (verified: old scanner walks past unknown preamble tokens,
  UetkxFileScan.cpp:271-294 behavior mirrored in TS).
- Formatter (`UetkxFormatter.cpp` + `src/formatUetkx.ts`): preamble canonical check (:740-752)
  currently accepts only whitespace + `#include` — ADD import lines; canonical order: `#include`
  block, blank, sorted import block, blank. Multi-decl: the between-decls verbatim fallback
  (:757-775) must instead iterate `Scan.Order` with `FmtComponent`/`FmtHook`/`FmtModule` (hook/
  module formatting paths exist from declarations v2 — extend for the `export ` prefix; verify
  Format Document is NOT silently disabled on mixed files). Idempotency corpus cases:
  import sorting, export preserved, mixed-decl file round-trip.
- `ide-extensions/vscode-uetkx` TextMate grammar: `import`/`export`/`from` keywords + string
  specifier scope; semantic tokens in server for imported names.
- `ide-extensions/visual-studio`: consumes the LSP — rebuild the vsix; grep for any duplicated
  keyword list (classifier) and sync.
- Tests: lsp `node --test out/test/*.test.js` (scanner corpus, formatter corpus, new workspace
  resolution tests in server.test.ts) + `node scripts/smoke.js`.

### M12 — Docs, changelog, versions

- README.md: uetkx section gains import/export + strictness + `~/` + privacy paragraphs.
- CLAUDE.md: grammar summary + codemod command line.
- `ReactiveUIUnrealDocs~`: add `src/pages/Uetkx/ImportsPage.tsx` (grammar, diagnostics table
  2300-2315, config `root` key, migration guide) — docs shell currently has Introduction only;
  wire the route; `npm run build && npm run lint`.
- `plans/PENDING_CHANGELOG.md`: lane A bullet (plugin: import/export + strict + fwd-decl aggregator
  + #line + codemod), lane B bullet (ide-extensions: import intelligence + grammar), lane C Discord.
- Versions: `Plugins/ReactiveUI/ReactiveUI.uplugin` VersionName stays `0.1.0-dev` (pre-release
  train — bump only at release per release process); `ide-extensions/lsp-server/package.json` +
  `vscode-uetkx/package.json` 0.1.0 → 0.2.0 (scripts/bump.mjs); `ide-extensions/changelog.json`
  entry. Root CHANGELOG.md untouched until release drain; `Plugins/ReactiveUI/CHANGELOG.md` must
  stay byte-identical (`node scripts/verify-mirror.mjs`).
- plans/TECH_DEBT.md: TD-009 status → "mechanism shipped leg 1; sibling PRs pending"; new TD entries
  for accepted caveats (interp global-name scoping; private-FName last-swap-wins; #line single-line
  expr granularity).
- Update `plans/IMPORT_EXPORT_MASTER_PLAN.md` status line when the leg merges.

### M13 — Test matrix (every suite/corpus/fixture + verify commands)

| Surface | Add/Update | Content |
|---|---|---|
| `uetkx-scanner-cases.json` | ADD ~20 cases | M2 list; familyCore-tagged import/markup cases |
| `uetkx-formatter-cases.json` | ADD ~8 cases | import canonicalization, export, mixed-decl idempotency |
| `ContractFixtures/` | UPDATE all `.inl.expected` **[REPIN]**; hand-add imports/exports; ADD ImportPair + ImportError fixtures | M4/M6/M7 shapes; strict diag text pinned |
| `ReactiveUIUetkxScannerTest.cpp` | corpus-driven — no change beyond corpus | |
| `ReactiveUIUetkxCodegenTest.cpp` | ADD | phase guards; defaults-on-decl-only; privacy namespace; #line placement/restore; mixed-decl order |
| `ReactiveUIUetkxResolveTest.cpp` | NEW | resolver (./ ../ ~/, shadowing), all 2300-2309 |
| `ReactiveUIUetkxDriverTest.cpp` | ADD | source-truth aggregator (sidecar-less), two-phase content, reverse staleness, verdict retry, 2106 exported-only, CompileAllRoots ledger |
| `ReactiveUIUetkxContractTest.cpp` | UPDATE | fixture resolver wiring |
| `ReactiveUIHmrTest.cpp` | ADD | mixed-file per-decl; importer note; private no-hot-link |
| `ReactiveUIUetkxFormatterTest.cpp` | corpus-driven | |
| `ReactiveUIDemoTests.cpp` / `ReactiveUIAcceptanceTest.cpp` | UPDATE | post-codemod gallery still renders; by-name consumers still resolve (export-everything proof) |
| lsp `src/test/*.test.ts` | ADD | workspace resolution, completions, skew tolerance |

Verify commands (run in this order; parse `report\index.json`, not exit codes):

```bat
:: 0. corpus + mirror gates (engine-free)
node scripts/corpus-hash.mjs && node scripts/verify-mirror.mjs
:: 1. codemod (once), then full compile + drift
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUIMigrateImports
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUICompile -full
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUICompile -check
:: 2. all suites headless (Boot is never optional)
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -ExecCmds="Automation RunTests ReactiveUI; Quit" -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput -ReportExportPath=<scratch>\report
:: 3. IDE tooling
cd ide-extensions\lsp-server && npm ci && npm run build && node --test out/test/*.test.js && node scripts\smoke.js
:: 4. docs
cd ReactiveUIUnrealDocs~ && npm ci && npm run build && npm run lint
```

### Milestone order & gates

M0 → M1 → M2 → M3 → M4 → M5 → M6+M7 (one **[REPIN]** window: CodegenVersion 2, all .inl +
aggregators + expecteds re-pinned ONCE) → M8 → M9 → M10 (codemod; zero-diagnostic tree = gate) →
M11 → M12 → M13 full matrix green → PR. Leg 2 (Godot) branches only after this PR merges AND
feat/doom-guitkx-port merges.

### Risk watch-list (leg-local)

- Defaults-on-first-decl correctness is test-pinned (M6) — regression = silent C++ redefinition errors.
- #line granularity: single-line attr/event exprs unmapped (documented; TD entry).
- Codemod is load-bearing: strict lands in the SAME PR — any scan-rule gap = red tree; the M10.7
  zero-diagnostic run is the merge gate.
- Private-name HMR collisions (last-swap-wins) + interp global registry: accepted v1 divergences,
  documented in M5/M9 + TECH_DEBT.
