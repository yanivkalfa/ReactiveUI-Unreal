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

## Batch 2 (Phase 7 step 8) — remaining (special designs / item-model)

| Widget | Notes |
|---|---|
| SComboBox / SComboButton | templated items → item-model treatment shares SListView design (TD-022) |
| SNumericEntryBox | numeric input variant (SSpinBox ✅ shipped) |
| SSuggestionTextBox | text-input family (SSearchBox ✅ shipped) |
| SSegmentedControl | templated selector |
| SExpandableArea | ✅ **shipped** (TD-012 tail) as `RUI::Slate::ExpandableArea` — the family's first TWO-NAMED-SLOT widget (children carry `slot.role="header"`/`"body"`; controlled `bIsExpanded` + `OnExpansionChanged`; `SRuiExpandableArea` reparents into two SBox holders) |
| SListView / STileView | ✅ **shipped** (TD-022) as `RUI::Slate::ListView` / `TileView` — the family's item-model treatment (declarative `Items` + `RenderItem` render-prop → per-row `FRuiRoot` sub-roots over SListView's virtualized generate/recycle). C++-first (render closure not markup-expressible). |
| STreeView | item-model with a hierarchical data shape — needs a per-item child accessor the flat `FRuiValue` item type doesn't carry; separate design (TD-022 note) |
| SHeaderRow | column headers for the item views (TD-022) |

## Batch 3 (v1.x long tail — target: all official)

| Widget | Notes |
|---|---|
| SCanvas / SConstraintCanvas | absolute-position panels (slot.offset/anchors) |
| SSplitter | resizable panes (slot.fraction) |
| SEditableText / SEditableLabel / SInlineEditableTextBlock | raw/label text-edit variants |
| SHyperlink / SRichTextHyperlink | link widgets |
| SColorBlock / SColorWheel / SColorSpectrum / SColorGradingWheel | color pickers |
| SSimpleGradient / SComplexGradient | gradient leaves |
| SVectorInputBox / SRotatorInputBox | struct input rows |
| SNumericDropDown | numeric preset dropdown |
| SBreadcrumbTrail | templated trail |
| SInputKeySelector | key binding capture |
| SVolumeControl | slider+mute composite |
| SVirtualJoystick / SVirtualKeyboardEntry | touch controls |
| SBackgroundBlur | blur container |
| SMenuAnchor | popup anchoring (pairs with the menu/popup story) |
| SNotificationList | toast notifications |
| SToolTip | via a `tooltip` prop on every element (cross-cutting, not a tag) |
| SLayeredImage | multi-layer image |
| STextScroller | marquee-style scroller |
| SEnableBox / SLinkedBox / SScissorRectBox / SResponsiveGridPanel / SRadialBox | niche containers |
| SExpandableButton | niche composite |
| SInvalidationPanel | perf wrapper — expose as an opt-in container |
| SWindowTitleBarArea | custom title bars (pairs with the SWindow surface) |

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
