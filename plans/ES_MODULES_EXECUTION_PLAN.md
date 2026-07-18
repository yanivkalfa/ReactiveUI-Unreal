# .uetkx ES-Modules Redesign — Unreal leg EXECUTION PLAN (leg 1 of 3)

> **✅ DONE 2026-07-18 (M0–M9 executed on `feat/es-modules`, commits 0e1cb48..HEAD).** Final
> evidence: full battery **128/128** · LSP **49/49** + smoke PASSED · `RUICompile -full`/`-check`
> 42 files 0 drift · codemod run ×2 idempotent (40 components + 2 hooks rewritten, 2 modules
> hoisted, 3 imports → `* as`, 0 errors, zero-2320 gate) · 31 contract goldens · corpus hash
> **f8ae9961…6cd47** pinned (tri-repo match = the RELEASE gate, owner-coordinated sibling PRs) ·
> plugin **0.12.0** / extensions **0.6.0** + all changelog lanes · both vscode vsix flavors +
> the vs2022 vsix rebuilt at 0.6.0. Deviations from this plan, all sanctioned in-place: the
> fixture re-pin used the "second deliberate window" (M8) option; the LSP rename handler took
> the TECH_DEBT route (TD-033 — owner call pending, §11.2); TD-034 records the accepted
> alias/value rewrite caveats; M7's bughunt fixed a ternary-`:`-vs-`::` scan-back bug (the
> IMPORT-3 rule) in THREE sites incl. one latent legacy one. Owner-side remainder: sibling
> corpus PRs (G-13), merge + Publish, HMR field-verification (`field-test-editor`).
>
> **Status:** EXECUTION-GRADE, authored 2026-07-17 against the owner-approved family contract
> [plans/ES_MODULES_GENERAL_PLAN.md](ES_MODULES_GENERAL_PLAN.md) (locked G-01..G-13 — this file may
> ADD Unreal detail, never contradict; a conflict = STOP AND ASK the owner). Direct prior art:
> [plans/archive/IMPORT_EXPORT_PLAN.md](archive/IMPORT_EXPORT_PLAN.md) (the strict-imports leg this
> plan extends). Every file:line anchor below was re-verified against the working tree on
> 2026-07-17 (post-`markup-everywhere`, plugin **0.11.0**, extensions **0.5.0**); the former
> `[VERIFY: …]` markers have been RESOLVED to verified facts (code audit 2026-07-17) — no
> unverified anchors remain.
> **Branch:** `feat/uetkx-es-modules` off `dev` — cut only after the in-flight
> `feat/click-counter-demo` (the CURRENTLY CHECKED-OUT branch; ahead of `dev` by 24 as of
> 2026-07-17) merges, else off its HEAD with a rebase note (the imports-leg precedent). **Vehicle:** one branch, one PR. **Never commit unless the
> owner explicitly asks; NEVER push.**
>
> **G-05 SUPERSEDE RECORD (write into the archive when this leg lands):** the family lock
> "**Named exports only.** No `default`, no `import *`" (IMPORT_EXPORT_PLAN §A1, locked rounds
> 1–3) is **SUPERSEDED by owner decision 2026-07-17** (G-05): full ES import surface — named,
> rename-as, `* as`, default, deferred export lists. Re-exports (`export { a } from "./x"`)
> remain DEFERRED. When this leg merges, add a dated supersede banner to
> `plans/archive/IMPORT_EXPORT_PLAN.md` §A1 pointing here.
>
> **M0 DIAGNOSTIC-BLOCK AUDIT (recorded 2026-07-17):** repo-wide grep (`Plugins` + `Source` +
> `ide-extensions`, all .cpp/.h/.ts/.json) finds exactly `UETKX2300–2309, 2316, 2317` in use.
> **2310–2315 stay reserved (family); 2320–2329 is FREE** — this plan allocates from it (§4).
> Sibling re-grep (`GUITKX232x`, `UITKX232x`) happens at each sibling's leg start per G-13.
>
> **Audience:** a lesser model executing cold. Do not re-research settled decisions. Follow the
> loop research → develop → test → bughunt → fix → commit per milestone (`dev-process` skill);
> never weaken a gate.

---

## §1 — Family locked decisions (echo — full text in ES_MODULES_GENERAL_PLAN.md §2)

| ID | One-line echo (Unreal-relevant reading) |
|---|---|
| G-01 | File = module; module identity = file path; renaming a file changes private-member hot-reload identity (documented, accepted). |
| G-02 | Imports are the ONLY cross-file mechanism for markup files; hand-written C++ headers stay ambient (rule A4 carries forward). |
| G-03 | `component`/`hook`/`module` wrappers removed; classification from the SIGNATURE alone: `FRuiNode` return ⇒ component (PascalCase ENFORCED — 2100 carries over); `Use`-prefix ⇒ hook; `=` after name ⇒ value export; else util. Cross-guards are errors. |
| G-04 | Declarations typed in native C++ idiom, type-first. Inference sugar ONLY where the initializer names the type. |
| G-05 | Full ES import surface (supersede recorded above). Specifiers stay extensionless `./ ../ ~/`; preamble-only; 2308 module-boundary rule unchanged. |
| G-06 | Privacy is real AT RUNTIME too: private registry keys become FILE-QUALIFIED — **the TD-026 fix is folded into THIS plan as its own mandatory milestone (M3)**, not carried forward. |
| G-07 | Escape hatch stays: host-include `import "@X.h"` (UetkxFileScan.cpp:935 `bHostInclude`; 2317 hint — 2316 is LSP-only, see §3) is untouched. |
| G-08 | Component references lazy (cycles legal via the two-phase aggregator); value/hook references eager — 2306 TDZ-style cycle error extends to the NEW value exports. |
| G-09 | Hot-reload identity = (file identity + name) for privates, exported name (2106-unique) for exports. State-preservation semantics must not regress; signature change still resets. |
| G-10 | Old wrapper syntax parses ONE minor with deprecation diagnostics (2320 block); idempotent codemod ships SAME release; removal later, owner-triggered. |
| G-11 | Plugin **0.11.0 → 0.12.0** (minor); extensions bump their own lane (0.5.0 → 0.6.0 at release). No 1.0 claims. |
| G-12 | Full sync surface is part of DONE — §9 checklist below is gating, incl. `family-corpus.hash` lockstep across all three repos. |
| G-13 | Rollout Unreal → Godot → Unity; corpus cases for the new grammar are agreed + pinned FIRST (M0.4) so legs can parallelize. |

## §2 — Engine-local decisions (U-01..U-10; settled here, do not re-litigate)

- **U-01 — New declaration grammar (C++-native, type-first).** A `.uetkx` file = preamble
  (host-includes + imports) + a SEQUENCE of plain typed declarations:
  ```cpp
  import "@Doom/DoomTypes.h"                                  // host include — unchanged (G-07)
  import { StatusChip } from "./StatusChip"                   // named — unchanged
  import { UseCounter as UseTick } from "~/Screens/X.hooks"   // NEW: rename-on-import
  import * as Palette from "./Palette"                        // NEW: namespace import
  import Screen from "./Screen"                               // NEW: default import

  export FLinearColor Cool = {0.23f, 0.65f, 0.95f, 1.0f};    // NEW: value export (typed)
  export Accent = FLinearColor(0.9f, 0.2f, 0.2f, 1.0f);      // NEW: inference sugar (T(..)/T{..})
  FLinearColor RowTint = {1,1,1,1};                           // private value (file-local)
  export FString FormatScore(int32 Score) { return ...; }     // NEW: util function
  export TTuple<int32, TFunction<void()>> UseCounter(int32 Start) { ... }   // hook (Use-prefix)
  export FRuiNode StatusChip(FString Label = FString(TEXT("ok"))) {         // component
      return ( <Border Padding="4"> ... </Border> );
  }
  export { RowTint };                                          // NEW: deferred export list
  export default StatusChip;                                   // NEW: default export
  ```
  Component **params flip to C++-native `Type Name = Default`** (the old colon form
  `Name: Type = Default` — ParseParams, UetkxFileScan.cpp:260-354 — survives ONLY inside the
  deprecated `component` wrapper). Components with no params need `()` in the new form (C++
  function syntax; the codemod adds it — today `export component Showcase {` is legal
  param-less). Hooks lose the trailing `-> Ret`: the return type LEADS; `void` must now be
  written explicitly. `Ctx` threading is UNCHANGED (still invisible in source, injected at emit).
