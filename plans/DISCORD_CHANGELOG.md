# Discord changelog

Community-facing release notes, newest first, pasted MANUALLY by the owner into the family
Discord server's `#unreal` channel after publishing — never posted by the AI.

**Entry format** (the family pattern — mirror `ReactiveUIToolKit/Plans~/DISCORD_CHANGELOG.md`):
`## [X.Y.Z] - YYYY-MM-DD`, a `### <headline>` subtitle, **bold-lead** prose paragraphs and
bullet groups distilled from that release's CHANGELOG section (real numbers, root causes named),
an update-instruction line, a `**Tooling:** UETKX <ver> (VS Code + VS 2022)` trailer when
extensions shipped, a battery/LSP test-count trailer, then a `---` separator. The repo's very
first announcement post lives at the BOTTOM of this file.

**Hard limit: ≤ 2000 characters per entry** (Discord message cap).

## [0.8.0] - 2026-07-16

### Wave 4 — TreeView lands; the widget-completion plan is DONE

**63 markup tags, 65+ wrapped widgets — every widget in the completion plan is now
implemented.** `TreeView` closes the last item-model gap (TD-022): a virtualized
hierarchical tree with per-row reconciler sub-roots, a `GetChildren` accessor, controlled
expansion (`ExpandedItems` in, `OnExpansionChanged` out), and `HeaderRow` columns built
from a declarative `Columns` list. C++-first like `ListView`. Plus `VectorInputBox` /
`RotatorInputBox` for controlled XYZ / Roll-Pitch-Yaw editing, and `Splitter2x2` — four
resizable quadrants routed by `Slot.Role`.

Also fixed: `ConstraintCanvas` slot ZOrder tripped an engine ensure when applied before
the slot committed — attach-then-apply now.

Update to **ReactiveUI for Unreal 0.8.0** (GitHub release). Battery `126/126`.

---

## [0.7.0] - 2026-07-16

### Wave 3 — popups, toasts, anchors, splitters

**60 markup tags.** `MenuAnchor` brings the popup primitive (anchor + `Slot.Role="menu"`
content, controlled open state); `NotificationList` + `PushNotification` bring toasts;
`ConstraintCanvas` brings UMG-style anchors to markup; `Splitter` brings resizable panes.
Plus `NumericDropDown`, `BreadcrumbTrail`, `LinkedBox`, `WindowTitleBarArea`,
`VirtualJoystick`, and `SearchableComboBox` — the first engine-gated widget (5.7+, schema-
annotated). 16 slot keys.

Update to **ReactiveUI for Unreal 0.7.0** (GitHub release). Battery `125/125`.

---

## [0.6.0] - 2026-07-16

### Wave 2 — 12 interactive composites + imperative handles

**50 markup tags.** New: `EditableText`, `InlineEditableTextBlock`, `VirtualKeyboardEntry`,
`ColorWheel`, `ColorSpectrum`, `ColorGradingWheel`, `InputKeySelector`, `VolumeControl`,
`RadialBox`, `TextScroller`, `LayeredImage`, `ExpandableButton` (role slots). Plus the
family's `UseImperativeHandle` ref-publishing form and `WidgetFromHandle<T>()` — call
`ScrollToEnd()`-class widget APIs through any captured `Ref`. Slider/ScrollBox/TextBlock
gained their remaining runtime setters (16 style keys).

Update to **ReactiveUI for Unreal 0.6.0** (GitHub release). Battery `125/125`.

---

## [0.5.0] - 2026-07-16

### Widget-completion wave 1 — 8 new widgets on the road to "1.0 wraps everything"

**The coverage campaign begins: 1.0 will ship with EVERY official runtime Slate widget
wrapped, and this is the first wave.** New tags: `ColorBlock`, `SimpleGradient`,
`ComplexGradient`, `Hyperlink`, `EnableBox`, `ScissorRectBox`, `BackgroundBlur`,
`InvalidationPanel` — 38 markup tags total.

**Tooltips, everywhere.** `ToolTipText` is a universal style key — put it on ANY element,
markup or C++, same as the Unity/Godot siblings' universal tooltip prop.

Also: `Button` gets `bIsFocusable`, `ProgressBar` gets `BarFillType`, the `RUI::Style()`/
`RUI::Slot()` C++ builders caught up with every markup key (and a new CI gate keeps it that
way), and a registry-wide contract test now enforces the widget-replacement rules every new
adapter must follow — the rails for the ~30 widgets still coming in waves 2-4.

Update to **ReactiveUI for Unreal 0.5.0** (GitHub release). Battery `125/125` · LSP `33/33`.

---

## [0.4.0] - 2026-07-16

### DOOM in the widget tree — plus localization closes the last v1 gap

**The family's marquee demo lands on Unreal, and a whole game frame costs ~0.2 ms of CPU.**
A fully playable raycast FPS where every wall strip, sprite, and tracer is a real keyed
`<Image>` in ONE `<Canvas>`, diffed by the reconciler at 60+ Hz. Six levels, 100% procedural
textures, zero assets — in the gallery under "Doom" (WASD + mouse, Ctrl+R shows FPS).
Measured: sim 19 µs, geometry 34 µs, reconcile + Slate apply of ~470 live widgets ≈ **197 µs
median per frame**.

**Localization.** `.uetkx` text has always compiled to real `NSLOCTEXT` — now the gather
pipeline is verified (add `*.inl` to your gather masks; a complete reference target ships),
and a culture switch re-renders every mounted screen live.

**Epic interop rounds out:** `URuiHostWidget` takes designer-set props + a viewmodel from the
Details panel (`UseHostProp`/`UseHostViewModel`), CommonUI screens answer
`GetDesiredFocusTarget()` for gamepad focus, and `UseOwnedViewModel` + public marshaling
helpers land.

