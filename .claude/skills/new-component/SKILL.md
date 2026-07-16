---
name: new-component
description: Author a .uetkx component for ReactiveUI-Unreal — file placement, declaration kinds and companion-file layout, props/hooks/events/directives/styling syntax, what the compiler generates, and the done-checklist. Use when writing or reviewing .uetkx markup. (Toolchain lands in Phase 3 — until then this is the authoring contract.)
---

# Writing a ReactiveUI-Unreal component (.uetkx)

The grammar is **byte-compatible with the family's** `.guitkx` (Godot) / `.uitkx` (Unity) —
pinned by the shared contract corpus (`grammar-contract` skill). Only the embedded language is
C++ here.

## Where files go (D-19)

- `.uetkx` files live under a C++ **module's Source dir** (house convention: `Private/`,
  e.g. `Source/RuiDemo/Private/UI/ScoreCard.uetkx`) — never under `Content/` (nothing compiles
  there; the watcher warns).
- Saving compiles a **sibling committed `Foo.uetkx.inl`** (reflection-free C++ builder calls) —
  never write or edit it; the module's `<Module>.Uetkx.gen.cpp` aggregator includes it. The
  machine-local `Foo.uetkx.diags.json` sidecar is gitignored.
- **One `component` per file** (convention; >1 is a lint), named after it; sub-components as
  sibling files; shared custom hooks in `hook` companion files (`score_card.hooks.uetkx`
  convention); `module` declarations group shared hooks/components (the family's companion-file
  layout — D-03). A file MAY hold a free sequence of these declarations.
- **A preamble holds imports ONLY** (before the first declaration; INCLUDE_RETIREMENT_PLAN.md):
  `import { A, B } from "./x"` (relative) or `from "~/Screens/X"` (`~/` = the module root, or the
  config `root`), extensionless, named-only (no `*`, no default) — cross-file names MUST be
  imported this way, the compiler errors otherwise (UETKX2305). **`export`** a
  `component`/`hook`/`module` to make it importable from other files; a non-exported declaration
  is file-private (tree-shaken).
- **You almost never write an `#include`.** Every generated file already carries the library's
  own headers (`RuiContext.h`, `RuiCoreElements.h`, `RuiRouter.h`, the UMG/CommonUI/MVVM interop
  headers when that plugin is linked, `UObject/StrongObjectPtr.h`, `Engine/World.h`, …) via the
  aggregator's auto-included prelude — see `FUetkxFileScan::AutoIncludedHeaders()`
  (`UetkxFileScan.cpp`) for the exact list. For **your own** project header, use the host-include
  import form: `import "@MyTypes.h"` compiles to `#include "MyTypes.h"`. It is nameless by
  design — the C++ compiler resolves the header's symbols, so there is nothing for the toolchain
  to name-check. A raw `#include "MyTypes.h"` line still works (legacy spelling, never removed),
  but naming an auto-included header either way is redundant (UETKX2317 hint).
- Migrating an existing tree? `<Engine>\UnrealEditor-Cmd <proj>.uproject -run=RUIMigrateImports`
  adds every `export` + the imports each file needs (idempotent), then `-run=RUICompile -check`
  must be clean. Add `-tidy` to ALSO rewrite every preamble to imports-only (drops auto-included
  headers, converts surviving `#include` lines to `import "@…"` — surgical: it only ever touches a
  construct alone on its own line, never a line carrying a comment too).

## Anatomy (family grammar, C++ embedded)

```
component ScoreCard(Title: FText, MaxScore: int32 = 100) {
	auto [Score, SetScore] = UseState(0);
	return (
		<VerticalBox>
			<TextBlock Text={ FText::Format(INVTEXT("{0}: {1} / {2}"), Title, Score, MaxScore) } />
			<HorizontalBox>
				<Button OnClicked={ SetScore(Score + 10) }>+10</Button>
				<Button OnClicked={ SetScore(0) }>Reset</Button>
			</HorizontalBox>
			{ children }
		</VerticalBox>
	)
}
```

Rules that matter:
- **Props** = the parameter list, typed with defaults; pass as attributes (`<ScoreCard Title=… />`).
- **Hooks**: `UseState/UseEffect/UseMemo/…` (all 23; PascalCase — this is C++). Rules of hooks
  apply: top level only, stable order, never inside `@if`/loops. Changing the hook SHAPE resets
  that component's state on hot reload (deliberate; the status line reports it).
- **Events**: the Unreal delegate name VERBATIM (`OnClicked`, `OnTextChanged`,
  `OnCheckStateChanged` — D-33 in MASTER_PLAN; no React aliases). Same rule as every other
  name: element tags = Slate class minus `S`, props = the setter/property name minus `Set`.
  Handlers are inline `{ expr }` bodies; the payload parameter is `Value` (an `FRuiValue` —
  `Value.TextValue`/`Value.BoolValue`/`Value.FloatValue` per event). Handler BODIES are compiled
  C++ — they ride the same Live-Coding patch as the markup (HMR v2). ~~`logic={UClass}` named
  handlers~~ — NOT implemented (audit 2026-07-14); do not author or document it.
- **Directives**: `@if/@elif/@else`, `@for` (the header is **verbatim C++** — classic or ranged
  for; + `key` on loop children), `@while`, `@match/@case/@default`. A directive block is a
  statement block — markup exits via `return ( … );` (corpus-pinned). There are NO `@uss`/`@theme`
  markup directives — themes/stylesheets are the C++ `RUI::Slate::LoadStylesheet`/`RegisterTheme`
  layer (audit 2026-07-14).
- **Styling**: element attrs + generic style keys use the exact Unreal names (`Padding="12"`,
  `Font.Size={ 16.0f }`, `ColorAndOpacity={ … }` — PascalCase, never snake_case); `classes` for
  the registered-class merge layer (cascade: theme < classes < inline). Style/event/ref props
  reset when removed; plain props don't (family semantic).
- **Structural primitives** (Portal/Suspense/ErrorBoundary/Memo) are `RUI::` calls inside
  `{expr}`, NOT tags (family convention).
- Formatter default: tabs (`uetkx.config.json` walk-up overrides; C++ doesn't require tabs —
  it's a style default).

## Done-checklist

1. Saved → sibling `.inl` regenerated, no `UETKX####` diagnostics (Message Log "ReactiveUI" clean).
2. Renders where used (demo mount or another component).
3. Reusable API surface → doc comment on the component, sensible prop defaults.
4. Keyed lists have stable `key`s; formatting passes.
5. The `.inl` is committed WITH the `.uetkx` (CI's `RUICompile -check` fails on drift).

## Scar tissue (why these steps exist)

- **Commit the `.inl` with the source** — a source-only commit green-built locally (stale
  sibling on disk) and broke CI's drift gate; they travel together, always.
- **Hooks at top level only**: conditional hooks scrambled positional state in the family long
  before Unreal — the editor diagnoses it; don't fight the diagnostic.
- **`Content/`-placed markup compiles never** — the watcher's warning exists because someone
  will try; the module-Source rule is what makes the aggregator/UBT story work (D-19).
