# Tech debt register

Deliberate deferrals and known-imperfect machinery — each entry names the root cause, why it's
still open, and what a production-grade resolution looks like. IDs are stable (`TD-###`) and
referenced from plans/PRs.

**Format (one section per item):**

```
## TD-### — <title>
- **Where:** <file/area, file:line when it exists>
- **What/why deferred:** <the decision and its reasoning — cite the MASTER_PLAN D-ref/gate>
- **Production-grade resolution:** <what "done" looks like>
- **Status:** OPEN | RESOLVED <date, how>
```

---

## TD-001 — Router subsystem (17 family router hooks)
- **Where:** would live in `ReactiveUICore` (+ suites `ReactiveUI.Router.*`)
- **What/why deferred:** OUT of the v1 ship gate by decision (MASTER_PLAN D-27 note + gate OUT
  list) — scope valve; the family's `router_match`/`router_spine` suites port when it lands.
- **Production-grade resolution:** full port of the family router (+17 hooks), both suites, docs
  page, gate entry in a v1.x plan.
- **Status:** RESOLVED 2026-07-12 — ported engine-blind into ReactiveUICore (`RuiRouter.h/.cpp`),
  a React-Router-shaped in-memory router. **Matching engine:** `MatchPath` (literal / `:param` /
  `*` splat, leaf-vs-prefix), `ParseLocation`, `ParseSearch`/`BuildSearch`, `ResolvePath`
  (absolute / append-relative / `..`). **Components:** `RUI::Router` (in-memory history w/ back +
  forward stacks + a blocker registry), `RUI::Routes` (specificity-ranked nested matching →
  outlet-wrapped element chain), `RUI::Link`, and Outlet nesting via a per-route context.
  **The 17 hooks:** UseInRouterContext, UseLocation, UsePathname, UseSearch, UseNavigationType,
  UseNavigate, UseGo, UseBackStack, UseParams, UseSearchParams, UseMatch, UseIsActive,
  UseResolvedPath, UseHref, UseOutlet, UseRoutes, UseBlocker. Both family suites ported:
  `ReactiveUI.Router.Match` (the pure primitives) and `ReactiveUI.Router.Spine` (end-to-end:
  history, nested layout+index outlets, params, search-param read/write, back-stack, blocker
  interception). Full suite 67/67. Docs page + a demo screen remain as docs-sync follow-ons.

## TD-002 — Stylesheet third style layer (`@uss`/`@theme`)
- **Where:** grammar recognizes both directives (D-03) and diagnoses "post-v1"
- **What/why deferred:** v1 ships inline `style` dicts + `classes` (D-13); the stylesheet layer
  is family parity work with real design surface.
- **Production-grade resolution:** family-compatible stylesheet layer; the diagnostic retires.
- **Status:** RESOLVED (runtime + loader) 2026-07-12 — the THIRD layer landed in `RuiStyle.h/.cpp`
  on top of the v1 class registry. **@theme tokens:** `RegisterTheme(name, tokens)` +
  `SetActiveTheme`/`GetActiveTheme` + `ResolveThemeToken`; a style value that is a `$name` String
  is a token reference, resolved against the active theme when `BuildEffectiveStyle` runs — so the
  cascade is theme tokens < class rules < inline (each higher layer wins; the token supplies the
  VALUE a class/inline key referenced). **@uss stylesheets:** `LoadStylesheet(source)` parses a
  `.uss`-style text (`@theme <name> { token: value; }` + `.<class> { key: value; }`, `/* */` and
  `//` comments) and registers themes + classes; `ParseStyleValue` is the value grammar
  (`#rrggbb[aa]` color, int/float, true/false, `$token`, "quoted", `x,y` vector2, bare Name) and
  is reusable by the future markup lowering. Test `ReactiveUI.Style.Stylesheet`: the value grammar,
  a loaded theme+class, token resolution to a real applied RenderOpacity, the theme on/off states,
  and the inline-wins cascade. Full suite 68/68. **REMAINING:** wiring the `.uetkx` compiler's
  preamble `@uss`/`@theme` directives to emit `LoadStylesheet`/`RegisterTheme` calls (the grammar
  already lexes them) — a thin toolchain hook over this now-shipped runtime; the "post-v1"
  diagnostic stays until that codegen lands.

## TD-003 — Exit-animation protocol (delayed unmount)
- **Where:** reconciler-level (designed in Phase 7 item 5, shipped v1.x per the gate)
- **What/why deferred:** retrofit cost is reconciler-level so the DESIGN lands in Phase 7 even
  though shipping may slip past v1 (critique gap 3 resolution).
- **Production-grade resolution:** design doc committed in Phase 7; implementation + tests +
  docs in v1.x.
