# Adversarial bug hunt — round 2 — `feat/uetkx-imports` (2026-07-12)

> **What this is.** A second hostile, self-adversarial review of the *entire* `feat/uetkx-imports`
> branch — code, the whole build/codegen/HMR flow, the VS2022/VSCode/LSP extensions, the import/export
> resolution engine, and the end-to-end semantics. It is deliberately weighted toward the half of the
> branch the **first** hunt never reached.
>
> **Why a second pass was warranted.** The first hunt ([BUGHUNT_2026-07-12.md](BUGHUNT_2026-07-12.md))
> used baseline `9778757..HEAD` — only the **last 27 commits**. The branch is **61 commits** ahead of
> `origin/dev` (merge-base `b38a51c`). So the first **34 commits** — the entire import/export engine
> (`UetkxResolve`, cross-file cycle detection, the `RUIMigrateImports` codemod, the scanner import
> grammar, driver strict resolution, sidecar-v3 staleness), the 1433-line `UetkxCodegen` rewrite
> (fwd-decl aggregator, `#line` directives, source-truth ordering), HMR hook-state migration + import
> blast radius, the LSP import intelligence, and the TD-010/011/013 Slate work — were **never deeply
> hunted**. This pass targets them, plus a dedicated regression audit of the 19 fixes in `0b8df00`.
>
> **Method.** 16 skeptical finder agents (one per subsystem), each candidate then handed to **two
> independent refutation-oriented verifiers** — a *reachability/correctness* skeptic (re-reads the real
> code, tries to prove the path is unreachable or misread) and an *intended-semantics/coverage* skeptic
> (tries to prove it is a locked family decision, an existing-test-covered case, or a documented
> limitation). A candidate is kept only if it survives at least one lens; `CONFIRMED` = both lenses
> agreed, `PLAUSIBLE` = the lenses split. 120 agents, ~7.7M tokens.
>
> **Result.** 52 candidates → **52 survived** (42 both-lens confirmed, 10 plausible/split), 0 dually-refuted.
> Because a single non-refuting lens is a permissive bar, I then **personally re-read the source** for
> the highest-impact findings (CG-2, SLOT-1, SLOT-2, the clangd frame decoder, NumericEntryBox) — all
> confirmed against real code — and **adjudicated the severities myself** (several finder-`HIGH`s are
> downgraded to `MEDIUM` below, with the reason noted). After merging one duplicate (DRV-5 → CHECK-1):
> **51 findings — 2 HIGH, 30 MEDIUM, 19 LOW**.
>
> **Status.** None of these are fixed — this document is the report the owner asked for. Every finding
> is **new** (distinct from B1–B17/P1–P2, already fixed in `0b8df00`); several are in subsystems the
> first hunt structurally could not have seen. As in round 1, essentially every bug has a matching
> green test that only asserts the happy path.

## ✅ RESOLVED — 2026-07-12 (all 51 findings fixed, built, and tested)

Every finding below is **fixed**. Verification: engine target builds clean; the full automation suite is
**101/101** (10 new `ReactiveUI.Bugfix2.*` regression tests + all prior suites, no regressions); LSP
**29/29** (`tsc` clean, + a new UTF-8-framing test); `RUICompile -check` **0 drifted / 0 errors** (the
codegen changes are byte-identical in-repo); every node CI gate green (headers, mirror, docs-drift,
lint-skills, corpus-hash, changelog); docs site build + lint clean. Notably, the new `Bugfix2.TagCase`
test caught **two real bugs in the CG-3 fix itself** (an `FName::ToString` casing assumption and a
case-*insensitive* `TSet<FString>`), which were then corrected — the regression tests did their job.

### Fix + verification map

