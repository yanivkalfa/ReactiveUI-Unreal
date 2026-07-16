# Slate widget inventory — every runtime widget, accounted for

**Owner directive (2026-07-10): the coverage target is ALL official user-facing widgets in the
engine's RUNTIME Slate modules** (`Runtime/Slate` + `Runtime/SlateCore`, UE 5.6 header sweep,
119 headers) — matching the Godot port, whose ~60 `V.*` factories cover essentially every
official Control. Editor-module widgets (graph panels, detail rows, dock chrome) are out of
scope the same way the Godot port never wrapped editor chrome.

This file is the authoritative tracking artifact: **every S-class from the sweep appears in
exactly one table.** A widget with no row here is a sweep bug, not a scope decision.

**Naming rule (D-33):** element tag = Slate class minus the `S` type-prefix; OUR widgets keep the `Rui` mark. Factories match tags verbatim.

Statuses: `SHIPPED` (adapter + tests on dev) · `BATCH-2` (Phase 7 step 8 production line —
the everyday game set) · `BATCH-3` (v1.x long tail — wrapped after Batch 2, before "all
official" is declared done) · `INFRA` (base class / internal part — not a user widget, nothing
to wrap) · `SPECIAL` (covered by a dedicated mechanism, not a plain adapter).

---

## Shipped (Phase 2) — 15

| Widget | Element tag | Notes |
|---|---|---|
| STextBlock | `TextBlock` | + style keys ColorAndOpacity/Font.Size/Justification/AutoWrapText |
| SButton | `Button` | event proxy exemplar; ContentPadding |
| SBorder | `Border` | padding/color/align |
| SBox | `Box` | size overrides + align |
| SVerticalBox | `VerticalBox` | Slot.* props (SBoxPanel child) |
| SHorizontalBox | `HorizontalBox` | slot.* props (SBoxPanel child) |
| SOverlay | `Overlay` | also the SRuiRoot panel; slot.zorder |
| SCanvas | `Canvas` | absolute placement: slot.position/slot.size (LIVE in-place slot mutation — the per-frame-movement hot path); paint order = child order; the Doom-demo framebuffer container (feat/doom-demo, 2026-07-15) |
| SScrollBox | `ScrollBox` | runtime Orientation (audit fix) |
| SSpacer | `Spacer` | |
| SImage | `Image` | v1 tint+size; brush content = D-17 asset work |
| SEditableTextBox | `EditableTextBox` | controlled input, caret rule (D-16) |
| SCheckBox | `CheckBox` | |
| SSlider | `Slider` | + StepSize |
| SProgressBar | `ProgressBar` | + fillColor style key |
| SRuiCanvas *(ours)* | `RuiCanvas` | the draw_fn paint trampoline (D-12); `Canvas` reserved for SCanvas |

## Shipped (Phase 7 — batch 2, rolling) — 14

| Widget | Element tag | Notes |
|---|---|---|
| SWidgetSwitcher | `WidgetSwitcher` | index-switched panel; WidgetIndex runtime setter; test `ReactiveUI.Widgets.Batch2` |
| SScaleBox | `ScaleBox` | Stretch + StretchDirection |
| SThrobber | `Throbber` | NumPieces + Animate |
| SWrapBox | `WrapBox` | Orientation/WrapSize/InnerSlotPadding/UseAllottedSize |
| SMultiLineEditableTextBox | `MultiLineEditableTextBox` | multi-line controlled input (D-16 caret rule) |
| SSearchBox | `SearchBox` | controlled search text (OnTextChanged/Committed); test `ReactiveUI.Widgets.Batch2b` |
| SSafeZone | `SafeZone` | title-safe + per-side padding |
| SDPIScaler | `DPIScaler` | DPIScale |
| SSeparator | `Separator` | Orientation/Thickness (construct-only, TD-011 reconstruct mask) + ColorAndOpacity |
| SSpinBox | `SpinBox` | numeric input (SSpinBox<float>); Value/Min/Max/Delta + OnValueChanged; test `ReactiveUI.Widgets.Batch2c` |
| SUniformWrapPanel | `UniformWrapPanel` | uniform-cell wrap; SlotPadding/HAlign |
| SRichTextBlock | `RichTextBlock` | Text (inline markup) + AutoWrapText |
| SGridPanel | `GridPanel` | slot.column / slot.row cell placement |
| SUniformGridPanel | `UniformGridPanel` | uniform cells by slot.column / slot.row |

