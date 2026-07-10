---
name: release-process
description: The ReactiveUI-Unreal release runbook — draining the pending changelog, version bumps, the two-lane changelog system, packaging per engine version, the merge→fast-forward→Publish flow, and the manual Fab checklist with its compliance items. Use when preparing, staging, or publishing a release of any artifact.
---

# Release process (ReactiveUI-Unreal)

Everything a release needs, in order. Deliverables version independently — check what actually
changed with `git diff origin/master --stat` against `Plugins/ReactiveUI/` and `ide-extensions/`
before deciding what bumps.

## 0. Drain the pending ledger

`plans/PENDING_CHANGELOG.md` accumulates one bullet per merged change (staged by
`plan-progress`). First step of every release: translate those bullets into Lane A/B entries
below, then EMPTY the file.

## 1. Versioning policy

**Patch by default** (`x.y.Z`). Minor for genuinely additive milestones; major/0.x-minor for
breaking. Perf-only changes to shipped bytes still bump + changelog. Bump via
`node scripts/bump.mjs <plugin|vscode|vs2022> <ver>` — it edits the manifest(s), keeps lockstep
partners in sync (vscode ⇒ vendored lsp-server; any lsp-server src change ⇒ BOTH extensions),
and prints the exact follow-up commands. Post-1.0 deprecations follow D-30: `UE_DEPRECATED` for
≥1 minor release + changelog notice before removal.

## 2. Changelogs — two lanes

### Lane A — the plugin (hand-written, mirrored)

1. Write the `## [X.Y.Z] — YYYY-MM-DD` section at the top of **root `CHANGELOG.md`**
   (keep-a-changelog: intro line, then `### Added/Changed/Fixed` with bold-lead bullets naming
   root causes).
2. Mirror: `cp CHANGELOG.md Plugins/ReactiveUI/CHANGELOG.md` — then `node scripts/verify-mirror.mjs`.
3. Entries cover the plugin surface only, never the demo host project.

### Lane B — the IDE extensions (generated from one json)

```bash
# 1. Message in a UTF-8 file (NEVER inline for text with em-dashes/quotes — argv transcoding)
node ide-extensions/scripts/changelog.mjs add --scope shared --message-file msg.txt --vscode X.Y.Z --vs2022 X.Y.Z
# 2. Regenerate EVERY target the entry names — the step that gets forgotten
node ide-extensions/scripts/changelog.mjs extract --ide vscode --out ide-extensions/vscode-uetkx/CHANGELOG.md
node ide-extensions/scripts/changelog.mjs extract --ide vs2022 --out ide-extensions/visual-studio/CHANGELOG.md
# 3. Gate locally — CI runs exactly this
node ide-extensions/scripts/changelog.mjs verify
```

### Discord (community note)

Entry at the top of `plans/DISCORD_CHANGELOG.md` matching the format pinned in that file.
**Hard limit ≤2000 characters** — count:
`awk '/^---$/{exit} {n+=length($0)+1} END{print n}' plans/DISCORD_CHANGELOG.md`.
Owner pastes it into the family server's `#unreal` after publishing — never posted by the AI.

## 3. Cross-repo grammar drift check

The `.uetkx` grammar is byte-compatible across the family (D-22e). Before release, hash-compare
the shared corpus files against the Unity and Godot repos (`grammar-contract` skill has the
procedure); a drift is a release blocker until reconciled or `.pending`-pinned.

## 4. Verification gates before committing

- `node ide-extensions/scripts/changelog.mjs verify` + `node scripts/verify-mirror.mjs`
- `node scripts/check-headers.mjs` + `node scripts/lint-skills.mjs` + `node scripts/docs-drift.mjs`
- The affected suites per dev-process's gate ladder (incl. `ReactiveUI.Boot`)
- Packaging sanity when shipping the plugin: `scripts/package-plugin.ps1` per supported engine
  version (`-StrictIncludes` is what Fab's toolchain effectively does), then the
  **packaged-fidelity test**: unzip into a FRESH project, enable the plugin, expect the
  `ReactiveUI <ver> loaded` banner + the demo renders (see `field-test-editor`).

## 5. Commit, merge, fast-forward

1. Commit release prep (bumps + changelogs together):
   `release: reactive_ui_unreal X.Y.Z[, extensions A.B.C] -- changelogs + version bumps`.
2. PR into `dev`; wait for green (changelog-sync + mirror are required checks); owner merges.
3. Fast-forward: `git fetch origin && git push origin origin/dev:master`.

## 6. Publish

- **The Publish button**: `.github/workflows/publish.yml` via workflow_dispatch — the owner runs
  it, never the AI. Jobs are idempotent and version-gated by tag (`v*`, `vscode-v*`,
  `vs2022-v*`): a job skips when its tag exists, so un-bumped artifacts don't release. The one
  `v*` tag carries ALL per-engine zips as release assets.
- **Fab (manual — no API exists)**, rendered from `templates/fab-listing.template.md`:
  1. One zip per supported engine version, each `.uplugin` carrying the matching `EngineVersion`;
     strip `Binaries/`/`Intermediate/` (package-plugin.ps1 does both).
  2. **Compliance items (review-enforced, D-29)**: the AI-generated-content disclosure line in
     the listing; an updated example project per engine version; per-file copyright lines
     (CI-linted); no trademark-adjacent listing title; supported-platforms declaration per D-28;
     support channel URL (GitHub Issues + family Discord).
  3. Upload, submit for review; expect days→weeks — GitHub Releases are already live meanwhile.
- **Discord**: owner pastes the prepared entry.
- **Repo metadata** (first public release): About/description/topics/social preview — AI drafts,
  owner applies.

## Scar tissue (why these steps exist)

- **"extract + commit" is mandatory**: the sibling repo's changelog.json fell 14 versions behind
  hand-edited files; the drift gate exists because of it and WILL fail the PR.
- **`--message-file`, not `--message`** — PowerShell/cmd transcode argv (mojibake) and strip
  embedded quotes; the script refuses corrupted text.
- **The mirror is byte-identical, literally** — resync with `cp`, never re-type.
- **`-StrictIncludes` locally before Fab** — marketplace builds compile `-DisableUnity -NoPCH`;
  include leakage that works locally is a guaranteed Fab rejection cycle (weeks).
- **The pending ledger gets DRAINED, not appended around** — bullets that never reached a lane
  were exactly how release notes went missing in the family before it existed.