| # | Fix (one line) | Verified by |
|---|---|---|
| CG-1 | Context-aware D-32 banner: `.rui-seller-repo` sentinel → seller in-repo, neutral in customer projects; threaded through CompileSource + the aggregator | `ReactiveUI.Bugfix2.CopyrightBanner` |
| CG-2 | `IndentRegion` re-indents via `SkipNoncode` (no tab injected into raw-string bodies) | build + drift gate |
| CG-3 | Case-sensitive tag check vs the def's canonical (Factory-derived) name → UETKX0105 on a mis-cased tag | `ReactiveUI.Bugfix2.TagCase` |
| CG-4 | Cycle remainder ordered by value-import (module/hook) edges, respecting module-defaults | build + drift gate |
| DRV-1 | Skip path populates `ExportedNames` (preamble scan) so the 2106 ledger sees skipped incumbents | build |
| DRV-2 / IMPORT-1 | Unresolved import records the resolver's `WouldBeLabel` (reconstructable staleness key) | `ReactiveUI.Bugfix2.WouldBeLabel` |
| DRV-3 | A skip over a standing-error verdict counts as an error (exit code reflects the broken tree) | build |
| DRV-4 | An env-hold (UETKX2507) is a retry, not a sweep error | build |
| CHECK-1 / DRV-5 | Value-import-cycle DFS factored into a shared helper; `CheckDrift` enforces 2306 too | build |
| IMPORT-2 | A 2308 cross-module import marks its names imported (no contradictory 2305) | build |
| IMPORT-3 | Only a real `::` blocks a reference; a ternary/label `:` no longer drops the following hook/module ref | build |
| IMPORT-4 | Self-import is not a value cycle (no self-edge) | build |
| FMT-1 / LSP-3 | Both formatters keep a preamble line verbatim when it carries a comment | build / LSP build |
| RESOLVE-1 | `EnsureIndex` sorts files → deterministic first-exporter; TS `sweptUetkxFiles` sorts too | build / `tsc` |
| SCAN-1 | Angle brackets nest only in TYPE position → a `<<`/comparison in a default no longer merges params | `ReactiveUI.Bugfix2.ParamOperator` |
| SCAN-2 | Comments skipped inside the import brace list | `ReactiveUI.Bugfix2.ImportComment` |
| SCAN-3 | Hook `-> Ret` scan stops at a declaration keyword (UETKX2202, no swallow) | `ReactiveUI.Bugfix2.HookNoBody` |
| HMR-1 | State snapshot packed by STATE-ordinal (matches the interp's UseState-only slots) | build |
| HMR-2 | Live Coding patch-complete → `FRuiHmr::ResetSession` clears stale interp overrides | build |
| MIGRATE-1 | Codemod gate runs the cross-file 2106 ledger (matches `-check`) | build |
| SLOT-1 / SLOT-2 | Shared `RUI::Slate::SlotValue::As{Int,Float,Margin}` — string/name literal slot forms resolve everywhere | build + drift gate |
| GRID-1 | Grid/UniformGrid `UpdateChildSlotProps` re-places on a cell change | build |
| WRAP-1 | WrapBox/ScrollBox mutate the live FSlot in place (no jump-to-end) | build |
| POOL-1 | Adapters with a reconstruct mask are non-poolable | build |
| SEP-REBUILD-1 | Reconstruct re-applies last-committed props; `ConstructOnlyChanged` gated on Has-bits (Separator/Segmented/ListView) | build |
| IW-1 | NumericEntryBox clamps typed input to [Min, Max] | `ReactiveUI.Bugfix2.NumericClamp` |
| IW-2 | ComboBox unmounts row sub-roots on menu-open + destruct | build |
| IW-3 / B6-1 | ComboBox never reports an INDEX_NONE prune as a user pick; SetOptions suppresses the callback | build |
| IW-4 | UseShortcut ignores OS auto-repeat | build |
| STYLE-1 | `@theme` keys stored bare (accepts `token:` and `$token:`); doc example corrected | `ReactiveUI.Bugfix2.ThemeToken` |
| STYLE-2 | Vector2 components trimmed (`"x, y"` parses as Vector2) | `ReactiveUI.Bugfix2.Vector2Space` |
| STYLE-3 | `$token` chains resolve transitively with a cycle guard | build |
| RECON-1 | A layout route with leftover path no child consumes no longer matches | build |
| RECON-2 | `UseBlocker` blocks POP (back/forward) navigation | `ReactiveUI.Bugfix2.BlockerPop` |
| RECON-3 | `UseSearchParams` setter pushes (React-Router default) | build |
| RECON-4 | `ResolvePath` comment corrected to match the tested behavior | — |
| LSP-1 | clangd frame decoder frames by UTF-8 byte length over a Buffer | `drainMessages` test |
| LSP-2 | Embedded hover range translated to `.uetkx` coordinates | build |
| LSP-4 | `enclosingAttrName` brace-walk is string-aware | `enclosingAttrName` cases |
| LSP-5 | Definition URI compared with `URI.sameUri` (drive-case / `%3A` tolerant) | build |
| LSW LSP-1 | `getDecls` uses a body-error-tolerant signature scan | build |
| LSW LSP-2 | LSP indexes only `Source/`+`Plugins/` (the compiler's sweep universe) | build |
| LSW LSP-3 | `importCursorAt` handles multi-line import statements | `importCursorAt` case |
| CMU-1 | Input-method subscription re-points on an owning-player change; teardown unbinds the bound subsystem | build |
| BUILD-1 | Family-corpus hash sorts on the normalized key (engine-agnostic) | `corpus-hash --check` |
| BUILD-2 | `engine-tests.yml` drift gate + automation suites uncommented | (armed CI — owner) |
| HOOKDOC-1 | Stale "23 hook names" comment → 20; ImportsPage adds UETKX2106 + codemod caveat | — |

> **Note on coverage.** The findings marked "build" are locked by the clean compile + the full
> suite + the drift gate (the layer that would surface a codegen/adapter regression); the ten
> `ReactiveUI.Bugfix2.*` + LSP tests add explicit assertions of the previously-uncovered cases.
> Engine-timing-only paths (a literal use-after-free, a Live Coding patch, split-screen player
> reassignment) and the armed-CI leg cannot be exercised headless — those rely on the fix logic +
> the adversarial re-read. The original diagnoses below are preserved as the record.

---

## Severity summary (adjudicated)

| # | Sev | Subsystem | Bug | Location |
|---|---|---|---|---|
| SLOT-1 ✔ | **HIGH** | ADP | Numeric slot values (column/row/fill/zorder) read Int/Float but codegen delivers String → collapse to 0 | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiWidgetAdaptersB2.cpp:177` |
| CG-1 | **HIGH** | GEN | D-32 context-aware copyright banner is unimplemented — every generated .inl/.gen.cpp stamps the seller's "All Rights Reserved" into customer projects | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp:1145` |
| GRID-1 | med | ADP | GridPanel/UniformGridPanel never re-place a child on slot.column/slot.row change (no UpdateChildSlotProps override) | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiWidgetAdaptersB2.cpp:708` |
| POOL-1 | med | ADP | Separator is pooled despite its reconstruct mask → construct-only Thickness/Orientation leak across pool reuse | `Plugins/ReactiveUI/Source/ReactiveUISlate/Public/RuiElementAdapter.h:76` |
| SLOT-2 ✔ | med | ADP | Slot.Padding string form silently becomes 0 on WrapBox/WidgetSwitcher/GridPanel/ScrollBox (box panels honor it → inconsistency) | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiWidgetAdaptersB2.cpp:144` |
| WRAP-1 | med | ADP | WrapBox/ScrollBox UpdateChildSlotProps remove+append reorders a non-last child to the end on any slot-prop change | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiWidgetAdaptersB2.cpp:373` |
| BUILD-1 | med | BLD | family-corpus hash sorts on the UN-normalized case name, so the cross-repo family hash is engine-prefix-sensitive and TD-009 byte-identity silently breaks | `scripts/corpus-hash.mjs:61` |
| BUILD-2 | med | BLD | The only generated-code drift gate (RUICompile -check) and the entire automation suite are commented out in engine-tests.yml — committed *.uetkx.inl can drift with no CI leg catching it, even when armed | `.github/workflows/engine-tests.yml:50` |
| RECON-1 | med | COR | Router layout route silently matches with unconsumed leftover path — drops trailing segments, shadows catch-all/sibling routes | `Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiRouter.cpp:492` |
| RECON-2 | med | COR | UseBlocker does not block Go()/back-forward (POP) navigation — the back button bypasses the navigation guard | `Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiRouter.cpp:361` |
| DRV-1 ↔ | med | DRV | Incremental sweep's UETKX2106 duplicate-export gate silently misses collisions when the incumbent file is skipped | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:729` |
| DRV-2 ↔ | med | DRV | DepsChanged mis-reconstructs unresolved-import dep keys, so creating a previously-missing imported file never re-stales the importer (M8 feature broken) | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:161` |
| B6-1 | med | FIX | ComboBox B6 fix leaks engine-originated selection-prune (ESelectInfo::Direct) as a phantom user OnSelectionChanged(-1) | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiComboBox.cpp:109` |
| CG-2 ✔ | med | GEN | Region re-indentation injects tabs into multi-line string-literal content — silent corruption of raw strings in module/hook/setup bodies | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp:510` |
| CG-3 | med | GEN | Case-variant TextBlock tag (e.g. <Textblock/>) compiles to uncompilable C++ with no diagnostic | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp:685` |
| FMT-1 ↔ | med | GRM | Format Document silently deletes a trailing comment on a preamble `import` line | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxFormatter.cpp:862` |
| HMR-1 | med | HMR | TD-019 hook-state migration mis-indexes: snapshot is keyed by compiled hook slot but re-seeded by interp UseState slot, so state migrates into the wrong variable (or is lost) when any non-UseState slot-consuming hook precedes a UseState | `Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiReconciler.cpp:453` |
| HMR-2 | med | HMR | Interp component override permanently shadows the recompiled component — nothing clears overrides after a Live Coding rebuild, so 'rebuild for full behavior' never takes effect within an editor session | `Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/RuiHmr.cpp:115` |
| IMPORT-1 ↔ | med | IMP | Unresolved-import dep_hash key can never match its reader, so creating the missing file never clears the importer's error (wedges the fix loop) | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxResolve.cpp:369` |
| LSP-1 ✔ | med | LSE | clangd frame decoder slices a UTF-16 string by a UTF-8 byte length — any non-ASCII clangd message permanently desyncs the stream and silently kills embedded intel | `ide-extensions/lsp-server/src/clangdProxy.ts:59` |
| LSP-2 | med | LSE | Embedded hover result returned without translating its range from virtual-doc to .uetkx coordinates → wrong/out-of-bounds highlight range | `ide-extensions/lsp-server/src/server.ts:263` |
| LSP-3 ↔ | med | LSE | Formatter silently deletes a trailing comment on an `import` line (preamble deemed canonical, imports reconstructed structurally) | `ide-extensions/lsp-server/src/formatUetkx.ts:96` |
| LSP-5 | med | LSE | Definition translation compares clangd's echoed URI to the virtual URI by exact string — a re-serialized (drive-case / %3A) URI leaks virtual-coordinate locations to the editor | `ide-extensions/lsp-server/src/server.ts:337` |
| LSP-1 | med | LSW | getDecls uses the full scanFile (aborts on a target body error) instead of a preamble-only scan, truncating the exported-decl list and diverging from the C++ ScanPreamble resolver | `ide-extensions/lsp-server/src/uetkxWorkspace.ts:59` |
| CHECK-1 ↔ | med | MIG | RUICompile -check (CheckDrift) does not detect UETKX2306 value-import cycles that the full RUICompile sweep detects | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:606` |
| MIGRATE-1 ↔ | med | MIG | RUIMigrateImports gate misses cross-file UETKX2106 — export-everything silently creates duplicate-export collisions the codemod green-lights but -check rejects | `Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/RUIMigrateImportsCommandlet.cpp:163` |
| SCAN-1 | med | SCN | Component param default containing a < / > / << / >> operator unbalances ParseParams' depth counter, merging or dropping later params into broken codegen | `Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp:48` |
| STYLE-1 | med | STY | Docs stylesheet declares @theme tokens with a leading '$' — but the resolver strips '$' from the reference and matches the BARE name, so doc-shaped tokens never resolve | `ReactiveUIUnrealDocs~/src/pages/Features/FeaturesPage.tsx:25` |
| STYLE-2 | med | STY | ParseStyleValue vector2 branch does not trim components, so 'x, y' (space after comma) silently parses to a Name instead of a Vector2 | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiStyle.cpp:365` |
| IW-1 | med | WDG | NumericEntryBox MinValue/MaxValue never clamp typed input (AllowSpin=false wires them only to a never-created SpinBox) | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiNumericEntryBox.cpp:18` |
| IW-2 | med | WDG | ComboBox leaks per-row FRuiRoot sub-roots — RowRoots strong-holds every generated row, only reset in test-only OpenMenu() | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiComboBox.cpp:98` |
| IW-3 | med | WDG | ComboBox fires OnSelectionChanged(-1) as a 'user pick' when the selected option is removed from Options | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiComboBox.cpp:37` |
| SEP-REBUILD-1 | low | ADP | Construct-only reconstruct resets previously-set runtime/plain props the new render omits (violates removed-props-don't-reset on the rebuild path) | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiSlateHost.cpp:471` |
| CMU-1 | low | BRG | Input-method delegate is bound once to the first owning player's subsystem and never re-pointed on an owning-player change, leaving UseInputMethod tracking the wrong (or a dead) player | `Plugins/ReactiveUI/Source/ReactiveUICommonUI/Private/RuiActivatableScreen.cpp:87` |
| RECON-3 | low | COR | UseSearchParams setter hard-codes bReplace=true — diverges from React-Router push default; prior search state cannot be restored via back | `Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiRouter.cpp:699` |
| RECON-4 | low | COR | ResolvePath comments claim 'drop From's last segment' but the code (and the locking test) append to the full pathname — stale, trap-inviting docs | `Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiRouter.cpp:193` |
| DRV-3 | low | DRV | RUICompile returns exit 0 on an unbuildable tree when re-run over a standing compile error | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:744` |
| DRV-4 | low | DRV | Transient env-hold (unreadable source) is counted as a sweep Error, contradicting the retry design | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:754` |
| CG-4 | low | GEN | Cross-file module-default dependency inside a component-import cycle breaks DECL-phase ordering and is missed by the value-cycle detector | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:501` |
| HOOKDOC-1 | low | GRM | Stale "23 hook names" doc comment; the table and shipped schema list 20 | `Plugins/ReactiveUI/Source/ReactiveUIInterp/Public/UetkxFileScan.h:171` |
| IMPORT-2 | low | IMP | Cross-module import (2308) also emits a contradictory 2305 'not imported — add this import' for the same binding | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxResolve.cpp:380` |
| IMPORT-3 | low | IMP | Reference scanner conflates ternary/case/label ':' with scope '::', dropping hook/module refs that follow a colon; migrate codemod then blesses un-buildable output | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxResolve.cpp:96` |
| IMPORT-4 | low | IMP | Self-import of a same-file hook/module is misreported as a value-import cycle; self-imported component is silently accepted and pollutes dep_hashes with a self-edge | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:811` |
| LSP-4 | low | LSE | enclosingAttrName brace-walk ignores string/comment literals — a `}`/`{` inside an event-handler string misdirects Value.<field> completion | `ide-extensions/lsp-server/src/eventPayload.ts:36` |
| LSP-2 | low | LSW | LSP indexes the whole .uproject tree for findExporter/completions, but the engine compiles only Source/ + Plugins/, so the LSP offers and navigates to exports the compiler never sweeps | `ide-extensions/lsp-server/src/uetkxWorkspace.ts:174` |
| LSP-3 | low | LSW | importCursorAt only inspects the single physical line, so import name/specifier completions silently fail inside valid multi-line import statements | `ide-extensions/lsp-server/src/uetkxWorkspace.ts:265` |
| RESOLVE-1 | low | MIG | FUetkxFsResolver::EnsureIndex builds the exporter index over unsorted FindFilesRecursive, making the 'first exporter wins' tie-break non-deterministic across machines | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxResolve.cpp:272` |
| SCAN-2 | low | SCN | Comment inside an import brace list is misread as a bad specifier: spurious UETKX0300 and the entire import is silently dropped | `Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp:183` |
| SCAN-3 | low | SCN | A hook declaration missing its body ('-> Ret' with no braces) scans forward unbounded to the next declaration's '{' and swallows that declaration as its body | `Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp:422` |
| STYLE-3 | low | STY | $token that resolves to another $token is not resolved transitively and emits no warning — a raw '$name' string reaches the adapter | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiStyle.cpp:167` |
| IW-4 | low | WDG | UseShortcut fires OnTrigger on every key auto-repeat (no IsRepeat guard) | `Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiInput.cpp:42` |

_✔ = I personally re-read the source and confirmed it this session. ↔ = has a cross-referenced duplicate/sibling. Full lens reasoning is in the workflow journal._

---

## HIGH severity

### SLOT-1 — Numeric slot values (column/row/fill/zorder) read Int/Float but codegen delivers String → collapse to 0
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiWidgetAdaptersB2.cpp:177`  ·  **HIGH**  ·  CONFIRMED  ·  ADP (the big Slate adapter file + slot mutation + widget replace)

> **✔ Owner-verified.** read UetkxCodegen.cpp:735 (literal Slot.* → `FRuiValue(TEXT(...))`, String kind) + SlotIntOf @ RuiWidgetAdaptersB2.cpp:177 (`Kind==Int ? IntValue : FloatValue`) — a String yields default 0. Confirmed.

**What's wrong.** SlotIntOf (RuiWidgetAdaptersB2.cpp:171-182, read at :177 `V->Kind == Int ? IntValue : FloatValue`), SlotFillOf (RuiCoreAdapters.cpp:126-137), and Overlay ZOrderOf (RuiCoreAdapters.cpp:477-487, `Z->IntValue`) all assume the slot value is an Int or Float FRuiValue. But the toolchain emits EVERY literal `Slot.*` attribute as a String: UetkxCodegen.cpp:732-735 produces `FRuiValue(TEXT("..."))`, and the interp (UetkxInterpComponent.cpp:476-480) produces `FRuiValue(Attr.Value)` (String kind). FRuiValue stores each kind in a separate default-initialized member (RuiTypes.h), so for a String value `Kind==Int` is false and `FloatValue`/`IntValue` return the default 0. Result: `Slot.Column`/`Slot.Row` resolve to 0, `Slot.Fill` to 0 (no fill), `Slot.ZOrder` to 0. Only the expression form `Slot.Column={2}` (which emits `FRuiValue(2)`, Int) works.

**Failure scenario.** Markup `<GridPanel><Box Slot.Column="1" Slot.Row="2"/><Box Slot.Column="0" Slot.Row="0"/></GridPanel>` compiles to `FRuiValue(TEXT("1"))` etc.; SlotIntOf returns 0 for every child, so `AddSlot(0,0)` places all children in cell (0,0), overlapping. Likewise `<VerticalBox><ScrollBox Slot.Fill="1"/></VerticalBox>` → SlotFillOf returns 0 → child stays content-hugging instead of expanding.

**Why the suite misses it.** The C++ widget/slot tests construct slot props directly with numeric FRuiValues — CellBox uses `FRuiValue(Column)` (Int, ReactiveUIWidgetTests.cpp:439) and the box test uses `FRuiValue((float)Pad)` (ReactiveUISlateTests.cpp:234) — so the numeric parsers succeed. The codegen test (ReactiveUIUetkxCodegenTest.cpp:94) only asserts the emitted TEXT contains `FRuiValue(TEXT("0,4,0,0"))`; it never renders the widget and asserts the resulting FSlot column/fill, so the runtime String→0 collapse is never observed.

**Suggested fix.** Give SlotIntOf/SlotFillOf/ZOrderOf a string-aware read (parse `V->StringValue` when Kind==String, e.g. FCString::Atoi/Atof), mirroring how ParsePadding/ValueAsLowerString already handle String; or normalize literal slot values to typed FRuiValues at codegen/interp time.

### CG-1 — D-32 context-aware copyright banner is unimplemented — every generated .inl/.gen.cpp stamps the seller's "All Rights Reserved" into customer projects
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp:1145`  ·  **HIGH**  ·  CONFIRMED  ·  GEN (codegen (fwd-decl aggregator, cycle parity, ordering, #line))

> **Note.** Lens A HIGH / Lens B MEDIUM. Adjudicated HIGH: a claimed-delivered locked decision (D-32a) is entirely absent and the effect contradicts a written customer/marketplace promise on the primary workflow. (Honest caveat: the harm is an incorrect copyright *comment*, not an enforceable rights change.)

**What's wrong.** D-32(a) (MASTER_PLAN §3, lines 503-506 & 846-849; claimed delivered in Phase 3 per ROADMAP) requires the codegen emitter to be context-aware: files inside THIS repo get the seller copyright, but output generated inside a customer's project must get the neutral banner `// Generated by ReactiveUI from Foo.uetkx — this generated code belongs to your project.`. That switch does not exist. CompileSource unconditionally writes `// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.` (UetkxCodegen.cpp:1145), and BuildAggregators does the same for the `<Module>.Uetkx.gen.cpp` aggregator (UetkxDriver.cpp:422). A grep across all of Plugins/ReactiveUI/Source finds no `belongs to your project` / `Generated by ReactiveUI` string anywhere, and CompileSource has no parameter or path check to distinguish this-repo from a consumer project. The Toolchain module is UncookedOnly, so it builds in the customer's editor and their `RUICompile` runs invoke this exact path.

**Failure scenario.** A Fab customer installs the plugin, authors `Source/MyGame/UI/MainMenu.uetkx`, and runs `UnrealEditor-Cmd MyGame.uproject -run=RUICompile`. The committed `MainMenu.uetkx.inl` and `MyGame.Uetkx.gen.cpp` in THEIR repository now both carry `// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.` — directly contradicting the MIT-plus-ownership promise (D-32: generated code in user projects belongs to the user) and the docs/FAQ statement. The customer ships code stamped with the seller's exclusive copyright.

**Why the suite misses it.** Every fixture, contract case, and demo `.uetkx` file lives inside THIS repo, where the seller copyright is the CORRECT output — so the drift gate and codegen tests all pass. No test compiles a source from a simulated external project root to assert the neutral banner is emitted, because the neutral-banner code path does not exist to test.

**Suggested fix.** Add a context flag to FUetkxCodegen::CompileSource (and thread it through the driver / BuildAggregators) that is true only when the source resolves under this plugin's own repo, and emit the neutral D-32 banner otherwise. Detection can key off whether the project dir is (a descendant of) the ReactiveUI plugin repo, or an explicit `bSellerRepo` passed by the commandlet. Cover it with a codegen test that compiles from a non-repo project root and asserts the neutral banner.

---

## MEDIUM severity

### GRID-1 — GridPanel/UniformGridPanel never re-place a child on slot.column/slot.row change (no UpdateChildSlotProps override)
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiWidgetAdaptersB2.cpp:708`  ·  **MEDIUM**  ·  CONFIRMED  ·  ADP (the big Slate adapter file + slot mutation + widget replace)

**What's wrong.** FRuiGridPanelAdapter (RuiWidgetAdaptersB2.cpp:708-758) and FRuiUniformGridPanelAdapter (:760-807) place children by slot.column/slot.row at InsertChild time but do NOT override UpdateChildSlotProps, so the base no-op (RuiElementAdapter.h:95) is used. CommitUpdate detects a slot-prop change (RuiSlateHost.cpp:176-191) and calls Parent->UpdateChildSlotProps, which for these panels does nothing. A child whose slot.column/row/padding changes at runtime therefore stays in its original cell. (Even with the working expression form of SLOT-1, dynamic cell moves are silently ignored.)

**Failure scenario.** A puzzle/inventory grid binds a cell's position to state: `<GridPanel><Tile Slot.Column={col} Slot.Row={row}/></GridPanel>`. When `col` changes 0→2 on a click, CommitUpdate calls the grid's UpdateChildSlotProps (no-op) and the tile never moves; it stays in column 0.

**Why the suite misses it.** The grid automation test (ReactiveUIWidgetTests.cpp:435-467) renders grids with static columns/rows and never mutates a child's slot.column/row across a commit, so the missing update path is never driven.

**Suggested fix.** Implement UpdateChildSlotProps on both grid adapters (SGridPanel/SUniformGridPanel expose RemoveSlot + AddSlot(Column,Row)); remove the child slot and re-add it at the new column/row with the current slot props, or mutate the live FSlot's column/row.

### POOL-1 — Separator is pooled despite its reconstruct mask → construct-only Thickness/Orientation leak across pool reuse
`Plugins/ReactiveUI/Source/ReactiveUISlate/Public/RuiElementAdapter.h:76`  ·  **MEDIUM**  ·  CONFIRMED  ·  ADP (the big Slate adapter file + slot mutation + widget replace)

**What's wrong.** IRuiElementAdapter::IsPoolable() defaults to `GetChildKind()==Leaf && !HasEvents()` and does NOT exclude adapters that declare a non-zero GetReconstructMask(). SSeparator is a Leaf with no events, so it is poolable, yet its Orientation and Thickness are construct-only (baked in CreateWidget, RuiWidgetAdaptersB2.cpp:555-562; reconstruct mask at :543). The GO-05 pool reuse path in FRuiSlateHost::CreateInstance (RuiSlateHost.cpp:96-113) reuses the detached SSeparator widget WITHOUT calling CreateWidget and then runs only ApplyDiff, which for Separator sets just ColorAndOpacity (a runtime prop). The construct-only Thickness/Orientation of the pooled widget are never updated. The reconstruct-mask machinery that handles construct-only changes only runs in CommitUpdate (the update path), not on pooled create. Pool is on by default (rui.HostNodePool = true, RuiCoreMisc.cpp:24).

**Failure scenario.** A screen mounts `Separator({Thickness:1})`, later unmounts it (childless leaf → stashed in the pool), then mounts `Separator({Thickness:10})`. CreateInstance pops the Thickness=1 widget, ApplyDiff only touches color, so the new separator renders 1px thick instead of 10px. Same for a horizontal vs vertical Orientation swap across reuse.

**Why the suite misses it.** The SSeparator reconstruct test (ReactiveUIWidgetTests.cpp ~:420-430) exercises the UPDATE path (CommitUpdate→ReplaceWidget) on a live node, never the release-to-pool-then-recreate cycle with differing thickness. Tests that touch pooling use event-less widgets with no construct-only props, so the leak never surfaces.

**Suggested fix.** Make IsPoolable() also require `GetReconstructMask()==0` (a widget whose construct-only props can differ between nodes must not be pooled), or have the pool-reuse branch in CreateInstance rebuild via CreateWidget when the adapter reports a reconstruct mask and the stashed props' construct-only fields differ.

### SLOT-2 — Slot.Padding string form silently becomes 0 on WrapBox/WidgetSwitcher/GridPanel/ScrollBox (box panels honor it → inconsistency)
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiWidgetAdaptersB2.cpp:144`  ·  **MEDIUM** _(finder: HIGH)_  ·  CONFIRMED  ·  ADP (the big Slate adapter file + slot mutation + widget replace)

> **✔ Owner-verified.** read ConfigureCommonSlot @ RuiWidgetAdaptersB2.cpp:144 — uniform-only Int/Float read; String padding → FMargin(0). Confirmed.

**What's wrong.** ConfigureCommonSlot (RuiWidgetAdaptersB2.cpp:142-147) and the ScrollBox ConfigureSlot (RuiWidgetAdapters.cpp:295-300) read slot.padding as `Padding->Kind == Int ? IntValue : FloatValue` and build `FMargin(Uniform)`. The codegen/interp always emit literal `Slot.Padding` as a String (`FRuiValue(TEXT("0,10,0,0"))` appears in every demo .inl). For a String value both `Kind==Int` is false and `FloatValue` is the default 0, so padding becomes FMargin(0). Meanwhile the box panels route slot.padding through ParsePadding (RuiCoreAdapters.cpp:33-69) which correctly parses `"l,t,r,b"`/`"h,v"`/`"u"` strings AND Vector2. So the identical markup honors padding on a VerticalBox but drops it to 0 on a WrapBox/WidgetSwitcher/GridPanel/ScrollBox child. Note halign/valign in ConfigureCommonSlot DO handle String (lines 150-154), which is why only padding is affected.

**Failure scenario.** `<WrapBox><Button Slot.Padding="8">A</Button><Button Slot.Padding="8">B</Button></WrapBox>` compiles to `FRuiValue(TEXT("8"))`; ConfigureCommonSlot yields FMargin(0), so the buttons render with no gap. The same `Slot.Padding="8"` on a VerticalBox child renders FMargin(8). Users get silently-different spacing depending on the parent panel type.

**Why the suite misses it.** No demo or automation test puts Slot.Padding on a batch-2/ScrollBox child — every demo .inl uses Slot.Padding only on VerticalBox/HorizontalBox (the ParsePadding path). The programmatic slot test (ReactiveUISlateTests.cpp:234) targets a box panel and supplies a Float, not a String.

**Suggested fix.** Replace the inline uniform-only read in ConfigureCommonSlot and ScrollBox::ConfigureSlot with a shared padding parser equivalent to ParsePadding (handling String and Vector2), so all panels honor slot.padding identically.

### WRAP-1 — WrapBox/ScrollBox UpdateChildSlotProps remove+append reorders a non-last child to the end on any slot-prop change
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiWidgetAdaptersB2.cpp:373`  ·  **MEDIUM**  ·  CONFIRMED  ·  ADP (the big Slate adapter file + slot mutation + widget replace)

**What's wrong.** FRuiWrapBoxAdapter::UpdateChildSlotProps (RuiWidgetAdaptersB2.cpp:366-377) removes the child's slot then re-adds it via `W.AddSlot()`, which APPENDS (SWrapBox has no insert-at-index). The identical pattern is in FRuiScrollBoxAdapter::UpdateChildSlotProps (RuiWidgetAdapters.cpp:262-286). A pure slot-prop change (no structural reorder) routes only through UpdateChildSlotProps; the reconciler does not follow with ReorderChildren, so the mutated child jumps to the end of the panel and the visual order is corrupted until an unrelated reorder happens. (WidgetSwitcher avoids this by re-adding at the removed index, :250-256; the box panels avoid it via in-place TD-010 slot mutation.)

**Failure scenario.** A horizontal WrapBox toolbar renders [A, B, C]. Hover/selection state changes B's slot.padding. CommitUpdate calls WrapBox::UpdateChildSlotProps → RemoveSlot(B) + AddSlot() → order becomes [A, C, B]; B visibly jumps to the end. The ScrollBox comment even assumes only order-stable reuse_by_slot lists reach this path, which is untrue for a plain ScrollBox with per-child padding.

**Why the suite misses it.** ReactiveUISlotTest.cpp exercises UpdateChildSlotProps on a VerticalBox (in-place path), not WrapBox/ScrollBox; and it uses a single child, so an append could not be observed as a reorder even if triggered.

**Suggested fix.** Preserve position: record the removed child's index (iterate GetChildren to find it) and re-insert at that index, or mutate the live FSlot in place (as the box panels do via ConfigureSlotLive) instead of remove+append.

### BUILD-1 — family-corpus hash sorts on the UN-normalized case name, so the cross-repo family hash is engine-prefix-sensitive and TD-009 byte-identity silently breaks
`scripts/corpus-hash.mjs:61`  ·  **MEDIUM**  ·  CONFIRMED  ·  BLD (build / packaging / CI / generated-code flow)

**What's wrong.** computeFamilyCoreHash() builds one row per familyCore case, then sorts the rows with `rows.sort((a,b)=>(a.owner+' '+a.name).localeCompare(b.owner+' '+b.name,'en'))` (line 61) and only AFTER that folds the engine code prefix (UETKX|GUITKX|UITKX -> TKX) via normalizeCodes() on the stringified array (line 62). The stated purpose of the whole script (its header, and _tiers._doc in the corpus) is that the three sibling repos produce a BYTE-IDENTICAL family hash once the code prefix is normalized. But because the sort key still contains the engine-specific prefix, the ROW ORDER can differ per repo, and since the hash is order-dependent (line 61's comment even relies on 'names are unique within a section'), the post-sort normalization cannot recover a common ordering. Two secondary aggravators live on the same line: (a) localeCompare(...,'en') is ICU/Node-version dependent, a second nondeterminism source for a hash meant to match across three independently-CI'd repos; (b) the un-normalized sort defeats the design intent regardless of ICU.

**Failure scenario.** Add two familyCore cases (e.g. under `fileScan`) named `"x UETKX y"` and `"x H y"`. This repo sorts them [H, UETKX]; the Godot sibling names the same mirrored pair `"x GUITKX y"` / `"x H y"` and sorts them [GUITKX, H] — opposite order. I reproduced this: identical (normalized) content yields sha256 17c429c80b26...4f65 for the Unreal naming vs a2fba316b9b5...4cf3 for the Godot naming. TD-009's release-time cross-repo hash match then fails even though the corpora are byte-identical modulo the prefix, or each repo pins its own post-flip hash and the divergence is only discovered at release.

**Why the suite misses it.** The current 32 familyCore case names happen not to embed a code prefix at a sort-deciding position, so codeunit order == locale order today (verified) and this repo's committed hash 657e5f4e... matches — the `--check` gate is green. The defect only manifests when a future mirrored case name places the UETKX/GUITKX/UITKX token where it flips ordering against a neighbor, or when a sibling repo runs a Node build with different ICU collation. No unit test exercises cross-repo naming.

**Suggested fix.** Normalize before sorting: build each row from normalizeCodes-applied fields (or at minimum compute the sort key as normalizeCodes(a.owner+' '+a.name)), and replace localeCompare with a code-unit comparison (`ka < kb ? -1 : ka > kb ? 1 : 0`) so the ordering is engine-agnostic and ICU-independent. Re-pin plans/family-corpus.hash afterward.

### BUILD-2 — The only generated-code drift gate (RUICompile -check) and the entire automation suite are commented out in engine-tests.yml — committed *.uetkx.inl can drift with no CI leg catching it, even when armed
`.github/workflows/engine-tests.yml:50`  ·  **MEDIUM**  ·  CONFIRMED  ·  BLD (build / packaging / CI / generated-code flow)

> **Note.** Pre-existing standing gap (engine-tests.yml is unchanged on this branch) but it is exactly what leaves this branch's new generated code unguarded — so in scope.

**What's wrong.** engine-tests.yml is the single definition of the engine gate sequence (called by both test.yml and publish.yml). Its step 3 'Markup drift gate' (RUICompile -check, lines 50-53) and step 4 'Automation suites (headless)' (lines 57-63) are both commented-out placeholders, labeled 'activates in Phase 3 when the RUICompile commandlet exists' and 'activates in Phase 1 with the first ReactiveUI.* specs'. Those preconditions are long met on this branch: Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/RUICompileCommandlet.cpp exists and the ReactiveUI.* suites exist (branch is at Phase 6+ per TD-021/TD-022). So even an ARMED engine matrix run (RUI_CI_ENGINE_ARMED=true) executes only the two Build.sh compile steps and reports green — running zero tests and zero drift checks. Meanwhile the always-on `gates` job runs only node scripts (no engine), and the clang-format job explicitly excludes `*.uetkx.inl` and `*.Uetkx.gen.cpp` (test.yml line 57). There is therefore NO gate, armed or unarmed, that verifies the 25 committed generated files (git ls-files '*.uetkx.inl' '*.Uetkx.gen.cpp' = 25) against their .uetkx sources.

**Failure scenario.** Hand-edit a committed `Source/RuiDemo/Screens/.../*.uetkx.inl` (or change a `.uetkx` source without re-running codegen) so the committed generated file no longer matches what RUICompile would emit, then push to dev. The `gates` job passes (node-only), clang-format skips the .inl, and the armed engine leg passes (compile-only). CI is fully green and the stale/hand-edited generated code merges. In Shipping — which uses the committed .inl, not the interpreter — the drift ships to users. The branch's new import-codegen output is subject to the same gap.

**Why the suite misses it.** The local dev loop runs `RUICompile -check` and `Automation RunTests ReactiveUI` via the test-run skill and is green, so the author sees a passing verification locally. Nobody notices CI never runs either: the engine matrix is default-unarmed (skipped), and when armed it still reports green from the compile-only steps — the absence of the drift/suite steps produces a false-confidence pass rather than a failure. grep over scripts/ and .github/ confirms RUICompile -check appears only in this commented block; no node-level staleness tripwire exists.

**Suggested fix.** Uncomment steps 3 (RUICompile -check) and 4 (Automation suites, parsing report/index.json not the exit code) in engine-tests.yml now that the commandlet and suites exist; the drift gate is cheap and is the sole guard for the 25 committed generated files. If engine-in-CI must stay deferred, add an unarmed node-level tripwire that at least fails when a *.uetkx is newer than its committed *.uetkx.inl, or document explicitly that arming does not imply suite/drift coverage. (Note: engine-tests.yml is unchanged on this branch — this is a standing gap, but it directly leaves this branch's new generated code unguarded and the subsystem brief calls it out.)

### RECON-1 — Router layout route silently matches with unconsumed leftover path — drops trailing segments, shadows catch-all/sibling routes
`Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiRouter.cpp:492`  ·  **MEDIUM**  ·  CONFIRMED  ·  COR (core reconciler / presence / router (post-fix))

**What's wrong.** MatchInto ranks only the sibling routes at each level and greedily descends into the first route whose pattern matches. For a layout route (Children.Num()>0) it calls MatchPath with bEnd=false (prefix match), appends the layout's params, adds it to the Chain, recurses into the children, and then unconditionally `return true` (line 492) — regardless of whether the recursion consumed the remaining path. There is no check that the leftover segments were actually consumed, and no backtracking to lower-ranked siblings when a layout produces no matching leaf. As a result a layout whose prefix matches but whose children require MORE (or different) segments still 'matches', the unmatched trailing path is silently discarded, and better-fitting sibling routes (a ':param' leaf or a '*' catch-all/404) are never tried because the layout outranks them and returns first.

**Failure scenario.** Route table at one level: [ {Path:"settings", Children:[{Path:"profile", Element:Profile}]}, {Path:"*", Element:NotFound} ]. Navigate to "/settings/xyz". SpecificityScore("settings")=5 > SpecificityScore("*")=1, so 'settings' is tried first: MatchPath("settings","/settings/xyz",bEnd=false) matches the prefix, Rest="/xyz"; the only child 'profile' does not match "/xyz", so ChildChain is empty; MatchInto returns true with Chain=[settings], DROPPING "/xyz". UseRoutes renders the Settings layout Element with an empty Fragment outlet. The '*' catch-all is never reached. Expected (React-Router semantics the header claims): "/settings/xyz" cannot be fully consumed by the settings branch, so it should fall through to NotFound. The app renders a blank layout shell instead of the 404 page.

**Why the suite misses it.** ReactiveUI.Router.Spine only navigates to "/dash" (its layout HAS an index child that matches at bAtEnd) and "/dash/settings" (the 'settings' child matches). It never navigates to a URL that prefix-matches a layout but leaves segments no child consumes, and it never puts a catch-all or dynamic sibling alongside a layout. ReactiveUI.Router.Match exercises MatchPath in isolation, never the MatchInto tree resolution with competing siblings.

**Suggested fix.** In MatchInto's layout branch, only accept the match when the remainder is fully consumed: after computing Rest and recursing, require that MatchInto(children,...) returned true OR Segments(Rest).Num()==0; otherwise roll back the params appended (line 483) and the Chain entry (line 484) and `continue` to try the next lower-ranked sibling (backtracking). Equivalently, flatten the route tree into full root->leaf branches, rank the branches, and pick the first branch that matches the whole pathname (React Router's flattenRoutes/rankRouteBranches approach).

### RECON-2 — UseBlocker does not block Go()/back-forward (POP) navigation — the back button bypasses the navigation guard
`Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiRouter.cpp:361`  ·  **MEDIUM**  ·  CONFIRMED  ·  COR (core reconciler / presence / router (post-fix))

**What's wrong.** The <Router> provides two navigation entry points: Navigate (push/replace, lines 335-359) and Go (relative history motion, lines 361-383). Navigate consults AnyBlockerActive before committing (line 342), so a registered UseBlocker intercepts it. Go (which backs UseGo and UseBackStack's GoBack -> Go(-1)) mutates the history stacks and bumps the version with NO blocker consultation at all — AnyBlockerActive is referenced only inside Navigate. Therefore any back/forward navigation silently bypasses an active blocker. UseBlocker's documented purpose ('while bBlock, navigations are intercepted and OnBlocked fires instead of committing') is exactly the unsaved-changes guard, and the most common way a user leaves a page — the back button — is not guarded. This is an in-memory router that fully owns its history, so POP is blockable (unlike a browser popstate).

**Failure scenario.** A form screen calls UseBlocker(Ctx, isDirty, onBlocked) to prevent losing unsaved edits. The user edits the form (isDirty=true) then presses the back button, which invokes the GoBack callback from UseBackStack -> Go(-1). Go pops H.Back into H.Current and re-renders the previous route immediately; onBlocked never fires and the edits are lost. A forward Go(+1) has the same gap.

**Why the suite misses it.** ReactiveUI.Router.Spine calls GBack() (Go(-1)) only in the early back-stack section while GBlock is still false, and it exercises blocking exclusively through Nav()/UseNavigate (push). It never presses back or forward while a blocker is active, so the Go path's missing blocker check is never observed.

**Suggested fix.** Before Go mutates history, compute the destination location (the entry it would land on) and call AnyBlockerActive(BlockerList, Dest.ToHref()); if a blocker is active, fire OnBlocked and return without mutating H, mirroring Navigate. Pass BlockerList into the Go lambda's capture list (currently it captures only HistRef and Bump).

### DRV-1 — Incremental sweep's UETKX2106 duplicate-export gate silently misses collisions when the incumbent file is skipped
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:729`  ·  **MEDIUM**  ·  CONFIRMED  ·  DRV (driver, config, contract, sidecar/staleness)

> **↔ Related.** Related to MIGRATE-1 — both are UETKX2106 duplicate-export detection gaps (incremental-sweep side here; codemod-gate side in MIGRATE-1).

**What's wrong.** The cross-file duplicate-export ledger in CompileAllRoots iterates `R.ExportedNames` for every file (line 729), but the CompileFile SKIP path (lines 319-325) sets only `ComponentNames = ReadSidecarRefs(...)` and leaves `ExportedNames` EMPTY. The sidecar (SerializeSidecar, lines 45-94) never persists exported names (it stores `refs` = all component names, not the exported set), so a skipped file cannot contribute to the ledger. A file that is up-to-date (fresh .inl, matching sidecar) is skipped in BOTH sweep passes, so its exported names are invisible to the 2106 check. The fixpoint's pass 2 does not help: a file that merely shares a name (no import edge) is not re-staled by DepsChanged, so it stays skipped with empty ExportedNames.

**Failure scenario.** 1) Create A.uetkx with `export component Foo`; run RUICompile -> A compiles, .inl+sidecar fresh, fingerprint refreshed. 2) Create B.uetkx with `export component Foo`. 3) Run RUICompile again (non-force; fingerprint unchanged): A is up-to-date -> SKIPPED (ExportedNames empty); B is new -> compiled (ExportedNames=[Foo]). The ledger sees only B's Foo, finds no incumbent, reports NO UETKX2106. Worse, once both are compiled once and both go up-to-date, EVERY later non-force sweep skips both, so the duplicate stops being reported entirely and RUICompile returns exit 0. The module aggregator #includes both A.uetkx.inl and B.uetkx.inl, each defining the `Foo` wrapper -> C++ redefinition, so the build breaks with a cryptic redefinition error instead of the intended friendly UETKX2106.

**Why the suite misses it.** ReactiveUIUetkxDriverTest's duplicate-binding block (lines 194-205) uses `CompileAll(Scratch, /*bForce*/ true)`, which force-compiles BOTH files so both populate ExportedNames and 2106 fires; it never exercises the incremental path where one colliding file is already up-to-date and skipped. The -check gate (CheckDrift) recompiles fresh so it would catch it, but per engine-tests.yml lines 48-53 the -check leg and the automation suites are commented-out placeholders, so nothing runs it in CI.

**Suggested fix.** Persist the exported-name set in the sidecar (add an `exports` array in SerializeSidecar) and have the skip path in CompileFile populate `Out.ExportedNames` from it (a new ReadSidecarExports), mirroring how ComponentNames is recovered from `refs`. Then a skipped file still contributes to the 2106 ledger.

### DRV-2 — DepsChanged mis-reconstructs unresolved-import dep keys, so creating a previously-missing imported file never re-stales the importer (M8 feature broken)
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:161`  ·  **MEDIUM**  ·  CONFIRMED  ·  DRV (driver, config, contract, sidecar/staleness)

> **↔ Related.** Same root defect as IMPORT-1 (M8 reverse-edge staleness for unresolved imports).

**What's wrong.** For an UNRESOLVED import, FUetkxResolve::Apply records the dep under the RAW specifier key with hash 0 (UetkxResolve.cpp:369, `DepHashes.Add(Imp.Specifier, 0)`) explicitly "so a future file appearing there flips staleness". But DepsChanged (UetkxDriver.cpp:159-166) treats every dep_hashes key as a project-relative .uetkx path: `FPaths::Combine(FPaths::ProjectDir(), Dep.Key)` then SidecarPathFor. For a specifier key like `./B` this yields `<ProjectDir>/./B.diags.json` (anchored at the project root, not the importer's directory, and with no `.uetkx` segment), which is NOT where the real target's sidecar lives (`<importerDir>/B.uetkx.diags.json`). ReadSidecarExportHash on that bogus path always returns 0, which equals the stored 0, so DepsChanged never fires for unresolved imports. Resolved imports are keyed by LabelForKey (project-relative) and reconstruct correctly, so only the from-nonexistent case is broken.

**Failure scenario.** Author writes `import { Foo } from "./Foo"` and `<Foo/>` in Panel.uetkx BEFORE Foo.uetkx exists. RUICompile: Panel fails UETKX2300 (unknown specifier), its .inl is deleted, sidecar records hadError + dep_hashes={"./Foo":0}. Author now creates Foo.uetkx (`export component Foo`) and re-runs RUICompile without touching Panel. IsStale(Panel): DepsChanged is false (bogus path -> 0==0); .inl is missing so it returns !MatchesErrorVerdict(); MatchesErrorVerdict sees hadError, DepsChanged false, unchanged src -> true -> IsStale=false -> Panel is SKIPPED. Panel stays broken (no .inl regenerated, no error re-emitted) until the author edits Panel or runs -full. The aggregator still #includes the missing Panel.uetkx.inl -> the eventual C++ build fails on a missing include.

**Why the suite misses it.** The verdict-poisoning test (ReactiveUIUetkxDriverTest lines 213-239) uses a dep B.uetkx that ALWAYS exists on disk and only gains an export; the import therefore always RESOLVES and is recorded under a correct project-relative label, so DepsChanged works. No test covers a truly unresolved specifier that later becomes resolvable by creating the file. -check would catch the drift, but its CI leg is commented out (engine-tests.yml:48-53).

**Suggested fix.** In DepsChanged, skip keys that are import specifiers (start with `./`, `../`, `~/`) or, better, re-resolve them through the resolver instead of Combine-ing with ProjectDir; alternatively store unresolved deps under a resolvable label. Simpler robust option: in IsStale, when the .inl is missing and the verdict had an error, also treat "a new file now resolves a previously-unresolved specifier" as stale by re-running resolution rather than relying on DepsChanged's path arithmetic.

### B6-1 — ComboBox B6 fix leaks engine-originated selection-prune (ESelectInfo::Direct) as a phantom user OnSelectionChanged(-1)
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiComboBox.cpp:109`  ·  **MEDIUM**  ·  CONFIRMED  ·  FIX (prior-fix regression audit (commit 0b8df00))

**What's wrong.** B6 replaced the old `SelectInfo != ESelectInfo::Direct` guard in SRuiComboBox::HandleSelectionChanged with a `bApplyingSelection` reentrancy flag that is set ONLY momentarily around the plugin's own `Combo->SetSelectedItem` call in SetSelectedIndex (lines 57-59). The premise was that ESelectInfo::Direct is also emitted for genuine user commits (keyboard-close / gamepad-accept), so suppressing all Direct events swallowed real picks. That is true — but the engine ALSO emits Direct for a NON-user event that the old code correctly suppressed: when the options source changes so the selected item is no longer present, SListView::UpdateSelectionSet() prunes it and calls Private_SignalSelectionChanged(ESelectInfo::Direct) (SListView.h:1332-1372). This runs on the deferred STableViewBase::Tick refresh (STableViewBase.cpp:349), i.e. LONG after SetOptions()->RefreshOptions()->RequestListRefresh returned, so bApplyingSelection is already false. SComboBox::OnSelectionChanged_Internal forwards it (nullptr, Direct) to SRuiComboBox::HandleSelectionChanged, which computes Index=INDEX_NONE and, because !bApplyingSelection, executes OnSelectionChanged(FRuiValue(-1)). The old code (SelectInfo==Direct) suppressed exactly this. The file header comment (lines 4-5) still asserts the old, now-false contract.

**Failure scenario.** A controlled/data-driven ComboBox has options [X,Y,Z] with SelectedIndex=1 (Y). The app reloads and clears/replaces the option list so Y is gone (e.g. SetOptions({}) during a loading state, or options rebuilt without Y). ApplyDiff calls SetOptions (RefreshOptions -> RequestListRefresh, deferred) and SetSelectedIndex; since -1/stale index is not a valid index, SetSelectedItem is skipped and bApplyingSelection stays false. On the next Slate tick, UpdateSelectionSet prunes the stale Y selection and broadcasts ESelectInfo::Direct. SRuiComboBox::HandleSelectionChanged then fires OnSelectionChanged(-1) as if the USER deselected. The parent's controlled onChange handler runs unexpectedly with index -1 — overwriting its intended SelectedIndex, clearing dependent state, or triggering side effects (navigation/analytics) the user never initiated. Before B6 this Direct-origin prune was silently ignored.

**Why the suite misses it.** ReactiveUI.Bugfix.ComboBox only (a) verifies a programmatic controlled set does not fire, and (b) drives GetComboWidget()->SetSelectedItem(valid item) to prove a user Direct pick to a VALID option now fires. It never shrinks/clears the options while a selection is set AND pumps a Slate tick, so UpdateSelectionSet's prune-broadcast path is never executed in any test. The broadcast is deferred to Tick, which the unit tests do not advance for this widget.

**Suggested fix.** Additionally gate the forward on the selected item being valid: `if (!bApplyingSelection && Item.IsValid() && OnSelectionChanged.IsBound())`. A single-select SComboBox has no UI to deselect to empty, so an Item.IsValid()==false change is necessarily programmatic/engine-originated (prune or ClearSelection) and must not surface as a user event; genuine keyboard-close/gamepad-accept commits (the B6 motivation) always carry a valid item and still fire. Also update the stale header comment at RuiComboBox.cpp:4-5 which still describes the removed ESelectInfo::Direct filtering.

### CG-2 — Region re-indentation injects tabs into multi-line string-literal content — silent corruption of raw strings in module/hook/setup bodies
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp:510`  ·  **MEDIUM** _(finder: HIGH)_  ·  CONFIRMED  ·  GEN (codegen (fwd-decl aggregator, cycle parity, ordering, #line))

> **✔ Owner-verified.** read UetkxCodegen.cpp:489 (hook) + :510 (module) — the blanket `Body.Replace("\n","\n\t")` is real; confirmed.

> **Note.** Downgraded HIGH→MEDIUM: real string corruption, but only triggers on a multi-line raw/continued string literal inside a module/hook/setup body.

**What's wrong.** Verbatim user regions (module body :510, hook body :489, component setup :1100) are pretty-printed into the `.inl` with `Body.Replace(TEXT("\n"), TEXT("\n\t"))`. This Replace is NOT noncode-aware: PrefixHooks/SkipNoncode correctly preserve raw-string content verbatim, but the subsequent blanket newline->newline+tab replace then injects a TAB character after every physical newline INSIDE a multi-line string literal (a `R"(...)"` raw string, or a backslash-continued literal). The line count is preserved (so #line mapping stays correct), but the string's runtime VALUE changes. The WithLine comment only claims line-count preservation; it never guarantees string-content safety, so this is unacknowledged.

**Failure scenario.** A `module` companion embeds data, e.g. `export module Data { const TCHAR* Json = TEXT(R"({\n  \"hp\": 100\n})"); }` where the raw string spans three physical lines. The generated `.inl` re-indents it to `TEXT(R"({\n\t  \"hp\": 100\n\t})")`, so at runtime `Data::Json` equals `{<newline><tab>  "hp": 100<newline><tab>}` instead of `{<newline>  "hp": 100<newline>}`. Any downstream that hashes, byte-compares, or strict-parses the blob (JSON with a strict tokenizer, an indentation-sensitive format, a checksum) now sees different, wrong bytes. It compiles cleanly and fails only at runtime.

**Why the suite misses it.** No fixture, contract case, or demo `.uetkx` places a multi-line raw string (or line-continued string) inside a component setup, hook body, or module body — the only construct that triggers it. The emitted C++ is still valid, so the drift gate and the build pass; only the string's content is wrong, which nothing asserts.

**Suggested fix.** Replace the blanket `.Replace("\n", "\n\t")` with a noncode-aware re-indenter that walks the region via FUetkxLexer::SkipNoncode and only inserts the indent tab on newlines that are OUTSIDE string/char/raw-string/comment tokens (mirroring how PrefixHookCalls already routes through SkipNoncode). Alternatively, drop cosmetic re-indentation of verbatim regions entirely (emit them at column 0), since column drift is already an accepted #line limitation.

### CG-3 — Case-variant TextBlock tag (e.g. <Textblock/>) compiles to uncompilable C++ with no diagnostic
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp:685`  ·  **MEDIUM**  ·  CONFIRMED  ·  GEN (codegen (fwd-decl aggregator, cycle parity, ordering, #line))

**What's wrong.** Host tags are looked up with `HostTags().Find(FName(*Node.Tag))` (:668). FName comparison is case-INSENSITIVE, so `<Textblock>`, `<TEXTBLOCK>`, etc. all match the `TextBlock` FTagDef and set bComponent=false — bypassing the uppercase-first component check at :670. But the TextBlock factory special-case at :685 uses a case-SENSITIVE `Node.Tag == TEXT("TextBlock")`, which is false for the mis-cased variant. Execution falls through to the generic element path (:763+). TextBlock is the one tag whose PropsType is empty (`FString()` at :45), so :772 emits `\t\t P;` (a variable of empty type) and :907 emits `RUI::TextBlock(MoveTemp(P), Key)` (the real factory takes an FText, not a props struct). No Fail() is ever called (`Text` is a known attr), so bOk stays true and the broken `.inl` is committed. Other host tags have a real PropsType, so their mis-cased variants happen to emit valid code (silently case-normalizing) — only TextBlock produces uncompilable output.

**Failure scenario.** Author writes `<Textblock Text="Hi"/>` (lowercase b typo). Codegen emits, with no UETKX diagnostic, a lambda containing `		 P;` and `return RUI::TextBlock(MoveTemp(P), FRuiKey());`. The committed `.inl` is drift-clean (a fresh recompile reproduces it), so `RUICompile -check` passes, but the aggregator TU then fails to build with a cryptic C++ error ('expected a type', 'no matching function for RUI::TextBlock') that points at generated code, not at the author's tag.

**Why the suite misses it.** All fixtures/demos spell tags with exact canonical case, so the FName case-insensitivity leak and the empty-PropsType fall-through are never exercised; the green suite only compiles well-cased markup.

**Suggested fix.** Reject non-canonical tag casing explicitly: after the FName Find succeeds, verify the matched key's string equals Node.Tag case-sensitively (or key HostTags by FString instead of FName), and emit UETKX0105 `unknown tag <Textblock>` when it differs. This also fixes the broader case-insensitive shadowing (e.g. a user component named `Button`/`button` silently resolving to the host widget).

### FMT-1 — Format Document silently deletes a trailing comment on a preamble `import` line
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxFormatter.cpp:862`  ·  **MEDIUM**  ·  CONFIRMED  ·  GRM (grammar/schema byte-consistency + generated-vs-source + formatter)

> **↔ Related.** Same behavior as LSE:LSP-3 (formatter deletes an import trailing comment); this is the C++ RUIFormat implementation.

**What's wrong.** The preamble 'is-canonical' gate (line 831-844) marks the preamble re-emittable when every non-empty line merely STARTS WITH `#include` or `import ` (line 838: `!T.StartsWith(TEXT("import "))`). When canonical, the import block is fully reconstructed from parsed data only — `import { <Names> } from "<Specifier>"` (line 862-866) — which carries no comment field. A line such as `import { Foo } from "./widgets" // shared button` still starts with `import `, so the gate stays true and the line is rebuilt WITHOUT the trailing comment. The scanner's ParseImport (UetkxFileScan.cpp:237-238) stops at the closing quote and never captures the comment, and the preamble loop (Scan, line 624-636) only stores `#include` lines, so the comment is neither preserved nor re-emitted. This directly violates the invariant asserted in this same function's own comment (line 828-829: "Leading comments or stray text are preserved byte-for-byte — Format Document must never delete user content"). Note `#include` trailing comments ARE safe (the whole preprocessor line is captured verbatim to newline via SkipNoncode and re-emitted through PreambleIncludes); the loss is specific to `import` lines. The identical defect exists in the TypeScript port formatUetkx.ts (gate line 84, re-emit line 96), so it fires in both the VSCode LSP formatter and the VS2022 path.

**Failure scenario.** A developer writes a preamble line `import { Button } from "~/widgets" // the shared primary button` and saves the file with format-on-save enabled (added for VS2022 in commit df1f8b1, and standard in VSCode). The formatter returns changed text with the import rebuilt as `import { Button } from "~/widgets"` — the `// the shared primary button` comment is gone. The file still compiles and resolves, so the loss is silent; on the next save it is already gone (idempotent loss).

**Why the suite misses it.** The shared golden corpus ide-extensions/lsp-server/test-fixtures/uetkx-formatter-cases.json has exactly two import cases (lines 97-99 and 102-104), neither with any comment on the import line. Both the C++ suite (Source/RuiHostTests/Private/ReactiveUIUetkxFormatterTest.cpp:25) and the TS suite (formatter.test.ts) replay only this corpus, so no case exercises a trailing comment on an import, and the standalone-comment cases that would hit the verbatim fallback do not cover the same-line case.

**Suggested fix.** In the canonical-preamble gate, treat a directive line as NON-canonical (forcing the safe verbatim fallback) when it carries a trailing comment — e.g. after confirming the line starts with `import `/`#include`, also require that it contains no `//` or `/*` outside the specifier string; if it does, set bPreCanonical=false. Apply the same guard in formatUetkx.ts line 84 to keep the C++/TS ports byte-identical. (Alternatively, capture the trailing comment into FUetkxImportDecl and re-emit it, but the fallback is the minimal, invariant-preserving fix.)

### HMR-1 — TD-019 hook-state migration mis-indexes: snapshot is keyed by compiled hook slot but re-seeded by interp UseState slot, so state migrates into the wrong variable (or is lost) when any non-UseState slot-consuming hook precedes a UseState
`Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiReconciler.cpp:453`  ·  **MEDIUM** _(finder: HIGH)_  ·  CONFIRMED  ·  HMR (HMR: hook-state migration + import blast radius + staleness)

> **Note.** Lenses corrected HIGH→MEDIUM: dev-loop only (compiled→interp save-swap), never in a shipped build. But within the loop it silently resets or cross-writes state — the exact thing TD-019 exists to prevent.

**What's wrong.** The compiled->interp migration snapshots exportable state indexed by ABSOLUTE hook slot: `for (h=0; h<State->Hooks.Num(); ++h) MigratedState[h] = Hooks[h]->ExportRuiValue()` (RuiReconciler.cpp:451-460). The compiled component's `Hooks` array contains EVERY slot-consuming hook cell in source order — State, Ref, Memo, Reducer, Signal, Tween, StableCallback, Deferred, Transition, SafeArea, Sfx (effects live in separate arrays, but all of these share the one `Hooks` array; see RuiComponentState.h:32 and the cell kinds in RuiHooksInternal.h:117-232). The consumer side, the interpreter, only ever materializes UseState cells: `FUetkxInterpDef::Invoke` iterates `HookPlan` which `ParseBindings` fills ONLY for `auto [A,B] = UseState(...)` statements (UetkxInterpComponent.cpp:143-166, 790-796); every other hook is a structural no-op (UetkxInterpComponent.cpp:172-190) and produces NO cell. So the interp's `Hooks` array is a dense sequence of State cells at slots 0..n-1, and `Ctx.UseState` re-seeds slot i from `MigratedState[i]` (RuiContext.h:56-62). The two indexing schemes only coincide when every UseState precedes every non-UseState slot hook. As soon as a UseRef/UseMemo/UseReducer/UseSignal/etc. sits before (or between) UseState hooks, `MigratedState[i]` for the interp's i-th State hook points at the WRONG compiled slot — either a non-exportable Null (state silently reset) or the exported value of a DIFFERENT state variable (state migrated into the wrong variable = data corruption). The comment at RuiReconciler.cpp:448-450 reveals the blind spot: it assumes a ref 'leaves a Null and re-inits' in place, not realizing the interp side has no slot there at all, which shifts every following state index.

**Failure scenario.** Compiled component (natural React ordering, a ref declared before state):
  auto Ref = Ctx.UseRef<int>(0);            // compiled Hooks[0] (Ref -> ExportRuiValue=false -> Null)
  auto [A, SetA] = Ctx.UseState(1);         // compiled Hooks[1]
  auto [B, SetB] = Ctx.UseState(2);         // compiled Hooks[2]
User drives A=10, B=20, then saves the .uetkx (first compiled->interp swap; HookSignature matches so bMigrate=true). Snapshot: MigratedState = [Null, 10, 20]. Interp HookPlan = [A@0, B@1]. Re-render: A@0 reads MigratedState[0]=Null -> A re-inits to 1 (lost); B@1 reads MigratedState[1]=10 -> B is seeded with A's old value 10 instead of its own 20 (wrong-variable corruption). The user sees A=1, B=10 after a save whose whole purpose (TD-019) was to preserve A=10, B=20. A single UseRef/UseMemo before one UseState is enough to silently reset that state to its init on the first save.

**Why the suite misses it.** ReactiveUIHmrTest's MigrateDemo (ReactiveUIHmrTest.cpp:69-80, 212-260) is a single UseState with no other hooks, so compiled-slot and interp-slot indices are trivially both 0. The only in-repo components that mix hooks (CustomDraw.uetkx, StressTest.uetkx) happen to place ALL their UseState calls first and their UseRef/UseMemo last, so the indices coincide by luck. No test exercises a migrating swap on a component with a non-UseState slot-consuming hook positioned before a UseState, which is the exact trigger.

**Suggested fix.** Align producer and consumer by STATE-ordinal, not absolute slot. On the snapshot side, only export cells whose GetKind()==ERuiHookKind::State and pack them by their running State-ordinal (skip non-State cells entirely); on the consume side, UseState already visits State hooks in order, so seed the k-th interp UseState from the k-th exported State value. (A non-exportable compiled State cell of a custom struct still consumes a State-ordinal and is exported as Null at that ordinal, keeping subsequent states aligned.) Equivalently, have ExportRuiValue/snapshot walk only State-kind slots and index MigratedState by that filtered position, and make RuiContext.h:58 read by the same filtered position.

### HMR-2 — Interp component override permanently shadows the recompiled component — nothing clears overrides after a Live Coding rebuild, so 'rebuild for full behavior' never takes effect within an editor session
`Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/RuiHmr.cpp:115`  ·  **MEDIUM**  ·  CONFIRMED  ·  HMR (HMR: hook-state migration + import blast radius + staleness)

**What's wrong.** Every component swap registers a process-global override via RUI::SetComponentOverride (RuiHmr.cpp:115), and RenderComponent unconditionally prefers a valid override over the compiled Fiber->Invoke (RuiReconciler.cpp:438-439, 484). The only code that clears an override is FRuiHmr::ResetSession / RUI::ClearComponentOverride (RuiHmr.cpp:161-174), and grep shows those are called ONLY from the automation tests — never from the watcher, the interp/editor modules, or any Live Coding completion callback. When a component swap emits fallback notes (effects are structural no-ops under interp, params render their DEFAULTS, child props are not passed — UetkxInterpComponent.cpp:188, 527, 501-505), the watcher fires Live Coding to 'catch up' (UetkxWatcher.cpp:219-223, and the hook/module rebuild note at RuiHmr.cpp:125-128 explicitly promises 'Live Coding (Ctrl+Alt+F11) or rebuild, then affected screens refresh'). But Live Coding only patches the compiled binary; it does not restart the process and does not clear the override map, so the partial-behavior interp Invoke keeps shadowing the freshly-recompiled full-behavior component for the rest of the editor session. The overrides (and the FHmrSession::InterpSigs map) are also never cleared on PIE stop, so the sticky interp version persists across PIE restarts too. Only a full editor restart (fresh statics) actually restores compiled behavior.

**Failure scenario.** A component Foo.uetkx uses UseEffect (e.g. a subscription or a frame loop like StressTest). Developer edits Foo mid-PIE and saves. The interp swaps Foo live; the swap notes 'effects/memo hooks are not interpreted — rebuild for full behavior' and, because notes are non-empty, the watcher triggers Live Coding (with rui.Hmr.AutoLiveCoding on). Live Coding rebuilds Foo with the effect intact, but FindComponentOverride(Foo) still returns the interp Invoke, so Foo continues rendering WITHOUT its effect running. The developer followed the tool's own instruction (Ctrl+Alt+F11) yet the effect stays silently dead until they fully close and relaunch the editor. Same for a component whose params should now render real prop values instead of defaults.

**Why the suite misses it.** The automation tests always call FRuiHmr::ResetSession() themselves (ReactiveUIHmrTest.cpp / ReactiveUIAcceptanceTest.cpp:102-112) to return 'back to the compiled definition', which masks the fact that production has no equivalent call. No headless test can drive a real Live Coding patch or a PIE stop/start, so the 'override outlives the rebuild' path is never exercised; the acceptance HMR case (#5) also swaps only a stateless/effect-free HelloWorld where interp and compiled behavior are identical.

**Suggested fix.** Register an ILiveCodingModule patch-complete callback (and a PIE-end / map-change hook) that calls FRuiHmr::ResetSession(), or clears the specific ComponentIds that were recompiled, so a rebuild actually drops the interp override and restores compiled behavior. At minimum, clear the override for a given ComponentId once its regenerated compiled TU has been Live-Coding-patched, and clear FHmrSession::InterpSigs + overrides on PIE end so a new session starts from the compiled definitions the note promises.

### IMPORT-1 — Unresolved-import dep_hash key can never match its reader, so creating the missing file never clears the importer's error (wedges the fix loop)
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxResolve.cpp:369`  ·  **MEDIUM**  ·  CONFIRMED  ·  IMP (import/export resolution engine)

> **↔ Related.** Same root defect as DRV-2 (unresolved-import dep_hash key can never re-match), seen from the resolver side.

**What's wrong.** When an import does not resolve (UETKX2300), Apply records the dependency in dep_hashes under the RAW SPECIFIER string: `DepHashes.Add(Imp.Specifier, 0)` (UetkxResolve.cpp:369). The comment states the intent: 'Record the unresolved specifier so a future file appearing there flips staleness (M8).' But the reader, `DepsChanged` (UetkxDriver.cpp:161), reconstructs the dep path as `FPaths::Combine(FPaths::ProjectDir(), Dep.Key)` then `SidecarPathFor(DepUetkx)`. For a resolved dep the key is a project-relative path ending in `.uetkx`, so this yields the correct `<abs>.uetkx.diags.json`. For an unresolved dep the key is a specifier like `./Foo` — it is (a) relative to the IMPORTER's directory, not the project root, and (b) missing the `.uetkx` extension. So the reader looks for `ProjectDir/Foo.diags.json`, which never exists and always reads export_hash 0, matching the stored 0. DepsChanged is therefore permanently false for unresolved deps, and the 'future file flips staleness' mechanism is completely dead.

**Failure scenario.** A.uetkx contains `import { Foo } from "./Foo"` and `<Foo/>`, but Foo.uetkx does not exist yet. A compiles with UETKX2300, its .inl is deleted, and its sidecar records the error verdict + dep_hashes {"./Foo": 0}. The developer now creates Foo.uetkx next to A. On the next sweep, IsStale(A): DepsChanged reads {"./Foo":0}, builds ProjectDir/Foo.diags.json (wrong: real sidecar is <A_dir>/Foo.uetkx.diags.json), gets 0==0 -> false; A.inl is missing so IsStale returns !MatchesErrorVerdict(); MatchesErrorVerdict sees the standing error verdict, DepsChanged false, A's source byte-identical -> returns true -> IsStale = false. A is SKIPPED. The two-pass fixpoint does not help (A's IsStale is still false in pass 2). `RUICompile` reports '1 compiled, N up-to-date, 0 errors' and exits 0, but A still has no .inl. `RUICompile -check` (CheckDrift, which recompiles in memory) fails with 'A: generated output missing — run -run=RUICompile', so -check and the plain sweep now contradict each other, and running the very command -check tells you to run does not fix it. The developer is wedged until they touch A's source or run -full/bump the fingerprint.

**Why the suite misses it.** ReactiveUIUetkxDriverTest's verdict-poisoning case (lines 213-239) uses a dep file that ALREADY EXISTS (B present, merely missing the exported symbol X). That path resolves, so the dep_hash is a correct project-relative LABEL and DepsChanged works. No test ever starts from a fully UNRESOLVED specifier and then creates the missing file, which is the only path that exercises the raw-specifier key. ReactiveUIUetkxResolveTest only checks that 2300 is emitted, never the subsequent staleness flip.

**Suggested fix.** Make the unresolved-dep key line up with what DepsChanged expects. Either (a) record no dep_hash for a truly unresolved specifier and instead have IsStale/DepsChanged treat 'a sidecar with a 2300 verdict' as always-stale until a resolve succeeds; or (b) record the WOULD-BE resolved project-relative path (compute the target path the same way Resolve does, sans the FileExists gate, and store it with `.uetkx`) so that when the file is created its real sidecar path matches. Additionally, in MatchesErrorVerdict, a standing 2300 verdict should be invalidated whenever any previously-unresolved specifier now resolves.

### LSP-1 — clangd frame decoder slices a UTF-16 string by a UTF-8 byte length — any non-ASCII clangd message permanently desyncs the stream and silently kills embedded intel
`ide-extensions/lsp-server/src/clangdProxy.ts:59`  ·  **MEDIUM** _(finder: HIGH)_  ·  CONFIRMED  ·  LSE (LSP embedded-C++ intel + server + formatter)

> **✔ Owner-verified.** read drainMessages @ clangdProxy.ts:44-67 — slices a UTF-16 JS string by the UTF-8 byte `Content-Length`; any non-ASCII frame desyncs the stream. Confirmed.

**What's wrong.** The send path frames bodies with a BYTE length (`Buffer.byteLength(body, "utf8")`, line 183), but the receive path (`drainMessages`) operates on a decoded JS string — `proc.stdout.setEncoding("utf8")` (line 109) makes `onData` chunks UTF-16 strings, and the buffer is a string (`private buffer = ""`). `drainMessages` then treats the header's `Content-Length` (bytes) as a count of UTF-16 code units: `rest.length < bodyStart + length` (line 56) and `rest.slice(bodyStart, bodyStart + length)` (line 59). For any body containing a character outside U+0000–U+007F, the UTF-8 byte count exceeds the UTF-16 unit count (e.g. an em dash '—' is 3 bytes but 1 unit; an astral char is 4 bytes / 2 units), so the slice over-reads into the following frame and `rest = rest.slice(bodyStart + length)` (line 60) leaves the buffer misaligned. Once misaligned, every subsequent frame boundary is wrong, so all further responses fail to parse and their pending requests time out (5s) and resolve null — the proxy is effectively dead for the rest of the session with no error surfaced.

**Failure scenario.** A user hovers a symbol whose clangd doc-comment/markdown contains a non-ASCII character (em dash, curly quote, © in an Unreal header comment), OR clangd proactively pushes a `textDocument/publishDiagnostics` for the virtual doc whose message quotes a non-ASCII identifier/string literal from the embedded C++ (e.g. `unknown type 'Café'`). That single frame's Content-Length (bytes) > the string's unit length, so `body` is over-sliced (JSON.parse throws → frame dropped) and the buffer desyncs. From then on every hover/definition request times out and returns null; embedded C++ intelligence silently stops working for the whole session.

**Why the suite misses it.** `drainMessages` is not exported and has no direct test. `embeddedServer.test.ts` uses `StubResponder`, which bypasses framing entirely, and `embeddedCpp.test.ts`'s only real-proxy test spawns a non-existent binary so `start()` resolves false and the decode path never runs. No test ever feeds `drainMessages` a multi-byte-UTF-8 LSP frame, and any purely-ASCII exchange (the common case) works because byte count == unit count there.

**Suggested fix.** Frame over Buffers, not decoded strings: remove `setEncoding("utf8")`, accumulate a `Buffer` in `onData`, find `\r\n\r\n` and slice by byte offsets with `Buffer`, and only `toString("utf8")` the exact `body` byte range before `JSON.parse`. (Equivalently, keep the string buffer but compute the split using `Buffer.byteLength` walking — but the Buffer approach is the robust fix.)

### LSP-2 — Embedded hover result returned without translating its range from virtual-doc to .uetkx coordinates → wrong/out-of-bounds highlight range
`ide-extensions/lsp-server/src/server.ts:263`  ·  **MEDIUM**  ·  CONFIRMED  ·  LSE (LSP embedded-C++ intel + server + formatter)

**What's wrong.** In `onHover`, the clangd result is returned verbatim: `if (hover) return hover as Hover;` (line 263). `embeddedPositionRequest` returns clangd's raw result whose ranges are in VIRTUAL-document coordinates (embeddedIntel.ts documents this explicitly: "ranges are in VIRTUAL coordinates — translate with the view"). The definition path does translate (via `translateEmbeddedDefinition`, lines 329-351), but the hover path does not. clangd's `textDocument/hover` response includes an optional `range` marking the hovered symbol; that range's line/character are offset by the 5-line PRELUDE plus the synthesized wrapper-function line, so applied to the .uetkx buffer it points at the wrong token — and for a short .uetkx file the virtual line number exceeds the document, yielding an out-of-range range.

**Failure scenario.** With clangd available, the user hovers `Count` inside a component `setup` block at .uetkx line 2. clangd returns `{contents: ..., range: {start:{line:7,...}, end:{line:7,...}}}` in virtual coordinates. The editor receives that range unchanged and highlights .uetkx line 7 (or clamps it), not line 2 — the hover content is right but the underlined region is wrong or off the end of the file.

**Why the suite misses it.** The `connection.onHover` handler is never unit-tested (no live-clangd harness). `embeddedServer.test.ts` only asserts that `embeddedPositionRequest` returns the raw stub result; it never checks that the server translates the hover's `range` back to source coordinates, and the stub result carries no `range` at all.

**Suggested fix.** Mirror the definition path: after receiving the hover, if it carries a `range`, map `range.start`/`range.end` through `buildEmbeddedView(text).sourceOffsetOf(...)` → `doc.positionAt(...)` and rewrite the range (dropping the range if it maps into the prelude), before returning.

### LSP-3 — Formatter silently deletes a trailing comment on an `import` line (preamble deemed canonical, imports reconstructed structurally)
`ide-extensions/lsp-server/src/formatUetkx.ts:96`  ·  **MEDIUM**  ·  CONFIRMED  ·  LSE (LSP embedded-C++ intel + server + formatter)

> **↔ Related.** Same behavior as FMT-1 (the C++ formatter) — both silently delete a trailing comment on an `import` line; this is the TS/LSP implementation.

**What's wrong.** The preamble is classified `preCanonical` when every non-empty line starts with `#include` or `import ` (lines 82-88). A line like `import { Foo } from "./foo" // note` starts with `import `, so `preCanonical` stays true, and the preamble is re-emitted purely from structured data: `block += \`import { ${imp.names.join(", ")} } from "${imp.specifier}"\n\`` (line 96). `scan.imports` never captures anything after the closing quote (parseImport stops at the quote; the comment is skipped by the scanner's noncode pass and stored nowhere), so the trailing `// note` is dropped from the output. This is asymmetric with `#include` lines, whose trailing comment IS preserved because the scanner stores the whole line in `preambleIncludes`. A standalone comment line (on its own line) is also preserved because it flips `preCanonical` to false → verbatim; only same-line-as-import comments vanish.

**Failure scenario.** User has `import { StatusChip } from "./StatusChip" // TODO: split out` then runs Format Document. `formatUetkx` returns `changed:true, fellBack:false`, so `onDocumentFormatting` emits a whole-document edit whose preamble is `import { StatusChip } from "./StatusChip"` — the `// TODO: split out` comment is silently deleted from the user's buffer.

**Why the suite misses it.** The shared corpus's comment-preservation case (`preamble with a comment is preserved byte-for-byte`) uses a STANDALONE `// header art` line (the verbatim path), and the two import cases (`M11: ...`) carry no trailing comments. No golden case places a comment on the same line as an import, so both the byte-identity assertion and the idempotence re-run pass (the comment-stripped output is itself stable).

**Suggested fix.** Either treat a preamble line bearing a trailing comment as non-canonical (fall back to verbatim for that preamble), or capture and re-emit any trailing trivia on an import decl the way `#include` lines already retain theirs. Verify parity against the C++ FUetkxFormatter and add a corpus case.

### LSP-5 — Definition translation compares clangd's echoed URI to the virtual URI by exact string — a re-serialized (drive-case / %3A) URI leaks virtual-coordinate locations to the editor
`ide-extensions/lsp-server/src/server.ts:337`  ·  **MEDIUM**  ·  CONFIRMED  ·  LSE (LSP embedded-C++ intel + server + formatter)

**What's wrong.** `translateEmbeddedDefinition` decides whether a clangd Location points back into the synthesized doc by `loc.uri !== virtualUri` (line 337), where `virtualUri = virtualUriOf(doc.uri)` is built by appending to VS Code's original (percent-encoded, e.g. `file:///c%3A/...`) document URI. clangd parses that URI to a file path and RE-SERIALIZES its own URIs (on Windows it typically emits `file:///c:/...` with a lowercased drive and an unencoded colon). If clangd's echoed URI for an in-TU definition differs by any such normalization from our exact string, the equality fails, the location is treated as a real engine-header file and passed through unchanged (lines 337-339) — a Location whose URI is the non-existent `*.uetkx.__rui_embedded__.cpp` and whose range is in un-translated virtual coordinates is returned to the editor.

**Failure scenario.** On Windows with clangd available, go-to-definition on a local symbol defined earlier in the same embedded body: clangd returns a Location into the virtual TU but serialized as `file:///c:/proj/Foo.uetkx.__rui_embedded__.cpp` while our `virtualUri` is `file:///c%3A/proj/Foo.uetkx.__rui_embedded__.cpp`. The strings differ, the location is passed through, and the editor tries to open a non-existent virtual .cpp at a wrong (prelude-shifted) line.

**Why the suite misses it.** There is no live-clangd integration test; `embeddedServer.test.ts` uses a stub that never returns a Location, and any stub would echo the exact `virtualUri` we pass, so the string comparison trivially matches. The mismatch only appears against clangd's real URI serializer.

**Suggested fix.** Compare URIs after normalizing both to fs paths (e.g. via `URI.toFsPath` with case-insensitive compare on Windows) instead of raw string equality, so a re-serialized virtual URI is still recognized and translated.

### LSP-1 — getDecls uses the full scanFile (aborts on a target body error) instead of a preamble-only scan, truncating the exported-decl list and diverging from the C++ ScanPreamble resolver
`ide-extensions/lsp-server/src/uetkxWorkspace.ts:59`  ·  **MEDIUM** _(finder: HIGH)_  ·  CONFIRMED  ·  LSW (LSP import intelligence (workspace/scan/schema))

**What's wrong.** getDecls() reads a target file and runs the FULL scanFile() (uetkxFileScan.ts:528) to enumerate its declarations. scanFile hard-returns on the first structural/markup error: parseComponent() returns -1 for any markup-level fault (no return-markup 2101, >1 root element 0108 at line 425-427, a parseMarkup errorCode, etc.), and scanFile then executes `if (next < 0) return out;` (uetkxFileScan.ts:601), dropping that decl AND every later decl. The C++ engine resolver does NOT do this: FUetkxFsResolver::GetDecls -> CachedScan -> FUetkxFileScan::ScanPreamble (UetkxFileScan.cpp:765) walks decl HEADERS only, skipping each body with brace/paren matching (FindMatchingMarkup) and never invoking the markup parser or the root-count check. ScanPreamble therefore still lists an exported decl that sits after a component whose body is malformed-but-brace-balanced. So for the same on-disk target, the C++ compiler sees the full exported set while the LSP sees a truncated set. There is no preamble-only scan in the TS port (grep confirms none), so getDecls has no robust option.

**Failure scenario.** Target Widgets.uetkx (saved to disk) contains, in order: `export component Broken { return ( <TextBlock Text="a" /> <TextBlock Text="b" /> ) }` (two root elements -> UETKX0108, but braces are balanced) followed by `export component Card { return ( <Border /> ) }`. Importer App.uetkx has `import { Card } from "./Widgets"`. Engine Apply(): GetDecls(Widgets) via ScanPreamble returns {Broken, Card}; Card is found+exported -> import is clean. LSP resolveDiagnostics(): getDecls(Widgets) runs scanFile, which aborts at Broken's 0108 before Card is parsed, returning [] -> byName.get('Card') is undefined -> it emits `UETKX2302: \`Card\` is not declared in Widgets` (uetkxWorkspace.ts:228) on a perfectly valid import, blaming the wrong file. Additionally go-to-definition on Card (server.ts:379/406) finds nothing and Card is missing from import-name/specifier completions (server.ts:143/167/171) -- silently broken navigation and completion for a valid export.

**Why the suite misses it.** server.test.ts's makeImportWorkspace() (line 128) always writes a fully well-formed target B.uetkx, and every getDecls/findExporter/resolveDiagnostics test resolves against that clean file. No test places a body-level markup error (balanced braces) ahead of a valid export in a TARGET file, so the scanFile-aborts-vs-ScanPreamble-continues divergence is never exercised. The C++/TS corpus-parity tests scan single files in isolation, not cross-file resolution against a partially-broken target; the ImportError.uetkx contract fixture tests a broken IMPORTER, not a broken target with a valid later export.

**Suggested fix.** Give the TS side a preamble-only scanner that mirrors FUetkxFileScan::ScanPreamble: enumerate `[export] component|hook|module Name` headers, skip each body with findMatching/findMatchingMarkup, and continue past bodies that fail the markup/root-count checks (bailing only on structural brace-matching failure). Have getDecls() call that instead of scanFile(), so its exported-decl set matches the engine resolver even when a co-resident component body is malformed.

### CHECK-1 — RUICompile -check (CheckDrift) does not detect UETKX2306 value-import cycles that the full RUICompile sweep detects
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:606`  ·  **MEDIUM**  ·  CONFIRMED  ·  MIG (RUIMigrateImports codemod + RUICompile -check + editor/watcher/preview)

> **↔ Related.** DRV-5 is the same bug from the driver side.

**What's wrong.** FUetkxDriver::CompileAllRoots (the default `RUICompile` sweep) runs a value-import-cycle DFS (lines 773-871) that flags a hook/module import cycle as UETKX2306 ('hooks/modules load eagerly — genuine initialization deadlock, TDZ parity') and increments Errors. FUetkxDriver::CheckDrift (lines 542-621), which backs `RUICompile -check` — the documented CI drift gate and, per CLAUDE.md, the enforcement that '`RUICompile -check` enforces resolution' — enforces per-file compile + the 2106 duplicate-export ledger + .inl/aggregator drift, but it NEVER runs the 2306 cycle DFS. Because each file in a cycle compiles fine individually and its committed .inl matches a fresh compile (the cycle is a cross-file graph property, invisible to per-file compilation and to drift comparison), `-check` returns 0 errors / 0 drift and PASSES a tree that `RUICompile` reports as errored. The two entry points enforce different invariant sets: CheckDrift added the 2106 cross-file ledger but overlooked the 2306 cross-file cycle, so the drift gate is strictly weaker than a plain compile.

**Failure scenario.** MA.uetkx: `import { StyleB } from "./MB"\nexport module StyleA { inline const int32 X = StyleB::Y; }` and MB.uetkx: `import { StyleA } from "./MA"\nexport module StyleB { inline const int32 Y = StyleA::X; }` (exactly the driver test's cycle). Commit both plus their generated .inl. `UnrealEditor-Cmd … -run=RUICompile -check` reports '0 error(s)' and exits 0 (green gate), whereas `-run=RUICompile` reports 'UETKX2306: value-import cycle: MA -> MB -> MA' and exits 1. A maintainer validating with `-check` (as CLAUDE.md instructs) sees a false green; the cycle is only caught later by the C++ build failing on use-before-declaration in the aggregator TU.

**Why the suite misses it.** The only 2306 test (ReactiveUIUetkxDriverTest.cpp:254-266) asserts on `FUetkxDriver::CompileAll(...).Errors >= 1` — the sweep path — and never calls CheckDrift, so the `-check`-specific path is completely unexercised for cycles.

**Suggested fix.** Factor the value-import-cycle DFS out of CompileAllRoots (lines 778-872) into a shared helper and call it from CheckDrift as well, folding any detected cycle into Out.Errors — so `-check` and `RUICompile` enforce the identical invariant set (as they already do for 2106).

### MIGRATE-1 — RUIMigrateImports gate misses cross-file UETKX2106 — export-everything silently creates duplicate-export collisions the codemod green-lights but -check rejects
`Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/RUIMigrateImportsCommandlet.cpp:163`  ·  **MEDIUM**  ·  CONFIRMED  ·  MIG (RUIMigrateImports codemod + RUICompile -check + editor/watcher/preview)

> **↔ Related.** Related to DRV-1 (UETKX2106 gaps).

**What's wrong.** Pass 1 (lines 61-92) inserts `export` before EVERY previously-un-exported declaration (export-everything, A3). In a pre-migration tree essentially nothing is exported, so pass 1 exports literally every component/hook/module. The final gate (lines 154-176) then compiles each file individually via FUetkxCodegen::CompileSource and counts only per-file severity-0 diagnostics. But the UETKX2106 'one exported name, one file' invariant is a CROSS-FILE ledger that lives only in FUetkxDriver::CompileAllRoots (UetkxDriver.cpp:724-743) and CheckDrift (UetkxDriver.cpp:556-581) — CompileSource never emits 2106. So two files that each declare a same-named decl (Row/Card/Item/Header/Panel — ubiquitous per-screen local names in real UI trees) both become `export`ed after pass 1, producing a 2106 collision that the codemod's own gate cannot see. The codemod's header advertises this as 'the strict-day-one merge gate … requiring ZERO diagnostics', so it prints '0 error(s) after migration' and returns 0, while the real merge gate `RUICompile -check` then fails the identical tree with UETKX2106. Compounding it: for other files that reference `<Row/>`, pass 2's FUetkxResolve::MissingImports calls Resolver.FindExporter('Row'), which returns an ARBITRARY one of the two colliding exporters (see RESOLVE-1), so the codemod auto-writes `import { Row } from "./ScreenA"` (or ScreenB) into committed source non-deterministically.

**Failure scenario.** A user tree has Source/Screens/Home/Home.uetkx declaring `component Row(...)` and Source/Screens/Shop/Shop.uetkx declaring a different `component Row(...)`; a third file uses `<Row/>`. Run `-run=RUIMigrateImports`: pass 1 writes `export component Row` into BOTH files; the gate compiles each file alone (no per-file error) and logs 'N file(s), 0 error(s) after migration', exit 0. The author commits. CI's `RUICompile -check` then fails: 'UETKX2106: exported binding `Row` is already bound by …'. The third file's auto-inserted `import { Row } from "./…"` points at whichever Row FindFilesRecursive happened to enumerate first.

**Why the suite misses it.** There is no test for RUIMigrateImports at all — grep shows the commandlet is referenced only by docs/plans, never by a spec. The driver's 2106 test (ReactiveUIUetkxDriverTest.cpp:193-204) drives FUetkxDriver::CheckDrift/CompileAllRoots directly, never the codemod, and the 'two PRIVATE same-name decls' case (line 248) deliberately keeps them private, so no test ever runs export-everything over a duplicate-named tree.

**Suggested fix.** Have the gate run the SAME cross-file invariants the driver's merge path runs: either fold `FUetkxDriver::CheckDrift(Roots).Errors` (which already builds the 2106 ledger) into the gate's Errors, or build a NameToFile ledger over each file's Compiled.ExportedNames and report duplicates. Better still, detect name collisions in pass 1 BEFORE export-everything and refuse to export a name that another file already declares (leaving it private + a diagnostic), so the migration never manufactures a 2106 the tool then hides.

### SCAN-1 — Component param default containing a < / > / << / >> operator unbalances ParseParams' depth counter, merging or dropping later params into broken codegen
`Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp:48`  ·  **MEDIUM**  ·  CONFIRMED  ·  SCN (scanner grammar + data model + lexer)

**What's wrong.** ParseParams splits the `Name: Type = Default, ...` list on top-level commas using a single Depth counter that treats every '<'/'>' as a bracket (lines 44-51), intended for template types like TArray<int>. But '<'/'>' are also C++ operators (comparison, bit-shift). A default value that contains a bare '<', '>', '<<' or '>>' drives Depth to a nonzero value that never returns to 0, so the guard `C == ',' && Depth == 0` at line 52 stops firing and the following top-level comma is NOT treated as a separator. The remaining params get swallowed into the previous param's default. The same '<'/'>'-as-bracket logic is repeated in the first-top-level-'=' scan (lines 75-88), so the split is doubly confused. The TypeScript mirror (uetkxFileScan.ts parseParams, lines 220-221) has the identical logic, so the shared scanner-cases.json contract cannot catch the divergence.

**Failure scenario.** `component Badge(flags: int = 1 << 3, size: int = 16)`. ParamText = `flags: int = 1 << 3, size: int = 16`. The two '<' of `<<` push Depth to 2 and it never comes back, so the comma before `size` is skipped. ParseParams yields a SINGLE param `flags` with Type `int` and Default `1 << 3, size: int = 16`; the `size` param is lost entirely. UetkxCodegen (UetkxCodegen.cpp:1054-1056) then emits into the props struct: `int flags = 1 << 3, size: int = 16;` — a syntax error in the generated .inl (`size: int` is not a valid declarator), and `size` never becomes a props field (no `Props.size`, no Equals() line). A ternary default such as `size: int = width > 100 ? 2 : 1, pad: int = 4` breaks identically (the lone '>' drives Depth to -1). RUICompile -check (which uses the scanner, not a C++ compiler) reports the file clean, so the drift gate is green while the C++ build fails with a cryptic error pointing at generated code the author never wrote.

**Why the suite misses it.** The fileScan corpus and the C++ RunFileScan harness (ReactiveUIUetkxScannerTest.cpp) never assert the `params` field at all — no expectation shape reads names/types/defaults — and no golden/codegen case uses a param default containing a comparison or shift operator. The happy-path defaults in the corpus are all literals with no '<'/'>'.

**Suggested fix.** Only treat '<'/'>' as nesting when they actually delimit a template argument list, or (simpler and robust) do not use '<'/'>' for depth at all when splitting the top-level comma list — rely on ()/[]/{} nesting for commas and require template-comma-bearing types to be parenthesized, OR clamp Depth at 0 so a stray operator can't make it negative and guard the '<' push so it only counts when preceded by an identifier/'>' (a plausible template context). At minimum, mirror the same fix in uetkxFileScan.ts and add a corpus case with `= 1 << 3` / `> 0 ? ...` defaults.

### STYLE-1 — Docs stylesheet declares @theme tokens with a leading '$' — but the resolver strips '$' from the reference and matches the BARE name, so doc-shaped tokens never resolve
`ReactiveUIUnrealDocs~/src/pages/Features/FeaturesPage.tsx:25`  ·  **MEDIUM**  ·  CONFIRMED  ·  STY (style/theme system (post-fix))

**What's wrong.** The shipped stylesheet example (FeaturesPage.tsx:23-32) declares theme tokens WITH a '$' prefix on the declaration side:

  @theme dark {
      $bg: #10131a;
      $accent: #4f8cff;
  }

LoadStylesheet parses each declaration by splitting on ':' (RuiStyle.cpp:469-474), so the token is registered under the literal key FName("$bg") / FName("$accent"). But token RESOLUTION (ResolveThemeTokens, RuiStyle.cpp:164) takes a reference like "$accent", does RightChop(1) to get "accent", and calls Theme->Find(FName("accent")). Because the theme key is "$accent" (with the '$'), Find("accent") MISSES. The canonical form is proven by (a) the passing test ReactiveUIStyleBuilderTest.cpp:86-89 which declares tokens WITHOUT '$' (accent:/fade:) and references them WITH '$', and (b) the public ResolveThemeToken(FName) API (RuiStyle.cpp:319) which does a bare Theme->Find(TokenName). So the docs teach an unusable declaration syntax that silently disagrees with the code.

**Failure scenario.** A user copies the documented theme verbatim (@theme dark { $bg: #10131a; $accent: #4f8cff; }), calls LoadStylesheet + SetActiveTheme("dark"), then writes a class .btn { ColorAndOpacity: $accent; }. Resolution does Theme->Find("accent") against a theme whose only keys are "$bg"/"$accent" → miss → WarnUnknownKey("token:accent") once, then the raw String "$accent" is handed to the adapter where a Color was expected → the widget renders a wrong/default color. Following the docs produces a silently broken theme.

**Why the suite misses it.** Doc snippets are never compiled or executed, and the only style test (ReactiveUIStyleBuilderTest) declares tokens in the CORRECT bare form (accent:/fade:), so it never exercises the doc's '$'-prefixed declaration.

**Suggested fix.** Fix the docs to declare tokens without the '$' (bg: #10131a; accent: #4f8cff;) — the '$' belongs only at the reference site. Alternatively, make registration/resolution tolerant of a leading '$' in token keys (strip it symmetrically on both sides), but the docs are the outlier vs the test and the public API.

### STYLE-2 — ParseStyleValue vector2 branch does not trim components, so 'x, y' (space after comma) silently parses to a Name instead of a Vector2
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiStyle.cpp:365`  ·  **MEDIUM**  ·  CONFIRMED  ·  STY (style/theme system (post-fix))

**What's wrong.** The Vector2 grammar (RuiStyle.cpp:362-368) does ParseIntoArray on ',' and then checks Parts[0].IsNumeric() && Parts[1].IsNumeric() WITHOUT trimming the parts. FString::IsNumeric() returns false on any leading whitespace, so "10, 20" splits to ["10", " 20"] and " 20".IsNumeric() is false. The vector branch is skipped, S.IsNumeric() is also false (contains ','), and the value falls through to the bare-identifier branch → FRuiValue(FName("10, 20")) (Kind=Name). This value grammar is a public, shipped API (REACTIVEUISLATE_API ParseStyleValue) documented (TECH_DEBT.md:51) to accept 'x,y' vector2 and is used by LoadStylesheet (RuiStyle.cpp:474). When such a Name value later drives RenderTranslation (RuiStyle.cpp:62) or RenderTransformPivot (RuiStyle.cpp:113), the code reads V.Vector2Value, which for Kind=Name is the zero-default (0,0) — the transform is silently lost.

**Failure scenario.** A stylesheet (or the future @uss markup lowering that reuses ParseStyleValue) contains `.hero { RenderTranslation: 10, 20; }` with a natural space after the comma. ParseStyleValue yields a Name, ApplyRenderTransform reads Vector2Value=(0,0), and the widget is NOT translated — no error, no warning. The identical input without the space ("10,20") works, making the failure order-of-whitespace-dependent and baffling.

**Why the suite misses it.** ReactiveUIStyleBuilderTest.cpp:82 only tests the no-space form ParseStyleValue("4,2"); no test passes a space-separated vector, and no test applies a stylesheet-sourced RenderTranslation/RenderTransformPivot end-to-end.

**Suggested fix.** Trim each component before the numeric check and Atod, e.g. `FString A = Parts[0].TrimStartAndEnd(), B = Parts[1].TrimStartAndEnd(); if (Parts.Num()==2 && A.IsNumeric() && B.IsNumeric()) return FRuiValue(FVector2D(FCString::Atod(*A), FCString::Atod(*B)));`

### IW-1 — NumericEntryBox MinValue/MaxValue never clamp typed input (AllowSpin=false wires them only to a never-created SpinBox)
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiNumericEntryBox.cpp:18`  ·  **MEDIUM** _(finder: HIGH)_  ·  CONFIRMED  ·  WDG (interactive widgets re-sweep (post-fix))

> **Note.** Lenses corrected HIGH→MEDIUM (a documented feature that silently does nothing, but no crash/corruption). Cross-checked against engine SNumericEntryBox: Min/Max forward only to the SSpinBox, which AllowSpin(false) never creates.

**What's wrong.** SRuiNumericEntryBox::Construct hard-codes .AllowSpin(false) (line 18) and passes MinValue/MaxValue (lines 19-20) to SNumericEntryBox<float>. In the engine SNumericEntryBox, MinValue/MaxValue are ONLY forwarded to the SSpinBox (SNumericEntryBox.h lines 277-278), which is constructed solely when bAllowSpin==true. With AllowSpin(false) the widget uses the editable-text path: OnTextCommitted/OnTextChanged -> SendChangesFromText (SNumericEntryBox.h line 709) does Interface->FromString and fires OnValueCommitted/OnValueChanged with the RAW value, with NO clamp against MinValue/MaxValue. The wrapper's HandleValueChanged/HandleValueCommitted (RuiNumericEntryBox.cpp lines 36-50) forward the value verbatim, and SetValue (lines 27-34) never clamps. So MinValue/MaxValue are completely inert, yet RuiNumericEntryBox.h lines 19-20 document 'MinValue/MaxValue bound the typed input' — a shipped, documented feature that silently does nothing. Secondary: ApplyDiff also never re-applies MinValue/MaxValue and there is no reconstruct mask, so even a runtime bound change is ignored.

**Failure scenario.** A NumericEntryBox with MinValue=0, MaxValue=100 is rendered. The user types 999 and presses Enter: SendChangesFromText fires OnValueCommitted(999.0) and the wrapper forwards FRuiValue(999.0) to the parent; no clamp to 100 happens. Typing -50 likewise forwards -50. The bounds have zero effect.

**Why the suite misses it.** Per RuiNumericEntryBox.h lines 6-8 the harness reads the 'value getter [which] reflects the controlled member' — tests SetValue then assert GetValue()==CurrentValue and drive OnValueChanged directly. No test types an out-of-range string through the SEditableText path and asserts the forwarded value was clamped, so inert min/max is never exercised.

**Suggested fix.** Enforce bounds in the wrapper: store Min/Max as members, FMath::Clamp in HandleValueChanged/HandleValueCommitted (and SetValue) before forwarding, and add SetMinValue/SetMaxValue wired in ApplyDiff. Alternatively install a clamping numeric TypeInterface. Update the header doc if bounds are meant to be advisory only.

### IW-2 — ComboBox leaks per-row FRuiRoot sub-roots — RowRoots strong-holds every generated row, only reset in test-only OpenMenu()
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiComboBox.cpp:98`  ·  **MEDIUM**  ·  CONFIRMED  ·  WDG (interactive widgets re-sweep (post-fix))

**What's wrong.** HandleGenerateRow (lines 94-100) creates a fresh FRuiRoot per dropdown row and appends it to TArray<TSharedPtr<FRuiRoot>> RowRoots (line 98) as a STRONG ref, returning only Row->GetWidget() to SComboBox. FRuiRoot owns its reconciler/host/widget tree and only runs effect cleanups + detaches widgets on destruction/Unmount (RuiRoot.h lines 34-64). RowRoots is only ever cleared by OpenMenu() (line 117), a test/tooling driver, not part of the normal user open/close flow. When SComboBox's ComboListView regenerates rows (options changed via SetOptions->RefreshOptions, or a long option list scrolls and recycles rows), the previously generated FRuiRoots stay strong-held in RowRoots forever: their widget trees stay alive and their sub-tree UseEffect cleanups never run. The sibling ListView deliberately avoids this by holding rows as TWeakPtr<SRuiListRow> and Unmounting each row's sub-root in ~SRuiListRow (RuiListView.cpp lines 46-55, 119).

**Failure scenario.** A ComboBox bound to a filterable option list whose row render closure registers an effect (e.g. subscribes to a signal). The user repeatedly changes the filter (parent SetOptions with new item pointers) and opens the dropdown; each regeneration appends N new FRuiRoots to RowRoots while the prior N (dropped by SComboBox) are never released/Unmounted — memory and live subscriptions accumulate unbounded across the session.

**Why the suite misses it.** NumGeneratedRows() returns RowRoots.Num() (line 126) and the suite drives menus via OpenMenu() (line 115) which Resets RowRoots first (line 117), so each measurement starts clean and accumulation across reopens/option-changes is never observed; no test asserts scrolled-out/regenerated row sub-roots were Unmounted.

**Suggested fix.** Track rows weakly (mirror ListView: TArray<TWeakPtr<FRuiRoot>>) or hook SListView OnRowReleased to Unmount+drop a row's sub-root when its widget is released; at minimum Reset+Unmount stale roots at the start of each generation pass rather than only in OpenMenu().

### IW-3 — ComboBox fires OnSelectionChanged(-1) as a 'user pick' when the selected option is removed from Options
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiComboBox.cpp:37`  ·  **MEDIUM**  ·  CONFIRMED  ·  WDG (interactive widgets re-sweep (post-fix))

> **Note.** Same class as the just-fixed B6 (ComboBox over-fires OnSelectionChanged); this is the option-removal path, distinct from B6-1.

**What's wrong.** The B6 guard (bApplyingSelection) only covers SetSelectedIndex->Combo->SetSelectedItem (lines 48-62). SetOptions calls Combo->RefreshOptions() (line 37) with no guard. When ComboListView next rebuilds (on menu realize/tick), SListView::UpdateSelectionSet (engine SListView.h lines 1332-1372) sees the previously-selected item pointer is gone from the source and fires Private_SignalSelectionChanged(ESelectInfo::Direct) -> OnSelectionChanged_Internal(nullptr, Direct) (SComboBox.h line 604, proceeds because null != cached SelectedItem) -> our HandleSelectionChanged(nullptr) (lines 102-113). bApplyingSelection is false at that deferred time, so HandleSelectionChanged executes OnSelectionChanged.Execute(FRuiValue(-1)) at line 111 — a spurious user-pick-style event caused purely by the parent editing the options list, exactly the programmatic-vs-user confusion B6 was meant to eliminate but left unguarded on the RefreshOptions path.

**Failure scenario.** Controlled ComboBox: state.selectedIndex=1 over Options=[A,B,C] (B selected). Parent re-renders with filtered Options=[A,C] (B removed), keeping OnSelectionChanged bound to setState. The user opens the dropdown; ComboListView rebuilds, UpdateSelectionSet clears the vanished selection and fires Direct, so HandleSelectionChanged pushes OnSelectionChanged(-1) to the parent, which mutates state.selectedIndex to -1 as an unexpected side effect of merely editing the option list.

**Why the suite misses it.** The clear-and-fire is deferred to the inner ComboListView tick when the menu is realized; headless suites drive selection synchronously via GetComboWidget()/SetSelectedItem and assert GetSelectedIndex(), and do not (a) remove the selected option, (b) realize/tick the dropdown, and (c) assert OnSelectionChanged was NOT invoked.

**Suggested fix.** Set bApplyingSelection=true (or a dedicated bRefreshingOptions flag) around the RefreshOptions-driven reconciliation so HandleSelectionChanged treats the resulting clear as programmatic; when the old selected pointer is absent after SetOptions, update SelectedIndex/RefreshSelectedDisplay without firing OnSelectionChanged.

---

## LOW severity

### SEP-REBUILD-1 — Construct-only reconstruct resets previously-set runtime/plain props the new render omits (violates removed-props-don't-reset on the rebuild path)
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiSlateHost.cpp:471`  ·  **LOW**  ·  CONFIRMED  ·  ADP (the big Slate adapter file + slot mutation + widget replace)

**What's wrong.** When a construct-only prop changes, ReplaceWidget rebuilds via CreateWidget(NewProps) + ApplyDiff(NewWidget, nullptr, NewProps) (RuiSlateHost.cpp:470-475) and re-applies style from scratch (:530-535). It only ever consults NewProps. Any prop that was set on a prior render but is OMITTED in the render that triggers the rebuild is lost: CreateWidget falls back to its literal defaults (e.g. Thickness 3.0, Orient_Horizontal at RuiWidgetAdaptersB2.cpp:559-561) and ApplyDiff does not re-apply the removed runtime prop. On a normal (non-reconstruct) update the same omission correctly preserves the value (RUI_ROW skips unset props), so the reconstruct path diverges from the family 'removed plain props don't reset' guarantee. Also, ConstructOnlyChanged compares raw field values ignoring Has-bits (:548-553), so merely REMOVING a construct-only prop whose old value differed from the type default triggers a spurious rebuild.

**Failure scenario.** Render 1: `Separator({Thickness:5, ColorAndOpacity: red})`. Render 2: `Separator({Thickness:8})` (color omitted). ConstructOnlyChanged sees 5≠8 → ReplaceWidget; the rebuilt SSeparator applies only Thickness=8 and its ColorAndOpacity resets to the default (white) instead of staying red. Separately, Render 1 `Separator({Thickness:5})` then Render 2 `Separator({})` rebuilds and resets Thickness 5→3 even though a plain prop was merely dropped.

**Why the suite misses it.** The separator reconstruct test changes Thickness while keeping other props stable and only asserts widget-pointer identity changed (a new SSeparator), not that omitted props survived; no test removes a prop across a reconstruct.

**Suggested fix.** Have ReplaceWidget diff against the last-committed props (pass the old props / node's stashed props so unset-but-previously-applied runtime props are re-applied), and gate ConstructOnlyChanged on the Has-bits so removing a construct-only prop does not force a reset-to-default rebuild.

### CMU-1 — Input-method delegate is bound once to the first owning player's subsystem and never re-pointed on an owning-player change, leaving UseInputMethod tracking the wrong (or a dead) player
`Plugins/ReactiveUI/Source/ReactiveUICommonUI/Private/RuiActivatableScreen.cpp:87`  ·  **LOW**  ·  PLAUSIBLE  ·  BRG (CommonUI / MVVM / UMG bridges (post-fix))

> **Note.** PLAUSIBLE (lens A plausible / lens B refute).

**What's wrong.** RefreshInputMethod() subscribes to UCommonInputSubsystem::OnInputMethodChangedNative guarded ONLY by `if (!InputMethodHandle.IsValid())`. Once a subscription is made, InputMethodHandle stays valid for the widget's entire constructed lifetime (it is Reset only in ReleaseSlateResources), so the live-change subscription is permanently anchored to whichever ULocalPlayer's subsystem was resolved at the first subscribe. If GetOwningLocalPlayer() later returns a DIFFERENT player, the guard suppresses re-subscription: the one-shot line `State.InputMethod = MapInputType(Input->GetCurrentInputType())` reads the new player correctly, but every subsequent device-switch callback continues to arrive from the ORIGINAL player's subsystem. HandleInputMethodChanged is therefore driven by the wrong player, and the original binding is never removed (ReleaseSlateResources removes from the then-current player's subsystem, not the one actually subscribed) — a benign leak only because AddUObject is a weak binding, so no use-after-free.

**Failure scenario.** Local co-op / player reassignment: a URuiActivatableScreen is constructed and activated under Player1, subscribing to P1's UCommonInputSubsystem. The screen is later reassigned to Player2 (SetOwningLocalPlayer / player-context change) WITHOUT a ReleaseSlateResources cycle. Player2 then switches gamepad->mouse: OnInputMethodChangedNative fires on P2's subsystem, but the screen listens on P1's, so HandleInputMethodChanged never runs and the hosted tree's UseInputMethod stays stale. Conversely, a P1 device switch wrongly re-renders the now-P2-owned screen to P1's method. Result: UseInputMethod consumers show the wrong input family after a player change.

**Why the suite misses it.** ReactiveUI.CommonUI.InputMethod (FRuiCommonUIInputMethodTest) constructs the screen inside a single standalone GameInstance with one LocalPlayer and toggles that one player's SetCurrentInputType. No test ever reassigns the owning player between subscribe and a subsequent device switch, so the 'subscription is stuck to the first player' assumption is never exercised.

**Suggested fix.** Do not gate the subscription solely on InputMethodHandle validity. Record the subsystem (or player) that was subscribed (e.g. a TWeakObjectPtr<UCommonInputSubsystem> SubscribedInput). In RefreshInputMethod, if the freshly-resolved Input differs from SubscribedInput, Remove the old handle from the old subsystem, then AddUObject to the new one and store the new SubscribedInput; only skip when they are the same. This keeps the live subscription pointed at the current owning player and removes the stale binding from the previous one.

### RECON-3 — UseSearchParams setter hard-codes bReplace=true — diverges from React-Router push default; prior search state cannot be restored via back
`Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiRouter.cpp:699`  ·  **LOW**  ·  PLAUSIBLE  ·  COR (core reconciler / presence / router (post-fix))

> **Note.** PLAUSIBLE (lenses split).

**What's wrong.** The setter returned by UseSearchParams always calls Navigate(Pathname + BuildSearch(Next), /*bReplace*/ true). React-Router v6's setSearchParams pushes a new history entry by default (replace:false, caller-overridable). Hard-coding replace=true means every search-param change overwrites the current history entry, so the previous query string is not recorded in the back stack and the back button cannot restore it. The setter also has no way to opt into push.

**Failure scenario.** On "/search?q=hi" a component calls setSearchParams({q:"bye"}). The URL becomes "/search?q=bye" but replaces the top history entry. The user presses back expecting "/search?q=hi"; instead they leave the search page entirely (the q=hi state was never pushed). Under React-Router the back button would restore q=hi.

**Why the suite misses it.** ReactiveUI.Router.Spine sets a search param and reads it back but never asserts back-stack behavior after setSearchParams (it does not press back following GSetSearch), so the replace-vs-push distinction is invisible.

**Suggested fix.** Give the setter an optional replace flag defaulting to false (push), matching React-Router: TFunction<void(TMap<FString,FString>, bool /*bReplace=false*/)>, and forward it to Navigate. If a single-arg signature must be kept, at least default to push to match the documented React-Router shape.

### RECON-4 — ResolvePath comments claim 'drop From's last segment' but the code (and the locking test) append to the full pathname — stale, trap-inviting docs
`Plugins/ReactiveUI/Source/ReactiveUICore/Private/RuiRouter.cpp:193`  ·  **LOW**  ·  CONFIRMED  ·  COR (core reconciler / presence / router (post-fix))

**What's wrong.** ResolvePath's inline comment (line 193: 'join onto From's directory (From is a full pathname; drop its last segment)') and the header contract (RuiRouter.h line 74: 'relative joins onto From's directory') describe browser-style relative resolution where the last segment of From is dropped before joining. The code does NOT drop the last segment — Base = Segments(From) uses the entire From, so relative resolution appends to the full pathname. ReactiveUI.Router.Match explicitly locks this: ResolvePath("edit","/users/42") == "/users/42/edit" (append, React-Router directory semantics). The behavior is correct and intended; the comments are wrong and contradict both the code and the test.

**Failure scenario.** Not a runtime failure: a maintainer reads 'drop its last segment', concludes ResolvePath("edit","/users/42") should yield "/users/edit", and 'fixes' the code to pop From's last segment — silently breaking every relative <Link>/navigate (they'd resolve one directory too high) and the round-trip the family shares with the Godot/Unity siblings. The stale comment is a latent regression trap.

**Why the suite misses it.** This is a comment/contract defect, not a code path; the suite tests the (correct) code behavior, so no test can surface the mismatch between the comment and the code.

**Suggested fix.** Update the header (RuiRouter.h:74) and the inline comment (RuiRouter.cpp:193) to state the actual React-Router semantics: a relative To is appended to the current pathname (treated as a directory), and '..' climbs one segment — do NOT drop From's last segment.

### DRV-3 — RUICompile returns exit 0 on an unbuildable tree when re-run over a standing compile error
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:744`  ·  **LOW** _(finder: MEDIUM)_  ·  PLAUSIBLE  ·  DRV (driver, config, contract, sidecar/staleness)

> **Note.** PLAUSIBLE (lenses split).

**What's wrong.** A file with a standing error verdict (busy-loop guard) returns from CompileFile with bOk=true AND bSkipped=true (lines 321-322). In the CompileAllRoots tally, `if (R.bSkipped) ++Out.Skipped;` (line 744) is checked before the error branch, so a persistently-broken file is counted as Skipped, not Error, on the second and later sweeps. The RUICompileCommandlet returns `Sweep.Errors == 0 ? 0 : 1` (RUICompileCommandlet.cpp:60), so the exit code flips from 1 (first run, error reported) to 0 (subsequent runs) even though the file still has no .inl and cannot build. Because the failed file remains in the module aggregator (BuildAggregators emits `#include "X.uetkx.inl"` for every source regardless of whether its .inl exists), the tree is genuinely unbuildable, yet the commandlet reports success.

**Failure scenario.** Broken.uetkx (under a module dir) has a syntax error. `RUICompile` -> compile fails, .inl deleted, error sidecar written, Errors=1, exit 1. Developer/build script re-runs `RUICompile` without fixing (or a self-hosted CI runner that retained Saved/ sidecars across runs) -> Broken is now IsStale=false (busy-loop guard) -> skipped -> Errors=0 -> exit 0. A `RUICompile && Build` pipeline now proceeds to the C++ build, which fails on the dangling `#include "Broken.uetkx.inl"`. The commandlet's exit code no longer reflects tree health.

**Why the suite misses it.** The driver test verifies the busy-loop guard makes a re-compile skip (lines 99-100) but never asserts the sweep's Errors count / commandlet exit code on a SECOND sweep over a still-broken tree. CI's engine legs (both the -check gate and the automation suites) are commented-out placeholders, and CI checkouts have no committed sidecars (gitignored), so a fresh clean run always re-errors; the masking only appears on incremental re-runs with retained sidecars.

**Suggested fix.** Distinguish a "standing error" skip from an "up-to-date" skip: add e.g. `bSkipped` + a `bStandingError` flag (or keep the recorded diagnostics on the skip result) and count standing-error skips toward Out.Errors, so the commandlet's exit code stays 1 while any file is broken. Alternatively, exclude files with no .inl from the aggregator and count them as errors.

### DRV-4 — Transient env-hold (unreadable source) is counted as a sweep Error, contradicting the retry design
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:754`  ·  **LOW**  ·  PLAUSIBLE  ·  DRV (driver, config, contract, sidecar/staleness)

> **Note.** PLAUSIBLE (lenses split).

**What's wrong.** When a source is unreadable (editor save race), CompileFile returns a UETKX2507 severity-1 diagnostic with bOk=false, bSkipped=false, and deliberately writes no sidecar/verdict (lines 327-335, "env hold: never delete outputs, never record a verdict"). In the CompileAllRoots tally the file falls into the final `else` branch and `++Out.Errors` runs (line 754) before the diagnostics loop sets bAnyHeld. So an env hold, which the design treats as transient-and-retry (and correctly uses bAnyHeld to SUPPRESS the fingerprint refresh at line 893), is nonetheless counted as an error. RUICompile then returns exit 1, and the watcher's `Errors += Result.Errors` reports a phantom error, for a purely transient read failure.

**Failure scenario.** During the editor watcher's sweep, an external editor is mid-write on a .uetkx (file momentarily unreadable/locked). CompileFile returns UETKX2507. The sweep tally increments Out.Errors, so the watcher logs "N error(s)" and the commandlet (if it hit the same race) would exit 1, even though the correct behavior is to hold outputs and retry on the next poll. bAnyHeld correctly prevents a fingerprint refresh, but the error count is still inflated.

**Why the suite misses it.** No test makes a source unreadable mid-sweep; the driver test only exercises well-formed and syntactically-broken sources, never an I/O hold, so the UETKX2507 tally path is never asserted.

**Suggested fix.** In the tally, detect a held file (any UETKX2507 diag) and count it separately (e.g. Out.Held) instead of Out.Errors, so a transient read failure does not fail the sweep. The bAnyHeld branch already exists — reuse it before the ++Out.Errors.

### CG-4 — Cross-file module-default dependency inside a component-import cycle breaks DECL-phase ordering and is missed by the value-cycle detector
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:501`  ·  **LOW**  ·  CONFIRMED  ·  GEN (codegen (fwd-decl aggregator, cycle parity, ordering, #line))

**What's wrong.** BuildAggregators topo-sorts files so a dependency's DECL phase precedes its importer's, whose only hard constraint (per the code comment at :438) is a module body preceding a struct whose member defaults read its constants. DepsByPath (:453-471) counts ALL imports (component and module alike), so a pair of files that mutually import forms a DepsByPath cycle; the Kahn loop makes no progress and falls back to appending the remainder ALPHABETICALLY (:501-508), which does not honor the module-before-struct ordering. Meanwhile the UETKX2306 value-cycle detector (UetkxDriver.cpp:773-872) only adds graph edges for imported HOOKS/MODULES, so a cycle whose back-edge is a COMPONENT import is not flagged. Thus a real ordering hazard slips past both mechanisms, contradicting the comment's claim that the alphabetical cycle remainder is 'now legal under the two-phase decl pass' (true only for component wrappers, not module-constant defaults).

**Failure scenario.** A.uetkx: `import { Palette } from "./B"` and `export component Card(color: FLinearColor = Palette::Primary){ return (<Border/>) }`. B.uetkx: `import { Card } from "./A"` and `export module Palette { const FLinearColor Primary = FLinearColor::White; }` plus `export component Gallery(){ return (<Card/>) }`. Value graph has only A->B (module) — no cycle — so 2306 stays silent. DepsByPath has A<->B, topo makes no progress, alphabetical remainder emits A before B. A's DECL-phase struct `FLinearColor color = Palette::Primary;` references `namespace Palette` which B (emitted after A) has not yet declared -> aggregator TU fails with 'Palette has not been declared'. Renaming so the module owner sorts first hides it — the correctness depends on filename alphabetics.

**Why the suite misses it.** The CycleProof fixtures exercise only pure component<->component cycles (no cross-file module-constant default sitting inside the cycle), and no test constructs the mixed component+module import cycle, so the topo fallback + 2306 gap is never hit.

**Suggested fix.** Split ordering constraints by kind: only MODULE/HOOK import edges are hard ordering constraints for the DECL phase (component wrapper edges are satisfied by forward declaration and can be dropped from the topo graph). Build DepsByPath from module/hook edges only; a genuine unbreakable cycle then involves a module/hook and should be reported as UETKX2306 (extend 2306's edge set, or add a dedicated diagnostic) rather than silently emitted in alphabetical order.

### HOOKDOC-1 — Stale "23 hook names" doc comment; the table and shipped schema list 20
`Plugins/ReactiveUI/Source/ReactiveUIInterp/Public/UetkxFileScan.h:171`  ·  **LOW**  ·  CONFIRMED  ·  GRM (grammar/schema byte-consistency + generated-vs-source + formatter)

**What's wrong.** The doc comment reads "The 23 hook names (auto-prefix + signature scanning)." but FUetkxFileScan::HookNames() (UetkxFileScan.cpp:498-518) defines exactly 20 entries, and the shipped schema uetkx-schema.json `hooks` array (lines 336-357) likewise lists 20. The count in the comment is wrong (likely carried over from a sibling repo). No functional impact — the array itself is the source of truth for auto-prefix, signature hashing, and schema export — but it is a concrete docs-vs-reality gap for a maintainer relying on the header.

**Failure scenario.** A maintainer reading UetkxFileScan.h trusts the '23' figure when reconciling the hook table against the schema or docs and wastes time hunting for 3 non-existent hooks, or wrongly concludes 3 hooks were dropped from the registry.

**Why the suite misses it.** It is a source-comment literal, not executable code; no test or CI gate (docs-drift.mjs checks registry-derived counts, not free-text comments) inspects this string.

**Suggested fix.** Change '23' to '20' (or make the comment count-agnostic, e.g. 'the built-in hook names'), matching HookNames() and the schema.

### IMPORT-2 — Cross-module import (2308) also emits a contradictory 2305 'not imported — add this import' for the same binding
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxResolve.cpp:380`  ·  **LOW**  ·  CONFIRMED  ·  IMP (import/export resolution engine)

**What's wrong.** In Apply, when an import crosses a module/root boundary the code emits UETKX2308 and `continue`s (UetkxResolve.cpp:376-382) BEFORE the loop that populates `ImportedNames` (lines 385-403). Consequently the imported name is never added to ImportedNames. The later strict-usage pass (lines 420-439) then sees the component tag as neither same-file nor imported, calls FindExporter (which finds it in the other module), and emits UETKX2305 telling the author to add the exact import they already wrote — with SuggestSpecifier reproducing the same cross-module specifier that 2308 just rejected. The two errors are mutually contradictory.

**Failure scenario.** Reproduced by the existing 2308 fixture tree: ModA/App.uetkx has `import { Chip } from "../ModB/Chip"` and renders `<Chip/>`; Chip is exported by ModB. Apply resolves the import, detects the module boundary, emits UETKX2308, and continues without adding Chip to ImportedNames. The tag loop then finds Chip's exporter (ModB) and emits UETKX2305: '`Chip` is defined in ModB/Chip but not imported — add: import { Chip } from "../ModB/Chip"'. The author is simultaneously told to remove the import (2308) and to add it (2305); an LSP quick-fix consuming the 2305 fix-it would re-introduce the forbidden import.

**Why the suite misses it.** ReactiveUIUetkxResolveTest line 135-137 asserts only `Has(..., UETKX2308)`; it never asserts the ABSENCE of 2305, so the spurious co-diagnostic passes unnoticed.

**Suggested fix.** Before the `continue` at UetkxResolve.cpp:382, add the import's names to `ImportedNames` (and to a set that suppresses their 2305/2304 downstream), or move the cross-module check to run per-name after ImportedNames is populated so 2308 is the sole diagnostic for a cross-module binding.

### IMPORT-3 — Reference scanner conflates ternary/case/label ':' with scope '::', dropping hook/module refs that follow a colon; migrate codemod then blesses un-buildable output
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxResolve.cpp:96`  ·  **LOW**  ·  PLAUSIBLE  ·  IMP (import/export resolution engine)

> **Note.** PLAUSIBLE (lenses split: A confirm, B refute).

**What's wrong.** CollectExternalRefs decides an identifier is not a fresh reference when the immediately preceding non-space/tab character is ':' (`bScope = (P == ':')`, UetkxResolve.cpp:96, applied at line 98). This is intended to skip the second+ segments of `A::B::C`, but a single ':' also appears in ternaries (`c ? x : Name`), switch cases (`case K: Name`), and labels. Any user hook call or `Ident::` module qual whose ONLY occurrence sits immediately after such a colon is silently dropped from the reference set, so no UETKX2305 is produced and MissingImports (which shares this scanner) never proposes the needed import.

**Failure scenario.** In a component's setup: `const int32 W = bWide ? 200 : Sizes::Narrow;` where `Sizes` is a module exported by another file and referenced nowhere else in this file. CollectExternalRefs sees `Sizes` preceded by ':' (the ternary), marks bScope, and skips it. Apply emits no 2305, and RUIMigrateImports' MissingImports adds no `import { Sizes } from ...`. The migrate commandlet's own final gate uses the same scanner, so it also sees zero errors and RUIMigrateImports exits 0 ('successfully migrated'). But the generated .inl contains `Sizes::Narrow` with no import, and the subsequent C++ build fails with an undeclared-identifier error the toolchain claimed to have resolved.

**Why the suite misses it.** ReactiveUIUetkxResolveTest exercises module quals only in statement-leading position (`RuiDemo::Value`, line 121) and never places a hook/module reference immediately after a ternary/case/label colon, so the false-negative path is untested. No migrate-idempotency test contains a post-colon module reference.

**Suggested fix.** Only treat ':' as a scope-suppressor when it is actually part of '::' — i.e. require the preceding char to be part of a `::` token (check that the char before it, or after, is also ':'), rather than any single ':'. A single ':' not adjacent to another ':' (ternary/case/label) must not suppress a fresh reference.

### IMPORT-4 — Self-import of a same-file hook/module is misreported as a value-import cycle; self-imported component is silently accepted and pollutes dep_hashes with a self-edge
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:811`  ·  **LOW**  ·  PLAUSIBLE  ·  IMP (import/export resolution engine)

> **Note.** PLAUSIBLE (lenses split).

**What's wrong.** There is no self-import guard anywhere in the resolution flow. Resolve happily returns the importer's own path for `import { X } from "./Self"`, so DepHashes records a self-edge and the value-import-cycle builder (UetkxDriver.cpp:808-816) adds `NormFull(Target) == NormFull(Path)` as an edge to itself; the DFS then hits `OnStack.Contains(Next)` immediately (line 830) and reports UETKX2306 'value-import cycle: Self.uetkx -> Self.uetkx'. For a self-imported HOOK/MODULE this is a hard error with a message that describes a non-existent inter-file cycle (the symbol is in the same TU, so there is no eager-load deadlock to break). For a self-imported COMPONENT no value edge is created, so there is NO diagnostic at all — the redundant import is accepted and a self-referential entry is written into the file's dep_hashes, which also makes the file appear in its own ImportersOf blast radius.

**Failure scenario.** Self.uetkx: `import { UseHelper } from "./Self"` + `export hook UseHelper() {}` + `export component Foo { UseHelper(); return (<Spacer/>); }`. Resolve(./Self) returns Self's own path; GetDecls finds UseHelper (exported) so no 2301/2302; the value-edge builder adds a self-edge and the sweep reports UETKX2306 'value-import cycle: Self.uetkx -> Self.uetkx', failing the whole build with a misleading message. Swapping the hook for `import { Foo } from "./Self"` where Foo is this file's own component yields the opposite: zero diagnostics, a self dep-hash recorded, and Foo listing itself as its own importer.

**Why the suite misses it.** No resolver or driver test constructs a file that imports from itself; the 2306 test uses two distinct files (MA/MB), and same-file references are only tested via the no-import same-file case (Resolve test lines 111-117).

**Suggested fix.** In Apply/Resolve, detect when a resolved Key equals the importer's own resolved path and emit a dedicated 'a file cannot import from itself' diagnostic (and skip recording a self dep_hash and self value-edge), rather than letting it fall through to a misleading 2306 for hooks/modules or to silent acceptance for components.

### LSP-4 — enclosingAttrName brace-walk ignores string/comment literals — a `}`/`{` inside an event-handler string misdirects Value.<field> completion
`ide-extensions/lsp-server/src/eventPayload.ts:36`  ·  **LOW**  ·  CONFIRMED  ·  LSE (LSP embedded-C++ intel + server + formatter)

**What's wrong.** `enclosingAttrName` walks backward balancing raw `{`/`}` characters (lines 36-52) with no awareness of string or comment literals. A brace embedded in a string literal inside the handler expression is counted as structural, throwing off the depth balance so the real attribute-value `{` is seen at depth>0 (skipped) and the walk runs to the tag boundary and returns null. It is used by the `Value.<field>` completion in server.ts (lines 183-195) to put the enclosing event's payload field first.

**Failure scenario.** Typing inside `<Slider OnValueChanged={ Log("}", Value.| }>`: walking back from the cursor, the `}` inside the string literal `"}"` increments depth, consuming the real attr `{` as its match, so `enclosingAttrName` returns null. The completion then loses the event-first ordering (offers all 7 FRuiValue fields alphabetically-ish instead of `FloatValue` first), so the user is not steered to `Value.FloatValue` for OnValueChanged.

**Why the suite misses it.** `server.test.ts` covers a nested brace in the expression (`FVector2D{1,2}`) but never a brace inside a string literal; the naive character walk passes every covered case. The result is only a completion-ordering degradation, not a crash or wrong data, so nothing else surfaces it.

**Suggested fix.** Run the backward walk through a noncode-aware scanner (skip string/comment spans, e.g. reuse the cppScanner's skip logic in reverse or scan forward from the attr open) so braces inside literals are not counted.

### LSP-2 — LSP indexes the whole .uproject tree for findExporter/completions, but the engine compiles only Source/ + Plugins/, so the LSP offers and navigates to exports the compiler never sweeps
`ide-extensions/lsp-server/src/uetkxWorkspace.ts:174`  ·  **LOW** _(finder: MEDIUM)_  ·  PLAUSIBLE  ·  LSW (LSP import intelligence (workspace/scan/schema))

> **Note.** PLAUSIBLE (lens B partial).

**What's wrong.** findExporter() searches workspaceRootFor(importer) (uetkxWorkspace.ts:90), which returns the nearest .uproject directory, and listUetkxFiles() (line 142) then recursively walks the ENTIRE project tree (skipping only node_modules/Intermediate/Saved/Binaries/DerivedDataCache/.git and /ContractFixtures/). The authoritative engine universe is narrower: RUICompileCommandlet::DefaultRoots() (RUICompileCommandlet.cpp:14) sweeps only <Project>/Source and <Project>/Plugins, and FUetkxFsResolver::EnsureIndex (UetkxResolve.cpp:262) builds its export index only from those SearchRoots. Thus the LSP's export/completion universe is a strict superset of the engine's. resolveSpecifier and the C++ Resolve both merely check file existence, so an explicit import to an out-of-sweep file resolves clean in BOTH (no 2300) -- but the driver never compiles that file (no .uetkx.inl is generated, it never enters the 2106 ledger or any aggregator), so the C++ build fails to link the referenced symbol, with no diagnostic warning the author.

**Failure scenario.** A user of the shipped plugin puts a reusable component at <Project>/Content/UI/Card.uetkx: `export component Card { return ( <Border /> ) }`. In Source/Demo/App.uetkx they type `<Card` -- the tag/import machinery and go-to-definition (server.ts markupDefinition -> findExporter, uetkxWorkspace.ts:405) resolve it to Content/UI/Card.uetkx, and import-specifier completion (server.ts:141 -> listUetkxFiles) offers `../../Content/UI/Card`. The author accepts `import { Card } from "../../Content/UI/Card"`. resolveSpecifier resolves it (file exists) so the LSP shows the import fully green; the engine front-end Resolve also resolves it (no 2300). But RUICompile only sweeps Source+Plugins, so Card.uetkx.inl is never generated and the aggregators never #include it -> the C++ build fails to link Card, while every diagnostic surface stayed green. The LSP actively guided the author into a broken layout.

**Why the suite misses it.** Every .uetkx file in this repo lives under Source/ (glob confirms), and the resolver tests build synthetic single-directory workspaces, so no test exercises a .uetkx placed outside the Source/Plugins sweep. The LSP tests assert findExporter results against files that would also be in the engine's sweep, so the universe-scope divergence never appears.

**Suggested fix.** Scope the LSP's findExporter/listUetkxFiles universe to the same roots the engine sweeps (project Source/ and Plugins/, plus any configured extra roots) rather than the entire .uproject tree, OR emit an advisory diagnostic when an import/reference resolves to a file outside the compile sweep. At minimum, keep the LSP index and the engine SearchRoots in one shared definition so they cannot drift.

### LSP-3 — importCursorAt only inspects the single physical line, so import name/specifier completions silently fail inside valid multi-line import statements
`ide-extensions/lsp-server/src/uetkxWorkspace.ts:265`  ·  **LOW**  ·  CONFIRMED  ·  LSW (LSP import intelligence (workspace/scan/schema))

**What's wrong.** importCursorAt() extracts the current physical line (scanning back/forward to the nearest \n) and requires it to match /^\s*import\b/ (line 265) before classifying an import-name or import-specifier cursor. The scanner, however, accepts multi-line imports: parseImport (uetkxFileScan.ts:304) uses findMatching across newlines for the `{ ... }` list and skipWsOnly (which consumes newlines) before `from` and the specifier, mirroring the C++ ParseImport. So a cursor on a continuation line of a multi-line `import { ... } from "..."` yields a current line like `  Card,` that fails the /^\s*import\b/ test, and importCursorAt returns null -- the onCompletion import branch (server.ts:135) is skipped and the author gets no exported-name or specifier completions there.

**Failure scenario.** Author writes a multi-line import:
import {
  Card,
  <cursor>
} from "./Widgets"
With the cursor on the third line, importCursorAt sees line `  ` (no `import` prefix) and returns null, so no import-name completions are offered even though the specifier resolves and exports exist. The same import parses and resolves correctly (go-to-definition, which uses scan.imports ranges, still works) -- only completion is dead, making the failure easy to miss.

**Why the suite misses it.** The importCursorAt/completion tests and fixtures use single-line imports exclusively; no test drives a cursor onto a continuation line of a multi-line import, so the single-line assumption is never challenged.

**Suggested fix.** Detect the enclosing import by scanning backward to the start of the statement (nearest preceding `import` keyword at preamble scope, or reuse scan.imports offset ranges) rather than restricting to one physical line; then compute the specifier/name-list partial from that statement span so completions work for multi-line imports too.

### RESOLVE-1 — FUetkxFsResolver::EnsureIndex builds the exporter index over unsorted FindFilesRecursive, making the 'first exporter wins' tie-break non-deterministic across machines
`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxResolve.cpp:272`  ·  **LOW**  ·  PLAUSIBLE  ·  MIG (RUIMigrateImports codemod + RUICompile -check + editor/watcher/preview)

> **Note.** PLAUSIBLE (lenses split).

**What's wrong.** EnsureIndex (lines 262-296) iterates the results of IFileManager::FindFilesRecursive WITHOUT sorting them, and records ExporterOf[Name]/ExportIndex[Name] on a first-seen ('!ExporterOf.Contains(D.Name)') basis. FindFilesRecursive does not guarantee a stable, sorted enumeration order (it is filesystem/platform dependent) — FUetkxDriver::FindAll (UetkxDriver.cpp:377) explicitly calls .Sort() on the same API's output precisely to be deterministic, but EnsureIndex omits it. Consequently, whenever a name is exported by two or more files, which file wins FindExporter is non-deterministic. FindExporter feeds (a) the codemod's chosen `import { X } from "…"` target (MissingImports -> SuggestSpecifier) which is WRITTEN into committed .uetkx source, and (b) the live UETKX2305 fix-it message text ('add: import { X } from "…"'). So the codemod's output — and the editor's suggested import — differ between machines/checkouts for the same tree, producing spurious source diffs and merge churn. In a clean tree UETKX2106 forbids duplicate exports so the tie-break is moot, but the codemod's export-everything pass (see MIGRATE-1) manufactures exactly this duplicate-export state, which is when determinism matters most.

**Failure scenario.** After RUIMigrateImports export-everything, both ScreenA/Foo.uetkx and ScreenB/Foo.uetkx export `component Foo`. On developer 1's machine FindFilesRecursive lists ScreenA first, so a third file gets `import { Foo } from "../ScreenA/Foo"`; on developer 2's machine (or CI on Linux) ScreenB is enumerated first, yielding `import { Foo } from "../ScreenB/Foo"`. The two runs of the 'idempotent' codemod over the same tree produce different committed source.

**Why the suite misses it.** The duplicate-export tests only cover a single exporter per name (the valid tree) or two PRIVATE (non-exported) decls; no test constructs a name exported by 2+ files and asserts WHICH file FindExporter/SuggestSpecifier selects, so the enumeration-order dependence is never observed. It also only surfaces when the underlying filesystem enumerates unsorted, which the Windows dev box typically hides.

**Suggested fix.** Sort the Files array before indexing in EnsureIndex (Files.Sort() right after each FindFilesRecursive), mirroring FUetkxDriver::FindAll, so the first-exporter tie-break is a stable, machine-independent alphabetical order.

### SCAN-2 — Comment inside an import brace list is misread as a bad specifier: spurious UETKX0300 and the entire import is silently dropped
`Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp:183`  ·  **LOW**  ·  CONFIRMED  ·  SCN (scanner grammar + data model + lexer)

**What's wrong.** ParseImport locates the closing brace with FindMatching (which is comment-aware via SkipNoncode), but the loop that reads the individual names (lines 181-206) advances with SkipWsOnly (whitespace only), not SkipNoncode. So a `/* ... */` or `//` comment between names lands the cursor on '/', which is neither a comma nor an identifier char; the `p == s` branch at line 198 fires and emits UETKX0300 ("import list expects bare names — named exports only (no `*`, no default)") and returns AdvancePastLine(Src, Bclose) at line 203 — BEFORE the specifier is parsed and before `Out.Imports.Add` at line 269. The whole import declaration is therefore discarded, not just diagnosed. The TypeScript mirror (uetkxFileScan.ts:317-329) uses skipWsOnly identically, so the behavior is consistent (not a mirror divergence) but still wrong.

**Failure scenario.** `import { A, /* keep B for later */ B } from "./Shared"` (legal ES-module syntax a migrating author would write). Result: one UETKX0300 error whose message talks about `*`/default (never used here), and the import is dropped from Out.Imports so BOTH A and B are treated as un-imported. Downstream the resolver (UetkxResolve.cpp) then raises UETKX2305/2307 on every use of A and B, compounding the misdirection. `import { A } // trailing\n from "./X"` fails the same way.

**Why the suite misses it.** Every import case in the fileScan corpus uses comment-free brace lists; there is no case with a comment inside `{ ... }`, and the resolver-level cascade is exercised only on clean inputs.

**Suggested fix.** In the names loop replace `p = SkipWsOnly(Src, p)` with a skip that also consumes comments (loop SkipNoncode + SkipWsOnly to a fixpoint, as SplitMarkupReturn's peek does at lines 573-582), so `/* */` and `//` inside the brace list are ignored rather than mistaken for an illegal token. Mirror in uetkxFileScan.ts and add a corpus case.

### SCAN-3 — A hook declaration missing its body ('-> Ret' with no braces) scans forward unbounded to the next declaration's '{' and swallows that declaration as its body
`Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp:422`  ·  **LOW**  ·  CONFIRMED  ·  SCN (scanner grammar + data model + lexer)

**What's wrong.** In ParseHook, after seeing `->` the return type is captured by scanning raw characters forward until the first '{' (`while (k < N && Src[k] != C_LBRACE) ++k;`, lines 419-427) with no upper bound and no SkipNoncode. If the hook has no body, the scan runs past the intended end of the declaration and stops at the '{' that opens the NEXT declaration, then FindMatching treats that declaration's body as the hook's body. ScanPreamble (lines 865-871, used by the import resolver to build the cross-file export table) has the same unbounded '-> Ret' forward scan, so both the compiler view and the resolver view mis-capture identically. No diagnostic is produced — the hook parse 'succeeds'.

**Failure scenario.** ```\nexport hook UseThing() -> int\nexport component Panel { return ( <Root/> ); }\n``` (author forgot the hook body / wrote a declaration-only line). ParseHook's Ret scan skips over `int\nexport component Panel ` and grabs Panel's `{ return (<Root/>); }` as UseThing's verbatim body. Scan then returns with Hooks=[UseThing] and Components EMPTY — Panel silently vanishes. Because a hook was captured, the 'no declaration' UETKX2101 does not fire, so Scan reports no error. Panel disappears from the export index (UetkxResolve EnsureIndex), so any other file doing `import { Panel } from ...` gets UETKX2302 'Panel is not declared in <file>' pointing at the wrong place, and the generated hook body `{ return (<Root/>); }` (markup, not C++) only fails much later in the real C++ compile with an inscrutable error.

**Why the suite misses it.** No corpus or automation case declares a hook without a body followed by another declaration; hook cases always have `{ ... }`, and ScanPreamble/export-table behavior is tested only on well-formed files.

**Suggested fix.** Bound the '-> Ret' scan so it cannot cross into another declaration: stop at the first '{' OR at a top-level ';'/newline-that-begins-a-new-decl-keyword, and if no '{' is found before such a boundary emit UETKX2202 ('hook body { ... } expected') instead of consuming the next decl. Apply the same guard in ScanPreamble (lines 865-871) and the TS mirror.

### STYLE-3 — $token that resolves to another $token is not resolved transitively and emits no warning — a raw '$name' string reaches the adapter
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiStyle.cpp:167`  ·  **LOW**  ·  PLAUSIBLE  ·  STY (style/theme system (post-fix))

> **Note.** PLAUSIBLE (lenses split).

**What's wrong.** ResolveThemeTokens (RuiStyle.cpp:160-174) resolves each dict value at most ONCE: if the value is a '$'-ref it does Pair.Value = *Resolved and moves on. If the theme token's own value is itself a '$'-ref (a semantic token pointing at a primitive token, e.g. theme { buttonBg: $blue500; blue500: #2f6fed; }), the reference resolves to the String "$blue500", which is left in the dict unchanged. Unlike the missing-token path (which at least calls WarnUnknownKey), this path finds the first token successfully, so it emits NO diagnostic. The public single-token ResolveThemeToken (RuiStyle.cpp:319) has the same non-transitive behavior. ParseStyleValue/RegisterTheme happily allow a token value to be a '$'-ref (ParseStyleValue returns Kind=String for '$'-prefixed input), so token→token themes are constructible.

**Failure scenario.** Theme registered as { buttonBg: $blue500; blue500: #2f6fed; }, active. A class .btn { ColorAndOpacity: $buttonBg; }. BuildEffectiveStyle resolves $buttonBg → the theme's buttonBg value which is the String "$blue500" (still a token ref). No second pass runs and no warning fires, so the adapter receives Kind=String "$blue500" where a Color was expected → wrong/default color, silently. This semantic-token→primitive-token indirection is a standard design-system pattern, so it is a realistic authoring shape.

**Why the suite misses it.** Both the bugfix and builder style tests define every theme token as a CONCRETE value (#00ff00, 0.25, an FLinearColor); none defines a token whose value is itself a '$'-ref, so the second-level resolution is never exercised.

**Suggested fix.** Resolve iteratively with a small cycle guard: after Pair.Value = *Resolved, keep re-resolving while IsTokenRef(Pair.Value) up to a bounded depth (e.g. 8), and if it is still a token ref at the cap, WarnUnknownKey so the failure is at least diagnosed instead of silent. Apply the same to ResolveThemeToken.

### IW-4 — UseShortcut fires OnTrigger on every key auto-repeat (no IsRepeat guard)
`Plugins/ReactiveUI/Source/ReactiveUISlate/Private/RuiInput.cpp:42`  ·  **LOW**  ·  CONFIRMED  ·  WDG (interactive widgets re-sweep (post-fix))

**What's wrong.** FRuiShortcutProcessor::HandleKeyDownEvent (lines 42-50) fires Box->Current() whenever Chord.Matches(Event), without checking Event.IsRepeat(). Slate delivers auto-repeat KeyDown events while a key is held, so the callback fires repeatedly for a single physical press. The public contract (RuiInput.h lines 3-6, 35) describes firing 'when Chord is pressed' — a single logical press — not once per OS key-repeat.

**Failure scenario.** A component registers UseShortcut(Ctrl+S, save). The user holds Ctrl+S for ~1s; Slate emits ~20-30 repeat KeyDown events, each matching the chord, so 'save' (or any non-idempotent action such as an increment / undo step) runs dozens of times for one intended activation.

**Why the suite misses it.** Headless tests synthesize a single FKeyEvent with IsRepeat()==false and assert exactly one invocation; they do not synthesize the repeat-flagged events that occur only on a held key, so the multi-fire path is never exercised.

**Suggested fix.** Guard with `if (!Event.IsRepeat() && Chord.Matches(Event) && Box->Current)` so the trigger fires once per press; make repeat opt-in via a chord flag if any use case needs it.

---

## Cross-cutting themes (root causes worth fixing once)

1. **The `FRuiValue` kind-vs-destination mismatch is systemic, not a one-off.** Round 1 found it in
   the UMG prop-map (B13); round 2 finds the *same shape* in the Slate slot readers — `SlotIntOf`,
   `SlotFillOf`, Overlay `ZOrderOf`, and common-slot padding all read `Int?IntValue:FloatValue` while
   codegen delivers **every literal `Slot.*` as a `String`** (SLOT-1, SLOT-2). The invariant to enforce:
   *no consumer may read a typed `FRuiValue` member without consulting `V.Kind`* — ideally a single
   `AsInt/AsFloat/AsMargin` helper that parses the String form, used everywhere.

2. **The import/export staleness graph has two mirror bugs that both defeat the M8 "catch a changed
   dep" feature.** Creating a previously-missing imported file never re-stales its importer because the
   unresolved-import `dep_hash` key can never match the reader's key (IMPORT-1 / DRV-2), and the
   `-check` drift gate and the codemod gate each miss a category the real sweep catches
   (CHECK-1/DRV-5 miss value-import cycles; DRV-1/MIGRATE-1 miss cross-file duplicate exports). The
   incremental resolver and the full sweep need a **shared, tested key-derivation and gate set** so the
   fast paths can never diverge from the authoritative one.

3. **Every text-region transform is string/comment-blind.** The codegen re-indenter injects tabs into
   raw-string bodies (CG-2); both formatters delete an import's trailing comment (FMT-1 / LSE:LSP-3);
   the scanner mis-reads a comment inside an import brace list (SCAN-2) and a `<`/`>` inside a param
   default (SCAN-1); the event-payload brace-walk ignores string literals (LSE:LSP-4). The family
   already has `FUetkxLexer::SkipNoncode` — the fix is to route **every** region walk through it.

4. **The C++ engine resolver and the TypeScript LSP resolver are two implementations that can
   disagree.** The LSP scans the whole `.uproject` tree while the compiler sweeps only `Source/` +
   `Plugins/` (LSP-2), uses a full body-scan where the engine uses a preamble-only scan (LSW:LSP-1),
   and its exporter-index tie-break is non-deterministic (RESOLVE-1). Any divergence shows up as "the
   IDE says it resolves but the build says it doesn't" (or vice-versa). These need a conformance test
   pinning the two resolvers to the same answer on a shared fixture set.

5. **Slate offset/encoding axes.** Round 1 found one code-point↔UTF-16 miss (B14); round 2 finds the
   clangd frame decoder slicing a UTF-16 string by a UTF-8 byte length (LSE:LSP-1) and a hover/def range
   returned in virtual-doc coordinates (LSP-2/LSP-5). Encoding-axis discipline (bytes vs UTF-16 vs
   code-points) needs to be a reviewed invariant at every process/document boundary.

6. **Controlled-widget semantics — still the gift that keeps giving.** Round 1's cluster (B6/B8/B9)
   is joined by ComboBox firing a phantom `OnSelectionChanged(-1)` when an option is removed (IW-3) and
   the residue of the B6 fix leaking an engine-originated prune as a user event (B6-1). The round-1
   recommendation stands and is now overdue: **one shared `FRuiControlledGuard` + a documented rule**
   (programmatic sets never fire the user event; the widget always repaints), applied uniformly.

7. **Slot-prop mutation reorders or ignores children depending on the panel.** GridPanel/UniformGrid
   never re-place a child on a `Slot.Column/Row` change (GRID-1); WrapBox/ScrollBox append-on-mutate,
   jumping a non-last child to the end (WRAP-1). The in-place `ConfigureSlotLive` the box panels use is
   the correct model; the others need it (or an index-preserving re-insert).

8. **CI does not actually guard the generated code.** The one drift gate (`RUICompile -check`) and the
   entire automation suite are commented out in `engine-tests.yml` (BUILD-2), and the family-corpus hash
   sorts on the un-normalized case name (BUILD-1) — so the 25 committed `*.uetkx.inl`/`*.gen.cpp` files
   have **no gate**, armed or not. This is the meta-risk: several findings above (CG-2, CG-3, SLOT-*)
   would surface immediately if the drift gate and suites actually ran in CI.

## Suggested priority

1. **CG-1** (licensing/marketing contradiction — every customer-generated file stamps the seller's
   copyright; a claimed-delivered D-32 decision is absent) and **SLOT-1** (literal grid `Slot.Column/Row`
   silently collapse to 0 → overlapping layout).
2. **BUILD-2** (turn the drift gate + suites back on — it is the guard that would catch much of the rest)
   and the **`FRuiValue` kind-blind cluster** (SLOT-2, and audit every typed-member read).
3. The **import/export staleness + gate-divergence cluster** (IMPORT-1/DRV-2, CHECK-1, DRV-1/MIGRATE-1)
   — these break the branch's headline feature (M8 "catch a changed dep", strict `-check`).
4. The **region-transform string-blindness cluster** (CG-2, FMT-1/LSP-3, SCAN-1, SCAN-2, LSP-4) via one
   `SkipNoncode`-routed pass, and the **controlled-widget cluster** (IW-3, B6-1) via `FRuiControlledGuard`.
5. **HMR-1/HMR-2** (dev-loop state-migration correctness), the **slot-mutation panels** (GRID-1, WRAP-1,
   POOL-1), **router** (RECON-1/RECON-2), **LSP embedded** (LSP-1/2/5), **style** (STYLE-1/2).
6. The **LOW / PLAUSIBLE** tail (docs drift, edge-case parsing, non-determinism) — cheap, and several
   are one-liners.

## Method notes & honest caveats

- **0 dually-refuted is a permissive result, not a perfect one.** The survival bar was "at least one
  lens did not refute." I compensated by re-reading the source for the top findings myself (marked ✔)
  and by adjudicating severities down where a lens over-rated impact. The `PLAUSIBLE` items are exactly
  the ones where the two lenses disagreed — treat them as leads, not verdicts.
- **A few are dev-loop- or IDE-only** (HMR-1/2 never affect a shipped build; the LSP findings degrade
  editor intelligence, not the runtime). Severity reflects that.
- **Duplicates were merged** (DRV-5 folded into CHECK-1) and siblings cross-referenced (↔) rather than
  double-counted.
- Full per-agent reasoning (both lenses per finding) is in the workflow journal:
  `subagents/workflows/wf_841c2f8f-cb6/journal.jsonl`.
