# ReactiveUI-Godot Production Ecosystem Inventory (reference for ReactiveUI-Unreal "step 0")

Repo root: `c:\Yanivs\GameDev\ReactiveUI\ReactiveUI-Gadot` (GitHub: `yanivkalfa/ReactiveUI-Godot`). Four independently-versioned deliverables in one repo, one manual Publish button, two changelog lanes, headless test harness, Claude skills encoding house process.

---

## 1. Top-level folder structure

```
.asset-template.json.hb          # Handlebars template for classic Godot Asset Library "version edit" (runtime addon listing)
.asset-template-editor.json.hb   # Same for the editor-addon listing (Custom download provider → release-zip URL)
.claude/skills/                  # 4 house skills: dev-process, release-process, field-test-editor, new-component
.gitattributes                   # export-ignore ALLOWLIST shaping `git archive` / AssetLib zips (see §10)
.github/workflows/               # test.yml, publish.yml, ide-extensions.yml
.gitignore                       # generated .gd siblings, .godot/, test scratch dirs, publisher-secrets.json
CHANGELOG.md                     # runtime library changelog (hand-written keep-a-changelog; SOURCE, mirrored into addon)
CLAUDE.md                        # agent instructions: repo map, commands, architecture, conventions
LICENSE, README.md, icon.png     # root docs/branding (root README = repo landing; addon carries its own)
ReactiveUIGodotDocs~/            # docs site (React 19 + Vite + MUI). Trailing `~` = Godot importer skips the dir
addons/reactive_ui/              # deliverable 1: runtime addon (GDScript). Self-contained: own README/CHANGELOG/LICENSE
addons/reactive_ui_editor/       # deliverable 2: in-Godot .guitkx editor plugin. Own README/CHANGELOG/LICENSE
build/                           # gitignored exported builds; export_presets.cfg at root
discordpost                      # canned Discord announcement copy (marketing text, kept in repo)
examples/                        # demo gallery (main.tscn + examples/app.gd); NOT shipped; generated .gd gitignored
ide-extensions/                  # deliverable 3: lsp-server/ + vscode/ + visual-studio/ + grammar/ + scripts/ + changelog.json
plans/                           # live design/status docs + archive/ + DISCORD_CHANGELOG.md + TECH_DEBT.md
research/                        # background research artifacts (GODOT_UI_SURFACE.md, dump_ui_surface.gd, godot_ui_surface.json)
tests/                           # headless GDScript suites + benches + contract/ (GD↔TS grammar corpus)
project.godot                    # the repo IS a runnable Godot project (demo gallery = main scene)
```

**Trailing-`~` trick**: Godot's importer ignores any dir ending in `~`, so the Node/Vite docs project lives inside the game project without being scanned/imported. **Unreal equivalent concern**: UnrealBuildTool/UHT scans `Source/`, and the editor's asset registry scans `Content/`; anything Node/TS (docs site, LSP server) must live outside those (repo root sibling dirs are naturally safe in Unreal — a plugin repo layout of `Plugins/ReactiveUI/` + top-level `docs/`, `ide-extensions/` never gets scanned). The real Unreal analogue problems: keeping non-plugin dirs out of the packaged plugin (`.uplugin` + Marketplace zip must whitelist, cf. `.gitattributes` allowlist) and keeping `Intermediate/`, `Binaries/`, `DerivedDataCache/`, `Saved/` gitignored (analogue of `.godot/`).

Also notable: the **repo doubles as the demo project** (open in Godot, press Play). Unreal analogue: a host `.uproject` at root with the plugin under `Plugins/`, demo maps in `Content/`.

---

## 2. CI/CD workflows (`.github/workflows/`)

### `test.yml` — "Tests"
- **Triggers**: push + pull_request on `[master, dev]`. `permissions: contents: read`. `env: GODOT_VERSION: "4.7.0"` (pinned to the verified engine; library floor is 4.1+).
- **One job** `headless-tests` (ubuntu-latest), toolchain via `chickensoft-games/setup-godot@v2`. Ordered steps (order is load-bearing):
  1. Compile all markup: `godot --headless --path . --script res://tests/guitkx_build.gd`
  2. Contract goldens fresh: `... res://tests/contract_dump.gd -- --check` (GD half of the GD↔TS grammar parity harness)
  3. Class cache: `godot --headless --path . --editor --quit || true` (editor import scan so global class_names resolve headlessly)
  4. Then 10 suites, one step each: `core_test.gd`, `style_test.gd`, `router_match_test.gd`, `router_spine_test.gd`, `update_test.gd`, `demos_test.gd`, `guitkx_test.gd`, `hmr_test.gd`, `guitkx_lsp_test.gd`, `guitkx_editor_test.gd`.

