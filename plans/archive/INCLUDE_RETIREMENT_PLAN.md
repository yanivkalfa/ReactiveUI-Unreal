# Include retirement — `.uetkx` preambles become imports-only (owner directive 2026-07-16)

> **The ask:** `.uetkx` files today open with raw C++ `#include` lines
> (`MvvmDemo.uetkx` has three). The owner wants them GONE: common headers auto-provided,
> and anything that must stay spelled in the IMPORT grammar —
> `import "@UObject/StrongObjectPtr.h"` → compiled to `#include "UObject/StrongObjectPtr.h"`.
> End state: a `.uetkx` preamble holds ONLY import lines.
>
> **The family precedent (researched 2026-07-16 — this is a PORT, not an invention):** Unity
> shipped exactly this. Their generator auto-injects standard usings into every file
> (`UnityEngine`, `System`, `ReactiveUITK.Router`); their unified import grammar has
> `import "@Namespace"` — a quoted, brace-less, `@`-prefixed side-effect specifier that emits
> a host-language `using` (payloads like `"@static UnityEngine.Mathf"` pass through); their
> UITKX2316 flags an unresolvable namespace, UITKX2317 hints a redundant (auto-injected) one;
> their codemod gained `--tidy` (convert `@using` → `import "@…"` + strip redundant); their
> formatter round-trips both spellings. Our leg maps the payload to a HEADER PATH instead of
> a namespace — per-leg payload, family-shared shape. (Godot has no equivalent need —
> GDScript resolves globally — and has NOT shipped the `@` form; see §E corpus placement.)
>
> **The two facts that make this cheap:**
> 1. Every `.uetkx` in a module compiles in ONE translation unit — the .inls are `#include`d
>    into `<Module>.Uetkx.gen.cpp`, which ALREADY emits a fixed prelude
>    (`UetkxDriver.cpp:551`: CoreMinimal/RuiContext/RuiCoreElements/RuiSignal/
>    RuiSlateElements/RuiStyle — why `HelloWorld.uetkx` needs zero includes today). Per-file
>    includes are already module-global in effect; extending the prelude costs nothing.
> 2. The LSP virtual doc (`virtualDoc.ts`) NEVER consumed the `#include` lines — clangd
>    intelligence doesn't depend on them, so nothing breaks there.
>
> **Execution notes for the implementing model:** one branch `feat/include-retirement` off
> `dev`. Follow `dev-process` (gate ladder; never weaken a gate), `grammar-contract`
> (Phase B is lexer/scan work — corpus cases land IN THE SAME COMMIT, both impls), and
> `release-process` for §F. Phases A→F in order; A is shippable alone if B stalls — say so
> in the PR rather than rushing B.

## §0 Current state (verified 2026-07-16, all line numbers current at plan time)

**Where `#include` flows today:**

