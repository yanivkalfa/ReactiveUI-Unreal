# Widget-completion plan — v1 = ALL official widgets (owner re-decision 2026-07-16)

> **Status: PLANNED** — research complete (engine-header recon of all 38 remaining widgets on
> UE 5.6/5.8 + family prior-art recon in both sibling repos, 2026-07-16); execution not started.
> **The re-decision this plan implements:** the locked ship gate phased Batch 3 as "v1.x";
> the owner overrode that 2026-07-16 — **full official-widget coverage (+ the TD-012 setter
> remainder) moves INTO the v1 gate. The 1.0 tag — and the Fab day-one listing — ship with
> everything.** MASTER_PLAN carries the banner; this file is the execution plan.
> **Versioning:** each wave ships as a pre-1.0 additive minor to GitHub (0.5.0, 0.6.0, …)
> exactly like 0.2→0.4 did — new tags/props are purely additive, no breaking changes, the
> schema/docs/LSP counts self-track through the existing drift gates. Fab never sees the
> intermediate states; it opens at 1.0 with the complete surface.

## §0 Owner decisions needed before/during execution

| # | Decision | Recommendation |
|---|---|---|
| D-W1 | **Spread attrs `{...obj}`** — the family is DIVERGENT: Godot shipped it (`V._spread_all`, corpus fixture "spread-with-key"); Unity deliberately omitted it (documented divergence, no diagnostic). Which do we align with? | Follow Unity (the origin): keep our UETKX3003 diagnostic, revisit if the family converges. Zero work now. |
| D-W2 | **SResponsiveGridPanel** — the header says EXPERIMENTAL, "API may change drastically or be removed". Wrap or hold? | HOLD with an inventory note (same reasoning as editor chrome: unstable surface = support burden). Revisit at the first engine version that drops the banner. |
| D-W3 | **SRichTextHyperlink** — requires a `FSlateHyperlinkRun::FWidgetViewModel` ctor arg; it is a rich-text building block, not a standalone widget. | Reclassify to the rich-text story (INFRA-adjacent), covered when `RichTextBlock` grows inline-widget support. Not a tag. |
| D-W4 | **SSplitter2x2** (bonus class found in SSplitter.h, 4 named quadrant slots) | Wrap in the same wave as SSplitter — cheap once the fraction-slot model exists. |

## §1 Ground truth from the recon (corrections to the inventory)

- **Deprecated — SKIP, record in inventory:** `SEditableLabel` (UE_DEPRECATED 4.21 → use
  SInlineEditableTextBlock); the **Slate-module** `SColorGradingWheel` (UE_DEPRECATED 5.5 —
  moved to **AdvancedWidgets** module, namespace `UE::ColorGrading`; the live one needs a NEW
  `AdvancedWidgets` dependency in ReactiveUISlate.Build.cs and is an FGCObject holding a
  UMaterial*).
- **Engine-version-gated:** `SSearchableComboBox` does NOT exist in 5.6 — it is the plugin's
  first `sinceUE: 5.7` widget (compile guards + schema annotation + docs badge per the
  engine-catchup skill §2).
- **Aliases:** `SVectorInputBox` = `SNumericVectorInputBox<float, TVector<float>, 3>`;
  `SRotatorInputBox` = `SNumericRotatorInputBox<float>`. We instantiate the float forms only
  (D-33 tags `VectorInputBox`/`RotatorInputBox`).
- **Non-standard Construct signatures** (won't fit the plain adapter template — each needs a
  small wrapper shim, the SRuiExpandableArea precedent): SLayeredImage (layers as extra
  Construct args), SLinkedBox (+shared FLinkedBoxManager), SBreadcrumbTrail (template),
  SRichTextHyperlink (+view-model — skipped per D-W3).
- **Tickers:** SEditableText, SVirtualKeyboardEntry, SVirtualJoystick, SRadialBox,
  STextScroller, SMenuAnchor, SInlineEditableTextBlock use active timers — mounted-widget
  tests must pump the active-timer path (the harness already does for SearchBox).

## §2 The five protocols (design-first; several widgets hang off each)

- **P1 — Universal `ToolTip` prop.** Family contract confirmed: tooltip is a universal
  base-prop attribute in BOTH siblings (Unity `BaseProps.Tooltip` → dict key `tooltip`;
  Godot `tooltip` → `tooltip_text`), NOT a wrapper widget. Ours: a universal `ToolTip` (FText)
  prop on FRuiPropsBase applied via `SWidget::SetToolTipText` — one reconciler touchpoint,
  one style-key-table entry, LSP/schema universal attr, docs row. `SToolTip` itself (custom
  tooltip CONTENT) becomes a follow-on: `ToolTipContent` accepting markup, applied via
  `SWidget::SetToolTip`. Small; rides Wave 0.
