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
| STreeView | ✅ **shipped wave 4 (TD-022 CLOSED)** as `RUI::Slate::TreeView` — hierarchical item model: `GetChildren` accessor (`MakeChildAccessor`), per-row sub-roots, CONTROLLED expansion (`ExpandedItems` diff + `OnExpansionChanged`), C++-first like ListView |
| SHeaderRow | ✅ **shipped wave 4** as the P5c `Columns` protocol on TreeView — declarative `FRuiHeaderColumn` list (id/label/fill-width) builds the `SHeaderRow`; construct-only (column change rebuilds header) |

## Shipped (Batch 3 — wave 2, 2026-07-16, 0.6.0) — 12

| Widget | Element tag | Notes |
|---|---|---|
| SEditableText | `EditableText` | borderless text edit (D-16 caret rule). **SEditableLabel: SKIP — UE_DEPRECATED(4.21)** |
| SInlineEditableTextBlock | `InlineEditableTextBlock` | click-to-edit label; controlled edit mode |
| SVirtualKeyboardEntry | `VirtualKeyboardEntry` | IVirtualKeyboardEntry impl (touch keyboards) |
| SColorWheel | `ColorWheel` | controlled HSV color (capture begin/end + OnValueChanged) |
| SColorSpectrum | `ColorSpectrum` | same controlled-color contract (shared TRuiColorPickerAdapter) |
| SColorGradingWheel | `ColorGradingWheel` | the LIVE `AdvancedWidgets` one (Slate twin is UE_DEPRECATED(5.5)); new Build.cs dep; attribute surface protected → construct-masked |
| SInputKeySelector | `InputKeySelector` | key-capture + FInputChord; OnKeySelected/OnIsSelectingKeyChanged |
| SVolumeControl | `VolumeControl` | two controlled values (Volume/Muted) |
| SRadialBox | `RadialBox` | multi-slot radial panel |
| STextScroller | `TextScroller` | marquee; imperative start/stop via P2 handles |
| SLayeredImage | `LayeredImage` | SImage subclass; layer stack via wrapper shim |
| SExpandableButton | `ExpandableButton` | three role slots (`slot.role="collapsed"/"expanded"` + body) via SRuiExpandableButton |

## Shipped (Batch 3 — wave 3, 2026-07-16, 0.7.0) — 10

| Widget | Element tag | Notes |
|---|---|---|
| SConstraintCanvas | `ConstraintCanvas` | P5a anchor slot model (Slot.Anchors/Offset/Alignment/AutoSize/ZOrder); attach-then-apply slot fix in 0.8.0 |
| SSplitter | `Splitter` | P5b fraction slots (Slot.SizeRule/SizeValue/MinSize/Resizable) + OnSplitterFinishedResizing |
| SMenuAnchor | `MenuAnchor` | **P3 popup core**: anchor child + `Slot.Role="menu"` content; controlled bIsOpen |
| SNotificationList | `NotificationList` | **P4 toasts**: declarative mount + imperative `RUI::Slate::PushNotification` |
| SNumericDropDown | `NumericDropDown` | float form; values+labels lists |
| SBreadcrumbTrail | `BreadcrumbTrail` | declarative crumbs diffed onto the imperative stack (SyncCrumbs) |
| SLinkedBox | `LinkedBox` | shared FLinkedBoxManager per `GroupKey` (uniform-size sibling groups) |
| SWindowTitleBarArea | `WindowTitleBarArea` | OS window-chrome integration |
| SVirtualJoystick | `VirtualJoystick` | touch joystick (imperative FControlInfo via P2) |
| SSearchableComboBox | `SearchableComboBox` | **first `sinceUE: 5.7` widget** — compiled out on 5.6, schema-annotated |

## Shipped (Batch 3 — wave 4, 2026-07-16, 0.8.0) — 3 (+TreeView/HeaderRow above)

| Widget | Element tag | Notes |
|---|---|---|
| SVectorInputBox | `VectorInputBox` | SNumericVectorInputBox<float,…,3> alias; controlled X/Y/Z + per-component commit |
| SRotatorInputBox | `RotatorInputBox` | SNumericRotatorInputBox<float> alias; controlled Roll/Pitch/Yaw |
| SSplitter2x2 | `Splitter2x2` | D-W4: four resizable quadrants routed by `Slot.Role` (topLeft default /bottomLeft/topRight/bottomRight); live `Percentages` |

## Batch 3 — remaining (holds/reclassifications only; the wrap-all target is MET)

| Widget | Notes |
|---|---|
| SResponsiveGridPanel | **HOLD (D-W2)** — header says EXPERIMENTAL/"API may change drastically or be removed"; revisit when the banner drops |
| SRichTextHyperlink | **reclassified to the rich-text story (D-W3)** — needs a FSlateHyperlinkRun view-model ctor arg; an inline-run building block, not a tag |
| SToolTip (custom content) | prop form ✅ shipped wave 0 (`ToolTipText` style key — family contract: a prop, never a tag); custom-CONTENT tooltips (SetToolTip with markup) are a tracked follow-on in REMAINING |

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