**New in markup:** the universal `Ref={ }` attribute, the `Canvas` widget (absolute placement,
live slot mutation), a universal `Clipping` style key, `ScaleBox` alignment, and
`<Image Image={ Brush } />`.

**Fixed** (found by the Doom playtests): `ColorAndOpacity` was silently dropped on
`Image`/`Separator` from markup; `UseStableFunc`/`UseStableAction` were uncallable from markup.

Update to **ReactiveUI for Unreal 0.4.0** (GitHub release / Fab listing).
**Tooling:** UETKX 0.2.0 (VS Code + VS 2022) — completion inside embedded C++ is clangd-backed.
Battery `124/124` · LSP `33/33`.

---

## [0.3.0] - 2026-07-14

### Unreal Engine 5.8 support

**The full battery is green on 5.6, 5.7, AND 5.8.** The 5.6→5.8 API diff found no removals —
additive-only FArguments surface across 26 wrapped widgets, all recorded as v1.x candidates.

**Fixed:** UE 5.8 changed `FJsonObject`'s key type (`FString` → `UE::FSharedString`) — the
`.uetkx` sidecar reader now iterates keys engine-agnostically. The engine-api-diff tool
hard-fails on partial engine installs instead of emitting a wrong diff.

Update to **ReactiveUI for Unreal 0.3.0** (GitHub release). Battery `103/103` per engine.

---

## [0.2.0] - 2026-07-14

### Unreal Engine 5.7 support + first-beta fixes

**5.6 and 5.7, one plugin.** The dev `.uplugin` no longer pins `EngineVersion` (the pin
silently skipped loading the whole plugin on 5.7 headless); per-engine packages stamp it at
release. Full 5.7 deprecation sweep (TObjectPtr `AddReferencedObject`, FieldNotification
include, C5038).

**Fixed:**
- **Portal content no longer leaks on unmount** — deleting a subtree containing a portal now
  explicitly detaches its children from the external target panel.
- **`URuiHostWidget::Remount()` actually remounts** — UMG caches `RebuildWidget`'s return, so
  the host now swaps content inside a stable wrapper; runtime `ComponentName` changes work.
- Packaged README described a "pre-alpha scaffold"; it now states the beta reality.

Update to **ReactiveUI for Unreal 0.2.0** (GitHub release). Battery `103/103`.

---

## [0.1.0] - 2026-07-13

### First beta — the complete React-style stack for Unreal, in pure C++

**Function components return a virtual tree; a fiber reconciler patches real Slate widgets;
state lives in hooks.** 23 hooks (`UseState`, `UseEffect`, `UseContext`, signals, tweens, …),
structural error boundaries, `<Presence>` exit animations — no UObject dependency in the core.

- **35+ wrapped Slate widgets** incl. virtualized `ListView`/`TileView`; setter-based styling
  (a style tweak never rebuilds a widget); typed events, drag-and-drop, shortcuts.
- **`.uetkx` markup** — JSX-like, compiles to native C++ (committed, drift-gated); static
  imports/exports from day one with a migration codemod.
- **HMR** — save a `.uetkx`, the running UI patches mid-PIE with state preserved (Live Coding).
- **Epic interop** — UMG both directions, CommonUI activatable screens, MVVM/FieldNotify via
  `UseField`.
- **`@theme`/`@uss` stylesheets**, a 17-hook router, a 15-screen example gallery.
- **IDE tooling** — `.uetkx` language server + VS Code / VS 2022 clients.

**ReactiveUI for Unreal 0.1.0** is on GitHub (beta — API may shift before 1.0).
**Tooling:** UETKX 0.1.0 (VS Code + VS 2022).

---

## The initial announcement post (pinned here; owner posts once, before/with 0.1.0)

Building UI in Unreal shouldn't feel like fighting the engine. **ReactiveUI for Unreal** brings
the React component model to Slate — reusable function components, state hooks, and a JSX-like
markup language called **UETKX**. Write your UI in clean `.uetkx` files, hit save, and watch it
update mid-PIE with hot reload (state preserved) — and it all compiles to plain C++ for
shipping. No script VM in your game.

**What it does:**
- Function components with hooks (state, effect, context, memo, reducer, signals, and more)
- `.uetkx` markup compiled to native C++ at build time — zero runtime overhead
- Hot reload that patches the running UI in PIE without restarting (Live Coding, ~seconds)
- Full IDE support — completion, go-to-definition, diagnostics, formatting — for VS Code and
  Visual Studio 2022
- UMG, CommonUI, and MVVM stay in place: host inside UserWidgets, ship CommonUI screens, feed
  FieldNotify viewmodels straight into components
- Works with UE 5.6 – 5.8

**A quick taste:**
```jsx
component Counter {
	auto [Count, SetCount] = UseState<int32>(0);

	return (
		<VerticalBox>
			<TextBlock Text={ RUI::Fmt(TEXT("Count: {}"), Count) } />
			<Button OnClicked={ SetCount(Count + 1) }><TextBlock Text="+" /></Button>
		</VerticalBox>
	);
}
```

:clapper: **Demo video:** __VIDEO_URL__ <!-- the Doom demo recording — owner records -->
:book: **Docs & guides:** https://yanivkalfa.github.io/ReactiveUI-Unreal/
:package: **GitHub:** https://github.com/yanivkalfa/ReactiveUI-Unreal

Happy to answer any questions about the approach or architecture. More updates to come as we
work toward v1.0!

Our discord channel - https://discord.gg/Knedqu4Wyv
