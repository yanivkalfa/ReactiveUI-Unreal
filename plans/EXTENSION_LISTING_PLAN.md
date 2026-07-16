# Extension marketplace-listing overhaul — UNREAL leg (family campaign, owner directive 2026-07-16)

> **The problem (owner-reported, with screenshots):** all six family extensions appear on the
> VS Marketplace publisher page as bare acronyms — `UETKX`, `UITKX`, `GUITKX`, each TWICE
> (VS Code + VS2022) — indistinguishable from each other. Worse, THIS repo's VS Code extension
> has **no README.md at all**, so its marketplace page body is empty.
>
> **The fix (all three repos, same canonical scheme):** distinguishable display names, and every
> extension page structured **Title → Description → Features → Requirements → Changelog** (the
> shape the UetkxVsix VS2022 page already has). Sibling plans:
> `ReactiveUI-Gadot/plans/EXTENSION_LISTING_PLAN.md` and
> `UnityComponents/Assets/ReactiveUIToolKit/Plans~/EXTENSION_LISTING_PLAN.md`.
>
> **Execution notes for the implementing model:** work on ONE branch (`feat/extension-listing`)
> off `dev`, follow the `release-process` skill for every release mechanic (it has a
> "Marketplace listing surfaces" section added in this campaign), never hand-edit a generated
> file, and run every gate listed in §6 before pushing. The owner merges + presses Publish.

## §0 Where every marketplace-visible string lives (researched 2026-07-16)

| Surface | VS Code extension | VS2022 extension |
|---|---|---|
| List/page title (H1 next to icon) | `ide-extensions/vscode-uetkx/package.json` → `displayName` (currently `"UETKX"`) | `ide-extensions/visual-studio/UetkxVsix/source.extension.vsixmanifest` → `<DisplayName>` (currently `UETKX`) |
| Short description (under the title) | same package.json → `description` | same vsixmanifest → `<Description>` |
| Page BODY | `ide-extensions/vscode-uetkx/README.md` — **DOES NOT EXIST → empty page (the bug)** | `UetkxVsix/overview.md`, generated at publish time by `changelog.mjs extract-overview` from `UetkxVsix/overview-template.md` + `ide-extensions/changelog.json` (see `publish.yml` "Generate overview.md" step) |
| Changelog tab / section | `vscode-uetkx/CHANGELOG.md` (generated from changelog.json — Lane B) renders as the marketplace "Changelog" tab; the body ALSO gets a `## Changelog` section (this plan) | appended to overview.md by `extract-overview` |
| Version | package.json `version` (0.3.1 after this plan) | vsixmanifest `Identity Version` |

`changelog.mjs extract-overview` already works for ANY `--ide` (it filters `changelog.json`
entries by `versions[ide]`); only its usage string claims `<vs2022>` — fixed in §3.

## §1 Canonical strings (family-wide decision — do not improvise)

| Field | New value |
|---|---|
| package.json `displayName` | `UETKX (Unreal - VS Code)` |
| vsixmanifest `<DisplayName>` | `UETKX (Unreal - VS2022)` |
| VS Code body H1 (readme-template) | `# Reactive UI - Unreal Engine - VS Code (UETKX)` |
| VS2022 body H1 (overview-template) | `# Reactive UI - Unreal Engine - VS2022 (UETKX)` |
| `description` / `<Description>` | keep the current sentence (it already names the engine); do NOT shorten |

Body structure (BOTH templates, in this exact order):

```markdown
# <H1 from the table>

<1-2 paragraph description: editor support for `.uetkx`, the JSX-like markup of
[ReactiveUI for Unreal](https://github.com/yanivkalfa/ReactiveUI-Unreal) — a React-style
reactive UI library for Unreal Engine 5.6+, in pure C++.>

## Features
<the existing bullets from UetkxVsix/overview-template.md — highlighting, markup IntelliSense,
import intelligence, diagnostics, formatting. For the VS Code variant, reword the
Format-on-Save pointer to the VS Code setting instead of Tools ▸ Options.>

## Requirements
<the existing "everything is bundled" paragraph>

## Changelog
<NOT written by hand — extract-overview appends it. The template file ENDS after Requirements.>
```

## §2 File changes (the work)

1. **`ide-extensions/vscode-uetkx/package.json`** — `displayName` per §1. Nothing else.
2. **`ide-extensions/visual-studio/UetkxVsix/source.extension.vsixmanifest`** — `<DisplayName>`
   per §1. Nothing else (Description stays).
3. **NEW `ide-extensions/vscode-uetkx/readme-template.md`** — write it per §1's structure,
   adapting the content of `UetkxVsix/overview-template.md` (same features, VS Code wording:
   the Format on Save option is the `uetkx.formatOnSave` setting — confirm the exact setting id
   in package.json `contributes.configuration` before writing it).