- **P2 — `UseImperativeHandle` + ref-commands.** The family hook exists in both siblings
  (`UseImperativeHandle<THandle>(factory, deps)`; Unity's media widgets expose
  Play/Pause/Stop/Seek controllers through it). Port the hook into RuiContext; refs already
  capture host handles, so native calls (`ScrollToEnd`-class) go through a typed accessor on
  the host handle (`Ref->As<SScrollBox>()`-shape, exact API at design time). Unlocks:
  BreadcrumbTrail's crumb stack, TextScroller start/stop, InlineEditableTextBlock
  Enter/ExitEditingMode, ScrollBox imperative scrolls (TD-012), NotificationList pushes.
- **P3 — Popup/menu protocol (family greenfield — we set the precedent).** Neither sibling
  has popup anchoring (Godot reaches PopupMenu imperatively via ref; Unity wraps only
  DropdownField). Ours: `MenuAnchor` tag over SMenuAnchor — anchor content = default child,
  popup content = a named slot (`Slot.Role="menu"`, the ExpandableArea two-named-slot
  precedent), controlled `bIsOpen` + `OnMenuOpenChanged` (D-16 skip-when-equal). The Special
  table's SPopup/SMenuOwner/STextEntryPopup/etc. story rides this. Dependents:
  NumericDropDown, SearchableComboBox, BreadcrumbTrail crumb menus.
- **P4 — Toast/notification protocol (family greenfield).** SNotificationList is populated
  imperatively (`AddNotification(FNotificationInfo)`) — no declarative slots. Ours: a
  `UseNotifications` hook returning a push function + a `NotificationList` mount tag; pushes
  are commands (P2 shape), content from FNotificationInfo-mapped props. Design doc first.
- **P5 — New slot models.** (a) ConstraintCanvas anchors: `Slot.Anchors` (FAnchors literal
  "minX,minY,maxX,maxY"), `Slot.Offset` (FMargin), `Slot.Alignment`, `Slot.AutoSize`,
  `Slot.ZOrder` — all live setters (SetPosition precedent from Canvas). (b) Splitter
  fractions: `Slot.SizeRule`/`Slot.Value`/`Slot.MinSize`/`Slot.Resizable` + `OnSplitterFinishedResizing`
  → controlled-fraction rule (user drag reports back; D-16). (c) HeaderRow columns: FColumn
  sub-objects — designed with TreeView in Wave 4.

## §3 Waves — the execution order

Ordering rationale: infrastructure-that-prevents-drift first (Wave 0), count-wins second
(Wave 1 mechanical), protocols before their dependents (P2 in Wave 2; P3/P4/P5 in Wave 3),
the big item-model design last among widgets (Wave 4), grammar as an independent parallel
campaign (Wave G — different subsystem, can interleave). Each wave = one campaign
(1 branch, 1 PR, full battery + drift gates + docs-sync), shipped as one minor.

### Wave 0 — anti-drift rails + the free wins (rides with Wave 1's PR or precedes it)

1. **TD-011 meta-test**: battery test iterating registered adapters — any adapter declaring a
   reconstruct-mask bit must implement `ConstructOnlyChanged` (turns the checklist into a gate
   before ~30 new adapters land).
2. **TD-013 builder generator**: engine-free node script generating the `RUI::Style()`/
   `RUI::Slot()` fluent methods from `uetkx-schema.json` (committed output + a
   verify-mirror-style drift gate). First run adds the missing `Clipping()`.
3. **P1 universal `ToolTip` prop** (family contract; trivial, high value).

### Wave 1 — mechanical leaves (8 widgets) → ships as 0.5.0 (with Wave 0)

`ColorBlock`, `SimpleGradient`, `ComplexGradient`, `Hyperlink`, `EnableBox`,
`ScissorRectBox`, `BackgroundBlur`, `InvalidationPanel` — plain component-pipeline runs;
delegation-friendly per the §8 matrix (templates + evidence packet per widget). Include the
TD-012 easy setters that touch no design: SProgressBar marquee/fill type, SButton
IsFocusable.

### Wave 2 — interactive composites (12 widgets) + P2 → 0.6.0

Design P2 (`UseImperativeHandle` + host-handle typed accessor) FIRST, then:
`EditableText`, `InlineEditableTextBlock`, `ColorWheel`, `ColorSpectrum`,
`ColorGradingWheel` (AdvancedWidgets dep — new Build.cs entry + GC note), `InputKeySelector`,
`VolumeControl`, `VirtualKeyboardEntry`, `RadialBox`, `TextScroller`, `LayeredImage`
(Construct shim), `ExpandableButton` (named slots — ExpandableArea precedent).
TD-012 remainder riding along: SButton sounds + press/click methods, SSlider bar/handle
colors + orientation/locked, SCheckBox per-state images, SScrollBox scrollbar knobs +
ScrollToEnd-class imperatives (P2), STextBlock shaping/highlight/strike/overflow/line-height.

