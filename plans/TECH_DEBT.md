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
- **Status:** RESOLVED 2026-07-12 — both halves shipped.
  - **Keyboard shortcut:** `RUI::Slate::UseShortcut(Ctx, FRuiShortcut{Key,Ctrl/Shift/Alt/Cmd},
    OnTrigger)` registers a Slate input pre-processor for the component's lifetime (UseEffect keyed
    on the chord; a stable ref box fires the LATEST callback; unregistered on unmount).
    `FRuiShortcut::Matches` exact-matches key+modifiers. Test `ReactiveUI.Slate.Shortcut`: matcher
    truth table + end-to-end mount → ProcessKeyDownEvent(Ctrl+S) fires once → non-match no-op →
    unmount stops firing.
  - **Drag-and-drop:** typed DnD over Slate's `FDragDropOperation` (`RuiDragDrop.h/.cpp`). Slate DnD
    is an event-OVERRIDE surface (not props/delegates), so the API is a pair of wrapper ELEMENTS:
    `RUI::Slate::DragSource` (carries an `FRuiValue` Payload + `DragType` tag; begins the op on a
    left-drag; `OnDragStart`/`OnDragEnd` events) and `DropTarget` (`AcceptTypes` filter — empty =
    accept any; `OnDrop` fires with the payload; `OnDragEnter`/`OnDragLeave` fire with the hovering
    payload + toggle an `IsOver` hover flag). `FRuiDragDropOp : FDragDropOperation` rides the payload
    across the drag and fires `OnEnded(bHandled)` exactly once. C++-FIRST (the drop closure + FRuiValue
    payload + array accept-list aren't cleanly markup-expressible — no `.uetkx` tag, like ListView).
    The widgets + op are exported so the suite drives a synthetic drop (a hand-built `FDragDropEvent`
    over the op) headless. Test `ReactiveUI.Slate.DragDrop`: op payload/type + fire-once OnEnded;
    accept/reject by type (handled vs unhandled reply); empty-accept-any; enter/leave hover toggle;
    drag-source OnDragDetected begins the op + fires OnDragStart.

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
- **Status:** RESOLVED 2026-07-12 — the read-only live preview shipped. `FUetkxPreview`
  (`ReactiveUIEditor/UetkxPreview.h/.cpp`) is the headless-testable core: scan (`FUetkxFileScan::Scan`)
  → pick a component (first or by name) → build the dev-loop interpreter def (`FUetkxInterpDef::Build`)
  → mount it as a live `FRuiRoot`, collecting parse diagnostics + interpreter fallback notes. Always
  returns a preview — on failure a placeholder widget + messages, never a crash. `SUetkxPreviewPanel`
  is the Slate shell (path box + Load, a live preview area, a scrolling message list); the editor
  module registers it as a **nomad tab** (`ReactiveUIPreview`) with a Tools-menu entry (guarded to the
  Slate-app path, so commandlets skip it). Test `ReactiveUI.Editor.Preview`: a valid component mounts a
  real widget, named-component selection (+ missing-name message), no-component and unterminated-parse
  sources yield a placeholder + surfaced diagnostics. The interactive tab UX is owner-verified in the
  editor; the mount/diagnostic pipeline is automated. (An editing tab remains the someday-step.)

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
  64/64. Schema later held at 29 (ListView/TileView are C++-first render-prop APIs, not markup
  tags — see TD-022). Full suite 71/71.
  - **ExpandableArea (two-named-slot special): DONE 2026-07-12.** `RUI::Slate::ExpandableArea`
    (`RuiExpandableArea.h/.cpp`, exported `SRuiExpandableArea`) — the family's FIRST two-named-slot
    widget. Children carry `slot.role = "header" | "body"`; the wrapper builds SExpandableArea once
    around two persistent SBox holders (its HeaderContent/BodyContent are construct-time named slots
    with no runtime setters) and the MultiSlot adapter reparents role-tagged children into them
    (role-less → body). Expansion is CONTROLLED: `bIsExpanded` applied skip-when-equal against the
    live state (D-16); `OnExpansionChanged` forwards the user toggle. Test
    `ReactiveUI.Widgets.ExpandableArea` (adapter-driven, like the slot suite): role routing
    (header/body/default), controlled collapse+re-expand, and body-holder clear on remove.
  - **SegmentedControl (tab bar): DONE 2026-07-12.** `RUI::Slate::SegmentedControl`
    (`RuiSegmentedControl.h/.cpp`, exported `SRuiSegmentedControl`) wraps `SSegmentedControl<int32>`:
    one text segment per `Labels` entry (value = index). `Labels` are construct-only (SSegmentedControl
    has no clear-children API — a label-set change trips the reconstruct mask and replaces the widget);
    `SelectedIndex` is CONTROLLED runtime (SetValue skip-when-equal, D-16); `OnSelectionChanged` fires
    the picked index. Test `ReactiveUI.Widgets.SegmentedControl` (adapter-driven): segment count from
    labels, controlled selection move, the Labels reconstruct-mask gate (same → no rebuild, changed →
    rebuild), and a relabelled widget's new segment count.
  - **Interaction harness + NumericEntryBox + ComboBox + SuggestionTextBox: DONE 2026-07-12.** Built
    a reusable headless **Slate interaction harness** (`Source/RuiHostTests/Private/RuiSlateTestHarness.h`:
    `FindDescendantByType`/`FindInAllWindowsByType` RTTI-off widget-tree walk, `FTestWindow` live
    window + prepass, menu/window diagnostics) — the thing that was actually missing to verify the
    "interactive-only" widgets. On it:
    - **NumericEntryBox** (`RuiNumericEntryBox.h/.cpp`) — controlled numeric field over
      `SNumericEntryBox<float>` (no value setter → the controlled value lives in a member its Value
      attribute reads; skip-when-equal D-16). Test READS THE VALUE BACK from the inner `SEditableText`
      (a real display round-trip): 42.5 shown, re-render 7 → 7 shown.
    - **ComboBox** (`RuiComboBox.h/.cpp`) — dropdown over `SComboBox<TSharedPtr<FRuiValue>>` reusing
      the ListView render-prop for BOTH the selected display and the generated rows (each an FRuiRoot
      sub-root). Controlled `SelectedIndex`; `OnSelectionChanged` fires only on a user pick. Test drives
      the REAL menu — opens it, ticks the pushed `SComboListType` with geometry, asserts all 3 option-row
      sub-roots generate — plus selected-display read-back + controlled move.
    - **SuggestionTextBox** (`RuiSuggestionTextBox.h/.cpp`) — autocomplete over `SSuggestionTextBox`;
      `OnShowingSuggestions` delegates to a case-insensitive substring filter over `Suggestions` (the
      real behaviour behind the dropdown). Test verifies the filter (substring, case-insensitive, empty
      → none) + controlled text round-trip through the live widget.
  - **REMAINING (tracked, not blocking):** STreeView (hierarchical item model — TD-022 note); and the
    batch-3 long tail (color
    pickers, gradients, vector inputs, virtual controls, etc.). Each wraps onto the established
    production line as demand surfaces; the ones above are deferred specifically because their core
    value (menu dropdowns / inner-text-widget behavior) is not cleanly verifiable headless — shipping
    them now would mean weak tests, against the quality bar.

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
- **Status:** OPEN — **owner design decision, deliberately not built.** This is not glue: it needs a
  proxy-asset design (a `UAssetDefinition` + factory importing a pointer-asset per `.uetkx` source
  file) whose value is genuinely in question — Epic's own Verse files ship WITHOUT Content-Browser
  presence (source stays in the IDE). Building speculative proxy assets against that precedent would be
  the wrong call; the in-editor preview tab (TD-006) now covers the "see a `.uetkx` inside the editor"
  need without importing anything. Left for the owner to decide if proxy assets earn their keep.

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
  Gaps (1) and (4a) RESOLVED 2026-07-16 (WIDGET_COMPLETION_PLAN wave G, 0.9.0): (1) EARLY
  RETURNS — `CollectMarkupReturns` gathers every markup `return ( ... )` (paren/bracket depth
  hides call-arg returns; brace depth does NOT — the if/else guard shape); the emitter splices
  the body VERBATIM (Unity's SpliceSetupCodeMarkup model) lowering each span to
  `return { <element> };` in place; the FINAL return must be top-level (new UETKX3007). Single-
  return bodies take the legacy path — all prior goldens byte-stable. (4a) SHORT-CIRCUIT —
  `cond && <X/>` / `cond || <X/>` desugar to ternaries with an empty-fragment `FRuiNode()` arm
  (the Unity Phase-1.5 port); **UETKX3002 retired**. Pinned by MultiReturn/ShortCircuit/
  NestedFinalReturn contract goldens + 4 fileScan corpus cases (both impls) + the compiled
  `GrammarProof/MultiReturnProof.uetkx` mounted by `ReactiveUI.Uetkx.WaveG`. **Cross-repo
  outbound (grammar-contract):** the new fileScan corpus cases + refreshed family-core hash need
  the mirrored PR against the Godot repo and a flag to the Unity repo — track until both merge.
  REMAINING: (3) `classes={expr}` string-form conditional classes, (4b) spread attrs — kept
  omitted per D-W1 (Unity-aligned; UETKX3003 stays).

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
- **Status:** RESOLVED 2026-07-11 — hook/module declarations + one-component-per-file + companion suffixes + refs-topo aggregator shipped on feat/uetkx-compiler (plans/archive/UETKX_DECLARATIONS_PLAN.md; owner field-test directive)

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
- **Status:** ✅ RESOLVED 2026-07-15 — the last hole closed on `feat/v1-gate-closeout`:
  `onCompletion` now consults the embedded layer first (`translateEmbeddedCompletion` maps
  clangd textEdit ranges — classic AND insert/replace forms — back to `.uetkx` coordinates;
  prelude-targeting edits are dropped from their item, `additionalTextEdits` never pass
  through; empty results fall to the markup baseline). LSP suite 33/33 + smoke. Hover,
  definition, and completion now all forward inside setup/hook/module bodies; markup
  `{expr}` attr values remain baseline-only by design (they reference markup-scope locals
  the virtual doc deliberately does not synthesize — the documented degradation).
  The 2026-07-12 record of the layers beneath:
  - **`sourceMap.ts`** — a bidirectional span-based map (.uetkx ↔ virtual C++), binary-searched,
    plus offset↔position helpers; a virtual offset inside synthetic scaffolding maps to null.
  - **`virtualDoc.ts`** — `buildVirtualCpp(source)` scans the file and lifts every embedded-C++
    region (component `setup` blocks, `hook`/`module` bodies) VERBATIM into synthetic
    functions/namespaces clangd can reason about, each contributing a 1:1 span; markup regions
    contribute nothing.
  - **`clangdProxy.ts`** — a clangd child-process client (Content-Length LSP framing, request
    correlation, `initialize` handshake, `compile_commands.json` walk-up via
    `findCompileCommands`) with the GRACEFUL-DEGRADATION contract: no clangd on PATH → `start()`
    resolves false and every `positionRequest` returns null (the LSP falls back to the
    fully-supported markup baseline — never an error).
  Test `embeddedCpp.test.ts`: source-map round-trips, position conversions, a lifted hook body +
  component setup mapped back to source, markup offsets → null, and the clangd-absent degradation
  path. LSP suite 22/22 (server.test.ts host-tag count updated 15 → 29 for the TD-012 widgets).
  - **`server.ts` wiring: DONE 2026-07-12.** New `embeddedIntel.ts` is the routing layer:
    `isEmbeddedOffset` gates whether a caret is inside a setup/hook/module body; `buildEmbeddedView`
    gives build-once bidirectional position mapping (source utf16 ↔ virtual-doc position);
    `embeddedPositionRequest` forwards to the proxy over the virtual document and returns null (→
    markup baseline) whenever the caret is NOT embedded OR the proxy is unavailable. `server.ts`
    holds one lazily-started `ClangdProxy` (idempotent `start()`; resolves false without clangd) and
    wires it in: **hover** consults the embedded layer first (C++ symbol hover wins inside a body,
    else falls to the schema baseline); **definition** consults it as a fallback after the markup
    index misses, with `translateEmbeddedDefinition` mapping clangd locations that point back into
    the virtual doc to `.uetkx` ranges (engine-header locations pass through). Test
    `embeddedServer.test.ts` (a stub `PositionResponder`): embedded-vs-markup gating, source↔virtual
    round-trip, forward-to-proxy-over-virtual-uri, markup positions never forwarded, and the
    unavailable-proxy degradation (never queries). LSP suite **27/27**.
  - **VS2022 polish set: DONE 2026-07-12.** `UetkxPackage` (an `AsyncPackage`, background-loaded on
    solution-exists) + `UetkxOptionsPage` (`DialogPage`) add **Tools > Options > ReactiveUI > UETKX**
    with a **Format on Save** setting; the package's Running-Document-Table `OnBeforeSave` hook runs
    `Edit.FormatDocument` on a saved `.uetkx` (the golden-corpus formatter over LSP) when enabled.
    Brace completion + smart indent ride the shipped `language-configuration.json` (brackets /
    autoClosingPairs / indentationRules). `GeneratePkgDefFile` flipped on now that a registered package
    exists; the hand-written grammar `uetkx.pkgdef` is kept alongside the generated one. Version bumped
    to **vs2022 0.2.0** with a Lane-B changelog entry (`changelog verify` green). Compile-verified: the
    VSIX builds into `UetkxVsix.vsix` with MSBuild + the VSSDK workload; the editor UX (options page
    present, format-on-save fires) is owner-verified by installing the VSIX in VS2022.

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
- **Status:** PARTIAL — the MVVM **reverse bridge** shipped 2026-07-12 (the one layer with no
  external-plugin dependency): `URuiSignalViewModel` (ReactiveUIUMG) is a FieldNotify UObject —
  INotifyFieldValueChanged implemented directly over the engine FieldNotification module, NO
  ModelViewViewModel-plugin dependency — with a generic bindable field set (Int/Float/Bool/Text).
  Rui writes it via typed setters or `Set(FRuiValue)` (routes by kind, skip-when-equal, broadcasts
  on change); a UMG widget (or a UseField consumer) bound to it updates when Rui state moves — the
  "ours feeding theirs" direction, complementing the shipped UseField "theirs feeding ours". Test
  `ReactiveUI.Mvvm.ReverseBridge`: broadcast-on-change + equal-set skip + FRuiValue routing + a
  full round-trip (Rui writes VM → VM broadcasts → a UseField consumer re-renders). Full suite
  68/68.
- **Status:** RESOLVED 2026-07-12 — all three plugin-coupled layers shipped after enabling CommonUI
  + ModelViewViewModel (optional plugin refs in `ReactiveUI.uplugin` per D-27; build deps; both on in
  the demo host `.uproject`; enablement verified non-destabilizing — full suite stayed 75/75, then
  79/79 with the new suites).
  - **CommonUI activatables: DONE.** `RuiActivation.h/.cpp` is the Rui-side seam — a plain context
    (`FRuiActivationState{bActive, InputMethod}`) an `ActivationProvider` publishes and the tree reads
    via `UseActivation`/`UseIsActive`/`UseInputMethod` (no UObject dependency → unit-testable headless).
    `URuiActivatableScreen` (`RuiActivatableScreen.h/.cpp`) is the UObject: a `UCommonActivatableWidget`
    that hosts a named component wrapped in ActivationProvider and re-renders on activation
    (`NativeOnActivated/Deactivated`) + input-method change (best-effort `UCommonInputSubsystem` read,
    guarded for player-less contexts). Tests: `ReactiveUI.CommonUI.Activation` (headless context
    re-render) + `ReactiveUI.CommonUI.Screen` (a standalone game instance → `CreateWidget` → real
    `ActivateWidget/DeactivateWidget` re-renders the hosted tree ACTIVE↔INACTIVE).
  - **MVVM global-collection registration: DONE.** `URuiMvvmViewModel` (`RuiMvvmViewModel.h/.cpp`,
    ReactiveUIMVVMBridge) is the MVVM-plugin sibling of `URuiSignalViewModel` — a `UMVVMViewModelBase`
    (Int/Float/Bool/Text FieldNotify props, `UE_MVVM_SET_PROPERTY_VALUE` skip+broadcast, `Set(FRuiValue)`
    routing) so it can be REGISTERED in the MVVM global viewmodel collection. `RUI::Mvvm::RegisterGlobalViewModel`
    / `FindGlobalViewModel` add/resolve by context name via `UMVVMGameSubsystem→GetViewModelCollection`.
    Test `ReactiveUI.Mvvm.GlobalCollection`: register → resolve-back-same-instance, unknown → null, Set
    routing + fire-once broadcast + equal-set skip.
  - **Per-class UMG prop maps: DONE.** `RUI::Umg::ApplyPropMap(UUserWidget*, FRuiStyleDict)` sets a
    hosted widget's UPROPERTYs by reflection (int/int64/float/double/bool/string/text/name, type-matched
    to the FRuiValue kind; unknown/mismatch skipped; `SynchronizeProperties` once the widget is
    constructed). The `UserWidget` element carries a `WidgetProps` map applied at construction and
    re-applied on diff (recovering the hosted widget via `SObjectWidget::GetWidgetObject`). Test
    `ReactiveUI.Umg.PropMap`: direct reflection application (5 typed props + string→FText coercion +
    unknown-skipped) and the end-to-end mounted element. **Note:** delegate TRAMPOLINES (binding Rui
    callbacks to a hosted widget's dynamic multicast delegates) remain a follow-on — they need
    per-signature UFUNCTION generation, a genuinely separate codegen effort.

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
- **Status:** RESOLVED 2026-07-12 — all 3 sub-surfaces delivered:
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
  - **Item-model views (SListView/STileView): DONE 2026-07-12.** The virtualized item-model
    adapter — the one genuinely large sub-surface — ships as `RUI::Slate::ListView` /
    `TileView` (`RuiListView.h/.cpp`, exported `SRuiListView`). Each generated/recycled row is
    an `SRuiListRow : STableRow<TSharedPtr<FRuiValue>>` that OWNS a per-row detached `FRuiRoot`
    sub-root rendering `RenderItem(item, index)` — an independent little reconciler per row over
    SListView's native generate/recycle lifecycle. The declarative shape is a **render prop**:
    a stable `Items` array (identity-keyed for row reuse, SListView's native contract) + a
    `TSharedPtr<FRuiItemRenderer>` closure (`MakeItemRenderer`, identity-compared like
    `MakeDrawFn`). **Reactive path:** re-handing a fresh `RenderItem` closure re-runs it against
    every LIVE row's sub-root in place (no widget churn — the SListView row is reused, only its
    content re-reconciles); a changed item set regenerates only the affected rows. Selection
    (`SelectionMode` none/single/singleToggle/multi + `OnSelectionChanged` forwarding the
    selected index) is wired. TileView adds construct-time `ItemWidth`/`ItemHeight` (reconstruct
    mask). **C++-FIRST by design** (the render closure is not markup-expressible, like
    `MakeDrawFn`/`MakeAssetBrush` — so NO `.uetkx` tag / schema entry; markup uses the
    non-virtualized `<VerticalBox>{Items.map(...)}` form). Rows generate only under an arranged
    geometry, so `SRuiListView::ForceGenerateRows(size)` drives deterministic headless generation
    (ticks the inner list measure→generate). Test `ReactiveUI.Widgets.ListView` proves generation
    (3 items → 3 sub-roots, renderer once per row), the reactive rebuild (renderer swap re-runs
    each row, zero churn), teardown (rows unmount with the root), selection-index forwarding, and
    TileView generation. **Note:** STreeView is NOT included — it needs a hierarchical item model
    (per-item child accessor) that the flat `FRuiValue` item type does not carry; that is a
    separate data-shape design (tracked, not blocking — the flat ListView/TileView complete the
    §4 virtualized-list parity target).

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
- **Status:** **RESOLVED** (ES-modules M3, CodegenVersion 3). The interpreter half died with HMR v2
  (interp deleted, TD-027). The registry half: a PRIVATE component's `RegisterComponentId` key is now
  the FILE-QUALIFIED emitted name `RuiPriv_<Basename>::<Name>` (built from `PrivNamespaceFor` — one
  source of truth), so two files' private same-named components hold distinct registry + HMR-map
  identities; exported components keep the short name (2106 ledger guarantees uniqueness). Pinned by
  `ContractFixtures/PrivPairA/B.uetkx` goldens + the TD-026 block in `ReactiveUIUetkxCodegenTest.cpp`.
  G-01 documented semantic: renaming a file renames `RuiPriv_<Basename>` ⇒ private members remount
  (state reset) on the next sweep. The preview's misleading "not compiled yet" hint for private
  components was fixed in the same window (`UetkxPreview.cpp` — "private — add `export` to preview").

## TD-027 — HMR v2: Live-Coding-driven whole-project HMR + `ReactiveUetkx` menu/window
- **Where:** `ReactiveUIInterp` (the interpreter executor), `RuiHmr.*`, `UetkxWatcher.cpp`,
  `ReactiveUIEditor` (new menu/window/commands/settings). Full design: `plans/archive/HMR_V2_PLAN.md`.
- **What/why deferred:** the shipped HMR makes a single-file INTERPRETER the default path — it can't
  resolve imports or run user hooks/effects, so a component using an imported hook (e.g. the
  `.hooks.uetkx` pattern) can't be hot-reloaded and (pre-fix) was swapped to a dead version. That is
  not React parity. The interim `aabec60` fix makes the interpreter fail SAFELY (keeps the compiled
  version, "rebuild to apply"), but the interpreter path is the wrong architecture for a compiled-C++
  library.
- **Production-grade resolution (planned, owner-approved):** DELETE the interpreter executor; make
  **Unreal Live Coding** the HMR engine — a Start/Stop mode (family parity with the Unity sibling's
  `UITKX Hot Reload` window) that, while active, recompiles + Live-Coding-patches on ANY `.uetkx` event
  (save / new / delete / rename / copy), automatically (no keystroke), state preserved, whole project.
  Adds the `ReactiveUetkx` main-menu + the HMR window + two rebindable shortcuts. `ReactiveUIInterp`
  shrinks to parser-only.
- **Sub-item deferred within v2:** the `Demos/…` submenu (port of the sibling's demo launchers under
  `ReactiveUetkx`) is NOT in the v2 scope — the PIE gallery already covers the demos; a menu launcher
  is a later nicety. Tracked as **TD-HMR-DEMOS** (below).
- **Status:** ✅ RESOLVED — shipped across `67f7035` (controller + watcher debounce), `1253510`
  (interpreter deleted; `ReactiveUIInterp` now parser-only; preview + acceptance §5 reworked to the
  compiled component), `fa819dc` (`ReactiveUetkx` menu + `SReactiveUetkxHmrPanel` window), `95db6ac`
  (commands + settings + in-window rebinding), `b02390f` (repeat-key bughunt). Build OK; drift 23/0/0;
  suite 99/99; gates green. Two deliberate naming deviations recorded in `plans/archive/HMR_V2_PLAN.md`'s
  status banner (controller is `FUetkxHmrController` not `FRuiHmr`; shortcut chords live only in the
  input binding manager, not the settings object). The live Live-Coding loop is owner-verified
  in-editor (no headless test can drive Live Coding).

## TD-HMR-DEMOS — `ReactiveUetkx ▸ Demos` launcher submenu
- **Where:** `ReactiveUetkxMenu.cpp` (a new `Demos` submenu next to HMR Mode / Preview / Debug).
- **What/why deferred:** the Unity sibling's `ReactiveUITK` menu launches each demo scene from the menu;
  ours could invoke the PIE gallery screens by name. Out of HMR v2 scope — the PIE gallery already
  exercises every demo, so a menu launcher is a convenience, not a gap.
- **Status:** OPEN — low priority; pick up when the gallery grows enough that a jump-list earns its keep.

## TD-HMR-XPLAT — live HMR is Windows-only (Live Coding); Hot Reload as the potential cross-platform path
- **Where:** `FUetkxHmrController` (the whole live-reload loop) + the console hider (`#if PLATFORM_WINDOWS`).
- **The limitation (documented, accepted for v1):** the live hot-reload loop depends on **Unreal Live
  Coding, which is Windows-only** (`Engine/Source/Developer/Windows/LiveCoding`, Win64-only build). The
  underlying tech (Molecular Matters' **Live++**) ships Windows + consoles only — no macOS/Linux; the
  same is true of every native-C++ hot-reload option in the ecosystem, because runtime machine-code
  patching is per-platform and macOS code-signing / hardened runtime is actively hostile to it. So:
  - **Windows editor:** full HMR (save → Live Coding patch → refresh, state preserved).
  - **Mac/Linux editor:** no Live Coding → `Start()` fails cleanly with "Live Coding module is not
    available" (no crash). The rest of the library is fully cross-platform — compile-to-C++, the Slate
    runtime, the watcher's compile-on-save codegen, and the drift gate all work; Mac/Linux devs iterate
    the way they already do for **all** UE C++ (edit → rebuild), since Live Coding is absent there
    regardless of our library. We are never worse than the platform's baseline; on Windows we're better.
  - **Ship note:** call this out on the Fab listing + docs — the library *runs* everywhere; the *live-
    reload DX* is Windows.
- **Potential solution (spiked by reading the 5.6 API, not yet prototyped):** the legacy **Hot Reload**
  module (`Developer/HotReload`, `IHotReloadModule` → `RecompileModule` / `DoHotReloadFromEditor` /
  `RebindPackages`, completion via `FCoreUObjectDelegates::ReloadCompleteDelegate`) **is cross-platform**
  and still present in 5.6. It lines up with our design (registry-FName component identity + the
  `ForEachLive`/`HmrRefreshAll` refresh seam; hook state lives in `ReactiveUICore`, which doesn't reload).
  The open risk:
  > Whether that's fatal depends on one question I can't answer by reading alone: does every render path
  > resolve components through the name registry, or do some hold raw `&Func` pointers across renders? If
  > it's fully name-indirected, a DLL swap works. If any raw pointers survive, it crashes. Plus Hot Reload
  > is deprecated, flakier, and slower (full-module reload vs incremental patch).
  >
  > Concrete next step I can actually run (on Windows): a throwaway branch that routes HMR through
  > `DoHotReloadFromEditor` instead of `LiveCoding->Compile()`, then exercises the refresh seam, and
  > observe: (a) does it survive without dangling-pointer crashes, (b) does hook state persist, (c) does
  > new code run. Windows can run both systems, so it's a clean lab for the Mac/Linux question — if it
  > survives here, it's a viable cross-platform fallback; if it crashes on `&Func`, we'd know we'd first
  > need to make composition fully name-based (bigger change, perf implications).
- **Status:** OPEN — limitation documented + accepted for v1 (Windows live HMR). The Hot Reload spike is
  the tracked avenue if cross-platform live HMR is ever prioritized; the whole-library cross-platform
  build/run is unaffected.

## TD-028 — `URuiHostWidget` has no props/viewmodel channel (audit N1)
- **Where:** `Plugins/ReactiveUI/Source/ReactiveUIUMG/Public/RuiHostWidget.h`
- **What/why deferred:** the ours-in-theirs UMG door hosts by `ComponentName` only — no
  `SynchronizeProperties` override, no Blueprint-passed initial props, no VM handoff (research
  D_interop b2 promised "BP can pass initial props and a VM"). Shipped minimal in Phase 6;
  surfaced by the 2026-07-14 audit (`plans/archive/AUDIT_2026-07-14.md` §11-N1). Workaround today:
  share state via a Signal or a FieldNotify VM read with `UseField` (documented in the UMG guide).
- **Production-grade resolution:** an `Instanced`/map UPROPERTY of initial props applied through
  the existing `ApplyPropMap`-style reflection path + an optional `TScriptInterface
  <INotifyFieldValueChanged>` VM property provided into the tree as context; `SynchronizeProperties`
  forwards designer edits; design-time stays placeholder-only. Tests: BP-set props reach the
  component; VM flows to `UseField`.
- **Status:** ✅ RESOLVED 2026-07-15 (`feat/v1-gate-closeout`) — `InitialProps`
  (TMap<FName,FString>) + `ViewModel` (TScriptInterface<INotifyFieldValueChanged>) UPROPERTYs on
  `URuiHostWidget`, published into the tree via the new `RuiHostProps` context seam (same
  provider-node pattern as the CommonUI activation seam); components read with
  `RUI::Umg::UseHostProp/UseHostProps/UseHostViewModel` (the VM feeds `UseField` directly).
  `SynchronizeProperties` re-publishes live (Update + FlushSync — no remount, hook state
  preserved); design time stays placeholder-only. Test `ReactiveUI.Umg.HostProps` (props + VM
  reach the component; live re-publish; quiet defaults). Docs: UMG guide "Passing props & a
  viewmodel from the Designer".

## TD-029 — `URuiActivatableScreen` lacks `GetDesiredFocusTarget()` (audit N2)
- **Where:** `Plugins/ReactiveUI/Source/ReactiveUICommonUI/Public/RuiActivatableScreen.h`
- **What/why deferred:** CommonUI restores gamepad focus via the activatable's
  `GetDesiredFocusTarget()`; our screen doesn't override it, so focus doesn't land on a designated
  widget on activation (research D_interop c2's `autofocus`). Surfaced by the 2026-07-14 audit
  (§11-N2). Workaround today: `RUI::Slate::UseFocus` + an activation effect (documented in the
  CommonUI guide).
- **Production-grade resolution:** a focus-target designation the hosted tree can set (e.g. a
  reserved prop or a `RUI::CommonUI::` focus-target registration hook) that the screen's
  `GetDesiredFocusTarget()` returns; gamepad-focus test on activation.
- **Status:** ✅ RESOLVED 2026-07-15 (`feat/v1-gate-closeout`) — `RUI::CommonUI::UseDesiredFocus`
  (pair with `RUI::Slate::UseFocus`) designates from inside the tree via the new
  `FRuiFocusTargetRegistry` context (screen-owned, provided by `FocusTargetProvider`; latest
  designation per commit wins, cleared on unmount). CommonUI's contract wants a UWidget while our
  tree is pure Slate, so `NativeGetDesiredFocusTarget()` returns the (now focusable) screen
  itself and `NativeOnFocusReceived` forwards the arriving focus to the designated widget; no
  designation → base behavior (a BP override still wins). Test `ReactiveUI.CommonUI.DesiredFocus`
  (mechanism headless + screen end-to-end + clear-on-teardown). Docs: CommonUI guide "Gamepad
  focus — UseDesiredFocus". The real-gamepad PIE pass stays on the owner verification list
  (`plans/REMAINING.md` §3).

## TD-030 — Family convergence: Unreal adopted `import "@…"`; Godot has not (INCLUDE_RETIREMENT_PLAN.md)
- **Where:** the shared `.uetkx`/`.uitkx`/`.guitkx` import grammar (grammar-contract skill,
  `fileScanLeg` corpus tier)
- **What/why:** Unity shipped `import "@Namespace"` first (a quoted, brace-less, `@`-prefixed
  side-effect specifier emitting a host-language `using`, plus a redundant-using hint and a
  `--tidy` codemod). This repo ported the SHAPE 2026-07-16 (`feat/include-retirement`) with a
  per-leg payload — a header PATH instead of a namespace — to retire raw `#include` lines from
  `.uetkx` preambles: `import "@Header.h"` emits `#include "Header.h"`, paired with an
  auto-included aggregator prelude (`FUetkxFileScan::AutoIncludedHeaders`) and the family's
  redundant-import hint (UETKX2317; Unity's UITKX2317 analogue). Godot (`ReactiveUI-Godot`,
  `.guitkx`) has NOT adopted the `@` form — GDScript resolves globally, so it never needed the
  escape hatch `#include` served here, and no request for one has surfaced there yet.
- **Production-grade resolution:** if the Godot repo ever needs an equivalent escape hatch (e.g.
  a preload path or an autoload/class reference the analyzer can't infer), port the SAME shape —
  `import "@<payload>"` — with a payload meaningful to GDScript, plus mirrored `fileScanLeg`
  corpus cases so the three repos' import grammars stay structurally aligned even though this
  particular form is leg-specific. Until then this is a one-way ledger entry, not a blocker.
- **Status:** OPEN (informational — no cross-repo PR pending). Unreal + the family's origin
  (Unity) both ship the `@` form; Godot tracked here for whenever the need arises.

## TD-031 — `<Provider>` element: slice-scoped context provision (owner decision 2026-07-17)
- **Where:** the family markup grammar (all three legs) + `ReactiveUICore` fibers + codegen/LSP
- **What/why:** context provision is hook-style (`ProvideContext(Handle, Value)` — family API,
  D-08.3): the provision boundary is the COMPONENT boundary, so providing to a SLICE of a
  component's markup requires extracting a child component. React spells this as a
  `<Context.Provider value={…}>` wrapper element scoping exactly its children. During the
  2026-07-17 testing session the owner reviewed the trade-off and DECIDED to adopt a
  `<Provider>` element in the future. No hard technical blocker exists: fragments already prove
  widget-less structural nodes, and since markup compiles to C++, a provider element can
  desugar to the existing typed call — "a fragment that carries a provided value" (a structural
  fiber holding the ProvidedContext map slot).
- **Production-grade resolution:** family RFC first (grammar change must land in all three
  repos or be recorded as a divergence): agree the element shape (e.g.
  `<Provider Context={GThemeCtx} Value={Theme}> …children… </Provider>`), then per leg: grammar
  + parser + a structural provider fiber + codegen desugar + LSP schema/completions + formatter
  + corpus cases + docs. Keep `ProvideContext` working (the element is sugar over it, not a
  replacement — same stack, same change detection).
- **Status:** OPEN — owner-approved direction ("we will switch to that in the future",
  2026-07-17); scheduled after the IDE-extension debugging passes; blocked on the family RFC.

## TD-032 — Markup-as-value (`auto X = (<Tag>…);`) — family grammar RFC (owner interest 2026-07-17)
- **Where:** the family markup grammar (all three legs) + codegen + formatter + LSP
- **What/why:** markup is legal only in return position; assigning a subtree to a local and
  embedding it with `{ X }` (a React idiom the owner reached for during the testing session,
  TESTING_BUGS.md TB-12) silently produced garbage C++ for MSVC. The stopgap shipped 2026-07-17:
  `UETKX0114` (error, both scanners, corpus-pinned) rejects the shape with a pointer here; the
  supported spelling is extracting a child component.
- **Production-grade resolution:** family RFC (the TD-031 lane): markup expressions as VALUES —
  grammar acceptance, codegen lowering to an `FRuiNode` local (the runtime type already exists
  and `{ X }` children already splice it), formatter layout for markup in initializer position,
  LSP/corpus in all three legs. No runtime work — this is purely a front-end feature.
- **Status:** **CLOSED (2026-07-17)** — shipped by the markup-everywhere campaign
  (`plans/MARKUP_EVERYWHERE_PLAN.md` §4): value-position markup lowers in place via the family
  jsx scan (`FUetkxJsxScan` was already the Godot-parity port; codegen's verbatim-splice sites
  now route through `EmitExpr`), the LSP neutralizes + lifts it, the family corpus pins it in
  all three repos (hash 917dd8cd…), and `UETKX0114` narrowed to the paren-less-return
  remainder. Formatter = family parity (reanchored, not restructured — a family-wide
  value-markup formatter remains future work, tracked by the corpus pin).

## TD-033 — LSP rename-symbol handler (ES-modules M6 — deferred, owner call pending)
- **Where:** `ide-extensions/lsp-server/src/server.ts` (no `onRenameRequest` handler exists)
- **What/why deferred:** G-12 lists rename among the sync surfaces, but no rename handler has
  EVER existed in this server — the ES-modules leg (M6) delivered completions / hover /
  go-to-def THROUGH aliases (rename imports, `* as`, default) and the plan marked a rename
  handler OPTIONAL scope with a TECH_DEBT entry as the sanctioned alternative
  (ES_MODULES_EXECUTION_PLAN.md §M6/§11.2). Correct rename needs the full cross-file reference
  set (tags + code refs + alias bindings + export lists + `export default` targets across the
  sweep universe) — a workspace-index feature, not an afternoon patch on the current
  per-request scans.
- **Production-grade resolution:** a WorkspaceEdit-producing rename over the swept-file index:
  rename at the DECLARATION renames every importer's binding (or inserts `as` to pin locals);
  rename at a LOCAL alias edits only the importer. Family-symmetric (the Godot/Unity servers
  lack rename too — candidate for a family fast-follow).
- **Status:** **RESOLVED (2026-07-18)** — shipped by the LSP-completion campaign
  (`plans/LSP_COMPLETION_PLAN.md`, N0–N3 + N6, same branch as ES-modules): a workspace
  reference index (`uetkxIndex.ts` — tags incl. close tags, code refs via the N-07 scope
  tracker, import/alias/export-list tokens) feeds find-all-references, prepareRename + rename
  with the N-04 semantics (decl rename edits every importer incl. `as`-target spellings;
  local-alias rename stays importer-local; plain-binding rename inserts `A as New`), refusal
  validation (N-05), document/workspace symbols, and four quickfix code actions
  (2305/2304/2301/2320 — the 2320 rewrite byte-matches the codemod). The N6 stretch landed
  too: embedded-C++ LOCALS rename through clangd over the virtual doc, all-or-nothing
  (cross-file/glue edits and partial-AST shapes refuse with a reason). The rename surfaces are
  family-firsts — the Godot/Unity servers remain candidates for the fast-follow port.

## TD-034 — ES-modules accepted caveats: alias/value reference rewriting limits (M2/M4)
- **Where:** `UetkxCodegen.cpp` (`RewriteAliases`, the generalized private-qualification branch),
  `UetkxResolve.cpp` (`CollectExternalRefs` Bare/Call kinds)
- **What:** (a) A body LOCAL VARIABLE that shadows an import alias or a same-file private
  value/util name is rewritten/qualified too — UETKX2325 polices declared-name collisions, not
  body locals (family watch-list limit, ES_MODULES_EXECUTION_PLAN.md §11). (b) Strict-usage
  policing of bare value references (2305) keys on the export table: a local identifier that
  happens to match a .uetkx-exported VALUE name diagnoses as a missing import. Both are the
  same accepted family model that hooks/module-quals always had — export-table names are
  effectively reserved within markup files. (c) Usage accounting (2304) scans setup/bodies but
  not markup attr expressions — a value used ONLY inside an attr expr may warn unused (warn
  severity; pre-existing hook behavior, now more visible with values).
- **Production-grade resolution:** scope-aware reference analysis in the rewrite planes (track
  local declarations in the brace walk) + jsx-aware attr-expr scanning for usage accounting.
- **Status:** **RESOLVED (2026-07-18, with a documented residual)** — the LSP-completion
  campaign (`plans/LSP_COMPLETION_PLAN.md` N4/N5) shipped exactly that resolution:
  (a)+(b) `FUetkxScopedLocals` (N-07; TS twin in `uetkxIndex.ts`, behaviorally identical and
  twin-pinned) tracks brace scopes + local declarations (params, `Type Name =/;/{`, `auto`,
  structured bindings) — locals shadowing an alias/private/exported name are no longer
  rewritten, qualified, Ctx-injected, or 2305'd (pre-declaration references still are);
  (c) `CollectExternalRefs` is markup-aware (N-08): attr/directive expressions join the
  reference set, so markup-only usage marks imports USED and missing imports diagnose 2305.
  **Residual (accepted):** the tracker is a heuristic, not a C++ parser — an unrecognized
  declaration shape stays invisible (its shadow still rewrites), and markup islands see a
  conservative locals-union seed (over-suppression = a silently missed diagnostic, never a
  false one). Contract-pinned by `ShadowedNames.uetkx`; corpus untouched.