4. **`ide-extensions/visual-studio/UetkxVsix/overview-template.md`** — replace the H1
   (`# UETKX — ReactiveUI for Unreal` → per §1). Keep everything else.
5. **Generate + COMMIT the VS Code README** (the same generated-and-committed pattern as the
   Lane B CHANGELOGs):
   ```bash
   node ide-extensions/scripts/changelog.mjs extract-overview --ide vscode \
     --template ide-extensions/vscode-uetkx/readme-template.md \
     --out ide-extensions/vscode-uetkx/README.md
   ```
6. **`.vscodeignore`** (in vscode-uetkx; create if absent) — ensure `readme-template.md` is
   excluded from the .vsix (README.md itself must NOT be excluded).

## §3 Script + CI wiring (keep it un-driftable)

1. **`ide-extensions/scripts/changelog.mjs`**:
   - Usage/help text: `extract-overview --ide <vscode|vs2022> --template <path> [--out file]`.
   - **Extend `verify`**: when `ide-extensions/vscode-uetkx/readme-template.md` exists, compose
     template + changelog exactly as `cmdExtractOverview` does and byte-compare against the
     committed `README.md` — mismatch fails with "run extract-overview … --out README.md".
     (Same scar as the CHANGELOGs: generated-and-committed files MUST have a drift gate.)
2. **`.github/workflows/publish.yml`**, `publish-vscode` job — add a step BEFORE
   `vsce package` (mirror of the vs2022 "Generate overview.md" step):
   ```yaml
   - name: Generate README.md from centralized changelog
     if: steps.check.outputs.skip == 'false'
     run: node ide-extensions/scripts/changelog.mjs extract-overview --ide vscode --template ide-extensions/vscode-uetkx/readme-template.md --out ide-extensions/vscode-uetkx/README.md
   ```
   (Belt and suspenders: the committed README is already current thanks to verify; the CI regen
   guarantees the packaged one includes the release's own changelog entry.)

## §4 Release mechanics (release-process skill, Lane B)

1. `node scripts/bump.mjs vscode 0.3.1` then `node scripts/bump.mjs vs2022 0.3.1`
   (LISTING-ONLY changes still bump — shipped bytes change; patch per the versioning policy).
2. Lane B entry — ONE `add` PER change (message via `--message-file`, never inline):
   - bullet 1: "Marketplace listing overhaul: distinguishable display names — `UETKX (Unreal - VS Code)` / `UETKX (Unreal - VS2022)` — and a structured page body (Title / Description / Features / Requirements / Changelog) on both marketplaces."
   - bullet 2 (vscode-scoped, `--scope vscode`): "The VS Code marketplace page now has a body at all — README.md is generated from the centralized changelog (it was missing entirely)."
3. Extract BOTH CHANGELOGs + regenerate README (§2.5) + `changelog.mjs verify`.
4. `plans/PENDING_CHANGELOG.md`: stage the bullets, then drain them into Lane B and EMPTY the
   file per release-process §0 (do not skip the drain).

## §5 What does NOT change

- Plugin version (0.9.0) — untouched; `release-plugin` will skip on the existing tag.
- publisher (`ReactiveUITK`), extension ids (`uetkx` / `UetkxVsix.ReactiveUITK`) — renaming IDs
  would orphan installs. ONLY display strings change.
- icons, categories, keywords, repository links.

## §6 Gates before push (all must be green)

```bash
node ide-extensions/scripts/changelog.mjs verify      # now also covers README.md
node scripts/verify-mirror.mjs
node scripts/check-headers.mjs                        # new .mjs edits keep the header line
node scripts/docs-drift.mjs
cd ide-extensions/lsp-server && npm test              # 33/33 (untouched, but prove it)
cd ../vscode-uetkx && npm run build                   # client bundle still builds
```

Then: PR `feat/extension-listing` → `dev` → owner merges → `git push origin origin/dev:master`
→ owner presses **Publish** (only the two extension legs fire — 0.3.1 tags).

## §7 Post-publish verification (owner or model with browser access)

- Publisher page (`marketplace.visualstudio.com/manage/publishers/reactiveuitk`): the two rows
  read `UETKX (Unreal - VS Code)` and `UETKX (Unreal - VS2022)`.
- `items?itemName=ReactiveUITK.uetkx`: page has the full §1 body INCLUDING the Changelog section.
- `items?itemName=ReactiveUITK.UetkxVsix`: H1 is `Reactive UI - Unreal Engine - VS2022 (UETKX)`.
- Open VSX (`open-vsx.org/extension/ReactiveUITK/uetkx`): README rendered there too.