### Wave 3 — the protocol widgets → 0.7.0

P3 + P5 designs, then their dependents:
`MenuAnchor` (P3 core), `NumericDropDown` (float instantiation), `SearchableComboBox`
(**sinceUE 5.7** — first version-gated widget: compile guard, schema annotation, docs badge),
`ConstraintCanvas` (P5a), `Splitter` + `Splitter2x2` (P5b, per D-W4), `BreadcrumbTrail`
(P2+P3; float/FString instantiation), `LinkedBox` (shared-manager wrapper: one manager per
markup parent, the family "group" shape), `WindowTitleBarArea` (window-chrome; pairs with
the shipped CreateInWindow surface), `VirtualJoystick` (imperative FControlInfo model via
P2; touch platforms — document desktop no-op), P4 toasts (`NotificationList` +
`UseNotifications`).

### Wave 4 — item-model tails (TD-022 closure) → 0.8.0

`TreeView` + `HeaderRow` together (they're one design): extend the shipped ListView
item-model (per-row FRuiRoot sub-roots) with a get-children accessor + expansion state
(controlled `ExpandedItems` + `OnExpansionChanged`) and the FColumn sub-object protocol
(P5c). Then the templated input rows: `VectorInputBox`/`RotatorInputBox` (float forms;
construct-only args → TD-011 reconstruct masks — the meta-test from Wave 0 catches misses).
Plus the 5.7/5.8 additive FArguments backlog on wrapped widgets (`sinceUE` annotations,
engine-catchup §2) — mechanical, delegation-friendly.

### Wave G — grammar alignment (TD-015; independent campaign, interleave anywhere)

Family recon corrected the old assumptions:
1. **Early component-level returns** — Unity's mechanism is NOT a return-splitter: the body
   is emitted VERBATIM and each markup span (including inside early returns) is spliced in
   place; C#'s own control flow branches. Ours mirrors it: the codegen already captures
   setup + markup ranges — extend the range collector to include `return ( <X/> )` spans
   anywhere at component depth-0, splice like Unity's `SpliceSetupCodeMarkup`. Adopt Unity's
   corpus/golden case shapes (MultiReturn_* series) as .uetkx contract goldens.
2. **Short-circuit `cond && <Tag/>`** — SHIPPED in Unity (Phase 1.5 desugar to
   `cond ? <Tag> : null`), so this is a PORT, not a family proposal: implement the same
   desugar at our splice sites, retire UETKX3002, corpus cases mirrored.
3. **Spread attrs** — per D-W1 (recommend: keep omitted, Unity-aligned; UETKX3003 stays).
4. `classes={ expr }` string-form conditionals — neither sibling has it; stays post-1.0.
Every change lands WITH corpus cases per grammar-contract; scanner changes mirror to the TS
language server in the same PR.

### Continuous / exit criteria

- Each wave: WIDGET_INVENTORY rows move to Shipped; schema/LSP/docs counts re-pinned;
  per-widget contract cases; battery green; docs-sync table walked.
- **1.0 gate (revised):** Batch-3 table EMPTY (every row Shipped, SKIP-annotated, or
  reclassified per D-W2/D-W3) + TD-012 remainder drained + TD-022 closed + Wave G items 1-2
  landed + the existing ship-gate audit run per engine → tag 1.0 → Fab listing (day-one full
  surface).
- TD-016 stays trigger-based: the first multi-field event (likely Wave 3's menu/drag work)
  triggers the schema `eventPayloads` field-list extension.

## §4 Estimates (calibrated on Batch 2: 14 widgets in ~1 day on the pipeline)

| Wave | Content | Rough size |
|---|---|---|
| 0+1 | rails + 8 mechanical + easy setters | 1 campaign day |
| 2 | P2 design + 12 moderate + setter sweep | 2-3 campaign days |
| 3 | P3/P4/P5 designs + 11 protocol widgets | 3-4 campaign days (designs dominate) |
| 4 | TreeView/HeaderRow + templated rows + sinceUE backlog | 2 campaign days |
| G | early returns + short-circuit port (+corpus, both impls) | 1-2 campaign days |

Owner-side per wave: merge the PR, optionally spot-check in PIE. Delegation per MASTER_PLAN
§8: Waves 1 and 4's mechanical halves are lesser-model conveyor work (skill + templates +
evidence packet); protocol designs and Wave G stay on the lead model.