- **Status:** RESOLVED 2026-07-12. Shipped as the React-community `<Presence>` boundary
  (framer-motion's AnimatePresence shape) — a pure userland composition over the existing hooks,
  NOT reconciler surgery, so core risk stayed low and the design is portable to the Unity/Godot
  siblings as-is. `RUI::Presence(Children, MaxExitSeconds=2)` keeps a removed keyed child mounted
  ("exiting") instead of deleting it, and `UsePresence(Ctx) -> {bPresent, NotifyDone}` (context)
  flips `bPresent=false` into the kept child; the child animates via its existing
  `UseAnimate(bPresent)` and calls `NotifyDone()` when settled, which performs the real unmount.
  A per-child `MaxExitSeconds` timeout fence (host-clock frame-poll) force-unmounts a child that
  never notifies; re-entry (the key reappearing mid-exit) cancels the exit with the SAME fiber, so
  tween state continues from the current value. State preservation falls out for free: because the
  boundary keeps rendering the same keyed child, the reconciler keeps the same fiber.
  Files: `RuiPresence.h/.cpp` (ReactiveUICore). Test `ReactiveUI.Core.Presence`: deferred deletion,
  NotifyDone-driven unmount, timeout fence, re-entry cancel. **Reconciler hardening (same commit):**
  `ScheduleUpdateOnFiber` now marks the alternate twins too (React's `markUpdateLaneFromFiberToRoot`
  parity) — a latent bug where an async `setState` (frame/timer callback) on a component reached
  through a bailed-out intermediate marked the wrong double-buffer and got skipped; Presence's
  timeout/NotifyDone through `PresenceHost` exposed it. Full suite 61/61.

## TD-004 — Drag-and-drop + keyboard-shortcut APIs
- **Where:** `ReactiveUISlate` event layer
- **What/why deferred:** input policy v1 covers handled/unhandled + CommonUI delegation
  (Phase 2/6); DnD + shortcuts are additive APIs (critique gap 15 tail).
- **Production-grade resolution:** typed DnD props over Slate's drag-drop ops + shortcut
  registration hook, with demos.
- **Status:** PARTIAL — the KEYBOARD-SHORTCUT half RESOLVED 2026-07-12: `RUI::Slate::UseShortcut(Ctx,
  FRuiShortcut{Key,Ctrl/Shift/Alt/Cmd}, OnTrigger)` registers a Slate input pre-processor for the
  component's lifetime (UseEffect keyed on the chord; a stable ref box fires the LATEST callback;
  unregistered on unmount). `FRuiShortcut::Matches` exact-matches key+modifiers. Test
  `ReactiveUI.Slate.Shortcut`: matcher truth table + end-to-end mount → ProcessKeyDownEvent(Ctrl+S)
  fires once → non-match no-op → unmount stops firing. REMAINING: drag-and-drop (typed DnD props
  over Slate's FDragDropOperation + drop targets) — a larger event-layer feature, tracked here.

## TD-005 — Rider support (skipped ENTIRELY for v1 — owner 2026-07-11)
- **Where:** `ide-extensions/` (nothing ships for Rider in v1 — no TextMate bundle, no plugin)
- **What/why deferred:** owner scope cut 2026-07-11 (was: grammar-only day-one tier); the
  JetBrains-LSP-API plugin was always a separate deliverable (D-23).
- **Production-grade resolution:** thin Rider plugin hosting the same stdio server.
- **Status:** OPEN

## TD-006 — In-Unreal-editor `.uetkx` tab + read-only live preview panel
- **Where:** `ReactiveUIEditor`
- **What/why deferred:** v1's minimum in-editor surface is asset actions + watcher + MessageLog
  (Phase 4 step 6); the tab/preview is the someday-step toward designer-ish workflows (gate OUT
  list).
- **Production-grade resolution:** preview panel rendering a mounted component per open
  `.uetkx`, then (maybe) an editing tab.
- **Status:** OPEN

## TD-007 — On-device remote reload (Phase-4-style, over TCP)
- **Where:** `ReactiveUIInterp` + a device transport
- **What/why deferred:** the v1 dev loop is editor-side (gate OUT list); device push is a
  post-v1 feature with console-TCP policy questions.
- **Production-grade resolution:** serialized-spec push + `rui.reload`, mobile-first.
- **Status:** OPEN

## TD-008 — Scripting adapters (UnrealSharp/AngelScript — or our own)
- **Where:** new optional module(s)
- **What/why deferred:** gate OUT list; owner's stated worst case is building our own — if so,
  plugin-level like UnrealSharp, NEVER an engine fork (kills Fab), always opt-in, never a
  mandatory VM under shipped UI (MASTER_PLAN §6).
- **Production-grade resolution:** demand-gated adapter with its own plan.
- **Status:** OPEN

## TD-009 — Cross-repo corpus mirroring PRs (Godot + Unity)
- **Where:** `scripts/corpus-hash.mjs`, `plans/family-corpus.hash`; sibling repos' corpus files
- **What/why deferred:** the import corpus is the FIRST mirrored set (A4). The mechanism SHIPPED in
  leg 1 (feat/uetkx-imports): `_tiers.familyCore` partition + `corpus-hash.mjs` (sha256 over the
  canonicalized family-core sections, `UETKX|GUITKX|UITKX->TKX`) + the committed hash + the CI gate.
  The sibling repos adopt the same script + hash file in legs 2/3; the outbound mirrored-case PRs
  land then.
- **Production-grade resolution:** corpus sections hash-match across all three repos at a
  release-time check.
- **Status:** OPEN — mechanism shipped leg 1; sibling adoption PRs pending (Godot leg 2, Unity leg 3)

## TD-010 — Reorder strategy: minimal-move (spike-decided) + slot-prop updates reinsert
- **Where:** `ReactiveUISlate/Private/RuiCoreAdapters.cpp` (box panels + overlay)
- **What/why deferred:** the Phase 2 step 1 spike (Bench.SlateReorder, rows in
  BENCH_BASELINES.md) decided **minimal-move**: 1-moved-child costs 3µs vs 60µs for a full
  rebuild (20×); only the pathological full reverse favors rebuild (1.5×). Two accepted
  imperfections ride along: (a) `slot.*` prop UPDATES remove+reinsert the child at its index
  instead of mutating the live FSlot (Slate's scoped-slot-args API makes insert-time config the
  clean path); (b) SOverlay reorders rebuild (AddSlot takes a Z-order, not an index — overlays
  stack a handful of layers, so rebuild is fine there).
- **Production-grade resolution:** in-place FSlot setters via `FChildren::GetSlotAt` +
  downcast for (a) if profiling ever shows reinsert churn on hot slot-prop animation paths; a
  hybrid "rebuild when moves > N/2" switch only if a real workload surfaces where reverse-heavy
  reorders dominate.
- **Status:** RESOLVED 2026-07-12 (imperfection (a)) — `UpdateChildSlotProps` on the box-panel
  adapter now mutates the LIVE FSlot in place: `const_cast<TBox::FSlot&>(GetSlotAt(i))` +
  `ConfigureSlotLive` (SetFillHeight/Width, SetAutoHeight/Width, SetPadding, SetHorizontal/
  VerticalAlignment; absent keys reset to the slot defaults), then `Invalidate(Layout)` — no more
  detach/reinsert on hot slot-prop animation. Test `ReactiveUI.Slate.SlotInPlace` proves the SAME
  FSlot object survives the update (address unchanged) while its padding retargets 10→20→reset.
  (b) SOverlay index-reorder rebuild + the hybrid "rebuild when moves > N/2" switch remain the
  documented-accepted design — both are gated on a profiling workload that has not surfaced; the
  minimal-move box-panel strategy (spike-decided) is unchanged.

## TD-011 — Construct-only prop changes don't replace widgets yet
- **Where:** `FRuiSlateHost::CommitUpdate` (warns via the adapter reconstruct mask)
- **What/why deferred:** none of the M7 pattern widgets has a construct-only prop, so the
  replacement path (rebuild widget + swap into the parent slot + rebind events + re-parent
  children) has nothing to test against. The mask is part of the adapter contract NOW
  (D-11) and a violating diff warns loudly instead of silently misrendering.
- **Production-grade resolution:** implement replacement-in-place when the first widget ships a
  reconstruct-mask prop, with a pointer-identity-change test proving the swap. (The header-sweep
  audit removed the last v1 mask user: SScrollBox Orientation turned out runtime-settable — as of
  the audit NO shipped widget uses the mask; it remains contract-only.)
- **Status:** RESOLVED 2026-07-12 — `FRuiSlateHost::ReplaceWidget` rebuilds the widget (REUSING the
  event proxy — bind-once-swap-inner), re-parents its children WITH their slot props (new
  `FRuiSlateNode::ChildNodes` bookkeeping maintained by Insert/Remove/Reorder), swaps it into the
  parent slot at the same index, and re-applies props/style. Trigger is now PRECISE:
  `IRuiElementAdapter::ConstructOnlyChanged(Old,New)` gates the coarse mask (a mask bit set on both
  sides with an unchanged value no longer forces a rebuild). `CommitUpdate` calls it and returns.
  Test: `ReactiveUI.Slate.Replace` drives the host with a construct-only `Flavor` adapter and
  asserts pointer-swap + child identity/order preserved + proxy reused on a Flavor change, and
  NO replacement on a runtime-only change. battery 56/56 (Slate/Widgets/Update reorder paths green
  — bookkeeping non-regressive).

## TD-012 — Batch-2 widget surface deliberately deferred to Phase 7 (header-sweep audit)
- **Where:** `ReactiveUISlate` adapters (audit: engine-header sweep of all 15 shipped widgets)
- **What/why deferred:** the sweep found these RUNTIME setters not yet mapped — deferred by
  decision, not omission: SButton sounds + click/press/touch methods + IsFocusable; SSlider
  bar/handle colors + orientation + locked/indent; SCheckBox per-state images; SProgressBar
  fill type/style + marquee; SScrollBox scrollbar knobs (thickness/visibility/overscroll/
  wheel) + ScrollToEnd-style imperative APIs; STextBlock shaping/flow/highlight/strike/
  overflow + line-height. Everyday keys WERE added in the audit commit (text justify/autoWrap,
  button ContentPadding, slider StepSize, progress fillColor, scrollbox runtime Orientation).
- **Production-grade resolution:** the Phase 7 batch-2 production line maps each remaining
  setter through the prop-map schema with per-widget tests; imperative APIs (ScrollToEnd)
  need the ref-command design (family `useImperativeHandle` pattern over host handles).
  SCOPE EXPANDED (owner, 2026-07-10): coverage target is ALL official runtime widgets —
  plans/WIDGET_INVENTORY.md is the authoritative tracker (every S-class classified).
- **Status:** SUBSTANTIALLY DELIVERED 2026-07-12 — the batch-2 **everyday game set is shipped**
  (14 widgets, WIDGET_INVENTORY.md "Shipped Phase 7"): WidgetSwitcher, ScaleBox, Throbber,
  WrapBox, MultiLineEditableTextBox, SearchBox, SafeZone, DPIScaler, Separator, SpinBox,
  UniformWrapPanel, RichTextBlock, GridPanel, UniformGridPanel — each wired through EVERY
  touchpoint (typed props + factory in RuiSlateElements.h, adapter in the new
  RuiWidgetAdaptersB2.cpp, codegen host tag in UetkxCodegen.cpp HostTags, interp builder in
  UetkxInterpElements.cpp, regenerated LSP schema, and a per-widget contract test
  `ReactiveUI.Widgets.Batch2{,b,c}`). Separator is the first shipped widget to exercise the
  TD-011 reconstruct mask (Orientation/Thickness construct-only → widget replacement; the test
  proves a color-only change reuses the widget while a thickness change replaces it). Grid
  panels place children by `slot.column`/`slot.row`. Schema grew 15 → 29 host tags. Full suite
  64/64. **REMAINING (tracked, not blocking):** the item-model views (SListView/STileView/
  STreeView/SComboBox/SHeaderRow) are TD-022's dedicated `items`-delegate mechanism, NOT plain
  adapters; the batch-2 specials (SegmentedControl, ExpandableArea two-slot, SuggestionTextBox)
  and the batch-3 long tail (color pickers, gradients, vector inputs, virtual controls, etc.)
  wrap onto this exact production line as demand surfaces.

## TD-013 — Typed authoring API for style dicts + slot.* props
- **Where:** `ReactiveUISlate` (would live next to RuiStyle.h)
- **What/why deferred:** style/slot STORAGE is `TMap<FName, FRuiValue>` by design (markup,
  classes merging, and the LSP speak open key sets) — but the C++ AUTHORING surface should be
  compile-time-safe too (owner: "everything strongly typed"). Markup users get compile-time
  key validation from the `.uetkx` compiler/LSP schema in Phase 3; C++ users currently write
  raw FName keys (typos = one-time runtime warning).
- **Production-grade resolution:** fluent typed builders producing the dicts —
  `RUI::Style().Opacity(0.5f).Color(...).FontSize(16)` and
  `RUI::Slot().Padding(8).HAlign(EH::Center).Fill(1.f)` — one method per registered key,
  generated from the same schema the markup compiler validates against (single source).
- **Status:** RESOLVED 2026-07-12 — `RUI::Style()` (`FRuiStyleBuilder`) + `RUI::Slot()`
  (`FRuiSlotBuilder`) in RuiStyle.h: one fluent method per registered v1 style key (RenderOpacity,
  Visibility, Enabled, RenderTranslation/Scale/TransformAngle/TransformPivot, ColorAndOpacity,
  FontSize, Justification, AutoWrapText, FillColorAndOpacity) and per slot key (Padding(FMargin|
  float), HAlign(EHorizontalAlignment), VAlign(EVerticalAlignment), Fill) — each emitting the SAME
  FName key + FRuiValue kind the .uetkx markup does (single vocabulary), with a `Set(Key,Value)`
  forward-compat escape hatch. Implicitly convertible to `TSharedPtr<FRuiStyleDict>` for `Props.Style`
  / `Props.SlotProps`; header-only (no runtime cost beyond the map). Test `ReactiveUI.Style.Builder`
  applies a built style dict to a widget (RenderOpacity/Enabled + reset-on-removal) and a built slot
  dict through the box adapter (padding + HAlign), and pins the stored value kinds. NOTE: enum-safe
  authoring; the "codegen from schema" single-source is deferred — the key set is small, fixed for
  v1, and cross-checked by this test against the markup path.

## TD-014 — Content-Browser presence for `.uetkx` files
- **Where:** `ReactiveUIEditor` (would extend `FUetkxFileActions`)
- **What/why deferred:** `.uetkx` files live in SOURCE trees (`Source/`, `Plugins/`), which
  the Content Browser does not browse — they are deliberately not imported assets (the
  committed-generated-code design, D-19). v1 ships the real file actions instead:
  `FUetkxFileActions::CreateComponentFile` (New Component template, compiler-validated by
  `ReactiveUI.Uetkx.Schema`) and `OpenExternal` (OS default editor; wired to MessageLog
  links in Phase 4). Browser visibility would need a `UAssetDefinition` + thin proxy-asset
  design (import a pointer-asset per source file) — a real design decision, not glue.
- **Production-grade resolution:** decide post-v1 whether proxy assets earn their keep
  (Epic's Verse files ship WITHOUT Content-Browser presence; precedent says source stays in
  the IDE). If yes: UAssetDefinition + factory + a sync pass in the Phase-4 watcher.
- **Status:** OPEN

## TD-015 — `.uetkx` v1 grammar limits (deliberate cuts, family Phase-C class)
- **Where:** `ReactiveUIToolchain` codegen / `ReactiveUIInterp` file scan
- **What/why deferred:** four expressiveness gaps, each diagnosed rather than silently
  miscompiled: (1) EARLY markup returns — only the LAST top-level `return ( ... )` in a
  component body wins (T1.4); guard-style `if (x) { return ( <A/> ); }` at component level
  lowers only via `@if` today (the family added early returns in its Phase C). (2) children
  FORWARDING — `{children}` in markup has no splice form; components that wrap arbitrary
  children stay hand-written (the gallery card is inlined markup per screen for this reason).
  (3) `classes={ expr }` requires the expression to yield `TArray<FName>` — no string-form
  conditional classes (static `classes="a b"` works; the StyledPanels screen shows the
  `@if`-duplication workaround). (4) UETKX3002/3003 — short-circuit markup and spread attrs
  (family post-v1 too).
- **Production-grade resolution:** port the family's Phase-C early-return splitter; add a
  `{children}` splice node lowered to `Ch.Append(children)`; classes ternary sugar can ride
  the schema. Each lands with corpus + codegen-test pins.
- **Status:** PARTIAL — gap (2) CHILDREN FORWARDING RESOLVED 2026-07-12: `{children}` (an Expr node
  whose code is exactly `children`) now SPLICES the component's forwarded children via
  `Ch.Append(children)` at both child-list emit sites (EmitChildren + directive-body EmitBody), so a
  component can wrap arbitrary children (the reason gallery cards were inlined per-screen). Pinned by
  the `ChildrenForward` contract golden AND proven end-to-end by `ReactiveUI.Uetkx.Children`
  (ChildParent wraps two rows in ChildHost → HOST-HEADER + FORWARDED-A/B + HOST-FOOTER all render).
  REMAINING: (1) early component-level markup returns (family Phase-C splitter port), (3)
  `classes={expr}` string-form conditional classes, (4) UETKX3002/3003 short-circuit + spread attrs
  (post-v1 in the family too).

## TD-016 — Event payload surface is the single magic `Value` (FRuiValue)
- **Where:** `ReactiveUIToolchain` codegen (event attr lowering)
- **What/why deferred:** every event handler expression compiles into
  `FRuiCallback::Create([=](const FRuiValue& Value) { expr; })` — text/bool/float payloads
  arrive as `Value.TextValue` / `Value.BoolValue` / etc. (see SimpleTextField/StyledPanels).
  Typed per-event payloads (the analyzer knowing OnTextChanged carries text) is LSP/schema
  work, not codegen work.
- **Production-grade resolution:** the Phase-5 schema already types attrs as `event`; extend
  it with a payload-kind field so the LSP can complete `Value.TextValue` correctly.
- **Status:** RESOLVED 2026-07-12 — `ExportSchemaJson` emits `eventPayloads` (event attr -> payload
  kind: OnTextChanged/OnTextCommitted=text, OnCheckStateChanged=bool, OnValueChanged=float,
  OnClicked=void); the shipped `uetkx-schema.json` was regenerated (Acceptance schema-sync green).
  The LSP (new `eventPayload.ts`) completes `Value.<Field>` typed by the ENCLOSING event — inside
  `OnTextChanged={ … Value.| }` it offers `TextValue` first (marked as the OnTextChanged payload) —
  and hovers an event attr with the exact `Value.<Field>` (`FText`/…) it carries; `enclosingAttrName`
  balances nested braces to find the owning attr. Tests: schema pins + enclosingAttrName/fieldForKind
  (server.test 16/16) + smoke `Value.` round-trip.

## TD-017 — `hook` / `module` companion declarations in `.uetkx`
- **Where:** `ReactiveUIInterp` file scan + `ReactiveUIToolchain` codegen
- **What/why deferred:** the family grammar also has `hook Name(params) { ... }` and
  `module Name { ... }` declarations (D-03's declaration inventory); the Phase-3 plan listed
  their codegen shapes. v1 ships `component` only: in UE a custom hook is ALREADY a plain
  C++ free function taking `FRuiContext&` (no sugar needed for capability — see the demo
  support-header pattern), and modules are C++ namespaces. The decls add family-parity
  authoring sugar, not capability.
- **Production-grade resolution:** port `_parse_hook_at`/module member loops from guitkx.gd
  into FUetkxFileScan, emit free functions in the `.inl`, corpus + contract fixtures. Do it
  when the docs site (Phase 8) starts teaching cross-family authoring, or on user demand.
- **Status:** RESOLVED 2026-07-11 — hook/module declarations + one-component-per-file + companion suffixes + refs-topo aggregator shipped on feat/uetkx-compiler (plans/UETKX_DECLARATIONS_PLAN.md; owner field-test directive)

## TD-018 — Cross-repo grammar-corpus mirroring (Godot follow-up PR)
- **Where:** `ide-extensions/lsp-server/test-fixtures/uetkx-*.json` ↔ the Godot repo's
  `guitkx` corpus
- **What/why deferred:** Phase 3 step 7 calls for the host-language-agnostic corpus cases to
  be mirrored INTO the Godot repo (its scanner corpus gains our markup-mode cases) via a
  follow-up PR there. Our corpus already contains the family-shared cases + C++-lexis cases;
  the Godot-side PR is out of this repo's branch scope.
- **Production-grade resolution:** open the Godot-repo PR copying the markup-mode/nonBmp case
  sections + the `grammar-contract` sync note; drop this entry when merged.
- **Status:** OPEN

## TD-019 — Hook-state VALUE migration across the compiled→interp swap
- **Where:** `ReactiveUICore` hook cells + `ReactiveUIInterp` hook execution
- **What/why deferred:** compiled components hold TYPED hook cells
  (`TRuiStateCell<int32>`); the interpreter's cells are `FRuiValue`-typed. The first
  hot-swap of a compiled component therefore RESETS its state (reported honestly as
  "first interp swap (representation)"); interp→interp saves preserve. The family does not
  have this seam (GDScript is engine-interpreted end to end).
- **Production-grade resolution:** `IRuiHookCell::ExportRuiValue(FRuiValue&)` (specialized
  via `if constexpr (std::is_constructible_v<FRuiValue, T>)`) + an interp-aware UseState
  path that migrates an exporting cell's value into the fresh FRuiValue cell when the hook
  signature matches. Numeric/string/bool/text state would then survive the FIRST save too.
- **Status:** RESOLVED 2026-07-12 — `IRuiHookCell::ExportRuiValue(FRuiValue&)` (default false;
  `TRuiStateCell<T>` overrides via `if constexpr (std::is_constructible_v<FRuiValue, const T&>)`);
  `SetComponentOverride` + `FRuiComponentOverride` gain `bMigrateState`; the reconciler snapshots
  exportable cells into `FRuiComponentState::MigratedState` (by hook slot) before the reset and
  `UseState<FRuiValue>` re-seeds from it; `RuiHmr` sets `bMigrate` on the same-shape first swap and
  no longer counts it a reset (note reads `state migrated`). Numeric/string/bool/text state now
  survives the first compiled→interp save; only container/opaque slots or a shape change reset.
  Test: `ReactiveUI.Hmr` "numeric state SURVIVED the first swap (TD-019)" (compiled counter → 5,
  first interp swap, still 5). battery green.

## TD-020 — Embedded-C++ intelligence (clangd proxy over a virtual document)
- **Where:** `ide-extensions/lsp-server` (virtualDoc/sourceMap/clangdProxy modules)
- **What/why deferred:** the Phase-5 enhancement layer — a length-preserving C++ virtual
  document + source map forwarding hover/completion/definition inside `{expr}`/setup blocks
  to a clangd child process (compile_commands.json walk-up). v1 ships the family's
  fully-supported BASELINE instead: markup intelligence from the compiler-exported schema,
  live parse diagnostics from the corpus-locked TS front-end, hash-gated sidecar compiler
  diagnostics, and golden-corpus formatting. The proxy needs clangd present and is only
  meaningfully verifiable interactively; the family precedent ("markup-only stays a fully-
  supported baseline") is the shipped degradation contract.
- **Production-grade resolution:** port virtualDoc.ts (third instantiation of the family
  pattern; HookStubs.h parity test), sourceMap.ts spans, clangdProxy.ts child process with
  graceful-degradation notice. The VS2022 polish set (options page, format-on-save, smart
  indent, brace completion — present in the Godot sibling) rides the same follow-up.
- **Status:** OPEN

## TD-021 — CommonUI activatables + MVVM-plugin glue + UMG prop-map bridge
- **Where:** `ReactiveUICommonUI`, `ReactiveUIMVVMBridge`, `ReactiveUIUMG`
- **What/why deferred:** Phase 6 shipped the interop CORE (URuiHostWidget, URuiWorldSubsystem
  teardown contract, RUI::Umg::UserWidget embedding, UseField over the engine FieldNotification
  module — deliberately MVVM-plugin-independent). Three plugin-coupled layers remain:
  (1) CommonUI — URuiActivatableScreen/UseActivation/UseInputMethod need the CommonUI plugin
  enabled (not in the demo project yet) + the D-27 optional-plugin gating decision exercised;
  (2) ModelViewViewModel plugin glue — URuiSignalViewModel reverse bridge + global-collection
  registration (UseField already serves UMVVMViewModelBase since it implements
  INotifyFieldValueChanged); (3) per-class UMG prop maps + delegate trampolines so hosted
  UUserWidgets receive Rui props declaratively (today the embedding seam is class+world).
  Input policy: Rui events consume via Slate FReply::Handled inside SRui widgets; unhandled
  input falls through to CommonUI's action router untouched — the docs note rides Phase 8.
- **Production-grade resolution:** enable CommonUI/MVVM in the demo project, implement each
  layer with its suite (ReactiveUI.CommonUI, the reverse-bridge test), per-class prop maps
  generated from UHT reflection.
- **Status:** OPEN

## TD-022 — Asset brushes (D-17) + focus extensions + item-model list views
- **Where:** `ReactiveUISlate` (+ `ReactiveUIUMG` for the GC root)
- **What/why deferred:** the remaining Phase-7 surface beyond the shipped animation/media
  hooks: (1) asset brushes — `BorderImage`/`Image` accepting texture/material ASSETS needs
  the FGCObject brush root keeping UObjects alive while Slate paints them (today: FCoreStyle
  brush NAMES only); (2) focus extensions — programmatic focus API + focus-path hooks beyond
  the Phase-2 fences; (3) item-model views (SListView/STileView/STreeView) — the virtualized
  item-model adapter treatment (WIDGET_INVENTORY.md classifies the full set; TD-012 carries
  the owner's ALL-runtime-widgets coverage target). The §4 parity LEDGER is otherwise
  substantially in place (Slate.Events/KeyedReorder, Style.Classes/NodePool, Update tail,
  Widgets.* — landed with Phases 1-2; custom_draw exercised via the compiled CustomDraw demo).
- **Production-grade resolution:** each lands as its own production line with suites; asset
  brushes first (unblocks real-game UIs), then SListView (the family item_list/tree port
  completes the ledger), then focus.
- **Status:** PARTIAL — 2 of 3 sub-surfaces delivered 2026-07-12:
  - **Asset brushes (D-17): DONE.** `RUI::Umg::MakeAssetBrush(UObject*, size, tint, drawAs)`
    (ReactiveUIUMG) builds an FSlateBrush and registers it with a process-wide FGCObject
    (`FRuiAssetBrushRoot`) that keeps every live brush's resource object referenced against GC;
    dead brushes are compacted. `FRuiImageProps::Brush` + `FRuiBorderProps::BorderImageBrush`
    (TSharedPtr<FSlateBrush>, identity-compared) carry it; the Image/Border adapters apply it
    (asset brush wins over an FCoreStyle name). Test `ReactiveUI.Umg.AssetBrush` proves the
    texture survives a full GC while the brush is live and the root compacts on release.
  - **Focus extensions: DONE.** `RUI::Slate::UseFocus(Ctx) -> {Ref, Focus(), IsFocused()}`
    (a UseRef weak-widget box the ref lifecycle syncs) + imperative `FocusWidget(handle)` /
    `ClearFocus()`. Test `ReactiveUI.Slate.Focus` drives the full round-trip through a real
    SWindow.
  - **Item-model views (SListView/STileView/STreeView): REMAINING.** The virtualized
    item-model adapter is the one genuinely large sub-surface here — each generated/recycled
    row must host its own reconciler subtree (per-row sub-root over SListView's
    generate/recycle lifecycle). Tracked as the follow-on; the declarative `items`-delegate
    shape is designed but not yet built.

## TD-023 — Two-phase fwd-decl aggregator + `#line` project-relative (uetkx-imports M6/M7)
- **Where:** `UetkxCodegen.cpp` (emit), `UetkxDriver.cpp::BuildAggregators`, `UetkxDriver.h::CodegenVersion`
- **What/why deferred:** cross-file COMPONENT cycles are a v1 locked decision (A1) but the current
  single-phase per-file `.inl` + callee-before-caller aggregator ordering (uses/import edges) only
  supports ACYCLIC cross-file references — a true cycle still errs (UETKX2107). Full parity needs
  the two-phase emit (a DECL phase: complete props structs + defaulted wrapper decls + hook
  fwd-decls + module bodies; a BODY phase: impls + default-free wrapper defs + registrations) with
  the aggregator including each `.inl` twice, and `#line "<project-rel>"` directives for VS
  debugger stepping into `.uetkx` (the M3 `ProjectRelPath` is already threaded for this). This is
  the CodegenVersion 1->2 golden re-pin window. The migrated gallery has no cycles, so the current
  emit ships correct; this is a self-contained enhancement.
- **Production-grade resolution:** two-phase `.inl` (phase-guarded, defaults-on-decl-only,
  test-pinned) + two-phase aggregator include + `#line`; CodegenVersion 2 with all goldens re-pinned.
- **Status:** RESOLVED 2026-07-11 (uetkx-imports) — two-phase emit shipped (`FEmittedDecl`
  DECL/BODY split in `UetkxCodegen.cpp`: complete props structs + defaulted wrapper fwd-decls + hook
  fwd-decls + module bodies in DECL; impls + default-free wrapper defs + registrations in BODY);
  `BuildAggregators` includes every `.inl` twice (decl phase for all, then body). Cross-file
  COMPONENT cycles now COMPILE — **UETKX2107 retired**; a `CycleProof/CycleA.uetkx`↔`CycleB.uetkx`
  fixture compiles + renders (`ReactiveUI.Uetkx.Cycle`). `#line <n> "<project-rel .uetkx>"` directives
  wrap every top-level verbatim region (`WithLine`/`BuildLineStarts`/`LineOf`); M7.1 spike verdict
  recorded in the plan — **interactive breakpoint bind CONFIRMED by owner in VS2022 (2026-07-11)**:
  a breakpoint on `SimpleCounter.uetkx` line 4 bound + hit on the `.uetkx` source (not the `.inl`).
  CodegenVersion 1->2, all goldens re-pinned. battery 55/55, -check 0 drift.

## TD-024 — LSP server import intelligence + support-file formatting + VS2022 vsix rebuild (M11 tail)
- **Where:** `ide-extensions/lsp-server/src/server.ts` (+ new `uetkxWorkspace.ts`), `formatUetkx.ts`,
  `ide-extensions/visual-studio`
- **What/why deferred:** the load-bearing IDE mirrors shipped in leg 1 (scanner mirror for
  imports/export/mixed-decl, formatter import canonicalization + `~/` config `root`, TextMate
  import/export/from keywords). The additive editor features remain: import-list + specifier
  completions, go-to-def (imported name -> decl, specifier -> file), workspace-index strict
  resolution diagnostics (skew-gated on sidecar schema >= 3), `FmtHook`/`FmtModule` so support/mixed
  files format instead of round-tripping verbatim, and rebuilding the VS2022 vsix.
- **Production-grade resolution:** the completions/go-to-def/resolution-diag set + FmtHook/FmtModule
  + a shipped vsix, covered by `server.test.ts`.
- **Status:** RESOLVED 2026-07-11 (uetkx-imports) — new `uetkxWorkspace.ts` mirrors
  `FUetkxFsResolver` + `FUetkxResolve::Apply` (resolveSpecifier ./ ../ ~/, getDecls mtime-cache,
  moduleRootFor/workspaceRootFor walk-ups, findExporter index, suggestSpecifier, resolveDiagnostics
  emitting live 2300/2301/2302/2308). `server.ts` wires import-name + specifier completions,
  `onDefinition` (name->export, specifier->file, bare ref->workspace exporter), and live resolution
  diags de-duped against the hash-gated sidecar. `FmtHook`/`FmtModule` land in BOTH the C++
  `UetkxFormatter.cpp` and the TS `formatUetkx.ts` (Scan.Order-driven loop; hook/module gain
  `ExportAt`) with 4 new shared golden-corpus cases. VS2022 vsix rebuilt green via
  `build-local.ps1` (8.6 MB, bundles the updated server). 7 new `server.test.ts` cases (15/15) +
  extended smoke round-trip (SMOKE PASSED).

## TD-025 — Import staleness tail: value cycles (2306), source-truth aggregator, single-pass fixpoint
- **Where:** `UetkxDriver.cpp` (`CompileAllRoots`, `BuildAggregators`)
- **What/why deferred:** M8 shipped the correctness core (sidecar v3 export_hash/dep_hashes,
  reverse-edge staleness, verdict-poisoning fix). Remaining: (a) UETKX2306 value-import cycles
  (hook/module import edges consumed at load — a DFS that prints the chain, TDZ parity); (b) the
  source-truth aggregator ORDER driven by declared import edges instead of the sidecar `uses` graph
  (kills the fresh-clone divergence, A2) — the uses-graph order is correct for the migrated tree
  today; (c) folding the max-2-pass reverse-recompile into ONE `CompileAllRoots` call (the editor's
  repeated sweeps + CI `-check` fresh-compile converge today; the A/B retry is test-pinned as
  green-on-second-sweep).
- **Production-grade resolution:** 2306 emitted with the printed chain; aggregator ordered by import
  edges; single-call fixpoint. All three test-pinned.
- **Status:** RESOLVED 2026-07-11 (uetkx-imports) — (a) **UETKX2306**: `CompileAllRoots` builds the
  value-import graph (hook/module import edges only; component edges exempt now that the two-phase
  pass fwd-declares wrappers) from fresh preamble scans and DFS-detects a cycle, printing the chain
  (module↔module test-pinned in `ReactiveUI.Uetkx.Driver`). (b) **Source-truth order**:
  `BuildAggregators` orders by RESOLVED preamble import edges (Kahn topo, alpha ties, cycle
  remainder), sidecars demoted to cache — `ReadSidecarOrdering` deleted. (c) **Single-sweep
  fixpoint**: `RunPass`/`ByPath`/`OldExport`+`bExportsMoved` — pass 1 compiles stale files; if any
  `export_hash` moved, pass 2 re-sweeps so an importer that sorted before its changed dep recompiles
  in ONE `CompileAll` (the A/B verdict-poisoning test now asserts single-sweep). battery 55/55,
  -check 0 drift; plan M8 test row updated.

## TD-026 — Accepted v1 divergences: interp global-name scoping + private-FName last-swap-wins
- **Where:** `RuiNode.cpp` (process-global name/factory registries), `RuiHmr.cpp`, `UetkxInterpComponent.cpp`
- **What/why deferred:** privacy (A5e) is a COMPILE-TIME scoping (per-file detail namespace +
  tree-shaken named factory). The RUNTIME registries (`RUI::Named`/`RegisterNamedFactory`,
  `RegisterComponentId`) are process-global and name-keyed: the interpreter resolves a name it never
  imported, and two files' private same-name decls collide last-swap-wins in the HMR registry. These
  are accepted v1 divergences (compile-time scoping only) — the compiler still fences cross-file
  reach and the aggregator TU still fences symbol collisions; only the live-reload registry is flat.
- **Production-grade resolution:** file-scoped runtime component identity (qualified ids in the
  registry) so private names never alias across files at runtime.
- **Status:** OPEN (accepted caveat, documented in M5/M9)