## Shipped (Batch 3 — wave 1, 2026-07-16, WIDGET_COMPLETION_PLAN) — 8

| Widget | Element tag | Notes |
|---|---|---|
| SColorBlock | `ColorBlock` | fully construct-only (no engine setters) — entire surface reconstruct-masked |
| SSimpleGradient | `SimpleGradient` | two-stop gradient; construct-only, masked |
| SComplexGradient | `ComplexGradient` | N-stop gradient (`GradientColors` expr); construct-only, masked |
| SHyperlink | `Hyperlink` | `OnNavigate` event; Text/Padding construct-only, masked |
| SEnableBox | `EnableBox` | renders child as-if-enabled |
| SScissorRectBox | `ScissorRectBox` | hardware scissor clip (render-transform-safe) |
| SBackgroundBlur | `BackgroundBlur` | live BlurStrength/BlurRadius/bApplyAlphaToBlur/Padding |
| SInvalidationPanel | `InvalidationPanel` | opt-in paint cache (`bCanCache`) |
| — | style key `ToolTipText` | P1 universal tooltip (never a tag — family contract) |

## Batch 2 (Phase 7 step 8) — remaining (special designs / item-model)

| Widget | Notes |
|---|---|
| SComboBox | ✅ **shipped** (TD-012 tail) as `RUI::Slate::ComboBox` — dropdown reusing the ListView render-prop for the selected display + generated rows (FRuiRoot sub-roots); controlled `SelectedIndex`; menu verified headless via the interaction harness |
| SNumericEntryBox | ✅ **shipped** (TD-012 tail) as `RUI::Slate::NumericEntryBox` — controlled numeric field; display read back from the inner editable text via the harness |
| SSuggestionTextBox | ✅ **shipped** (TD-012 tail) as `RUI::Slate::SuggestionTextBox` — controlled text + case-insensitive substring suggestion filter (`OnShowingSuggestions`) |
| SSegmentedControl | ✅ **shipped** (TD-012 tail) as `RUI::Slate::SegmentedControl` — labelled tab-bar selector (`Labels` bake the segments = construct-only reconstruct mask; `SelectedIndex` controlled; `OnSelectionChanged` fires the index) |
| SExpandableArea | ✅ **shipped** (TD-012 tail) as `RUI::Slate::ExpandableArea` — the family's first TWO-NAMED-SLOT widget (children carry `slot.role="header"`/`"body"`; controlled `bIsExpanded` + `OnExpansionChanged`; `SRuiExpandableArea` reparents into two SBox holders) |
| SListView / STileView | ✅ **shipped** (TD-022) as `RUI::Slate::ListView` / `TileView` — the family's item-model treatment (declarative `Items` + `RenderItem` render-prop → per-row `FRuiRoot` sub-roots over SListView's virtualized generate/recycle). C++-first (render closure not markup-expressible). |
| STreeView | item-model with a hierarchical data shape — needs a per-item child accessor the flat `FRuiValue` item type doesn't carry; designed together with SHeaderRow in WIDGET_COMPLETION_PLAN wave 4 (TD-022 closure) |
| SHeaderRow | column headers for the item views — FColumn sub-object protocol (P5c); wave 4 with STreeView |

## Batch 3 — **IN v1 (owner re-decision 2026-07-16; was "v1.x")** — target: all official

> Execution: [WIDGET_COMPLETION_PLAN.md](WIDGET_COMPLETION_PLAN.md) (waves 1-4 + protocols
> P1-P5). Engine-header recon 2026-07-16 annotated each row below (module / gating / wave).