### `ide-extensions.yml` — "IDE Extensions"
- **Triggers**: push on `[master, dev]` with `paths:` filter (`ide-extensions/**`, `addons/reactive_ui_editor/CHANGELOG.md`, the workflow itself); PRs on `[master, dev]` with **NO paths filter** — deliberately, because these jobs are *required status checks* and must always report a conclusion or unrelated PRs become permanently un-mergeable. (Key gotcha to replicate.)
- **Jobs**:
  - `changelog-sync` — `node ide-extensions/scripts/changelog.mjs verify`. Header comment documents the incident it guards: hand-edited CHANGELOG.md let `changelog.json` fall **14 versions behind** (GUITKX 0.6.0–0.8.4).
  - `language-server` — `npm ci && npm run build`, unit tests `node --test out/test/*.test.js`, then e2e stdio LSP smoke `node scripts/smoke.js`.
  - `vscode-extension` — build server + extension, `node scripts/bundle-server.js`, `npx --yes @vscode/vsce package --no-dependencies --out guitkx.vsix`, upload artifact `guitkx-vsix`.
- VS2022 is excluded from this matrix (needs VS SDK/Windows; built only in publish).

### `publish.yml` — "Publish" (the Publish-button release model)
- **Trigger**: `workflow_dispatch` only. `permissions: contents: write`. `concurrency: group: publish, cancel-in-progress: false`.
- **Model**: ONE Run-workflow button publishes EVERYTHING whose version changed. Each job **reads its own version from its own manifest, checks whether the matching git tag exists, skips if so, and tags-on-success** — idempotent re-runs; un-bumped artifacts simply don't release. The AI never triggers it; the user clicks it.
- **Jobs**:
  - `deploy-docs` — NOT version-gated (every dispatch redeploys). `npm ci && npm run build` in `ReactiveUIGodotDocs~`, then git-worktree push of `dist/` to the `documentations` branch (GitHub Pages serves branch root); `touch .nojekyll`; skips commit when no diff.
  - `release-addon` — reads `version=` from `addons/reactive_ui/plugin.cfg` (`grep -oP '^version="\K[^"]+'`); tag `v<ver>`; if tag absent: **re-runs the full headless test suite**, zips `dist/reactive_ui-<ver>.zip` (only `addons/reactive_ui`), pushes tag as `github-actions[bot]`, creates GitHub Release via `softprops/action-gh-release@v2` with `generate_release_notes: true`.
  - `assetlib-update` — `needs: release-addon`, runs only if new tag AND repo variable `ASSETLIB_ASSET_ID` set ("inert until armed"). `sed`-materializes `.asset-template.json.hb` (`{{ version }}`, `{{ commit }}`), posts version edit via `deep-entertainment/godot-asset-lib-action@v0.6.0` with secrets `ASSETLIB_USERNAME`/`ASSETLIB_PASSWORD`. Emits a `::notice` reminding the NEW Asset Store (store.godotengine.org) still needs a manual version-add (no API).
  - `release-editor-addon` — same pattern, version from `addons/reactive_ui_editor/plugin.cfg`, tag `editor-v<ver>`. Extra: **bundles a pinned native analyzer** (`ANALYZER_VERSION: "0.6.1"`) downloaded from the sibling `gdscript-analyzer` repo's release; **refuses partial bundles** (checks descriptor + every platform binary: `gdscript_gdext.windows.x86_64.dll`, `libgdscript_gdext.linux.x86_64.so`, `.linux.arm64.so`, `.macos.universal.dylib`). Runs only the editor-relevant suites against the bundle. Release body = **top `## [` section of its CHANGELOG.md** extracted via `awk '/^## \[/{n++} n==1'` + appended dependency/bundle notes.
  - `assetlib-editor-update` — as above, armed by `ASSETLIB_EDITOR_ASSET_ID`; editor listing uses "Custom" download provider (release-zip URL) since export-ignore strips it from git archives.
  - `publish-vscode` — **6-leg platform matrix** (`win32-x64/arm64`, `linux-x64/arm64`, `darwin-x64/arm64` ↔ napi packages `@gdscript-analyzer/core-<triple>`), because the bundled server embeds a native napi addon so no universal `.vsix` exists. **Per-target idempotency tag** `vscode-v<version>-<target>` — a single shared tag once stranded 5 platforms after one leg succeeded (documented in-file). Cross-platform binary fetch trick: `npm pack "@gdscript-analyzer/<napi>@<resolved-ver>"` + manual untar into `node_modules` (plain `npm i` rejects cross-target with EBADPLATFORM). Regenerates `CHANGELOG.md` from changelog.json at publish time (`changelog.mjs extract --ide vscode`). `bundle-server.js --target <t>` then `vsce package --target <t>`; publishes to VS Marketplace (`VSCE_PAT`) and Open VSX (`OVSX_TOKEN`, `continue-on-error: true`); tags only on vsce success; uploads artifact `guitkx-vscode-<ver>-<target>`.
  - `publish-vs2022` — windows-latest. Version from `source.extension.vsixmanifest` `Identity/@Version`; tag `vs2022-v<ver>`. Builds server, bundles into VSIX dir, `fetch-node.ps1` embeds a pinned `node.exe` (self-contained ~80 MB VSIX, no Node on user PATH). Generates `overview.md` via `changelog.mjs extract-overview --template overview-template.md`. Builds via vswhere→VsDevCmd→`msbuild -t:Restore/Build/CreateVsixContainer`. **Verifies the VSIX zip actually contains `server/server.js` + `server/node.exe`** before publishing. Publishes via `VsixPublisher.exe` from the NuGet cache with `publishManifest.json` + `VS_MARKETPLACE_TOKEN`; treats "Uploaded" in output as success even if the exe crashes on telemetry shutdown. Tags + uploads artifact.