| Layer | Location | Behavior |
|---|---|---|
| C++ preamble scan | `Plugins/ReactiveUI/Source/ReactiveUIInterp/Private/UetkxFileScan.cpp:783-789` (inside `Scan`'s preamble loop) | any noncode line starting `#include` → verbatim into `FUetkxFileScanResult::PreambleIncludes` |
| TS preamble scan | `ide-extensions/lsp-server/src/uetkxFileScan.ts:657` | same, into `preambleIncludes` |
| Codegen emission | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp:1582-1585` | each line emitted verbatim at the top of the generated `.inl` (outside the DECL/BODY phase guard — emitted text is include-guarded headers, safe to hit twice) |
| Aggregator prelude | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp:551-556` | the fixed 6-header prelude, before any `.inl` |
| C++ formatter | `Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxFormatter.cpp:860-863` | re-emits the `#include` block, then a blank line, then the import block |
| TS formatter | `ide-extensions/lsp-server/src/formatUetkx.ts:100` | same |
| LSP virtual doc | `ide-extensions/lsp-server/src/virtualDoc.ts:26-32` | fixed PRELUDE (`RuiContext.h`, `RuiCoreElements.h`, `using namespace RUI;`) — preamble includes IGNORED |
| Contract fixture | `Source/RuiHostTests/ContractFixtures/Preamble.uetkx` | pins `#include "CoreMinimal.h"` passthrough |
| Docs | `ReactiveUIUnrealDocs~/src/pages/Reference/LanguageReferencePage.tsx` `PREAMBLE` sample (`#include "MyTypes.h"` line); `Uetkx/ImportsPage.tsx` (import grammar page) |
| Skill | `.claude/skills/new-component/SKILL.md` (authoring contract — check for preamble/include prose and update) |
| HMR / export-hash | NO impact: HMR v2 is whole-project Live-Coding recompile through the same gen.cpp; `FUetkxResolve::ExportHash` hashes decls only (`UetkxResolve.cpp:127-134`) |

**Include inventory across all 21 `.uetkx` files that have any** (grep `^#include` under `Source/`):

- Library headers (closed set, 6): `RuiRouter.h` (ReactiveUICore/Public — always available),
  `RuiAssetBrush.h` (ReactiveUISlate/Public), `RuiFieldHooks.h` + `RuiUmgElement.h` +
  `RuiSignalViewModel.h` (ReactiveUIUMG/Public), `RuiActivation.h` (ReactiveUICommonUI/Public).
- Engine headers: `UObject/StrongObjectPtr.h` (CoreUObject), `Engine/World.h` (Engine),
  `CoreMinimal.h` (already in the aggregator prelude — pure redundancy).
- User/project headers (unbounded): `RuiDemoSupport.h`, `DemoInteropWidgets.h`,
  `DemoUmgWidget.h`, `Doom/DoomTypes.h`, `Doom/DoomMaps.h`, `Doom/DoomGameHook.h`,
  `Doom/DoomTextures.h`.

## §A Tier 1 — extend the auto-prelude (kills library + engine includes; NO grammar change)

1. **`UetkxDriver.cpp:551-556`** — after the existing 6 lines, append (exact text):
   ```cpp
   Contents += TEXT("#include \"RuiRouter.h\"\n");
   Contents += TEXT("#include \"RuiAssetBrush.h\"\n");
   Contents += TEXT("// Optional-module + engine headers: guarded so a consuming module that\n");
   Contents += TEXT("// does not depend on the interop plugins (or Engine/CoreUObject) still compiles.\n");
   Contents += TEXT("#if __has_include(\"RuiFieldHooks.h\")\n#include \"RuiFieldHooks.h\"\n#endif\n");
   Contents += TEXT("#if __has_include(\"RuiUmgElement.h\")\n#include \"RuiUmgElement.h\"\n#endif\n");
   Contents += TEXT("#if __has_include(\"RuiSignalViewModel.h\")\n#include \"RuiSignalViewModel.h\"\n#endif\n");
   Contents += TEXT("#if __has_include(\"RuiHostWidget.h\")\n#include \"RuiHostWidget.h\"\n#endif\n");
   Contents += TEXT("#if __has_include(\"RuiWorldSubsystem.h\")\n#include \"RuiWorldSubsystem.h\"\n#endif\n");
   Contents += TEXT("#if __has_include(\"RuiActivation.h\")\n#include \"RuiActivation.h\"\n#endif\n");
   Contents += TEXT("#if __has_include(\"RuiActivatableScreen.h\")\n#include \"RuiActivatableScreen.h\"\n#endif\n");
   Contents += TEXT("#if __has_include(\"RuiMvvmViewModel.h\")\n#include \"RuiMvvmViewModel.h\"\n#endif\n");
   Contents += TEXT("#if __has_include(\"UObject/StrongObjectPtr.h\")\n#include \"UObject/StrongObjectPtr.h\"\n#endif\n");
   Contents += TEXT("#if __has_include(\"Engine/World.h\")\n#include \"Engine/World.h\"\n#endif\n");
   ```
   (`__has_include` is C++17, supported by MSVC/clang and used inside UE itself. Do NOT guard
   RuiRouter/RuiAssetBrush — Core and Slate are hard deps of any markup-consuming module.)
2. **`virtualDoc.ts` PRELUDE** — mirror: add the same header list (the guarded ones WITHOUT
   guards — clangd's virtual TU only needs the declarations it can find; missing headers in a
   non-interop workspace degrade exactly as today).
3. **The prelude list is now a CONTRACT** — add a comment block above the Driver list naming
   this plan and the rule: "a header added here must also be added to virtualDoc.ts PRELUDE
   and the docs' auto-included table (§D)". Same comment on the TS side pointing back.
4. Verify (engine gates): `Build.bat` → `RUICompile -full` → `Build.bat` again (the regenerated
   gen.cpp must compile) → full battery per the `test-run` skill. Nothing else changes yet.

## §B Tier 2 — the `import "@Header.h"` form (grammar work — grammar-contract skill applies)

**Spec (mirror Unity's shape exactly; per-leg payload):**
- Syntax: `import "@<payload>"` — the token after `import` is a STRING LITERAL (no braces,
  no `from`) whose first character is `@`. Payload = everything after the `@`, a header path.
- Emission: `#include "<payload>"` in the generated `.inl` (verbatim, same emission point as
  PreambleIncludes today). Quoted form only — `<angle>` includes are a NON-GOAL (write
  `import "@vector"` is invalid; nobody includes system headers from markup; document it).
- A string not starting with `@` in that position is an error (UETKX0303 shape: "import
  expects `{ Name, ... }` or a `\"@Header.h\"` host include").
- Duplicate same-payload `@` imports: second one diagnosed UETKX2303-style
  ("duplicate host include `X.h`", severity 0, same code — it's the same books-stay-honest rule).
- `@` imports carry NO names → the resolver's name machinery (UETKX2300/2301/2304 unused-import
  at `UetkxResolve.cpp:519`) must SKIP them entirely.
- **UETKX2317** (family number — Unity's redundant-using hint): severity 2 hint when a `#include`
  line OR `@` import names a header on the §A prelude list (keep the list in ONE shared place the
  scan can read — a static array in `UetkxFileScan.cpp` + exported constant in the TS scan; the
  Driver and virtualDoc preludes reference it in comments). Message:
  `` `X.h` is auto-included by the generated prelude — this line is redundant ``.
- **UETKX2316** (Unity's unresolvable-namespace error) is **POST-V1 / NOT in this plan**: we
  cannot see UBT include paths, so header-existence checking would be guesswork; clangd and the
  C++ compile surface a wrong path immediately anyway. Record the reservation in the
  Diagnostics docs table as "reserved".

**Data model:** add to `FUetkxImportDecl` (`UetkxFileScan.h:48`) / `UetkxImportDecl`
(`uetkxFileScan.ts:40`): `bool bHostInclude = false;` (TS: `hostInclude: boolean`). For host
includes: `Specifier` = the payload WITHOUT the `@`, `Names`/`NameAts` empty, `SpecifierAt`/`At`
as usual. Every existing consumer that iterates `Imports` must be audited for the empty-Names
case — the known list:
- `UetkxResolve.cpp` — resolution + 2300/2301/2304: skip when `bHostInclude`.
- The aggregator topo-sort (`UetkxDriver.cpp`, "Order is SOURCE-TRUTH (A2)" block): host
  includes resolve to NO file — skip.
- `ScanPreamble` (`UetkxFileScan.cpp:926-940` reuses ParseImport): must parse the form without
  choking; host includes don't contribute to export tables/hashes.
- Formatters (both): emit host imports in the import block, authored order preserved,
  spelled `import "@<payload>"`.
- LSP import intelligence (specifier/name completion, go-to-definition in
  `server.ts`/`uetkxWorkspace.ts`): a specifier starting `@` gets NO file resolution, NO
  diagnostics from the resolver path, NO go-to-def (return null). Offer one completion
  snippet `import "@"` in the preamble (mirror Unity's).

**Parse implementation:** in `ParseImport` (C++ `UetkxFileScan.cpp:183`; TS `parseImport`),
after skipping ws past the `import` keyword: if the next char is `"` or `'`, read the string;
if its first char is `@` → host-include path (fill decl, `bHostInclude`, dup-check by payload,
return); else the UETKX0303 error above. The existing `{`-expecting path is untouched.

**Codegen:** `UetkxCodegen.cpp:1582` — after the PreambleIncludes loop, emit
`#include "<payload>"` for each `bHostInclude` import (same spot; keep raw-`#include` output
byte-identical so Tier-1-untouched fixtures don't churn). The UETKX2317 hint surfaces via the
normal diags sidecar path — it must NOT fail the compile (severity 2).

**Syntax highlighting:** `ide-extensions/vscode-uetkx/syntaxes/uetkx.tmLanguage.json:32` —
the `import-decl` match requires `from`; add a second pattern for
`\b(import)\b\s*("@[^"]*")` (keyword + string scopes, same colors as file imports — Unity
deliberately colors them identically). The VS2022 extension consumes the same grammar file
via its `Syntaxes/` copy — check `ide-extensions/visual-studio/UetkxVsix/Syntaxes/` and sync
if it's a physical copy.

**Corpus + fixtures (SAME COMMIT as the scan change — grammar-contract law):**
- `ide-extensions/lsp-server/test-fixtures/uetkx-scanner-cases.json`, section **`fileScanLeg`**
  (NOT family-core: Godot hasn't shipped the form, and family-core sections must replay
  byte-identically in all three repos): cases for (1) accepted host import + file import mix →
  components parse, noError; (2) `import "NotAt"` string without `@` → diag UETKX0303;
  (3) duplicate `@` payload → UETKX2303; (4) prelude-covered `@` payload → UETKX2317 (and it is
  severity-2, so `noError: true` still holds). Then `node scripts/corpus-hash.mjs --check`
  (fileScanLeg is per-leg — the family hash must NOT change; if it does you put a case in the
  wrong section).
- Contract fixtures: new `HostImport.uetkx` (host import + file import + a component using the
  included type via a simple `FString`-safe payload — fixtures compile through the pipeline but
  are NOT MSVC-compiled, so the payload can be a fictional header) + regenerate goldens via
  `-run=RUIContractDump`; `Preamble.uetkx` stays UNTOUCHED (pins that raw `#include` still works).
- Compiled proof (the WaveG pattern): `Source/RuiHostTests/GrammarProof/` gains
  `HostImportProof.uetkx` using `import "@Misc/DateTime.h"` (a real engine header not in the
  prelude) + a type from it in setup; the sweep compiles it into RuiHostTests via MSVC. Update
  the acceptance sweep-count pin (`ReactiveUIAcceptanceTest.cpp:77`, currently 40 → 41).
- TS: `contract.test.ts` replays fileScanLeg automatically; extend `server.test.ts` only if it
  pins import behavior counts.

**Cross-repo (grammar-contract §outbound/inbound):** this is INBOUND from Unity — check
`UnityComponents/.../ide-extensions~` for their scanner-corpus cases covering `import "@"` and
mirror the case SHAPES (adapted payload). Track in `plans/TECH_DEBT.md`: one entry noting
(a) Unreal adopted the family `@` form 2026-07-XX, (b) Godot has not — flag to the Godot repo
so the family converges (their payload would be a preload path/class name if they ever need it).

## §C Migration — the demo tree goes imports-only

1. **Codemod:** extend `RUIMigrateImportsCommandlet`
   (`Plugins/ReactiveUI/Source/ReactiveUIEditor/Private/RUIMigrateImportsCommandlet.cpp`) with a
   `-tidy` switch (Unity's `--tidy` analogue): for each swept `.uetkx`, (a) DELETE any
   `#include`/`@` line whose header is on the §A prelude list, (b) convert surviving `#include "X.h"`
   lines to `import "@X.h"` (placed with the imports, file-import block first, authored order
   otherwise preserved). Idempotent — running twice changes nothing. Commandlet must remain
   usable WITHOUT `-tidy` exactly as before.
2. Run `-run=RUIMigrateImports -tidy` over the repo; expected outcome per §0's inventory:
   `MvvmDemo`, all 3 Router files, `ShowcaseProbe`, `ActivationProbe`, and `CustomDraw`-class
   files end up with ZERO includes; the Doom/demo files keep exactly their user headers as
   `import "@Doom/DoomTypes.h"`-style lines; `Preamble` fixture untouched (fixtures are
   excluded from the sweep — verify the ContractFixtures exclusion still holds after).
3. `RUICompile -full` → Build → full battery. The generated `.inl`s for migrated files change
   (includes emitted from `@` imports now) — committed generated code updates ride along.

## §D Docs + skills (docs-sync table: "Grammar / directive change" row)

1. `ImportsPage.tsx` — add the third specifier form to the grammar block + prose:
   file imports (`./`, `~/`) are name-checked; `import "@Header.h"` is a side-effect host
   include (nameless BY DESIGN — the C++ compiler resolves the names, so there is nothing for
   the toolchain to verify); plus the auto-included table (the §A prelude list) and "you only
   ever need `@` imports for YOUR OWN headers".
2. `LanguageReferencePage.tsx` — the `PREAMBLE` sample: replace the `#include "MyTypes.h"` line
   with `import "@MyTypes.h"   // your own C++ header — library headers are auto-included`.
3. `DiagnosticsPage.tsx` — 23xx row gains 2317 (+2316 as "reserved, post-v1"); EXAMPLES table
   gains the 2317 hint row.
4. `.claude/skills/new-component/SKILL.md` — preamble section rewritten: imports only;
   the auto-included list; `@` form for own headers; `#include` marked legacy-but-working.
5. `node scripts/docs-drift.mjs` still green (no counts change).

## §E What does NOT change

- Raw `#include` lines keep parsing and emitting FOREVER-for-now (Unity kept `@using`) —
  deprecation is by docs + the 2317 hint + the codemod, not by removal. No removal diagnostic.
- The import RESOLVER file-format (`.uetkx` peer resolution), export hashes, HMR, the schema,
  widget/docs counts, `Preamble.uetkx` fixture.
- Family-core corpus hash (all new cases are fileScanLeg).

## §F Release mechanics (release-process skill)

- **Plugin** → **0.10.0** (additive minor: new grammar form + prelude). Lane A `## [0.10.0]`
  section (Added: auto-included prelude with the header list; `import "@Header.h"`; UETKX2317;
  `-tidy` codemod. Changed: demo tree migrated imports-only) + `cp` mirror + verify-mirror.
- **Extensions** → **0.4.0** both (lsp-server scan/formatter changed → D-30 lockstep; additive
  feature = minor). Lane B bullets: (1) shared — the `@` host-include import form parsed/
  formatted/highlighted + 2317 hint; (2) shared — preamble includes now understood as legacy.
  Extract both CHANGELOGs + regenerate `vscode-uetkx/README.md` (it embeds the changelog —
  `extract-overview`; `changelog.mjs verify` gates it). Lockfiles sync (`npm install
  --package-lock-only` in lsp-server + vscode-uetkx).
- Discord (Lane C): one entry — "`.uetkx` preambles are imports-only now" with the before/after
  of MvvmDemo's three lines.
- PENDING_CHANGELOG: stage → drain → EMPTY per release-process §0.
- Gates before push (ALL green): clang-format over touched C++ · Build ·
  `RUICompile -check` (0 drift) · `RUIContractDump -check` · full battery (expect +1 test file
  count movements ONLY where §B/§C pinned them) · lsp-server `npm test` ·
  `changelog.mjs verify` · verify-mirror · check-headers · lint-skills · docs-drift ·
  `corpus-hash --check` · docs site `npm run build && npm run lint`.
- PR `feat/include-retirement` → `dev`; owner merges, fast-forwards, presses Publish
  (plugin v0.10.0 + both extension 0.4.0 legs fire).

## §G Post-merge verification checklist

- `MvvmDemo.uetkx` line 1 is `// MVVM / FieldNotify — …` (no includes, no imports needed).
- `DoomHUD.uetkx` line 1 is `import "@Doom/DoomTypes.h"`.
- A fresh `RUICompile -full` produces zero gen-file churn (idempotent).
- In the VS Code dev host (rebuild-ide-extensions skill): the `@` import line highlights like
  a file import; a prelude-covered `#include "RuiRouter.h"` shows the faded 2317 hint;
  completion offers the `import "@"` snippet in the preamble.
- Battery green on 5.6 (owner re-runs 5.7/5.8 per engine-catchup cadence).