| Widget | Notes |
|---|---|
| SConstraintCanvas | anchor-based absolute panel — P5a anchors slot model (Slot.Anchors/Offset/Alignment/AutoSize/ZOrder, all live setters); wave 3 |
| SSplitter (+ SSplitter2x2, same header) | P5b fraction slot model (Slot.SizeRule/Value/MinSize/Resizable + OnSplitterFinishedResizing controlled rule); wave 3 |
| SEditableText / SInlineEditableTextBlock | text-edit variants (rich setters; both tick via active timers); wave 2. **SEditableLabel: SKIP — UE_DEPRECATED(4.21)**, superseded by SInlineEditableTextBlock |
| ~~SHyperlink~~ | ✅ **shipped wave 1** (see Shipped table). **SRichTextHyperlink: reclassified to the rich-text story (D-W3)** — needs a FSlateHyperlinkRun view-model ctor arg; an inline-run building block, not a tag |
| SColorWheel / SColorSpectrum | color pickers (capture-begin/end + OnValueChanged); wave 2. ~~SColorBlock~~ ✅ shipped wave 1 |
| SColorGradingWheel | **the Slate-module one is UE_DEPRECATED(5.5) — SKIP; the live one is in the `AdvancedWidgets` module** (namespace UE::ColorGrading; FGCObject holding a UMaterial*) → NEW Build.cs dependency; wave 2 |
| ~~SSimpleGradient / SComplexGradient~~ | ✅ **shipped wave 1** |
| SVectorInputBox / SRotatorInputBox | aliases of SNumericVectorInputBox<float,…,3> / SNumericRotatorInputBox<float> — templated, construct-only (TD-011 reconstruct masks); wave 4 |
| SNumericDropDown | templated (float form); spawns a menu → rides P3; wave 3 |
| SBreadcrumbTrail | templated + fully imperative crumb-stack API → rides P2+P3; wave 3 |
| SInputKeySelector | key-capture mode + FInputChord (InputCore, already linked); rich runtime setters; wave 2 |
| SVolumeControl | two controlled values (Volume/Muted) + events; wave 2 |
| SVirtualJoystick / SVirtualKeyboardEntry | touch controls; joystick is imperative FControlInfo config (P2) + ticks; keyboard entry implements IVirtualKeyboardEntry + ticks; waves 3/2 |
| ~~SBackgroundBlur~~ | ✅ **shipped wave 1** |
| SMenuAnchor | **P3 popup protocol core** (spawns windows; ticks; controlled bIsOpen); wave 3 |
| SNotificationList | **P4 toast protocol** — imperative AddNotification(FNotificationInfo), no declarative slots; wave 3 |
| ~~SToolTip~~ | ✅ **shipped wave 0** as the universal `ToolTipText` style key (family contract — a prop, never a tag); custom-CONTENT tooltips (SetToolTip with markup) remain a wave-3 follow-on |
| SLayeredImage | SImage subclass (SlateCore); layers via EXTRA Construct args + imperative AddLayer/SetLayerBrush → wrapper shim; wave 2 |
| STextScroller | marquee (ticks; imperative start/stop via P2); wave 2 |
| SLinkedBox / SRadialBox | LinkedBox needs a shared FLinkedBoxManager across siblings (wave 3); RadialBox is a real multi-slot panel with radial params (wave 2). ~~SEnableBox / SScissorRectBox~~ ✅ shipped wave 1 |
| SResponsiveGridPanel | **HOLD (D-W2)** — header says EXPERIMENTAL/"API may change drastically or be removed"; revisit when the banner drops |
| SExpandableButton | named-slot state machine (Collapsed/Expanded/Child slots + expansion events); wave 2 |
| ~~SInvalidationPanel~~ | ✅ **shipped wave 1** |
| SWindowTitleBarArea | OS window-chrome integration (SetGameWindow, fullscreen toggle delegate); pairs with CreateInWindow; wave 3 |
| SSearchableComboBox | **5.7+ ONLY (absent from 5.6)** — the first `sinceUE`-gated widget (compile guard + schema annotation + docs badge); popup + item-source model rides P3; wave 3 |

## Special — covered by a dedicated mechanism