- **Secrets matrix**: `VSCE_PAT`, `OVSX_TOKEN`, `VS_MARKETPLACE_TOKEN`, `ASSETLIB_USERNAME`, `ASSETLIB_PASSWORD`; repo variables `ASSETLIB_ASSET_ID`, `ASSETLIB_EDITOR_ASSET_ID`. Local mirror: git-ignored `publisher-secrets.json` (`vscePatToken`/`ovsxToken`/`vsMarketplaceToken`) consumed by `scripts/publish-extension.ps1` / `publish-vsix.ps1`.

---

## 3. Changelog system — two lanes

### Lane A — runtime library (hand-written)
- Root `CHANGELOG.md`, Keep-a-Changelog + SemVer header. Section shape: `## [0.8.7] — 2026-07-10`, an intro paragraph, then `### Added` / `### Changed` / `### Fixed` with **bold-lead bullets** explaining root cause, not just symptom.
- **Byte-identical mirror** at `addons/reactive_ui/CHANGELOG.md` (resync: `cp CHANGELOG.md addons/reactive_ui/CHANGELOG.md`). **Test tripwire** in `tests/guitkx_editor_test.gd:362-367`: `FileAccess.get_file_as_string("res://addons/reactive_ui/CHANGELOG.md") == FileAccess.get_file_as_string("res://CHANGELOG.md")`.
- Entries cover the addon surface only, never `examples/`.