- **U-02 — Classification algorithm (parse-time, signature-only — G-03).** After optional
  `export`, read the decl head to the first top-level `(`, `=` or `{` (reuse the depth/noncode
  machinery; `<...>` nests in type position exactly as ParseParams already handles,
  UetkxFileScan.cpp:265-296). NAME = last identifier before that token; TYPE = everything before
  the name (trimmed; may be empty = inference).
  1. name + `(` and TYPE == `FRuiNode` ⇒ **component** (2100 PascalCase carries over verbatim:
     UetkxFileScan.cpp:1097-1101 message `component name `%s` must be PascalCase`).
  2. name matches `Use[A-Z]\w*` + `(` ⇒ **hook** (2203 Use-prefix warn logic carries over,
     :1231). Cross-guard: hook-classified AND TYPE == `FRuiNode` ⇒ **UETKX2321** error.
  3. name + `=` ⇒ **value export**. Empty TYPE ⇒ inference: initializer must begin
     `Ident(`/`Ident{`/`Ident<` (names the type) else **UETKX2322** error.
  4. name + `(` and any other TYPE ⇒ **util function**.
  Anything else at top level (e.g. `struct`/`class`/`using`) is NOT classified — see the
  STOP-AND-ASK table (§11): today such code only exists inside `module{}` bodies.
- **U-03 — Alias plane = codegen-time REWRITE, not C++ shims.** Exported symbols keep their
  current emission home (global scope / `namespace <Module>` for legacy wrappers), so:
  - `import { A as B }` — every reference to `B` in the importer (markup tag `<B/>`, bare hook
    call `UseB(...)`, bare value/util identifier) is REWRITTEN to `A` at emit via the existing
    `Qualified` substitution plane (`PrefixHookCalls(Code, Qualified)` UetkxCodegen.cpp:540;
    `FEmitter::PrefixHooks` :871; tag resolution consults the import map so `<B/>` emits
    `FAUetkxProps` + `A(...)`). The skip-noncode scanner already protects strings/comments.
  - `import * as X` — `X::Member` references are rewritten to `Member` (the `Name::` scan
    pattern already exists in the codemod's module-ref pass, RUIMigrateImportsCommandlet.cpp
    Pass 2). Each `X::Member` is validated against the target's export table (2301/2302 wording,
    §4 note).
  - `import D from "./x"` — `D` aliases the target's `export default` symbol; same rewrite plane.
  Parse-time guard **UETKX2325**: an import alias (`as` name, `* as` name, default name) that
  collides with any same-file top-level declaration name or another import = error.
- **U-04 — Value/util emission (two-phase-aware).** The `.inl` is included TWICE per aggregator
  TU (`#define RUI_UETKX_DECL_PHASE` … `#undef` … body include — UetkxDriver.cpp:759-764), so:
  - **value** ⇒ DECL PHASE ONLY: `inline const <T> Name = <expr>;` (inference ⇒
    `inline const auto Name = <expr>;`). Exports are immutable by construction (family
    semantic — module-level mutable state is not a thing). `#line`-mapped exactly like module
    bodies today (see `Palette.uetkx.inl.expected` — the module body sits in the DECL phase
    with `#line 2 "Palette.uetkx"` / restore).
  - **util** ⇒ hook shape minus Ctx: DECL PHASE `inline R F(Params);` fwd-decl, BODY PHASE
    `inline R F(Params) { body }` (mirror `EmitHookInl`, UetkxCodegen.cpp:790-816; no
    registration, no HookSig contribution).
  - Non-exported values/utils are wrapped by the existing `WrapPrivate(E, PrivNamespaceFor(
    Basename))` path (UetkxCodegen.cpp:1538-1541, PrivNamespaceFor :673) and same-file
    references qualified via the `Qualified` map — identical to private hooks today.
- **U-05 — Deprecation-window parsing.** The scanner keeps `ParseComponent`/`ParseHook`/
  `ParseModule` (UetkxFileScan.cpp:1078/:1211/:1301) as the LEGACY path behind the same
  keyword dispatch (:1677-1700), each emitting **UETKX2320** (warn) at the keyword. The NEW
  path triggers when the token after optional `export` is NOT one of
  `component|hook|module|import` and not `{`/markup — i.e. a typed decl head. Mixed files are
  legal (§8 matrix). Wrapper REMOVAL is a later minor, owner-triggered (G-10) — this plan does
  NOT delete the legacy path.