| Widget | How it's covered |
|---|---|
| SWindow | `FRuiRoot::CreateInWindow` mount surface (shipped); portals target windows in Phase 7 |
| SViewport | the game viewport mount surface (`CreateInViewport`, shipped) |
| SPopup / SMenuOwner / SSubMenuHandler / STextEntryPopup / STextComboPopup / STextComboBox / SEditableComboBox | the menu/popup/combo story rides the SMenuAnchor design (Batch 2/3 with SComboBox) |
| SErrorText / SErrorHint / SPopUpErrorText | form-validation story (with the input widgets) |
| SDockTab | editor tab mount surface — ships with Phase 8's Inspector (editor module) |
| SUserWidget | UMG's bridge class — the ReactiveUIUMG interop module (Phase 6) |

## Infrastructure / internal — nothing to wrap

`SWidget`, `SPanel`, `SCompoundWidget`, `SLeafWidget` (base classes) · `SBoxPanel` (base of
V/H box) · `STableViewBase`, `STableRow`, `SExpanderArrow`, `SHeader` (item-view internals —
consumed by the item-model design) · `SScrollBar`, `SScrollBarTrack`, `SScrollBorder`
(scroll internals, reached through scroll-box props) · `SNullWidget`, `SMissingWidget`,
`SWeakWidget`, `SVirtualWindow`, `SLayerManager`, `STooltipPresenter`, `SFxWidget`
(deprecated in-engine) · non-widget helpers from the sweep: `SlateEditableTextLayout`,
`SlateTextBlockLayout`, `SlateEditableTextTypes`, `SlateControlledConstruction`,
`SlateWidgetTracker`, `SlateAccessible*` (accessibility plumbing).

---

**Process:** each Batch-2/3 widget ships through the component pipeline (prop-map entry →
typed props struct → adapter → contract case → per-widget test → docs row). When a batch
lands, move its rows into "Shipped" with the tag names. Re-run the header sweep against every
new engine version and diff this file — new engine widgets must land a row here in the same
PR that bumps the supported engine version.

## UE 5.7/5.8 additive surface (engine-catchup 2026-07-14 — scripts/diff-reports/5.6-to-5.8.json)

5.6→5.8: THREE Slate widgets added (SSearchableComboBox — a real BATCH-3 candidate; SDockingNode/SDockingTabStack — editor docking chrome, out of scope), one UMG extension class (UContextDataWidgetExtension, not a wrappable widget), ZERO removed anywhere. 26 widgets gained ADDITIVE FArguments
surface; the ones we wrap are candidates for v1.x exposure (each needs a 5.7+ version guard,
schema `sinceUE: 5.7` annotation, and docs badge — see the `engine-catchup` skill §2):

| Widget (wrapped) | New in 5.7 |
|---|---|
| SButton | arg `AllowDragDrop`; events `OnReceivedFocus`, `OnLostFocus`, `OnSlateButtonDragDetected/Enter/Leave/Over/Drop` |
| SEditableTextBox / SMultiLineEditableTextBox | events `OnBeginTextEdit`, `OnAllFontFacesFinishLoading`, `OnCursorMovedWithSelection` (5.8); attr `FontFacesLoadingPaintPolicy` (5.8) |
| SScrollBox | args `AllowContentToShrink`, `ScrollBarRightClickDragAllowed` (5.8); event `OnFocusChanging` (5.8) |
| SListView / STileView (our lists) | args `bEnableShadowBoxStyle`, `EnableProximateEntryNavigation` (5.8); events `OnTouchStart/Move/End` (5.8) |
| SSpinBox | event `OnEditEditableText`; attrs `CtrlAltMultiplier`, `ShiftAltMultiplier`, `VirtualKeyboardDismissAction` (5.8) |
| STextBlock / SRichTextBlock / SSearchBox | `FontFacesLoadingPaintPolicy` + font-loading/text-edit events (5.8) |

Not wrapped / not ours: SColorWheel, SHeaderRow, SHyperlink, SInvalidationPanel, SRadialBox,
SScrollPanel, STreeView, SSuggestionTextBox (the ENGINE's — ours is SRuiSuggestionTextBox),
SEditableText, SMultiLineEditableText — tracked by the diff report, no action until wrapped.
