---
name: engine-catchup
description: The runbook for supporting a NEW Unreal Engine version (5.7, 5.8, …) — run the header-diff automation, classify what changed in Slate/UMG, and walk the implementation checklist (widgets, schema, docs version manifest, CI matrix, per-engine packaging). Ported from the Unity sibling's add-unity-version process. Use whenever Epic releases an engine version the plugin should support.
---

# Engine catch-up — supporting a new Unreal Engine version

The Unreal port of the Unity sibling's process (`AUTOMATION.md` +
`automation~/unity-api-diff.ps1` + `Plans~/VERSIONING_PROCESS.md` there). One run =
one new engine version, ending with the version added to every registry, gate, and
listing. Policy context: `VERSIONING.md` (supporting a new engine version is a MINOR bump).

## 0. Preconditions (owner)

- The new engine version installed via the Epic launcher (the diff script needs BOTH
  versions' `Engine/Source` on disk).
- Read Epic's release notes ("What's New" + upgrade notes for UMG/Slate) — the diff
  script sees API shape, not behavior changes; the notes are where behavior changes live.

## 1. Discovery — run the diff automation

```powershell
.\scripts\engine-api-diff.ps1 -From 5.6 -To 5.7 -OutFile .\scripts\diff-reports\5.6-to-5.7.json
```

Scans both engines' `Runtime/{SlateCore,Slate,UMG}/Public` headers and reports, as JSON:
added/removed **Slate widget classes**, per-widget added/removed
**SLATE_ARGUMENT/SLATE_ATTRIBUTE/SLATE_EVENT** names, and added/removed **UMG widget
classes**. It is a header scan (UE 5.4+ exports per-function, so widget classes carry no
`*_API` macro — the scan keys on `S-class : public S-class`). Reports are gitignored
(`scripts/diff-reports/`).

## 2. Classification — sort every reported item into exactly one bucket

| Bucket | Goes to |
|---|---|
| New Slate widget worth wrapping | `plans/WIDGET_INVENTORY.md` row (BATCH-3 or next batch) → the `component-pipeline` skill, one run per widget, with `sinceUE` noted |
| New argument/attribute/event on an ALREADY-WRAPPED widget | the widget's adapter + typed props struct + the schema (then `-run=RUIExportSchema` refreshes `uetkx-schema.json` → LSP + generated docs pages pick it up) |
| Removed/renamed API we consume | fix the adapter; if OUR public surface must change, follow `VERSIONING.md` deprecation policy |
| New widget NOT worth wrapping (editor-only, niche) | `WIDGET_INVENTORY.md` row with status + reason — "a widget with no row is a sweep bug, not a scope decision" |
| Behavior change (no API change — from the release notes) | test it; document in the docs page of the affected subsystem + Known Issues if user-visible |

## 3. Implementation checklist (in order; each row names its gate)

1. **Plugin manifest**: the dev repo's `ReactiveUI.uplugin` carries **NO `EngineVersion`
   pin** — that field is a hard per-version compatibility gate, not a floor declaration
   (a 5.6.0 pin made 5.7 show "'ReactiveUI' is Incompatible… Attempt to load anyway?",
   which headless/-unattended auto-answers NO → the plugin silently never loads and every
   adapter lookup dies). The floor lives in VERSIONING.md/README; per-engine zips stamp
   `EngineVersion` at packaging (release-process).
2. **Adapters/widgets**: per the classification — `component-pipeline` runs for new
   widgets; adapter edits for changed ones. Gate: the widget's suite + `ReactiveUI.Boot`.
3. **Schema**: `-run=RUIExportSchema`, commit the refreshed
   `ide-extensions/lsp-server/src/uetkx-schema.json` — the LSP completions AND the
   generated docs Components pages both read it. Gate: `ReactiveUI.Uetkx.Schema` +
   `docs-drift` (the 29-tags check will catch a count change — update the Components
   Overview prose in the same PR).
4. **Docs version manifest**: `ReactiveUIUnrealDocs~/src/versionManifest.ts` —
   add the version to `SUPPORTED_VERSIONS`; add `ELEMENT_VERSIONS` entries for new
   widgets (`sinceUE`), `PAGE_VERSIONS` for new pages. The version dropdown, sidebar
   filtering, and the generated pages' `VersionBadge` all key off this file. Gate:
   docs `npm run build && npm run lint`.
5. **CI matrix**: `.github/workflows/` engine matrix (`verify-engines` in publish.yml
   reads `["5.6", "5.7", …]`) + the `test-run`/CLAUDE.md environment facts if the owner's
   local default engine moves. Gate: armed engine legs green (or the local battery per
   engine when CI is unarmed).
6. **Full battery on the NEW engine**: build + `RUICompile -check` + the full
   `ReactiveUI` suite against the new version (the `test-run` skill's ladder, pointing
   `<Engine>` at the new install). Bench re-run per `plans/BENCH_BASELINES.md` rules if
   perf claims are printed per-engine.
7. **Packaging/release**: per-engine zip via the `release-process` skill; the Fab
   listing gains the new engine version (owner uploads). MINOR version bump + changelog
   line ("Adds Unreal Engine X.Y support").
8. **Docs prose**: README/VERSIONING.md compatibility tables; anything the release notes
   flagged as behavior-changed.

## 4. Done when

A fresh project on the NEW engine version installs the packaged plugin, enables it, and
the demo gallery renders (the `field-test-editor` skill's packaged-fidelity test) — and
every row above names green gates in the PR description.

## Scar tissue (why these steps exist)

- **The diff script exists** because the Unity sibling's manual "read the release notes
  and hope" pass missed IStyle properties two versions running; the assembly diff (here:
  header diff) made discovery mechanical. Ours is a TEXT scan — pair it with the release
  notes (step 0), never substitute for them: headers don't show behavior changes.
- **"A widget with no row is a sweep bug"** (WIDGET_INVENTORY's own law): unclassified
  additions are how coverage silently rots — every reported item lands in a bucket, even
  if the bucket is "not worth wrapping, because…".
- **Schema before docs** (step 3 before 4): the generated Components pages read the
  committed schema — refreshing docs first shows stale tables and the docs-drift tripwire
  fires in the wrong direction.
- **`EngineVersion` is a gate, not a floor** (learned on the 5.7 catch-up, 2026-07-14): the
  pinned dev uplugin made 5.7 skip loading the ENTIRE plugin in headless runs — 96 of 103
  tests never ran and the 7 that did crashed on empty registries. The dev repo stays
  unpinned; packaging stamps per-engine. Dropping SUPPORT for an old engine remains a
  MAJOR decision (VERSIONING.md), never a side effect of adding a newer one.
- **Run the battery, not just the build** (same catch-up): 5.7 compiled green on the first
  try while the plugin couldn't even LOAD there — "it builds on the new engine" proves
  almost nothing; the Boot check + full battery are where engine-compat problems surface.
- **Engine installs can be partial/husked** (5.8 catch-up — the owner freed disk space by
  removing UE_5.7's payload): the diff script then silently produced "+127 widgets added"
  against the empty side. The script now hard-fails on a <50-widget scan
  (`Assert-SurfaceSane`) — never weaken that guard, it caught a real wrong-diff.
- **Clean intermediates when switching engines BACK** (same catch-up): 5.8's UnrealHeaderTool
  output in `Intermediate/` doesn't compile on 5.6 (`ETypeConstructPhase`,
  `UE_WITH_CONSTINIT_UOBJECT`) and UBT does not always invalidate it — a switch-back build
  exploding with hundreds of errors in `*.generated.h` means stale UHT, not broken code:
  `rm -rf Intermediate/Build Plugins/ReactiveUI/Intermediate Binaries Plugins/ReactiveUI/Binaries`.
