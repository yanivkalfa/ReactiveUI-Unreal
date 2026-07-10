---
name: component-pipeline
description: The production line for wrapping one Slate widget end-to-end — research the real header, fill the adapter from the template, style keys, contract case, test, docs. The repetitive lesser-model workload; one run = one widget. Use when adding any RUI::* widget wrapper (Phase 2 batch 1, Phase 7 batch 2, and beyond).
---

# Component pipeline — one Slate widget, six stations

Exit gate for a run: build green + `Automation RunTests ReactiveUI.Widgets.<Name>` green +
`ReactiveUI.Contract` green + docs-sync done. Model tiers (MASTER_PLAN §8): station 1 is
sonnet-class (judgment about the widget's API); stations 2–6 are haiku-class once the prop-map
entry exists. Anything requiring a NEW host-config *mechanism* (a new slot model, an item-view
adapter) is NOT this pipeline — STOP and escalate to an opus-class session.

## Stations

1. **Research the real widget.** Read the actual header —
   `Engine/Source/Runtime/Slate/Public/Widgets/**/S<Name>.h` (or SlateCore) — plus its style
   struct (`F<Name>Style` in `Styling/SlateTypes.h`). Enumerate every `SLATE_ARGUMENT`,
   `SLATE_ATTRIBUTE`, `SLATE_EVENT`, `SLATE_SLOT_ARGUMENT`, and post-construction setter.
   **The header outranks the docs** — Epic's web docs lag. Write the findings into the widget's
   entry in the prop-map schema (`templates/prop-map.schema.json` documents the shape) — the
   single source the adapter, the LSP vocabulary, and the docs all consume.
2. **Wrapper.** Instantiate `templates/widget_wrapper.template.cpp`: props struct
   (`RUI_PROP` rows), builder methods, adapter (Create with construct-only args → ApplyFull;
   ApplyDiff setter rows; Equals; child-container kind; reconstruct-on mask; event table rows).
3. **Style keys.** Add each styleable property to the central style-key registry with type,
   default, setter, and reset behavior. Hot-path keys (colors/opacity/font/padding) must NOT be
   on the reconstruct mask (MASTER_PLAN D-13).
4. **Contract case.** Add the widget's apply/remove/reset case to the contract corpus: props
   applied → widget state asserted; style prop removed → default restored; plain prop removed →
   NOT reset (the family semantic).
5. **Test.** Instantiate `templates/widget_test.template.cpp` →
   `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRui<Name>Test, "ReactiveUI.Widgets.<Name>", …)`.
   Demonstrate it red→green once (break the adapter, watch it fail, restore).
6. **Docs.** Run the `docs-sync` skill (new-widget row).

## Known traps (read BEFORE station 1)

- **`SLATE_ARGUMENT` vs `SLATE_ATTRIBUTE`**: attributes are `TAttribute<T>` and usually have
  runtime setters; arguments are construct-only → they go on the **reconstruct mask** (changing
  one replaces the widget: focus/caret/scroll loss — document per widget).
- We pass **constant** attribute values and mutate via setters — never bind delegate
  TAttributes (D-12; polling kills the fast path).
- **Slot props live on the SLOT, not the widget** (padding/alignment/fill/ZOrder) — they belong
  in the child's `slot.*` namespace, applied via the parent adapter's slot setters.
- **Events return `FReply`** on many widgets: the proxy returns `Handled()` for void user
  callbacks; expose the FReply-returning overload only where pass-through matters (D-27/Phase 2
  decision).
- Editable-text widgets: **skip the setter when the value is equal** (caret preservation) — the
  generalized self-notifying-setter guard (D-16).
- Not every FArguments arg has a setter — verify against the header, not hope; a missing setter
  means reconstruct-mask, not a silent no-op.
- Stateful widgets (editable text, list views) are **excluded from the widget pool**.

## Scar tissue (why these steps exist)

- **"Header outranks docs"**: a docs-sourced prop list once included an attribute the shipping
  header had removed — the header is ground truth, the sweep verifies against it.
- **The prop-map entry comes FIRST** (station 1 output): wrappers written before the schema
  entry drifted from the LSP vocabulary — one source, everything generated from it.
- **Red→green once** is mandatory: a widget test that never failed was later found to be
  asserting nothing.
- **The reconstruct mask is per-widget documented**, because "why did my style tweak eat my
  focus" is the exact bug class D-13 exists to prevent — keep hot-path styling off the mask.
