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
- **One `component` per file**, named after it; sub-components as sibling files; shared custom
  hooks in `hook` companion files (`score_card.hooks.uetkx` convention); `module` declarations
  group shared hooks/components (the family's companion-file layout — D-03).

## Anatomy (family grammar, C++ embedded)

```
component ScoreCard(Title: FText, MaxScore: int32 = 100) {
	auto [Score, SetScore] = UseState(0);
	return (
		<VBox style={ {"separation": 8} }>
			<Text text={ FText::Format(INVTEXT("{0}: {1} / {2}"), Title, Score, MaxScore) } />
			<HBox>
				<Button text="+10" onClick={ SetScore(Score + 10) } />
				<Button text="Reset" onClick={ SetScore(0) } />
			</HBox>
			{ children }
		</VBox>
	)
}
```

Rules that matter:
- **Props** = the parameter list, typed with defaults; pass as attributes (`<ScoreCard Title=… />`).
- **Hooks**: `UseState/UseEffect/UseMemo/…` (all 23; PascalCase — this is C++). Rules of hooks
  apply: top level only, stable order, never inside `@if`/loops. Changing the hook SHAPE resets
  that component's state on hot reload (deliberate; the status line reports it).
- **Events**: React aliases (`onClick`, `onChange`, …) or `on_<DelegateName>` escape hatch.
  Handlers: setter calls / whitelisted expressions inline, or a **named function** on the
  component's logic class — `logic={UShopLogic}` (a plain attribute; may be a Blueprint class),
  handler referenced by name. Handler BODIES are compiled C++ — new bodies need a compile;
  structure/style/expressions hot-reload without one (D-20).
- **Directives**: `@if/@elif/@else`, `@for x in xs` (+ `key` on loop children), `@while`
  (interpreter caps iterations), `@match/@case/@default`. `@uss`/`@theme` parse but diagnose
  "stylesheet layer is post-v1".
- **Styling**: `style={ {"font_size": 24, …} }` dicts over the style-key registry; `classes` for
  the merge layer. Style/event/ref props reset when removed; plain props don't (family semantic).
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
