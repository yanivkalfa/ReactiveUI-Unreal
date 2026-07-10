---
name: docs-sync
description: When a feature lands or behavior changes, update every documentation artifact it touches — driven by a change-type → checklist table, ended by the docs-drift tripwire. Use after merging any user-visible change (new hook, widget, grammar/directive change, behavior change, perf change).
---

# Docs sync

Input: the merged change (PR title/diff or plan phase). Look it up in the table; do every cell;
finish with the tripwire. Haiku-class-executable: table lookup + templated edits + one command.

## Change-type → artifact checklist

| Change | Update ALL of |
|---|---|
| New hook | docs hooks reference page (from `templates/hook_doc.template.tsx`) · README hook-count line (if claimed) · docs-site hooks table · `new-component` skill's hook list |
| New widget wrapper (`RUI::*`) | prop-map schema entry (single source — the LSP + docs both read it) · docs elements page regenerates · README widget-count line (if claimed) |
| New style key | style-keys docs section · the widget's prop-map entry |
| Grammar / directive change | `.uetkx` syntax docs page · `new-component` skill · LSP schema/vocabulary note · **cross-repo byte-compat: corpus case landed per `grammar-contract` (and siblings notified)** |
| Behavior change | README "Notes & limitations" · docs page of the affected subsystem · store-listing template if user-facing (`templates/fab-listing.template.md`) |
| Perf change | README/docs perf claims ONLY with the measured numbers from `plans/BENCH_BASELINES.md` (never unmeasured — MASTER_PLAN §6 DO-NOT-CLAIM) |
| New diagnostic code | the Diagnostics catalog source (docs page generates from it) |
| New CVar / console command | Debugging docs page · CLAUDE.md conventions block |

## The tripwire (always the last step)

```bash
node scripts/docs-drift.mjs
```

**The wiring rule**: any PR that introduces a user-facing COUNT claim ("N hooks", "M widgets")
must add a matching check to `scripts/docs-drift.mjs`'s CHECKS in the same PR — a claim without
a tripwire is how READMEs rot.

## Scar tissue (why these steps exist)

- The sibling repo's README once claimed "MVP / 10 elements / 6 hooks" while shipping ~60
  elements and 23 hooks — months stale, found by accident. The tripwire + this table are the
  automated version of "never again".
- **Counts are claimed in ONE place per artifact** and always tripwired — duplicated counts
  drifted independently.
- **Grammar changes notify siblings** — a "small" directive tweak that skipped the corpus once
  cost a cross-repo divergence hunt; the corpus case IS the notification.