### Lane B — tooling family (script-generated from one JSON)
- Source of truth: `ide-extensions/changelog.json`. Targets (`KNOWN_IDES`): `vscode`, `vs2022`, `editor` (the Godot editor addon, since editor 0.6.3).
- **JSON shape**: `{ "entries": [ { "date": "YYYY-MM-DD", "versions": { "vscode": "0.8.8", "vs2022": "0.8.8" }, "shared": ["msg", …], "vscode": ["msg"], "vs2022": ["msg"], "editor": ["msg"] }, … ], "legacyTailHashes": { "editor": "<sha256>" } }` — newest first; `shared` messages render into every target the entry versions; same scope+versions+date accumulate as extra bullets in one entry.
- **`scripts/changelog.mjs` commands** (`node ide-extensions/scripts/changelog.mjs <cmd>`):
  - `add --scope <shared|vscode|vs2022|editor> (--message "…" | --message-file <utf8-path>) [--vscode X.Y.Z] [--vs2022 X.Y.Z] [--editor X.Y.Z] [--date YYYY-MM-DD]` — **refuses mojibake**: `detectArgvCorruption` regexes for CP1252→UTF-8 artifacts (`â€`, `Ã`, `ï¿½`, U+FFFD) because PowerShell/cmd transcode argv and strip embedded `"` — hence `--message-file` for anything non-ASCII.
  - `extract --ide <target> [--version X.Y.Z] [--out file]` — regenerates the full committed CHANGELOG.md (or one release's notes with `--version`).
  - `extract-overview --ide vs2022 --template <overview-template.md> --out overview.md` — VS Marketplace overview = template + `## Changelog` rendered section.
  - `verify` — re-renders each target with the SAME `renderChangelog` function used by extract and compares against the committed file (`CHANGELOG_FILES = { vscode: 'vscode/CHANGELOG.md', vs2022: 'visual-studio/CHANGELOG.md', editor: '../addons/reactive_ui_editor/CHANGELOG.md' }`); exits 1 with a "regenerate with…" hint. Run by CI on every push/PR.
  - `import --ide <t> --file <md>` — one-time backfill from an existing markdown changelog.
- **Cutover + frozen tail**: the editor addon's file has `LEGACY_MARKER = '<!-- changelog.mjs cutover: entries above are generated from ide-extensions/changelog.json; the history below is frozen and preserved verbatim. -->'`. Everything below is pre-cutover history re-emitted byte-verbatim. Because a naive read-back would launder hand-edits, `legacyTailHashes.<target>` in changelog.json pins the tail's **sha256**; extract/verify both refuse a mismatched or missing tail (deliberate history corrections update the pinned hash alongside).
- **Flow is always**: `add` → `extract` for EVERY target the entry names → commit json + regenerated files together → CI `verify` gates drift. Generated files are committed build products, NOT publish-time-only artifacts (publish regenerates them again as a belt).
- Per-target headers: `HEADERS.editor` states the cutover contract; vscode/vs2022 keep bare `# Changelog` (changing it would churn every committed file).

### Lane C — community (`plans/DISCORD_CHANGELOG.md`)
- Top-of-file entries, `## [X.Y.Z] - date`, `---` separators. Format: **bold-lead paragraphs** (what + why it matters, numbers like "~28% faster", "1.5–2.5× faster"), an `Update to **Reactive UI X.Y.Z** (AssetLib "Reactive UI", or copy addons/reactive_ui/ into your project).` line, and a `**Tooling:** GUITKX **0.8.8** (VS Code + VS 2022) … Editor addon **0.6.3** …` footer tying the family's versions together. **Hard cap ≤2000 chars/entry** (Discord message limit), counted via `awk '/^---$/{exit} {n+=length($0)+1} END{print n}' plans/DISCORD_CHANGELOG.md`. Manually pasted into Discord post-publish, never automated. Separate `discordpost` file at root holds the evergreen product pitch (feature bullets + code taste + video/docs/discord links).

---

## 4. `.claude/skills/` — four SKILL.md files (frontmatter `name` + `description`, then markdown)

### `dev-process` — house methodology
- Loop: **research → develop → test → bughunt → fix → commit → repeat**. Rules: root cause observed before fixing ("no fix ships on a theory"); **production-grade only, never a patch/bandaid** — wrong-altitude special-casing is the smell; test before handing over including boot/e2e paths (unit suites don't execute editor `_enter_tree`); **never weaken a lint/test/CI gate to get green**; campaigns are **1 branch, 1 PR** feature→`dev`, master is a **fast-forward** of dev (`git push origin origin/dev:master`), master is release-only; git authorship belongs to the user (no Co-Authored-By, no unrequested commits).
- Contains the cross-repo changelog table (artifact → changelog file → mechanism) covering RG runtime, RG editor, VS Code+VS2022, gdscript-analyzer crates (release-plz/git-cliff from Conventional-Commit squash titles), Unity ReactiveUIToolKit, and Discord.
- Version-bump section: **patch by default**; bump-location list per artifact; release mechanics summary (Publish button idempotency, release-plz for analyzer, AssetLib arming).
- "Definition of done": root cause named; suites green incl. e2e; changelog + version bump staged; docs updated when behavior changed; feature branch → PR into dev → user merges → master fast-forwarded.

### `release-process` — release runbook
- §0 versioning policy + version-location table (see §7 below); rule: any `lsp-server/src/` change bumps BOTH extensions (repackaging is how server fixes ship); behavior-invisible changes still bump + changelog ("shipped code never changes silently").
- §1 the two-lane changelog procedure with exact commands (as §3 above) + Discord entry spec.
- §2 verification gates before committing: `changelog.mjs verify`; `guitkx_editor_test.gd` (mirror tripwire); affected suites; eyeball the awk-extracted release body.
- §3 commit/merge/fast-forward with commit-message style: `release: reactive_ui X.Y.Z, editor A.B.C, guitkx extensions D.E.F -- changelogs + version bumps`.
- §4 publish (user clicks the button; AL auto; new store manual; paste Discord).
- **"Scar tissue" section** — each rule annotated with the incident that created it (14-version drift; argv mojibake; byte-identical means `cp` not retype; perf-only still ships; release bodies parse the top `## [` heading).

### `field-test-editor` — human-in-the-loop editor bug loop
- Two-tree model: **live tree** (user's checkout, never edited while their Godot is open, never kill their process) vs **work tree** (`RG-work` git worktree; branches off `origin/dev`). Godot console exe path pinned; always redirect output to a file (piping block-buffers). Loop: reproduce+fix with a regression test → verify (guitkx_build, `--editor --quit` boot check "plugins actually load", lsp + editor suites with expected check counts: 382 with analyzer / 364 without) → commit → apply to live tree → user tests and pastes Output panel → fixed? merge flow : loop with new evidence ("never re-try the same theory twice — get more instrumentation"). Plus a **store-zip fidelity test**: download the release zips into a FRESH project and verify the green "native analyzer <ver> detected" banner.

### `new-component` — authoring guide
- File placement (snake_case `.guitkx`, sibling generated `.gd`, gitignored), full anatomy example, prop/hook/event/directive/style rules, keyed lists, tabs-not-spaces, and a done-checklist (no `GUITKX####` diagnostics, renders, doc comment, formatting passes).

Unreal equivalents to author: dev-process (nearly verbatim), release-process (retargeted lanes/venues: Fab/Marketplace instead of AssetLib), field-test-editor → "field-test-unreal-editor" (live vs work tree, UnrealEditor-Cmd headless runs, log redirection), new-component (.uetkx authoring).

---

## 5. `plans/` conventions

- **Live docs** (7): `PARITY_PLAN.md` (living status source-of-truth), `ASSET_STORE_PLAN.md`, `VSCODE_VS2022_PARITY_PLAN.md`, `FINAL_AUDIT_GODOT_FINDINGS.md` (numbered G-01…G-23 findings ledger), `FINAL_AUDIT_GODOT_OPTIMIZATIONS.md` (perf backlog, GO-xx ids), `TECH_DEBT.md` (TD-xx register: root cause + why still open + what a production fix looks like, with file:line anchors), `DISCORD_CHANGELOG.md`.
- **Status banners**: living plans carry stacked, dated blockquote banners at the top — `> **STATUS UPDATE — 2026-07-10 (code-verified sweep).**` — newest first, each correcting the stale body below rather than rewriting it; explicitly names SHIPPED/STALE/DEAD items and where remaining scope moved. Plans use phase/milestone/task ids (T6.1, G2, M1–M3, P1–P3, Wave 1–3).
- **`plans/archive/`**: "done or dead" plans moved by dated audits; `archive/README.md` is a **table of plan → why archived**, capturing execution outcomes ("all 6 phases done", "31/32 tasks shipped (0.7.0)", reversals like "HMR deletes state was later *reversed*") and ends with the live-plans-remaining summary. Archived docs are kept as design/root-cause record, not TODO.
- Bug ledgers (BUG_AUDIT/BUG_SPLIT/BUG_V1 with A-series/G-series ids) archive once every row is resolved.
- `research/` is separate from `plans/`: raw evidence (a dump script + its JSON output + a written surface survey).

---

## 6. `tests/` harness pattern

- Each suite is `extends SceneTree` with `_initialize()`; counts `_passes`/`_fails` via an `_ok(cond, label)` helper; prints `[suite_name] N passed, M failed`; ends `quit(1 if _fails > 0 else 0)` — exit code IS the pass/fail signal. Run: `godot --headless --path . --script res://tests/<suite>.gd` (also how you run a single file; no test-framework dependency).
- **Pre-steps are part of the harness contract**: (1) `guitkx_build.gd` compiles every `examples/**/*.guitkx` to sibling `.gd` using the same `Codegen.project_bindings` pass as the editor plugin so output is **byte-identical** (fails run on compile errors, prints OK/WARN/COMPILE FAIL per file); (2) `--editor --quit` builds the class-name cache. Only then do suites run.
- Suites: `core_test.gd` (36 awaited test funcs: effects/bailout/context/keyed/suspense/router/tween/pooling/reuse_by_slot…), `style_test.gd`, `router_match_test.gd`, `router_spine_test.gd`, `update_test.gd`, `demos_test.gd` (renders every demo — the real check generated code runs), `guitkx_test.gd` (compiler+codegen; also consumes the shared TS fixture corpus), `hmr_test.gd`, `guitkx_lsp_test.gd`, `guitkx_editor_test.gd` (prints per-section markers so hangs name their culprit; hosts the changelog-mirror tripwire; known check counts 382/364). Benchmarks (`bench*.gd`, `microbench.gd`, `recon_bench.gd`, `apply_bench.gd`, `doom_bsp_bench.gd`) are explicitly not pass/fail.
- Scratch dirs: suites create/clean `tests/__*_tmp/`; gitignored so crash debris never commits.
- **`tests/contract/`** — the GD↔TS grammar contract harness: `fixtures/*.guitkx` (flattened copies of every shipped demo + targeted `t*_…` edge cases, with a `.gdignore` so Godot never compiles deliberately-broken fixtures), `golden/*.json` dumped by the **compiler of record** (`{ ok, diagnostics:[{code,severity,off,len}], windows:[{start,end}], markup:[{error,error_code,error_at,tree}] }`), `tests/contract_dump.gd` regenerates (`-- --check` fails on drift in test.yml), and `ide-extensions/lsp-server/src/test/contract.test.ts` asserts the TS side reproduces every golden. **Offsets are Unicode code points** (GDScript String indices), converted from UTF-16 on the TS side via `src/codePoints.ts`. `*.pending.guitkx` fixtures pin *known* divergences — the TS test asserts the divergence still exists, so fixing one forces the rename+regen.
- **Unreal analog needs**: a commandlet/automation-test equivalent of headless suites (`UnrealEditor-Cmd <proj> -run=Automation RunTests <filter> -unattended -nopause -nullrhi`, exit-code gated), a markup-compile pre-step for `.uetkx` corpora, a "renders every demo" smoke against Slate (null RHI), the byte-compatible contract corpus shared C++↔TS (grammar of record decision), and per-section progress markers for hang diagnosis.

---

## 7. Versioning

| Artifact | Version source | Tag | Published to |
|---|---|---|---|
| Runtime addon | `addons/reactive_ui/plugin.cfg` → `version="0.8.7"` | `v<ver>` | GitHub Release zip + classic AssetLib (+ manual new Asset Store) |
| Editor addon | `addons/reactive_ui_editor/plugin.cfg` → `0.6.3` | `editor-v<ver>` | GitHub Release zip (bundles pinned analyzer) + AssetLib Custom provider |
| VS Code ext | `ide-extensions/vscode/package.json` → `0.8.8` (name `guitkx`, publisher `ReactiveUITK`) | `vscode-v<ver>-<target>` (per-platform!) | VS Marketplace + Open VSX |
| lsp-server | `ide-extensions/lsp-server/package.json` → `0.8.8` (no own tag; ships inside both extensions) | — | — |
| VS2022 ext | `GuitkxVsix/source.extension.vsixmanifest` `Identity/@Version` (kept == lsp-server version) | `vs2022-v<ver>` | VS Marketplace via VsixPublisher |
| Docs site | `package.json` `0.0.0` (not version-gated; redeploys every dispatch) | — | GitHub Pages (`documentations` branch) |

Policy: **patch by default** (bump only Z); minor for genuinely additive milestones; major/0.x-minor for breaking; on 0.x, `feat` = patch. Perf/refactor-only changes to shipped bytes still bump + get a changelog line. Extensions + lsp-server bump in lockstep whenever server src changes.

---

## 8. `ide-extensions/` structure

```
changelog.json               # single changelog source (Lane B) + legacyTailHashes
README.md / VERSIONING.md / PUBLISHING.md    # architecture, release model, manual publish steps
grammar/                     # guitkx.tmLanguage.json (self-contained TextMate) + guitkx-schema.json (element/attr schema)
scripts/                     # changelog.mjs, publish-extension.ps1, publish-vsix.ps1
lsp-server/                  # TypeScript LS (stdio), name guitkx-language-server
  src/: server.ts, schema.ts (schema embedded), markup.ts, scanner.ts, jsxScan.ts, declScan.ts,
        virtualDoc.ts + sourceMap.ts (length-preserving synthetic .gd projection, Volar-style hand-rolled),
        analyzerAdapter.ts (in-process @gdscript-analyzer/core napi), codePoints.ts (UTF-16↔code-point),
        formatGuitkx.ts/guitkxFormat.ts/reflowEmbedded.ts, diagsSidecar.ts, workspaceIndex.ts, refs.ts,
        semanticTokens.ts, liveMarkup.ts, context.ts, events.ts, vocabulary.json, test/
  test-fixtures/: scanner-cases.json, markup-cases.json, formatter-cases.json
        # scanner-cases.json: shared corpus asserting guitkx_lexer.gd and scanner.ts stay byte-identical —
        # cases like { "input": "\"abc\"x", "at": 0, "expect": 5 } for skip_noncode/skipExpected +
        # findMatching blocks; run by BOTH tests/guitkx_test.gd and lsp-server core.test.ts
  classdb/                   # Godot ClassDB dump feeding host-element intelligence
  scripts/smoke.js           # e2e stdio handshake + markup completion
  deps: @gdscript-analyzer/core ^0.6.0, vscode-languageserver ^9.0.1, vscode-languageserver-textdocument ^1.0.11
vscode/                      # extension: LSP client + grammar + language-configuration.json + syntaxes/
  scripts/bundle-server.js   # copies built server (+ this-or-target platform's .node) into ./server; --target for CI matrix
  scripts/verify-server.js   # vscode:prepublish gate that the bundle exists
  npm scripts: build/watch/bundle-server/package/publish/publish:ovsx/deploy (vsce+ovsx always via npx, never deps)
visual-studio/
  CHANGELOG.md               # generated (Lane B target vs2022)
  GuitkxVsix/                # C# VSIX: GuitkxPackage.cs, GuitkxLanguageClient.cs (ILanguageClient→stdio server),
                             # GuitkxBraceCompletion/FormatOnSave/SmartIndent/OptionsPage/Settings/ContentDefinition,
                             # guitkx.pkgdef (TextMate registration — VS never colorizes over LSP),
                             # fetch-node.ps1 (pins node.exe into server/ → self-contained ~80MB VSIX),
                             # overview-template.md → overview.md, publishManifest.json, source.extension.vsixmanifest
```

Key architectural facts to carry over: two languages in one file split into **markup intelligence answered from the schema locally** + **embedded-language intelligence via a synthetic virtual doc with a length-preserving source map analyzed fully in-process/offline** (for Unreal: embedded C++ is the hard part — the analog seam is clangd or a compile_commands-backed service); the shared server drives both IDE clients over stdio; the grammar is duplicated GD↔TS with the compiler as **grammar of record** and the contract corpus as the safety net.

---

## 9. Docs site (`ReactiveUIGodotDocs~/`)

- Stack: **React 19 + Vite (rolldown) + MUI v7 (@mui/material, @mui/lab, @mui/icons-material) + @emotion + react-router-dom 7 + prism-react-renderer**; SPA on GitHub Pages; `npm run build` = `tsc -b && vite build` + copies `dist/index.html → dist/404.html` (SPA fallback); `.nojekyll` at deploy.
- It's a **clone of the Unity ReactiveUIToolKit docs site retargeted** — same shell (TopBar/Sidebar/CodeBlock/SearchModal/Pager/theme/routing), content ported.
- `src/` structure: `pages/` one dir per doc area — `Introduction, GettingStarted, Concepts, Components, SpecialHooks, Router, Signals, API, Differences, FAQ, KnownIssues, Roadmap, Tooling/{Editor,HMR}, UITKX/{API, AdvancedAPI, Assets, CompanionFiles, Components, Concepts, Config, Context, CssHelpersRef, CustomRendering, Debugging, Diagnostics, GettingStarted, …}` — each page a `XxxPage.tsx` + `XxxPage.style.ts` pair; central `docs.tsx`/`pages.tsx` route+nav registry; `searchIndex.ts`; `hostElements.ts`/`hostContent.ts` data-driven from the schema + `classdb/godot-control.json`; **`versionManifest.ts`** — single source of engine-version awareness: `SUPPORTED_VERSIONS` ordered list (floor first, 4.2–4.5) + feature maps for non-floor additions, powering a version selector.
- `vite.config.ts` historically parses the source-of-truth version manifest (Unity package.json / C# Props) to generate prop tables — the intended pattern is docs generated from the same schema/ClassDB data the LSP uses.

---

## 10. README / store-listing conventions

- **Root README.md** (repo landing): Discord invite first line; one-paragraph identity + sibling cross-link; a real code sample immediately; a **bold `> Status:` blockquote** with concrete numbers (23 hooks, 17 router hooks, ~61 V.* factories); section order `Install / Quick start / Authoring in .guitkx / Architecture / Diagnostics / IDE tooling / Notes & limitations / Roadmap`. Written "the way you should author" (markup-first, raw API as escape hatch). Honest **Notes & limitations** section documenting faithful-to-reference constraints.
- **Addon-local README/CHANGELOG/LICENSE inside `addons/reactive_ui/` and `addons/reactive_ui_editor/`** — store installs must be self-contained and never drop root files into a user project (learned from first AssetLib user feedback, 0.8.3).
- **`.gitattributes` export ALLOWLIST**: `* text=auto eol=lf`; then `/** export-ignore` + re-include only `/addons/reactive_ui/**`; editor addon and `addons/reactive_ui/dev/` explicitly export-ignored. Rationale in-file: the old denylist shipped `.claude/`, `discordpost`, `export_presets.cfg` to users; an allowlist means new top-level files can never silently leak.
- **Store listing templates** (`.asset-template*.json.hb`): title, long feature-dense description (editor one even documents "Developed with AI assistance (Anthropic Claude) under human direction", macOS unsigned-binary note, export-preset exclusion), `category_id`, `godot_version` floor, `{{ version }}`/`{{ commit }}` placeholders, provider (GitHub commit vs Custom zip URL), browse/issues/icon URLs.
- **Per-extension marketplace READMEs** (`vscode/README.md`) are the store page: feature-grouped, settings table, coexistence notes (godot-tools).
- CLAUDE.md itself is a production artifact: repo map table (deliverable/location/language/version source), exact CI-mirroring commands, architecture summary, "known constraints — preserve these" list.

---

## IMPROVE FOR UNREAL

1. **Grammar of record should be single-sourced, not duplicated.** Godot maintains the grammar twice (GDScript compiler + TS LSP) and needs the entire contract-golden harness + code-point conversion layer + `.pending` fixture protocol just to keep them honest. For Unreal, consider one canonical implementation compiled to both targets (Rust core → napi for the LSP *and* FFI/static lib for the UE plugin, exactly like `@gdscript-analyzer/core` already proves out) — the contract corpus then becomes a regression suite, not a parity crutch. If duplication is unavoidable, adopt the corpus + goldens + offsets-contract from day one, and decide the offset unit (UTF-8 bytes vs UTF-16) explicitly in the spec.
2. **Changelog Lane B from day zero, no cutover scar.** The `LEGACY_MARKER`/`legacyTailHashes` machinery exists only because the editor addon started hand-written and cut over at 0.6.3. A new repo starts every generated changelog from json → no frozen tails, no sha256 pinning. Keep: `verify` as a required check from the first commit; `--message-file` mojibake guard (Windows argv transcoding hits Unreal devs equally).
3. **Per-target publish tags from day one.** The `vscode-v<ver>-<target>` per-platform idempotency gate was retrofitted after a partial publish stranded 5 platforms. Any Unreal artifact with a platform matrix (native LSP binaries, prebuilt plugin binaries per engine version) should gate per-leg immediately. Similarly, plan tags per engine version if shipping prebuilt binaries (`ue5.5-v*`-style) — Unreal's analogue of Godot's single-engine zip is a *matrix* of engine versions, which Godot never had to solve.
4. **Version-bump automation.** Versions live in 5 hand-edited manifests with lockstep rules enforced only by skill docs ("keep vsixmanifest == lsp-server version"). Add a `scripts/bump.mjs` that bumps an artifact + its lockstep partners + scaffolds the changelog entry in one command, and a CI check that lockstep pairs match.
5. **Publish-time test duplication.** `publish.yml` re-lists the full test command sequence inline (drifted risk vs `test.yml` — it already lacks the `contract_dump.gd --check` step present in test.yml). Factor the suite list into one reusable workflow (`workflow_call`) or a single script both call.
6. **Required-checks vs paths-filter trap**: replicate the documented solution (PR triggers unfiltered when jobs are required checks) or use GitHub's newer "skipped counts as success" patterns deliberately — but write the reason in the workflow like Godot does; the in-file incident commentary (`# guards the exact failure mode that shipped 0.6.0-0.8.4…`) is one of this repo's best practices and should be house style.
7. **Store venue realities differ**: Godot's classic AssetLib has a publish API (the `assetlib-update` job); **Fab/Unreal Marketplace has none** — plan the manual-step checklist as a first-class release-process section (like the "New Asset Store reminder" `::notice` pattern), and consider GitHub Releases + a plugin-zip (`ReactiveUI-Unreal-<ver>-UE<engine>.zip` per engine version, built by CI with `RunUAT BuildPlugin`) as the primary distribution while Fab is manual.
8. **Packaging allowlist** — Unreal's equivalent of the `.gitattributes` allowlist is twofold: `git archive` shaping (same mechanism works) AND the `.uplugin`/BuildPlugin packaging filter (`FilterPlugin.ini`). Start allowlist-style in both; the Godot repo learned this by shipping `.claude/` to users.
9. **Docs site**: clone the shell again but fix the known retarget debt immediately — `vite.config.ts` should read the plugin version + generate prop tables from the Slate-widget schema (the analog of ClassDB dump) from day one, not "tracked as remaining". Version manifest = UE 5.x versions.
10. **Headless testing is harder in Unreal** — bake the harness early: automation specs run via `UnrealEditor-Cmd -run=Automation … -nullrhi -unattended`, exit-code gated, with the markup-compile pre-step as a commandlet; add the per-section progress markers convention (hang diagnosis) and scratch-dir cleanup convention (`tests/__*_tmp/` analogue in `Saved/Temp`). CI engine install is the big cost Godot never paid (4.7 zip vs ~100GB UE) — plan a pinned engine container (e.g. `ghcr.io/epicgames/unreal-engine:dev-slim-5.x`, requires Epic org access) or a self-hosted runner as part of step 0, not later.
11. **Skills**: port all four, plus add what Godot lacks — a `verify`-style skill for driving the built editor, and encode the two-tree field-test model with Unreal specifics (live editor never killed; `-log` file redirection; plugin hot-reload unreliability → restart editor guidance).
12. **Keep**: the Publish-button single-dispatch model; tag-gated idempotency; scar-tissue documentation culture (plans/archive README, TECH_DEBT.md with root causes, incident comments in workflows); the two-lane changelog split (hand-written library prose vs generated tooling notes); byte-identical mirror + test tripwire for any duplicated file; `plans/` status-banner convention; DISCORD_CHANGELOG with the 2000-char awk check; benchmarks-separate-from-tests; research/ as evidence store.