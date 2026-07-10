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
- **Status:** OPEN

## TD-002 — Stylesheet third style layer (`@uss`/`@theme`)
- **Where:** grammar recognizes both directives (D-03) and diagnoses "post-v1"
- **What/why deferred:** v1 ships inline `style` dicts + `classes` (D-13); the stylesheet layer
  is family parity work with real design surface.
- **Production-grade resolution:** family-compatible stylesheet layer; the diagnostic retires.
- **Status:** OPEN

## TD-003 — Exit-animation protocol (delayed unmount)
- **Where:** reconciler-level (designed in Phase 7 item 5, shipped v1.x per the gate)
- **What/why deferred:** retrofit cost is reconciler-level so the DESIGN lands in Phase 7 even
  though shipping may slip past v1 (critique gap 3 resolution).
- **Production-grade resolution:** design doc committed in Phase 7; implementation + tests +
  docs in v1.x.
- **Status:** OPEN

## TD-004 — Drag-and-drop + keyboard-shortcut APIs
- **Where:** `ReactiveUISlate` event layer
- **What/why deferred:** input policy v1 covers handled/unhandled + CommonUI delegation
  (Phase 2/6); DnD + shortcuts are additive APIs (critique gap 15 tail).
- **Production-grade resolution:** typed DnD props over Slate's drag-drop ops + shortcut
  registration hook, with demos.
- **Status:** OPEN

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
- **Where:** `grammar-contract` skill §cross-repo; sibling repos' corpus files
- **What/why deferred:** Phase 3 creates this repo's corpus; the outbound mirrored-case PRs to
  the siblings land then — tracked here until BOTH merge.
- **Production-grade resolution:** corpus sections hash-match across all three repos at a
  release-time check.
- **Status:** OPEN (activates at Phase 3)

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
- **Status:** OPEN (decision RECORDED — minimal-move ships; this entry tracks the two
  accepted imperfections, not the strategy)

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
- **Status:** OPEN

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
- **Status:** OPEN

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
- **Status:** OPEN