- **U-06 — Value edges are eager (G-08).** The 2306 value-cycle DFS (UetkxDriver.cpp:241-242
  comment; DFS shared with CheckDrift, :1040) extends its edge set from hook/module imports to
  hook/module/**value/util/default** imports. Component edges stay exempt (two-phase pass
  legalizes them — TD-023 RESOLVED record).
- **U-07 — TD-026 key shape.** Private-member runtime identity key =
  `RuiPriv_<Basename>::<Name>` — exactly the emitted C++ qualified name, so registry identity
  and symbol identity coincide. Exported names stay SHORT (already globally unique via the
  UETKX2106 ledger — UetkxDriver.cpp:798 `NameToFile`, message :816). Renaming a file renames
  `RuiPriv_<Basename>` ⇒ private components remount (state reset) on the next sweep —
  **documented, accepted semantic (G-01)**, written into Diagnostics/HMR docs in M8.
- **U-08 — `export default` is a STATEMENT, one per file.** `export default <Name>;` may appear
  anywhere in the decl region; `<Name>` must be a declared top-level name (else UETKX2323);
  second occurrence = UETKX2327. It does NOT implicitly export the name for named import — a
  file may `export default X;` while X stays otherwise private (ES parity); the default symbol
  is recorded in the sidecar/export table as kind + `"default": "X"`.
- **U-09 — `export { a, b };` list.** Preamble-or-decl-region statement; sets `bExported` on
  already-scanned OR later-scanned decls (two-pass flag resolution at end of scan). Unknown
  name ⇒ UETKX2323; name already exported (inline or a previous list) ⇒ UETKX2324.
- **U-10 — CodegenVersion 2 → 3.** Value/util emission, alias rewrites, and the TD-026 key
  change all alter committed bytes ⇒ bump `FUetkxDriver::CodegenVersion` (UetkxDriver.h:56;
  fingerprint format UetkxDriver.cpp:27) ONCE, and re-pin every committed `.uetkx.inl`,
  `<Module>.Uetkx.gen.cpp`, and `ContractFixtures/*.expected` in ONE **[REPIN]** window (M3,
  after M1+M2+M3 all land — the imports-leg M6+M7 precedent).

## §3 — Verified anchor table (every touch point; re-verify any line that looks shifted)

| Surface | File (repo-relative) | Anchors (verified 2026-07-17) |
|---|---|---|
| Scanner (C++) | `Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp` | ParseParams :260-354 (colon form, `<`-in-type note :265-296); ParseImport :901 (host-include `bHostInclude` :935; 2303 :943/:1051/:1058); ParseComponent :1078 (2100 :1097-1101, params :1111, 2101 :1152, 3007 :1157, Order push :1205); ParseHook :1211 (2203 :1231); ParseModule :1301; main decl loop :1645+ (2309 :1665-1668, `export` gate :1670-1676, dispatch :1677-1700); 2317 hints :1617-1640 (emits :1627/:1637 — **2316 is never emitted in C++**; it is LSP-only, uetkxWorkspace.ts L307-323); `ScanPreamble` :1759 |
| Scanner (header) | `Plugins/ReactiveUI/Source/ReactiveUIInterp/Public/UetkxFileScan.h` | `EUetkxDeclKind` :37; decl structs' `bExported`/`ExportAt` :82-124; `Imports` :136; `Order` :144; `FUetkxPreambleScan` :175-180; `ScanPreamble` decl :219 |
| Codegen | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp` | `PrefixHookCalls(Code, Qualified)` :540; `PrivNamespaceFor` :673; `EmitHookInl` :790; `EmitModuleInl` :817; FEmitter `PrefixHooks` :871; **RegisterComponentId emit :1520-1522** (shape below, §M3); RegisterNamedFactory emit :1532 (exported-only); `WrapPrivate` + "RegisterComponentId is KEPT either way" comment :1538-1541; `CompileSource` :1567 (PrivNs :1602; phase-wrap comment :1634-1637, `#if RUI_UETKX_DECL_PHASE` assembly :1677-1681; decl-kind switch :1643-1675; file-level HookSig = FIRST component's, :1657-1660; per-component `_RUI_HOOK_SIG` constant emit :1524-1525; hook emit :1665; module emit :1672) |
| Codegen (header) | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Public/UetkxCodegen.h` | resolver fwd-decl :16; `FUetkxCompileOutput.Diags` :22; `CompileSource` :42 |
| Resolver | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Public/UetkxResolve.h` + `Private/UetkxResolve.cpp` | `IUetkxImportResolver::Resolve` h:30; `FUetkxFsResolver::Resolve` h:65 / cpp:187; resolution loop cpp:387; **2300 :390 · 2308 :408 · 2302 :432 · 2301 :437 · 2305 :451 · 2307 :474 · 2304 :526**. ALSO (M4 touch points): `KindWord` cpp:31-36 maps EUetkxDeclKind→word for diag text (component/hook/else-module — needs Value/Util words); `FindExporter(Name, OutKind)` h:50/:71 impl cpp:317 + `ExportIndex` name→kind cache h:92 (feeds 2305/2307 fix-its — must learn the new kinds) |
| Driver | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp` + `Public/UetkxDriver.h` | sidecar writer `"v", 3` :50 (diags land in field `"diagnostics"` :82); 2306 invariant comment :241-242; **eager-edge kind filters :287 and :703** (`Kind == Hook \|\| Kind == Module` — the exact two sites U-06/M4 widens to value/util/default); `BuildAggregators` :526; two-phase emit :759-764; `CheckDrift` :784; 2106 ledger :798 + message :816 (+ skip-incumbent note :439); `ImportersOf` :874 (**currently has ZERO callers** — see M5); `CompileAllRoots` :909/:912; cycle DFS :1040. Header: **CodegenVersion = 2** h:56 (v2 note h:55); API decls h:91/:99/:104/:109 |
| Codemod | `Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/RUIMigrateImportsCommandlet.cpp` (+ .h) | idempotent-write rule :38; redundant-include tidy :212; Pass 1 export-everything :247-274 (`InsertCp` `export ` :274); Pass 2 imports :287; duplicate-export ledger :344 |
| HMR / identity | `Plugins/ReactiveUI/Source/ReactiveUICore/Public/RuiNode.h` + `Private/RuiNode.cpp`; `Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/UetkxHmrController.cpp/.h` | `RegisterComponentId(void*, FName)` h:98, impl cpp:55; `RUI_COMPONENT` macro h:157; anon-id fallback h:184; process-global registries cpp:17 (`PtrToId`), cpp:42 (`Factories`), cpp:107 (`HookSignatures`), cpp:108 (`Overrides`). HMR controller (VERIFIED 2026-07-17 — recompile-based, no interp swap, and **no site reconstructs a component id from a decl name**): `NotifyCodegen` :278-309 (RecentErrors tail :284-291; "ReactiveUI" Message Log named in comment :283), `TriggerCompile` :311-322 (`LC->Compile` :320), `OnPatchComplete` :324-348 (Swaps counter :331, Slate note "HMR: patched %s" :335-342), `RefreshLiveRoots` :350-360 (`FRuiReconciler::ForEachLive` → `HmrRefreshAll`) |
| Watcher / Message Log | `Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/UetkxWatcher.cpp` | `MessageLogName` "ReactiveUI" :30; `ReportDiags` :233 (resolved-note :243, TB-8 tokenized jump rows :255-281); **severity-0-only filter :318** — warnings (incl. the new 2320) NEVER reach the Message Log, only errors do (2320 surfaces via sidecar/LSP; M7's zero-2320 gate must parse the compile/commandlet output, not the log); `Sweep` :284; `NotifyCodegen` handoff :330. Owned by M5/M7 |
| Editor preview | `Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/UetkxPreview.cpp` (+ `SUetkxPreviewPanel.cpp` — status UI only, VERIFIED no id construction) | scan :23; no-component message :30-34 (value/util-only files land here — acceptable); pick-by-name from `Scan.Components` :37-46; **bare-FName mount** `RUI::HasNamedFactory`/`RUI::Named` :53-65 — reads the EXPORTED-only named-factory registry, so M3's short-key-for-exports claim holds; but a PRIVATE component gets the misleading "not compiled yet — save the file" hint (it never registers a factory) — fix the message in M3. Test: `Source/RuiHostTests/Private/ReactiveUIEditorPreviewTest.cpp` |
| LSP embedded-C++ (was MISSED) | `ide-extensions/lsp-server/src/virtualDoc.ts` (+ `sourceMap.ts`, `clangdProxy.ts` downstream) | import-surface interface L81-83 carries hooks/modules/components ONLY; surface build L142-170; per-decl virtual-doc emit L401 (components) / L414 (hooks) / L422 (modules). **Value/util decls + alias/`* as`/default bindings must join both** or clangd never sees the new C++ regions — owned by M6 |
| LSP scanner mirror | `ide-extensions/lsp-server/src/uetkxFileScan.ts` | `parseImport` L938-1045 (host-include L944-977, bindings L989-1004, `from` L1009); export gate L1327 + dispatch L1332/L1335/L1338; 2309 L1321; result interfaces L513-608 (`UetkxFileScanResult` L599-608); 2100 L1060; 2203 L1154; 2105-warn L1386-1390; 2317 L1287/L1299 |
| LSP workspace | `ide-extensions/lsp-server/src/uetkxWorkspace.ts` + `src/server.ts` | export index `getDecls` L43-68, sweep L272-279, `findExporter` L283-292; `resolveDiagnostics` L300-371 (2300 L328-335, 2308 L341-349, 2302 L358, 2301 L360-366, 2316 L316-323); `resolveSpecifier` L113-128; `importCursorAt` L392-440. server.ts: completion L268 (import branches L289-343), definition L542-562 / `markupDefinition` L608-674, diag merge L230-244, formatting L678-698. **No rename handler exists** |
| LSP sidecar gate | `ide-extensions/lsp-server/src/diagsSidecar.ts` | **DISCREPANCY (fix in M6): reader gates `parsed.v !== 2` at L27 while the driver writes v3 (UetkxDriver.cpp:50)** — CONFIRMED 2026-07-17: every compiler sidecar is silently dropped by the LSP today (live-parse diags only). Field name `"diagnostics"` matches both sides (writer :82, reader L29). TD-024 (TECH_DEBT.md L615) intends "≥ 3" |
| Formatter (C++) | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxFormatter.cpp` | `Format` :765-931; preamble canonicalization :838-894 (import emit :881-888); `TrueStart` :795-812; `DeclNext` :813-824 (**third kind-switch — easy to miss**); `FmtDecl` :825-836 + loop :897-912; NOTE all three switches fall through `default:` → `Scan.Modules` and will silently MIS-INDEX when `Value`/`Util` kinds appear — each needs explicit new cases (TS mirror has the same shape); `FmtComponent` :697-720 (`export ` :699); `FmtHook` :744-756; `FmtModule` :758-762; config via `ResolveOptions` :933-946 |
| Config walk-up | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxConfig.cpp` | `FUetkxConfig::Load` :12-68; `ModuleRootFor` :70-92; `RootAnchorFor` :94-106 |
| Formatter (TS) | `ide-extensions/lsp-server/src/formatUetkx.ts` | `formatUetkx` L41-132; preamble L78-112; per-decl dispatch L57-76/L115-123; `fmtComponent` L503-516, `fmtHook` L532-539, `fmtModule` L541-544 |
| TextMate grammar | `ide-extensions/vscode-uetkx/syntaxes/uetkx.tmLanguage.json` | includes L7-21; `import-decl` L33-49; `import-host-include` L50-57; `export-keyword` L58-61 (lookahead `(?=component|hook|module)` L59 — MUST widen); `component-decl` L62-68; `hook-decl` L69-75; `module-decl` L76-82 |
| Contract fixtures | `Source/RuiHostTests/ContractFixtures/` | 15 `.uetkx` sources; goldens: 12 `.inl.expected` + 3 `.diags.expected` (BadAttr, ImportError, NestedFinalReturn). TD-026 evidence: `ChildrenForward.uetkx.inl.expected` line 35 — a PRIVATE component registers the BARE name (see §M3) |
| Schema/vocabulary + file template | `Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/RUIExportSchemaCommandlet.cpp` (22 lines); `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxFileActions.cpp` | VERIFIED 2026-07-17: the commandlet (run name `-run=RUIExportSchema`) just writes `FUetkxCodegen::ExportSchemaJson()` → `Saved/ReactiveUI/schema.json` (:11-22). `ExportSchemaJson` = UetkxCodegen.cpp:1753-1888; fields `v:1`, `elements`, `styleKeys`, `slotPrefix`+`slotKeys`, `hooks` (builtin hook names), `eventPayloads` — **NO diagnostics and NO decl kinds** → the schema is untouched by this leg. Committed copy: `ide-extensions/lsp-server/src/uetkx-schema.json` (+ vendored `vscode-uetkx/server/`, `visual-studio/UetkxVsix/server/` copies; consumer `uetkxSchema.ts`). The "New Component" template = `FUetkxFileActions::CreateComponentFile` (UetkxFileActions.cpp:27-34) — emits legacy `component %s {`, pinned by `ReactiveUIUetkxSchemaTest.cpp:98` (`component MyPanel {`); migrate template + test assertion in M7 |
| Tests | `Source/RuiHostTests/Private/` | `ReactiveUIUetkxScannerTest.cpp` (corpus replay) · `...CodegenTest.cpp` · `...ContractTest.cpp` · `...FormatterTest.cpp` · `...ResolveTest.cpp` · `...DriverTest.cpp` · `...CycleTest.cpp` · `...SchemaTest.cpp` · `...WaveGTest.cpp` · `...IncludeRetirementTest.cpp` (+ `RuiHostTests.Uetkx.gen.cpp` aggregator) |
| Corpus | `ide-extensions/lsp-server/test-fixtures/uetkx-scanner-cases.json` (+ `uetkx-formatter-cases.json`) | `_tiers.familyCore = ["skipNoncodeMarkup","findMatchingMarkup","fileScan"]` L19 (perLeg L20); formatter corpus 25 cases under `"cases"` |
| Corpus gate | `scripts/corpus-hash.mjs` + `plans/family-corpus.hash` | normalize `UETKX\|GUITKX\|UITKX → TKX` L29-31; hash calc L34-69; current pinned hash `917dd8cd…9169` — **identical in all three repos today** (verified: Unity `Plans~/family-corpus.hash`, Godot `plans/family-corpus.hash`) |
| Changelogs | root `CHANGELOG.md` (top `## [0.11.0] — 2026-07-17`) · `Plugins/ReactiveUI/CHANGELOG.md` (byte-identical mirror; `scripts/verify-mirror.mjs`) · `ide-extensions/changelog.json` + `ide-extensions/scripts/changelog.mjs` (Lane B) · `plans/PENDING_CHANGELOG.md` (lanes at L5, format L9, currently drained) · `plans/DISCORD_CHANGELOG.md` (entry shape L6-13, ≤2000 chars) | |
| Versions | `Plugins/ReactiveUI/ReactiveUI.uplugin` L4 `"VersionName": "0.11.0"` · `scripts/bump.mjs` (lockstep partners) · `VERSIONING.md` (deprecation one-minor rule; grammar-contract binding at "### `.uetkx` Syntax") | |
| Docs site | `ReactiveUIUnrealDocs~/src/pages/Uetkx/ImportsPage.tsx` (GRAMMAR L15-22, DIAGS table L37-50, sections L64-158) · `src/pages/CompanionFiles/CompanionFilesPage.tsx` (module/hook companion examples L9-26) · routes `src/docs.tsx` L138-143 (`/uetkx/imports`) + L148-153 (`/guides/companion-files`) | |
| Skills | `.claude/skills/grammar-contract/SKILL.md` (compiler-is-grammar-of-record L10-11; **Invariant 2 L12-15 spells the family decl-kind list as `component`/`hook`/`module` — rewrite in M8**; corpus-case-with-change rule L20-22; cross-repo sync L45-58) · `release-process` · `docs-sync` · `rebuild-ide-extensions` · `new-component` (SKILL.md L48 scaffold snippet `component ScoreCard(Title: FText, …) {` — wrapper + colon params, migrate in M7/M8) | |
| Tech debt | `plans/TECH_DEBT.md` | **TD-026 L653-663** (the runtime-privacy gap M3 closes) · TD-024 L615 (sidecar skew gate) · TD-009 L150-159 / TD-018 L384-393 (corpus mirroring) · TD-023 (two-phase, RESOLVED) · TD-025 (staleness fixpoint, RESOLVED) · TD-030 L778 (`import "@…"` family convergence) |

## §4 — New diagnostics (family block 2320–2329; wording is FAMILY-CANONICAL — siblings copy modulo prefix)

> **FAMILY CANONICAL — this table is the family's registration of record for the 2320 band.**
> Both sibling execution plans (Godot `GUITKX232x`, Unity `UITKX232x`) are being renumbered to
> match THIS table's numbering and meanings. Any change here (code, meaning, severity, message
> shape) requires tri-repo re-registration — update all three execution plans in the same
> coordination window, never unilaterally.

| Code | Sev | Exact message (printf shapes) |
|---|---|---|
| UETKX2320 | warn (1) | `` `%s` wrapper syntax is deprecated — write a plain typed declaration (`export FRuiNode Name(...)` / `export <Type> UseName(...)` / `export <Type> Name = ...`); run `-run=RUIMigrateEsModules`. Removed in the next minor. `` (`%s` = `component`/`hook`/`module`; anchored at the keyword) |
| UETKX2321 | err (0) | `` `%s` is `Use`-prefixed but returns FRuiNode — did you mean a component? (hooks must not return markup nodes) `` |
| UETKX2322 | err (0) | `` cannot infer the type of `%s` — the initializer must name the type (`T(...)` / `T{...}`), or declare it: `export <Type> %s = ...` `` |
| UETKX2323 | err (0) | `` `export` names `%s`, which is not declared in this file `` (covers `export { … }` lists AND `export default`) |
| UETKX2324 | err (0) | `` duplicate export of `%s` (already exported inline or in a previous export list) `` |
| UETKX2325 | err (0) | `` import alias `%s` collides with a declaration or another import in this file `` |
| UETKX2326 | err (0) | `` %s has no default export — use a named import: import { ... } from "%s" `` |
| UETKX2327 | err (0) | `` duplicate `export default` (a file has at most one default export) `` |
| UETKX2328–2329 | — | reserved (family) |

**Recorded family divergences** (precedent: the 2105 severity divergence already on record):
- **2322 (value type-inference failure) is not emitted in Godot** — GDScript is dynamically
  typed, so the inference-sugar failure mode does not exist there; the number stays RESERVED to
  this meaning in the Godot band (never reassigned).
- **Mixed wrapper+plain files are LEGAL in Unreal and Godot** (the §8 matrix "Mixed old + new"
  row) **but an ERROR in Unity** — Unity's per-file namespace mode makes mixing ambiguous;
  Unity polices it with a Unity-LOCAL code outside the family 2320–2329 band (its execution
  plan allocates it). The Unreal scanner/codegen/formatter/codemod behavior in §8 is unchanged.

Registries to update in lockstep (same commit as each code's first emit): `UetkxFileScan.cpp` /
`UetkxResolve.cpp` (emit sites), `ide-extensions/lsp-server/src/uetkxFileScan.ts` +
`uetkxWorkspace.ts` (mirrors), `ImportsPage.tsx` DIAGS table (L37-50), corpus cases (normalized
`TKX` codes). The schema/vocabulary export needs NO update — RESOLVED 2026-07-17:
`ExportSchemaJson` (UetkxCodegen.cpp:1753-1888) carries no diagnostics (fields: v/elements/
styleKeys/slotKeys/hooks/eventPayloads only; §3 schema row). Existing codes
2300–2317 keep their meanings and messages BYTE-IDENTICAL — do not touch them.

---

## §5 — Milestones (mechanical order; each ends green on its verify block)

Legend: **[REPIN]** = changes committed generated output — ALL fold into ONE CodegenVersion-3
re-pin window at the end of M3. Engine command prefix used below:
`<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject`
(engines at `C:\Program Files\Epic Games\UE_<ver>`; always redirect output to a file; parse
`report\index.json`, never exit codes).

### M0 — Gates, audits, corpus agreement

- M0.1 Branch `feat/uetkx-es-modules` per the banner rule. Confirm tree state:
  `git log --oneline -5`, `RUICompile -check` green before ANY edit.
- M0.2 Diagnostic audit: done, recorded in the banner (2320–2329 free). Re-run the grep after
  branching; if anything new appeared in 232x, STOP AND ASK.
- M0.3 Record the G-05 supersede banner in `plans/archive/IMPORT_EXPORT_PLAN.md` §A1 (edit is
  part of this leg's PR, not a separate commit train).
- M0.4 **Corpus-first (G-13):** author the new-grammar `familyCore` scanner cases (new decl
  forms ×4 kinds, export list, default, rename, `* as`, every 2320–2327, mixed old+new file,
  private-vs-export shapes) in `uetkx-scanner-cases.json` as a DRAFT; compute
  `node scripts/corpus-hash.mjs` and hand the case list + prospective hash to the owner for
  sibling agreement (TD-009/TD-018 lanes). The hash is RE-pinned when the leg lands; siblings
  adopt the same cases in their legs. Do not gate local work on the sibling PRs (G-13 allows
  parallel), but the FINAL hash must match tri-repo before release (§9).

Verify: `node scripts/corpus-hash.mjs` (prints hash; `--check` fails until `--write`),
`node scripts/verify-mirror.mjs`, `node ide-extensions/scripts/changelog.mjs verify`.

### M1 — Scanner: new grammar + classification (C++ + TS mirror, corpus-locked)

- Files: `UetkxFileScan.h/.cpp` + `uetkxFileScan.ts` (anchors §3 row 1-2, 8).
- Data model (`UetkxFileScan.h`): `EUetkxDeclKind` (:37) gains `Value`, `Util`. New
  `FUetkxValueDecl { Name, NameAt, Type /*empty=inferred*/, Init, InitAt, bExported, ExportAt, At }`
  and `FUetkxUtilDecl { Name, NameAt, RetType, Params /*raw C++ text*/, Body, BodyAt, bExported,
  ExportAt, At }`. `FUetkxComponentDecl`/`FUetkxHookDecl` gain `bLegacySyntax` (drives 2320 +
  formatter form-preservation). `FUetkxImportDecl` gains
  `TArray<FString> LocalNames /*as-aliases; == Names when no rename*/`, `bool bNamespace`,
  `FString NamespaceAlias`, `bool bDefault`, `FString DefaultAlias`. `FUetkxFileScanResult`
  gains `FString DefaultExportName; int32 DefaultExportAt; TArray<FUetkxValueDecl> Values;
  TArray<FUetkxUtilDecl> Utils;` plus export-list records. Mirror 1:1 in `uetkxFileScan.ts`
  (interfaces L513-608).
- `ParseImport` (:901): add the three new heads —
  `import * as X from "…"`, `import Name from "…"`, and `as` inside braces
  (`{ A as B, C }`); 2303 dedup keys on LOCAL name; 2325 collision check at end of scan.
- Main loop (:1645+): keep the legacy keyword dispatch (:1677-1700) BUT emit **2320** at each
  wrapper keyword; add the new-path head parser (U-02) for typed decls, `export { … };`, and
  `export default Name;` (U-08/U-09). New-form component params parse as raw C++ text split by
  the ParseParams comma/depth walker (:260-305) with per-piece `Type Name = Default`
  extraction (name = trailing identifier; pin with corpus cases incl. `TArray<TMap<A,B>> X`,
  `const FString& S`, defaults containing `<`/`>>`).
- New-form hooks: head `R UseX(raw-params) {` — store `RetType` (replaces the old `-> R`);
  cross-guard 2321.
- 2105 one-component-per-file warn (TS L1386) applies unchanged to new-form components.
- `ScanPreamble` (:1759) picks up the new import heads for free (it reuses ParseImport) —
  verify with a driver test.
- Corpus: add the M0.4 cases; run BOTH sides.

Verify:
```bat
<Engine>… -ExecCmds="Automation RunTests ReactiveUI.Uetkx.Scanner; Quit" -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput -ReportExportPath=<scratch>\report
cd ide-extensions\lsp-server && npm ci && npm run build && node --test out/test/*.test.js
node scripts/corpus-hash.mjs
```

### M2 — Codegen: values/utils + alias plane + new-form emission

- File: `UetkxCodegen.cpp` (anchors §3 row 3).
- Values: new `EmitValueInl` (module-body precedent `EmitModuleInl` :817 — DECL-phase only,
  `#line`-wrapped): `inline const <T> Name = <Init>;` / `inline const auto Name = <Init>;`.
  Private ⇒ `WrapPrivate` (:1538-1541).
- Utils: new `EmitUtilInl` cloned from `EmitHookInl` (:790) minus the `FRuiContext& Ctx`
  injection and minus HookSig participation.
- New-form components/hooks feed the EXISTING emitters (props struct + impl + wrapper +
  registrations :1520-1532 unchanged in this milestone; file-level HookSig :1657-1660 unchanged).
- Alias plane (U-03): `CompileSource` builds `TMap<FString,FString> AliasToTarget` from the
  scan's imports; merge into the `Qualified` substitution consumed by `PrefixHookCalls` (:540)
  and tag resolution; add the `X::Member → Member` namespace-alias rewrite in the same pass.
  Emission ORDER (`Scan.Order`) now interleaves 5 kinds — values/utils slot into both phase
  strings exactly where components/hooks/modules do today (:1636-1677 region).
- **DO NOT re-pin goldens yet** — M3 shares the window.
- Tests: `ReactiveUIUetkxCodegenTest.cpp` — value decl-phase-only emission; util fwd-decl +
  body split; inferred `auto`; alias rewrite (tag, hook call, `X::` qual, renamed value);
  private value/util wrapped + qualified; mixed 5-kind file emits in source order.

Verify: `Automation RunTests ReactiveUI.Uetkx.Codegen` (+ `.Scanner` regression) — same
command shape as M1.

### M3 — **TD-026 runtime-privacy fix (MANDATORY, its own milestone)** **[REPIN]**

The defect (TECH_DEBT.md L653-663): compile-time privacy is real (per-file
`RuiPriv_<Basename>` namespace), but the RUNTIME registries are process-global and keyed by
the BARE name — two files' private `Row`s collide last-swap-wins in HMR. Verified evidence:
`ChildrenForward.uetkx.inl.expected:35` — a private component inside
`namespace RuiPriv_ChildrenForward` still registers the short name.

- Emission change (UetkxCodegen.cpp:1520-1522):
  ```cpp
  // BEFORE (every component, exported or not):
  static const FName GChildrenForwardUetkxId = RUI::RegisterComponentId(
      (void*)&ChildrenForward_UetkxImpl, FName(TEXT("ChildrenForward")));
  // AFTER — non-exported decls ONLY (exported keep the short name; 2106 ledger guarantees
  // uniqueness for those — UetkxDriver.cpp:798/:816):
  static const FName GChildrenForwardUetkxId = RUI::RegisterComponentId(
      (void*)&ChildrenForward_UetkxImpl, FName(TEXT("RuiPriv_ChildrenForward::ChildrenForward")));
  ```
  Gate on `Decl.bExported` (already in scope at the emit site — the RegisterNamedFactory gate
  :1532 and the WrapPrivate decision :1538-1541 read it). The qualifier string MUST be
  `PrivNamespaceFor(Basename) + TEXT("::") + Name` — one source of truth, no re-derivation.
- Ripple: `RuiNode.cpp` registries (`PtrToId` :17, `Factories` :42) and the HMR-side maps
  (`HookSignatures` :107, `Overrides` :108) key by FName — file-qualified ids flow through with
  NO code change there (the KEY changes, not the mechanism). `RUI_COMPONENT` (RuiNode.h:157,
  hand-written C++) is untouched — hand code is ambient (G-02).
  VERIFIED 2026-07-17: neither `UetkxHmrController.cpp` nor `SReactiveUetkxHmrPanel.cpp`
  reconstructs a component id from a decl name (the controller only drives Live Coding +
  `FRuiReconciler::ForEachLive`/`HmrRefreshAll`; the panel is status UI) — no routing needed.
  The ONE by-name consumer is the preview: `UetkxPreview.cpp:53-65` mounts via
  `RUI::HasNamedFactory`/`RUI::Named(FName(Name))` against the EXPORTED-only named-factory
  registry (gate UetkxCodegen.cpp:1529-1535), so it keeps working unchanged; but its
  "not compiled yet — save the file" hint is WRONG for private components (they never register
  a factory) — reword for the un-exported case in this milestone ("private — add `export` to
  preview it"). `ReactiveUIEditorPreviewTest.cpp` covers the preview path.
- **[REPIN] window (with U-10):** bump `CodegenVersion` 2→3 (UetkxDriver.h:56; update the
  version-history comment h:55); run `-run=RUICompile -full`; re-commit every `.uetkx.inl`,
  `<Module>.Uetkx.gen.cpp`, and re-pin all `ContractFixtures/*.inl.expected` (M1/M2 shapes
  land in the same window).
- New fixture pair pinning NON-COLLISION: `PrivPairA.uetkx` + `PrivPairB.uetkx`, each with a
  private (un-exported) component `Row` + an exported component using it; goldens assert the
  two distinct `FName(TEXT("RuiPriv_PrivPairA::Row"))` / `…PrivPairB::Row` keys. Automation
  test (ContractTest or a new `ReactiveUI.Uetkx.PrivateIdentity` in CodegenTest): compile both,
  assert registry ids differ and `RUI::` named-factory table contains NEITHER.
- Documentation of the rename-remount semantic (U-07): Diagnostics/HMR docs page + a code
  comment at the emit site: "renaming a file renames RuiPriv_<Basename> ⇒ private members get
  fresh runtime identity ⇒ remount/state-reset — G-01 documented semantic."
- Close the ledger: TECH_DEBT TD-026 → RESOLVED (this section's evidence), same commit.

Verify:
```bat
<Engine>… -run=RUICompile -full   & <Engine>… -run=RUICompile -check
<Engine>… -ExecCmds="Automation RunTests ReactiveUI.Uetkx; Quit" … -ReportExportPath=<scratch>\report
git diff --stat -- "*.inl" "*.gen.cpp" "*.expected"   :: the [REPIN] is deliberate and reviewed
```

### M4 — Resolver + driver: full ES surface

- Files: `UetkxResolve.h/.cpp`, `UetkxDriver.cpp` (anchors §3 rows 5-6).
- Export tables (`GetExports`) gain value/util kinds + the default-export marker; sidecar
  `decls` entries gain `"kind": "value"|"util"` and the file record gains
  `"default": "<Name>"` (writer UetkxDriver.cpp:40-70 region, `"v": 3` stays — the SCHEMA
  version does not bump for additive fields; the LSP reader fix is M6).
- Resolution loop (UetkxResolve.cpp:387-530): named/renamed imports validate the TARGET name
  (`Imp.Names[n]`, not the local alias) → 2301/2302 unchanged; `* as` validates each
  `X::Member` use against the table (2301/2302 wording with the qualified spelling in `%s`);
  default import → 2326 when the target has no default. 2304 unused-import (:526) counts uses
  of the LOCAL alias (incl. `X::` quals and the default alias).
- 2306 value-cycle DFS (:1040 + CheckDrift share :241-242): edge set widens per U-06 (any
  import that binds a hook/module/value/util/default-value symbol is an eager edge; imports
  binding ONLY components stay lazy). The two concrete kind-filter sites are
  **UetkxDriver.cpp:287 and :703** (`Kind == Hook || Kind == Module` today — widen both).
  `export_hash` input rows now include value/util/default
  entries so the TD-025 fixpoint (`CompileAllRoots` :912) re-sweeps importers when any of
  them change.
- Diag-text plumbing: `KindWord` (UetkxResolve.cpp:31-36) gains `value`/`util` words;
  `FindExporter`/`ExportIndex` (UetkxResolve.h:50/:71/:92, cpp:317 — the 2305/2307 fix-it
  index) learns the new kinds so unimported value/util references get fix-it import lines.
- 2106 ledger (:798/:816): UNCHANGED — still exported names only; value/util exported names
  join the same ledger (they are C++ symbols in the same TU space).
- Tests: `ReactiveUIUetkxResolveTest.cpp` — every 2300-2309 regression + new 2321-2327 cases;
  rename/`* as`/default resolution matrix; value-cycle A⇄B via value import errs 2306 with the
  chain; `ReactiveUIUetkxDriverTest.cpp` — export_hash moves on value edit → importer
  recompiles in ONE sweep (TD-025 pattern); sidecar round-trip with default/value kinds.

Verify: `Automation RunTests ReactiveUI.Uetkx.Resolve+ReactiveUI.Uetkx.Driver+ReactiveUI.Uetkx.Cycle`.

### M5 — HMR + staleness semantics

- File: `Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/UetkxHmrController.cpp` (+ the
  watcher path). HMR v2 is Live-Coding-driven whole-project recompile — there is NO interp
  swap to extend; the work here is identity + messaging:
  - Value/util-only file edit → compiled + sidecar as today. CORRECTED CLAIM (audit
    2026-07-17): there is NO importer-naming rebuild note today — `FUetkxDriver::ImportersOf`
    (:874) has **zero callers** (its header comment "the watcher names these in the HMR
    hook/module rebuild note" is aspirational; the watcher only logs `[RUI] sweep (%s): %d
    compiled, %d error(s)` at UetkxWatcher.cpp:334 and the controller notes "HMR: patched
    %s"). This milestone WIRES ImportersOf into the sweep/notify path (name the importers of
    an edited support file, all five kinds) — or, if the owner prefers, records the dead API
    in TECH_DEBT instead. Do not "extend" a message that does not exist.
  - Private-member identity: after M3, a private component's id is file-qualified — the HMR
    maps (`HookSignatures`/`Overrides`, RuiNode.cpp:107-108) stop aliasing across files.
    Pin with a test: two files' private `Row`s, edit one, other's state untouched.
  - Rename-remount: renaming `A.uetkx` → `B.uetkx` remounts A's PRIVATE members (new ids) —
    assert + document (M3's doc item). Exported members keep identity (name-keyed).
  - G-09: signature-change reset rules untouched (file-level HookSig :1657-1660; per-component
    `_RUI_HOOK_SIG` :1524-1525).
- Tests: extend the driver/codegen suites (there is no `.Hmr` suite — CLAUDE.md suite list);
  editor-loop spot-check via the `field-test-editor` skill is an OWNER item, listed in §11.

Verify: `Automation RunTests ReactiveUI.Uetkx` + `ReactiveUI.Boot` (never optional).

### M6 — LSP intelligence + sidecar-gate fix + TextMate + VS2022

- `uetkxFileScan.ts`: M1 mirror (corpus-locked, same JSON).
- Embedded-C++ intelligence (`virtualDoc.ts` — was MISSING from this plan): the import-surface
  interface (L81-83) and surface build (L142-170) carry hooks/modules/components only, and the
  per-decl virtual-doc emit handles components L401 / hooks L414 / modules L422 — add
  value/util decl regions + alias/`* as`/default import bindings so clangd sees the new C++
  (mirror U-04's shapes; `sourceMap.ts` mappings follow; `embeddedCpp.test.ts` +
  `embeddedServer.test.ts` gain cases).
- `uetkxWorkspace.ts` / `server.ts`: completion inside `import` gains `* as` / default /
  `as`-rename forms (branches at server.ts L289-343); export index (`getDecls` L43-68) carries
  value/util/default; `resolveDiagnostics` (L300-371) adds 2321-2327 locals + alias-aware
  2301/2302/2304; go-to-def resolves THROUGH aliases (markupDefinition L608-674): local alias →
  target decl; `X::Member` → member decl. Rename handler: still ABSENT — adding one is OPTIONAL
  scope; if skipped, record in TECH_DEBT (do not silently drop, G-12 lists rename — STOP AND
  ASK the owner whether v-this or follow-up).
- **Sidecar gate fix (TD-024 fold-in):** `diagsSidecar.ts` L27 `parsed.v !== 2` → `parsed.v < 2`
  is WRONG-shaped; correct per TD-024: accept `v >= 2` now that the driver writes 3 (and gate
  STRICT diags on `v >= 3` as originally specified). Update the header comment L2. Add a
  server test.
- TextMate (`uetkx.tmLanguage.json`): widen `export-keyword` L58-61 (drop the
  `(?=component|hook|module)` lookahead — `export` now precedes typed decls, `{`, `default`);
  add `export default` and `export { … }` rules; add `as` + `*` to `import-decl` L33-49;
  keep wrapper rules L62-82 for the deprecation window (recolor as deprecated is optional).
- VS2022 extension consumes the LSP — rebuild the vsix (`rebuild-ide-extensions` skill); grep
  the classifier for a duplicated keyword list and sync if found.
- Formatter (BOTH `UetkxFormatter.cpp` :765-931 and `formatUetkx.ts` L41-132): preamble
  canonicalization emits the new import heads (sorted: host-includes, blank, imports — rename
  spelled `A as B`, namespace `* as X`, default bare); per-decl dispatch gains `FmtValue`/
  `FmtUtil`; legacy wrappers format in LEGACY shape (form-preserving — the formatter NEVER
  migrates; that is the codemod's job); `export { … }` / `export default` lines canonical at
  their source positions. Idempotency corpus cases for every new form.

Verify:
```bat
cd ide-extensions\lsp-server && npm ci && npm run build && node --test out/test/*.test.js && node scripts\smoke.js
<Engine>… -ExecCmds="Automation RunTests ReactiveUI.Uetkx.Formatter; Quit" …
```

### M7 — Codemod: `-run=RUIMigrateEsModules`

- File: extend `RUIMigrateImportsCommandlet.cpp/.h` (same commandlet class family; register the
  new commandlet name; the old `-run=RUIMigrateImports` stays working). **Idempotent** (skip
  write when unchanged — :38 precedent) and re-runnable on half-migrated trees.
- Pipeline (G-10 canonical order):
  1. **Tidy** — reuse the redundant-include tidy (:212 path).
  2. **Export-normalize** — existing Pass-1 machinery (:247-274) already inserts `export `;
     re-verify no-op on the current tree (everything is already exported post-imports-leg).
  3. **Rewrite wrappers → plain declarations** (NEW):
     - `export component Name(P1: T1 = D1, …) {` → `export FRuiNode Name(T1 P1 = D1, …) {`
       (param colon-flip via the scanned `FUetkxParam` records — never regex on raw text);
       param-less `component Name {` → `Name() {`. Body verbatim.
     - `export hook UseX(raw) -> R {` → `export R UseX(raw) {` (no `->` ⇒ `void`).
     - `export module M { members }` → hoist each member: `inline const T N = init;` /
       `inline const T N{init};` → `export T N = init;` / `export T N = {init};` (brace-init
       normalizes to `= { … }` — U-01 values require `=`); member functions → `export` utils.
       A member that is NOT a simple variable/function (struct/class/using/typedef/static
       state) → **DO NOT rewrite that module; print the file + reason and leave the wrapper**
       (2320 remains; surfaced in the summary; §11 STOP-AND-ASK).
     - Cross-file `M::x` references in OTHER files (the Pass-2 `Name::` scan) → replace the
       named import of `M` with `import * as M from "<spec>"` — references keep compiling
       as `M::x` through the U-03 rewrite plane, zero body edits.
  4. **Insert/fix imports** — existing Pass 2 (:287) extended for alias/namespace forms.
  5. **Zero-diagnostics compile gate** — `CompileAllRoots(-full)`; REQUIRE zero errors AND
     zero 2320 warnings outside the explicitly-skipped module files; print the summary.
     Count 2320 from the sweep's diag results / sidecars, NOT the editor Message Log — the
     watcher's Message Log path only reports severity-0 errors (UetkxWatcher.cpp:318), so
     warnings never appear there.
- Sweep universe: `Source/` + `Plugins/` roots; ContractFixtures excluded (hand-edited in M8).
  Real targets today (counted 2026-07-17): **42 `.uetkx` files** — 38 component files + 4 demo
  hook/module support files (`AcceptanceLab.hooks/.style`, `ContextDemo.style`,
  `SimpleCounter.hooks`; module bodies use BOTH `inline const T N = init;` and
  `inline const T N{init};` forms — the hoist covers both) + skill scaffolds
  (`new-component` SKILL.md L48 wrapper/colon-param snippet) + the New Component template:
  RESOLVED — it lives in `FUetkxFileActions::CreateComponentFile`
  (UetkxFileActions.cpp:27-34, emits `component %s {`), NOT in the schema commandlet; its pin
  is `ReactiveUIUetkxSchemaTest.cpp:98` (`Source.Contains("component MyPanel {")`) — migrate
  template text AND that assertion together.
- Migration guide outline (lands in docs, M8): why (file=module) → what changed (table:
  wrapper→plain, colon-params→C++ params, `->R`→leading R, module→values/utils + `* as`) →
  run the codemod → deprecation timeline (0.12 warns, removal later minor) → rename-remount
  note → troubleshooting (each 2320-2327 with fix).

Verify:
```bat
<Engine>… -run=RUIMigrateEsModules          :: run TWICE — second run must be a no-op (idempotency)
<Engine>… -run=RUICompile -full & <Engine>… -run=RUICompile -check
<Engine>… -ExecCmds="Automation RunTests ReactiveUI; Quit" …   :: FULL battery incl. .Demos/.Acceptance/.Boot
```

### M8 — Fixtures, corpus lockstep, docs, changelogs, versions

- **Fixtures:** hand-migrate `ContractFixtures/*.uetkx` to the NEW grammar (goldens re-pin —
  inside the M3 window if sequenced there, else a second deliberate re-pin commit-set); KEEP
  two legacy-form fixtures (e.g. `LegacyWrappers.uetkx` + `.diags.expected` pinning 2320 text;
  `Palette.uetkx` may stay legacy as the module-wrapper regression pin). ADD: `ValueExports`,
  `DefaultExport` (+ importer), `NamespaceImport`, `RenameImport`, `ExportList`,
  `PrivPairA/B` (M3), error fixtures for 2321/2322/2323/2324/2325/2326/2327.
- **Corpus lockstep (G-12):** finalize the M0.4 cases; `node scripts/corpus-hash.mjs --write`;
  the new pinned hash goes to the owner for the sibling PRs (TD-009/TD-018) — release-blocking
  tri-repo match, tracked in §9.
- **Docs site:** rewrite `ImportsPage.tsx` (new grammar block, full import-surface section,
  DIAGS table + 2320-2327 rows, migration guide section or a new `/uetkx/migration-es-modules`
  page — wire the route in `docs.tsx`); rewrite `CompanionFilesPage.tsx` (companion suffixes
  stay cosmetic; module/hook wrapper examples → plain-declaration examples; deprecation call-out
  box); README `.uetkx` section; CLAUDE.md grammar summary + codemod command (lines 15-19).
  `npm run build && npm run lint` + `node scripts/docs-drift.mjs`.
- **Changelogs (all lanes):** stage bullets in `plans/PENDING_CHANGELOG.md` per its L9 format
  as milestones land; at release, drain via `release-process`: Lane A root `CHANGELOG.md`
  `## [0.12.0]` (+ byte-mirror `Plugins/ReactiveUI/CHANGELOG.md`, `verify-mirror.mjs` green);
  Lane B `changelog.mjs add` → `ide-extensions/changelog.json` (vscode+vs2022 0.6.0); Lane C
  `plans/DISCORD_CHANGELOG.md` `## [0.12.0]` entry ≤2000 ASCII chars (awk count per the file's
  header).
- **Versions:** `node scripts/bump.mjs` — plugin `0.12.0` (uplugin L4 + `Version` integer),
  extensions `0.6.0` (lockstep partners incl. vendored lsp-server). VERSIONING.md needs no
  edit (policy already covers syntax deprecation) — but the deprecation entry MUST appear in
  CHANGELOG with the planned removal version, per its Communication section.
- **TECH_DEBT ledger:** TD-026 → RESOLVED (M3); TD-024 → RESOLVED (M6); new entries for any
  accepted caveat (e.g. rename-alias bare-identifier rewrite limits; skipped module files).

Verify: every engine-free gate —
```bash
node scripts/corpus-hash.mjs --check && node scripts/verify-mirror.mjs && node ide-extensions/scripts/changelog.mjs verify && node scripts/check-headers.mjs && node scripts/lint-skills.mjs && node scripts/docs-drift.mjs
cd ReactiveUIUnrealDocs~ && npm ci && npm run build && npm run lint
```

### M9 — Full-matrix close-out (merge gate)

| Surface | Requirement |
|---|---|
| `uetkx-scanner-cases.json` | all M0.4/M1 cases green BOTH sides (C++ Scanner suite + `node --test`) |
| `uetkx-formatter-cases.json` | new-form + legacy-form + mixed idempotency cases green both sides |
| ContractFixtures | every golden byte-matches; new fixtures wired; drift `-check` green |
| Suites | FULL battery `Automation RunTests ReactiveUI` green (incl. `.Boot`, `.Demos`, `.Acceptance`, `.Contract`, `.Uetkx.*`) on UE 5.6 (owner may spot-run 5.7/5.8) |
| Codemod | double-run no-op proof + zero-diagnostics tree |
| LSP | `node --test` + `node scripts/smoke.js` + vsix rebuild |
| Docs/changelogs/versions | §M8 gates green |
| Corpus | tri-repo `family-corpus.hash` match (owner-coordinated; release-blocking, not merge-blocking per G-13) |

Full ordered verify block (copy-paste, run last):
```bat
node scripts/corpus-hash.mjs --check && node scripts/verify-mirror.mjs && node ide-extensions/scripts/changelog.mjs verify && node scripts/check-headers.mjs && node scripts/lint-skills.mjs
<Engine>\Engine\Build\BatchFiles\Build.bat ReactiveUIUnrealDemoEditor Win64 Development -Project=<abs>\ReactiveUIUnrealDemo.uproject -WaitMutex
<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUIMigrateEsModules
<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUICompile -full
<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUICompile -check
<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -ExecCmds="Automation RunTests ReactiveUI; Quit" -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput -ReportExportPath=<scratch>\report
cd ide-extensions\lsp-server && npm ci && npm run build && node --test out/test/*.test.js && node scripts\smoke.js
cd ReactiveUIUnrealDocs~ && npm ci && npm run build && npm run lint
```

---

## §6 — Committed-generated-output flag list (everything a re-pin touches)

- Every `*.uetkx.inl` under `Source/` + `Plugins/` (committed, D-19).
- Every `<Module>.Uetkx.gen.cpp` aggregator (incl. `RuiHostTests.Uetkx.gen.cpp`).
- `Source/RuiHostTests/ContractFixtures/*.inl.expected` + `*.diags.expected`.
- The committed vocabulary/schema JSON copies (`ide-extensions/lsp-server/src/uetkx-schema.json`
  + vendored `vscode-uetkx/server/` and `visual-studio/UetkxVsix/server/` copies of the
  `RUIExportSchema` output; shape asserted in-memory by `ReactiveUIUetkxSchemaTest.cpp`) —
  expected UNCHANGED this leg (no diagnostics/decl kinds in the schema, §3 schema row); flag
  any diff as a smell.
- `plans/family-corpus.hash` (tri-repo lockstep).
- `ide-extensions/changelog.json`, both CHANGELOG.md mirrors, `.uplugin` + both extension
  manifests (versions).
- Sidecars (`*.uetkx.diags.json`) are NOT committed — machine-local, gitignored (never add).

## §7 — Codemod contract (summary — spec in M7)

Idempotent; re-runnable; scanned-record-driven (never raw-regex over params); leaves
un-rewritable `module{}` bodies in place WITH a printed report; ends with the zero-diagnostics
`CompileAllRoots(-full)` gate; ships in the SAME release as the grammar (G-10).

## §8 — Deprecation-window behavior matrix (0.12.x)

| File content | Scanner | Codegen | Formatter | Codemod |
|---|---|---|---|---|
| Old wrappers only | parses; **2320 per wrapper** | identical output (legacy emitters) | legacy shape preserved, idempotent | fully rewrites (or reports skip) |
| New plain decls only | parses; no warnings | new emitters (U-04) + alias plane | new shape, idempotent | no-op |
| Mixed old + new | legal; 2320 on wrappers only | per-decl by `Order`, both emitters coexist in one `.inl` | each decl in its own form | rewrites only the wrappers |
| Old syntax in 0.13+ (post-removal, owner-triggered later) | hard error (allocate the code THEN — not in this leg) | — | — | codemod remains available |

Old and new IMPORT surfaces are not versioned apart: the new forms are additive; existing
named imports parse unchanged.

## §9 — Full sync-surface checklist (G-12 — every box is a merge/release gate)

- [x] Scanner C++ + TS mirror (corpus-locked) — M1/M6
- [x] Codegen (values/utils/alias plane) + TD-026 keys + CodegenVersion 3 re-pin — M2/M3
- [x] Resolver + driver (full ES surface, eager value edges, export_hash) — M4
- [x] HMR identity + rename-remount doc + ImportersOf wiring (or TECH_DEBT entry) + preview
      private-component message — M3/M5
- [x] Formatter C++ + TS (form-preserving; all THREE kind-switches per side) — M6
- [x] TextMate grammar + VS Code + VS2022 vsix rebuild — M6
- [x] LSP completions / hover / go-to-def through aliases (rename handler: owner call) +
      embedded-C++ virtual doc (value/util regions, alias bindings) — M6
- [x] Codemod `-run=RUIMigrateEsModules` + gallery/templates/skills migrated — M7
- [x] ContractFixtures + new fixtures + goldens — M3/M8
- [x] Corpus cases + `family-corpus.hash` TRI-REPO lockstep — M0.4/M8 (release gate)
- [x] Docs: ImportsPage rewrite, CompanionFilesPage rewrite, migration guide, README, CLAUDE.md — M8
- [x] Changelogs: Lane A (root + plugin mirror), Lane B (changelog.json 0.6.0), Lane C (Discord ≤2000), PENDING drained — M8
- [x] Versions: plugin 0.12.0, extensions 0.6.0 via `bump.mjs` — M8
- [x] TECH_DEBT: TD-026/TD-024 resolved; new caveat entries — M8
- [x] `plans/archive/IMPORT_EXPORT_PLAN.md` G-05 supersede banner — M0.3

## §10 — Guardrails (what NOT to do) + error-signature table

**Never:**
- Push, or commit without an explicit owner ask. Never add `Co-Authored-By`.
- Touch the meanings/messages of UETKX 0xxx/21xx/2300–2317 — frozen; this leg only ADDS 2320–2327.
- Hand-edit any `.inl`/`.gen.cpp`/`.expected` — only `RUICompile -full` + deliberate re-pin.
- Land a lexer/parser/formatter change without its corpus case IN THE SAME COMMIT
  (grammar-contract skill L20-22 — no exceptions).
- Let the formatter migrate syntax — form-preserving only; migration is the codemod's.
- Weaken a gate to get green (skip a suite, loosen a golden, drop a corpus case).
- Re-derive the private qualifier anywhere but `PrivNamespaceFor` (one source of truth).
- Commit sidecars, tokens, or `Config/` churn from editor boots (CLAUDE.md env facts).
- Break LF/copyright-header rules on new files (`check-headers.mjs` gates CI).

**Error-signature table (see one of these → the likely cause):**

| Signature | Likely cause / fix |
|---|---|
| MSVC C2572 "redefinition of default argument" in an aggregator TU | defaults emitted on BOTH phases — defaults belong ONLY on the DECL-phase declaration (TD-023 rule, test-pinned) |
| LNK2005 duplicate symbol for a value export | value emitted in BOTH phases or missing `inline` — values are DECL-PHASE-ONLY `inline const` |
| `RUICompile -check` red immediately after a codegen edit | forgot the CodegenVersion bump + `-full` re-pin (fingerprint stale) |
| `corpus-hash.mjs --check` red | corpus case added/edited without `--write`, or sibling drift — re-pin + owner sync |
| A storm of 2306 after the codemod | module hoist turned a lazy component edge into an eager value edge — break the chain or keep component-only imports lazy (U-06) |
| 2305/2307 after `* as` migration | the `X::` rewrite plane missed a reference form — add the corpus case FIRST, then fix the scan |
| Private component state resets on unrelated edits post-M3 | id string built inconsistently (basename sanitization) — must be exactly `PrivNamespaceFor(Basename)::Name` |
| verify-mirror red | edited root CHANGELOG.md without recopying the plugin mirror (byte-identical, LF) |
| Automation report green-exit-code but failures inside | you parsed the exit code — ALWAYS parse `report\index.json` |

## §11 — Risks / watch-list / STOP-AND-ASK

- **STOP AND ASK the owner (do not guess):**
  1. Top-level `struct`/`class`/`using` declarations in the NEW grammar (today only legal
     inside `module{}` bodies): allowed as a 5th "type decl" kind, or must such code move to a
     hand-written header? (G-03 does not classify them; codemod skips such modules meanwhile.)
  2. LSP rename-symbol support: in this leg or a fast-follow TD entry? (G-12 names rename;
     no handler exists today.)
  3. Sibling corpus-PR timing (G-13) and who opens the Godot/Unity mirror PRs (TD-009/TD-018).
  4. Branch cut point if `feat/click-counter-demo` has not merged when work starts.
  5. Any conflict discovered between this plan and ES_MODULES_GENERAL_PLAN.md.
- **Watch-list:**
  - Rename-alias bare-identifier rewrites in verbatim C++ (U-03) are the riskiest surface —
    corpus-pin every reference form; 2325 catches declared-name collisions but NOT local
    variables inside bodies (documented limit; TECH_DEBT entry).
  - The C++ param parser (`Type Name = Default` extraction) vs gnarly types
    (`TFunction<void(int32)>`, `const T&`, `T<TMap<A,B>>`) — the ParseParams depth walker
    handles nesting, but the trailing-identifier name rule needs its own case set.
  - `#line` mapping for value initializers spanning lines — follow the module-body precedent
    (Palette golden); single-line granularity limits carry over (TD-023 record).
  - The [REPIN] window is repo-wide — sequence M1+M2+M3 before re-pinning ONCE; a second
    re-pin is a smell.
  - HMR field-verification (Live Coding, editor loop) is owner-hands work — schedule a
    `field-test-editor` pass after M5.
  - 2320 noise during the window is expected in third-party trees but must be ZERO in this
    repo post-codemod (M7 gate).
