# Changelog

## [0.6.0] - 2026-07-18
- Workspace language features land: find-all-references (tags, code references, import/export tokens — close tags included), rename with the family's ES semantics (renaming a declaration updates every importer including `A as B` target spellings; renaming a local alias stays in the importer; renaming a plain imported binding inserts `A as NewName` so the export is untouched — invalid names, collisions and host-tag shadowing refuse with a reason), document + workspace symbols, and four quickfixes: add the missing import (UETKX2305), remove the unused import (UETKX2304), add `export` in the TARGET file (UETKX2301), and migrate a deprecated wrapper to the plain declaration form (UETKX2320 — byte-identical to the codemod's rewrite). Reference analysis is scope-aware: a local variable shadowing an exported name is never miscounted as a symbol reference.
- Embedded-C++ locals rename too: a cursor on a body local forwards the rename to clangd over the virtual document and translates the edits back into your .uetkx — all-or-nothing by design (an engine-header symbol, an occurrence in generated glue, or a block whose C++ has unresolved errors refuses with a reason instead of applying a partial rename). Works without a compile database for UE's numeric types; engine types need one (the refusal says so).

## [0.6.0] - 2026-07-17
- ES modules: the .uetkx language server understands the whole new grammar — signature-classified plain declarations (components/hooks/values/utils), rename (`A as B`), namespace (`* as X`) and default imports, `export { … };` lists and `export default`. Completions, hover and go-to-definition resolve THROUGH aliases; embedded-C++ intelligence types imported values/utils and your own value initializers/util bodies; the new UETKX2320–2327 diagnostics surface live, and the deprecated wrapper keywords warn with the codemod hint.
- Compiler diagnostics are back in the editor: the sidecar reader silently dropped EVERY compiler-written diagnostics file after the driver moved to schema v3 (a strict version check) — only live-parse diagnostics showed. The gate now accepts v2+, so full compiler errors/warnings flow into the IDE again.
- Typed event-payload completion (`Value.` → the enclosing event's field first) now wins over the embedded-C++ forward inside event expressions — clangd's generic member list (or, with no compile database, its identifier fallback) previously preempted the payload-first ordering.

## [0.5.0] - 2026-07-17
- Crash-proof language server: process-level traps, a guarded clangd message pump, and a URI re-serialization fallback (clangd's percent-encoded drive letters silently dropped every diagnostic) — a single bad message or background rejection can no longer take the server down.
- Embedded C++ intelligence, root-caused clean: every markup expression (attr values, event handlers, expr children, directive headers/bodies) now lifts into the virtual TU with byte-exact source mapping — completion, hover, and diagnostics work inside ALL embedded C++, not just setup. The clangd channel was rebuilt end-to-end: the UBT compile database is sanitized (quote-aware .rsp expansion, MSVC STL + Windows SDK include derivation, SharedPCH force-include dropped, -fmsc-version pinned to the toolchain), clangd is discovered across PATH/LLVM/VS/managed installs, and the demo tree sweeps with ZERO false positives.
- Markup everywhere (plugin 0.11.0 parity): markup-as-value scans, colors, and analyzes like any body markup — UETKX0114 narrowed to the paren-less markup return, the family rules-of-hooks (UETKX0013-0016) land in the scanner, hook signatures ignore markup text (HMR protection), and the virtual doc neutralizes value markup to a typed placeholder so clangd keeps full intelligence around (and inside) it. The family scanner corpus is reconciled across all three sibling repos (shared hash 917dd8cd).
- clangd 22.1.6 ships INSIDE the artifacts: the VS Code extension publishes a win32-x64 flavor with clangd bundled (embedded C++ intelligence with zero machine setup) plus a universal flavor that falls back to discovery; the VS2022 VSIX bundles clangd.exe directly. The bundled binary wins over machine discovery; an explicit uetkx.clangd.path still overrides. LLVM license ships beside the binary.

## [0.4.2] - 2026-07-16
- Embedded-C++ intelligence now sees the full auto-included prelude: the virtual document's header list gains `CoreMinimal.h`, `RuiSignal.h`, `RuiSlateElements.h`, and `RuiStyle.h`, matching the compiler's 18-header list exactly — clangd-backed completion/hover inside setup bodies now resolves Slate element factories, signals, and style types.

## [0.4.1] - 2026-07-16
- The marketplace description now credits the family Discord (discord.gg/Knedqu4Wyv) and the GitHub repo, kept under 280 characters for both listings.

## [0.4.0] - 2026-07-16
- `.uetkx` preambles are imports-only now: an `import "@Header.h"` host-include form (family shape, ported from Unity's `import "@Namespace"`; our payload is a header path) compiles to `#include "Header.h"` — nameless by design, since the C++ compiler resolves the header's symbols. A raw `#include` line still works too, but naming a header the generated file already auto-includes now gets a redundancy hint (`UETKX2317`). Syntax highlighting, live diagnostics, and a discoverable `import "@"` completion snippet ship for the new form.

## [0.3.1] - 2026-07-16
- Marketplace listing overhaul: distinguishable display names — `UETKX (Unreal - VS Code)` / `UETKX (Unreal - VS2022)` — and a structured page body (Title / Description / Features / Requirements / Changelog) on both marketplaces.
- The VS Code marketplace page now has a body at all — README.md is generated from the centralized changelog (it was missing entirely).

## [0.3.0] - 2026-07-16
- Schema catch-up to plugin 0.9.0: the bundled .uetkx vocabulary grows 38 → 63 tags (the widget-completion campaign — ConstraintCanvas, Splitter, MenuAnchor, NotificationList, color pickers, input boxes, and 20+ more), 16 style keys, 16 slot keys.
- First engine-gated tag: SearchableComboBox carries sinceUE: 5.7 in the schema — completions/hover know it does not exist on UE 5.6.
- Grammar wave G: early component-level markup returns are accepted (multi-return collector, byte-identical to the C++ compiler per the grammar contract); the final return must be top-level (new UETKX3007); short-circuit &&/|| markup is no longer a diagnostic (UETKX3002 retired). Scanner corpus +4 cases.

## [0.2.0] - 2026-07-15
- Embedded C++ completion: inside a component setup / hook / module body, completion now forwards to clangd over the virtual document (locals, engine symbols, members) — hover, go-to-definition, and completion are all clangd-backed there. Replace ranges map back to .uetkx coordinates; without clangd on PATH everything degrades to the markup baseline as before.

## [0.1.2] - 2026-07-14
- Refreshed the ReactiveUI brand icon.

## [0.1.1] - 2026-07-13
- Added the ReactiveUI brand icon to the marketplace listing.

## [0.1.0] - 2026-07-13
- First beta — .uetkx language support for VS Code and Visual Studio 2022: schema-driven tag/attribute/event completions and hover, live parse diagnostics, import intelligence (specifier + name completions, go-to-definition, and strict resolution diagnostics), golden-corpus formatting, and embedded-C++ awareness. Visual Studio 2022 adds a Tools > Options Format-on-Save setting.
