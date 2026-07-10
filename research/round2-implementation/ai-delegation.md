# ReactiveUI-Unreal — AI-Execution Ecosystem Design Digest

## 0. Extracted house conventions (verified from ReactiveUI-Godot, `.claude/skills/*` + `plans/*`)

**Source files read:** `.claude/skills/dev-process/SKILL.md`, `release-process/SKILL.md`, `field-test-editor/SKILL.md`, `new-component/SKILL.md`; `plans/archive/DOOM_GAME_GUITKX_PORT_PLAN.md`, `plans/FINAL_AUDIT_GODOT_OPTIMIZATIONS.md`, `plans/PARITY_PLAN.md`, `plans/TECH_DEBT.md`, `plans/archive/README.md`.

**The loop (verbatim house law):** `research → develop → test → bughunt → fix → commit → repeat.` No fix ships on a theory that hasn't been observed. "Production-grade, long-term solutions only. Never a patch, never a bandaid" — special-casing on top of shared machinery = fix at wrong altitude. "Never weaken a lint, test, or CI gate to get green." "Never re-try the same theory twice — get more instrumentation instead" (temporary print probes fine; remove before commit).

**Branch/merge model:** campaigns are **1 branch, 1 PR**: feature → `dev` (PR title = squash title), master is a **fast-forward of dev** (`git push origin origin/dev:master`), master is release-only. Many commits per campaign (per milestone), one PR at the end (Doom plan: "phases marked done incrementally, one PR at the end"). Git authorship belongs to the user: **no `Co-Authored-By`**, no commits/pushes beyond what the task established. User memory: **never auto-commit, even mid-release-work** — commit only when explicitly asked or when the invoked skill establishes it (field-test loop is "a standing ask to commit").

**Status-marker vocabulary (used consistently across plans):**
- Per-phase: `- [x] **Phase N — title.** … Verification: <specific observable>` checkbox lines.
- Living docs get prepended reverse-chronological banners: `> **STATUS UPDATE — YYYY-MM-DD (what kind of sweep).**` with corrections listed; body below kept as historical record ("kept as record, not as work to do").
- Goal blockquote carries the terminal state: `**Status: COMPLETE 2026-07-08 — all 6 phases done; X consciously deferred as out-of-scope optionals (see Phase 6)** … tests/doom_game_test.gd green (179 checks)`.
- Item states: `SHIPPED <date>`, `SKIPPED (measured negligible)`, `DEFERRED — documented for later`, `DEAD/SUPERSEDED`, `~~struck text~~ — **SUPERSEDED <date>**`, "consciously deferred".
- **Stable IDs across documents** (G-##, GO-##, TD-##, T6.1, E17): "IDs are stable across both docs".
- Finished plans move to `plans/archive/` and get a **row in `plans/archive/README.md`'s table** (`| Plan | Why archived |`), which also lists what's "Still live in `plans/`" with each doc's one-line open scope.
- One doc (`PARITY_PLAN.md`) is anointed "**the living source of truth for *where we stand***" — the roadmap doc that all status propagates to.

**Verification-gate conventions:** run suites exactly like CI, **in a documented order** with order-dependencies explained (guitkx compile → class-cache scan → suites); a **boot/e2e check is never optional** ("suites do NOT run `_enter_tree`"); perf work requires **measured before/after numbers in the PR** against a permanent committed bench harness (`tests/recon_bench.gd` — "recreate it… keep permanently"); contract suites (`scanner-cases.json` / `tests/contract`) **pin behavior** — "any contract diff during a perf change is a bug in the change"; tests print per-section markers "so hangs name their culprit"; byte-identity tripwires enforced by tests (root `CHANGELOG.md` ↔ addon copy).

**Changelog/version machinery:** two lanes — Lane A hand-written library changelog with byte-identical mirror (regenerate with `cp`, never re-type); Lane B single-source `changelog.json` + `changelog.mjs add --message-file … / extract / verify` with a CI drift guard (`changelog-sync` job) — "add → extract → commit together", `--message-file` never `--message` (argv transcoding corrupts em-dashes). Version policy: **patch by default**; perf-only change still bumps + one changelog line ("tokenizes ~17% faster, output hash-identical"); publish is a **user-pressed workflow_dispatch button** with idempotent tag-gated jobs (`v*`, `editor-v*`, `vscode-v*`, `vs2022-v*`) — un-bumped artifacts simply don't release. Discord note in `plans/DISCORD_CHANGELOG.md`, **hard ≤2000 chars/entry**, counted with an awk one-liner, pasted manually. Skills include a "**Scar tissue (why these steps exist)**" section documenting past failures — keep this pattern.

**Plan-document skeleton (Doom plan, the exemplar):** §0 "The one fact that shapes everything" → §1 "Key architectural decisions… each with its reasoning, **so implementation doesn't re-litigate them mid-port**" → §2 file-by-file map → §3 phased plan (checkbox + per-phase Verification, each phase "independently demoable") → §4 testing & verification plan (incl. explicit "what CANNOT be verified headlessly") → §5–6 resolved concerns / **explicit deviations ("not gaps — decisions")** → §7 risks → §8 open questions **for the repo owner** → §9 "What this research did and didn't verify". Open empirical questions are assigned to a phase, not decided in the plan ("measure it during Phase 1; don't preemptively design around it").

**Field-test division of labor:** "You (the AI) do everything except the actual in-editor testing. The human tests, reports, and decides 'fixed' or 'persists'." Separate **live tree** (user's) vs **work tree** (AI's, a git worktree); NEVER edit the live tree while the user's editor is open, NEVER kill their process; environment facts (binary paths, output-redirection quirks) are written into the skill.

---

## a) Skill roster for ReactiveUI-Unreal (`.claude/skills/<name>/SKILL.md`)

1. **`dev-process`** (port) — The house methodology, adapted for a compiled language. Same loop (research→develop→test→bughunt→fix→commit→repeat), same laws (root cause only, never weaken gates, 1-branch-1-PR feature→`dev`, master fast-forward, no Co-Authored-By, user merges). UE deltas it must encode: there IS a compile step — the gate ladder is *compiles (both `Development Editor` and a packaged target) → unit/automation tests green → editor boot check → e2e sample renders*; the boot check is `UnrealEditor-Cmd.exe <proj>.uproject -run=<SmokeCommandlet>` or an `-ExecCmds="Automation RunTests ReactiveUI.Boot; Quit"` pass, because unit tests don't exercise module `StartupModule()`/Slate registration (the exact analogue of Godot's "suites don't run `_enter_tree`"). Carries the changelog table (which artifact → which changelog → which command) and version-bump locations for all deliverables: runtime plugin (`ReactiveUIUnreal.uplugin` → `"VersionName"`), uetkx compiler/hot-reload module, VS Code/VS2022 extensions (`ide-extensions/*/package.json` / `.vsixmanifest`), docs site.

2. **`release-process`** (port) — The runbook, in order: `git diff origin/master --stat` against each deliverable root to decide what bumps; patch-by-default policy incl. "perf-only still ships"; Lane A hand-written `CHANGELOG.md` + byte-identical plugin mirror (cp, tripwire test); Lane B `changelog.mjs add/extract/verify` reused verbatim; Fab packaging (`RunUAT.bat BuildPlugin -Plugin=<path>.uplugin -Package=<out> -TargetPlatforms=Win64` per supported engine version — Fab wants one archive per engine major.minor, source-only, no `Binaries/`/`Intermediate/`); Discord entry ≤2000 chars in `plans/DISCORD_CHANGELOG.md`; verification gates (changelog verify, mirror tripwire, affected suites); commit-message shape `release: reactive_ui_unreal X.Y.Z, … -- changelogs + version bumps`; PR→dev→fast-forward; **the user presses Publish** (workflow_dispatch, idempotent tag-gated jobs); Fab listing version-add is manual until Fab has an API. Include a "Scar tissue" section seeded from the Godot one.

3. **`plan-progress`** ⭐ new — *"when we finish a phase in a plan, mark it complete."* Input: plan file + phase ID + evidence. Mechanical procedure: (1) flip the phase's `- [ ]` → `- [x]` and append an evidence clause to the phase line (date, test name + check count, key commit sha, measured numbers if perf); (2) if the phase changed any fact stated elsewhere in the plan, strike it `~~…~~ — **SUPERSEDED YYYY-MM-DD**` in place (never delete history); (3) prepend/update the plan's top `> **STATUS UPDATE — YYYY-MM-DD.**` banner; (4) update the matching row in `plans/ROADMAP.md` (the anointed living-status doc, PARITY_PLAN's successor role); (5) stage a draft changelog bullet in `plans/PENDING_CHANGELOG.md` so release-process finds it; (6) when the LAST phase closes: rewrite the goal blockquote to `**Status: COMPLETE YYYY-MM-DD — all N phases done; <deferred items> consciously deferred**`, `git mv` the plan to `plans/archive/`, add its `| Plan | Why archived |` row to `plans/archive/README.md`, and update ROADMAP. Explicitly **does not commit** — it leaves the working tree staged-ready and reports what changed. This is a haiku-class-executable skill: every step is a deterministic text edit with a template.

4. **`docs-sync`** ⭐ new — *"when a new feature lands, update the docs."* Input: the merged feature (PR title/diff or plan phase). A decision table mapping change-type → artifact checklist: new hook → hooks reference page + README hook-count line + docs-site hooks table + `new-component` skill's hook list; new `V.*` widget wrapper → element table in README + docs-site components page + count line; grammar/directive change → `.uetkx` syntax docs + `new-component` skill + LSP schema note + **byte-compat contract note vs `.guitkx`**; behavior change → README "Notes & limitations" + store listing if user-facing; perf change → README perf section with the measured numbers. Ends by running the **docs-drift tripwire** (a committed test that greps README's claimed counts — "N hooks / M factories" — against the actual registry source, the automated fix for Godot's "badly stale README claims MVP / 10 elements / 6 hooks" episode). Also haiku/sonnet-executable: table lookup + templated edits + one verification command.

5. **`component-pipeline`** ⭐ new — the production line for the repetitive widget/style work (the user's explicit "lesser model" workload). One run = one Slate widget wrapper end-to-end, 6 fixed stations: (1) **research** — read the actual `SWidget` header (`Engine/Source/Runtime/Slate/.../S<Name>.h`) + UE docs page; enumerate `SLATE_ARGUMENT`/`SLATE_ATTRIBUTE`/`SLATE_EVENT`/`SLATE_SLOT_ARGUMENT` and the style struct (`F<Name>Style`); write the findings into the widget's prop-map YAML/JSON entry (the machine-readable schema the LSP also consumes — single source); (2) **wrapper** — fill the `V.*` factory + host-config mapping from the template file (`templates/widget_wrapper.template.cpp` — create/update/event-bind/style-apply sections); (3) **style keys** — add each style key to the central style registry with type + default + reset behavior; (4) **contract case** — add the widget to the contract corpus (props applied → widget state asserted; removed-style-prop reset asserted); (5) **test boilerplate** — instantiate the per-widget automation test from `templates/widget_test.template.cpp` (`IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveUI<Name>Test, "ReactiveUI.Widgets.<Name>", …)`); (6) **docs row** — invoke `docs-sync`. Skill body contains the checklist, the template paths, one fully-worked example (SButton), and the known traps list (attributes vs arguments — attributes are `TAttribute<T>` bindable, arguments construct-only and force widget recreation on change; slot props live on the slot not the widget; events return `FReply`). Exit gate: build + `Automation RunTests ReactiveUI.Widgets.<Name>` green + contract suite green.

6. **`test-run`** ⭐ new — the UE automation incantations, the analogue of CLAUDE.md's "run them exactly like CI, in this order" block, because UE's CLI is genuinely arcane. Contents: build first (`Engine\Build\BatchFiles\Build.bat <Project>Editor Win64 Development -Project=<abs>.uproject -WaitMutex`); run headless automation:
   ```
   UnrealEditor-Cmd.exe <abs>.uproject -ExecCmds="Automation RunTests ReactiveUI; Quit" ^
     -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput ^
     -ReportExportPath=<scratch>\report
   ```
   suite filters (`ReactiveUI.Core`, `ReactiveUI.Widgets`, `ReactiveUI.Style`, `ReactiveUI.Uetkx`, `ReactiveUI.Contract`, `ReactiveUI.Demos`, `ReactiveUI.Bench` non-pass/fail); how to read `index.json` in the report dir for pass/fail (exit code alone is unreliable); the `.uetkx`-compile-all step that must precede suites (mirror of `tests/guitkx_build.gd`); environment facts section (engine install path not on PATH, always redirect output to a file — same buffering scar as Godot's `_console` exe note); single-test invocation; and the rule that `RunTests` filter match is prefix-based.

7. **`field-test-editor`** (port) — same contract: AI prepares/fixes/verifies/applies, user tests in a real UE editor, loop until dead. UE deltas: live tree vs work tree (git worktree); **Live Coding** hot-patches `.cpp` bodies only — any header/UCLASS/module change ⇒ tell the user to close the editor and rebuild (the analogue of "plugin scripts don't hot-swap reliably; restart Godot"); never touch the live tree while their editor is open (UE locks DLLs — edits + rebuild will fail loudly anyway); ask for `Saved/Logs/<Project>.log` + the Message Log, and read the user's scratch error file. Packaged-fidelity test: BuildPlugin zip into a FRESH project, enable plugin, expect the startup log banner.

8. **`new-component`** (port) — the `.uetkx` authoring guide: file placement, `component Name(props…)` grammar (byte-compatible with `.guitkx` — state this and point at the shared grammar corpus), hooks list, events (`onClick` aliases + `on_<delegate>` escape hatch), style dict keys, `@if`/`@for`/`@match`, keyed lists, tabs-not-spaces if inherited, what the compiler generates (sibling generated `.h/.cpp` or bytecode — per runtime design), done-checklist (compiles clean, no `UETKX####` diagnostics, renders, doc comment on reusable components).

9. **`grammar-contract`** ⭐ new (small but load-bearing for the "byte-compatible with .guitkx" ship gate) — how to add a case to the shared cross-repo grammar corpus (the `scanner-cases.json` idea generalized): every parser/lexer change lands with a corpus case; the corpus is consumed by BOTH repos' test suites; a divergence is a bug in whichever side changed last; procedure for syncing the corpus file across repos and the tripwire that pins it.

---

## b) Delegation matrix — the production line

Tiers: **haiku-class** = mechanical, template-in/template-out, zero design decisions; **sonnet-class** = bounded implementation with judgment inside fixed rails; **opus-class** = architecture, root-causing, anything touching the reconciler core or a gate. Rule of thumb encoded in dev-process: *the tier is chosen by the task's blast radius, not its size; anything that can silently corrupt shared machinery escalates.*

| Recurring task | Tier | Exact inputs the lesser model needs | Verification gate (stronger model / CI) |
|---|---|---|---|
| **Add Slate widget wrapper** | sonnet-class (station 1 research + wiring); stations 2–6 haiku-class once the prop-map entry exists | `component-pipeline` skill; the widget's `S<Name>.h` path; `templates/widget_wrapper.template.cpp` + the worked SButton example; the prop-map schema; traps list (attribute vs argument, slot props, `FReply`) | CI: build + `ReactiveUI.Widgets.<Name>` + full contract suite. Opus-class review only if the widget needed a new host-config *mechanism* (new slot model, item-view adapter) rather than filling the existing one |
| **Add style key** | haiku-class | Style-registry file + one existing key as example; the reset-behavior rule (style/events/refs reset on removal; plain props don't — a documented faithful-to-reference constraint, preserve it); the contract-case template | CI: `ReactiveUI.Style` + contract case asserting apply→remove→default-restored; changelog line present |
| **Add hook doc page** | haiku-class | `docs-sync` skill; the hook's source + doc comment; an existing hook page as the template; docs-site file layout | CI: docs-site `npm run build && npm run lint`; docs-drift tripwire (claimed counts match registry); sonnet-class skim for factual claims about semantics |
| **Changelog entry** | haiku-class | `release-process` §1 verbatim; the diff/PR title; `--message-file` rule; lane-selection table (which artifact → which lane) | CI: `changelog.mjs verify` + byte-identity tripwire test; release-process runner eyeballs the top `## [` section shape |
| **Contract-corpus case addition** | haiku-class | `grammar-contract` skill; the corpus JSON schema; 3 existing cases as examples; the behavior sentence to pin (given by whoever made the change) | CI: corpus runs green in THIS repo; cross-repo sync job (or scheduled sweep) confirms Godot repo also green — divergence blocks |
| **Web-research sweep for missing widget props** | haiku-class execution, sonnet-class synthesis | The current prop-map; the target list of `SWidget` classes; the sweep procedure (official API docs URL pattern `dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Slate/Widgets/...`, then the header as ground truth — **the header outranks the docs**); output format = diff proposals against the prop-map, one PR-able file | Sonnet-class verifies each proposed prop against the actual header before it enters the prop-map (adversarial check, per the house "adversarially verified" research pattern); CI contract suite catches wrong types |
| **README / store-listing refresh** | haiku-class draft | `docs-sync` skill; current README; the delta list (what shipped since last refresh, from ROADMAP + changelogs); Fab listing char/format constraints; the counts tripwire | Docs-drift tripwire; opus/sonnet-class pass for claims accuracy ("says what's true, not what's aspirational" — the stale-README scar); user approves store-listing text (public surface) |
| **Test boilerplate** | haiku-class | `templates/widget_test.template.cpp` / `hook_test.template.cpp`; naming convention `ReactiveUI.<Suite>.<Name>`; the per-section print-marker rule ("hangs name their culprit") | CI: the new test runs and FAILS when its target is broken (sonnet-class must demonstrate red→green once, per "checks that would catch its regression exist") |
| **Plan status bookkeeping** | haiku-class | `plan-progress` skill; the evidence packet (test counts, shas, numbers) from the session that finished the phase | Deterministic: the skill's own checklist; ROADMAP row diff reviewed at PR time |
| **Reconciler / host-config core, perf work, gate changes** | opus-class, always | Full plan doc + `FINAL_AUDIT`-style measured baseline; the permanent bench harness | Measured before/after in the PR; contract + full suite; never delegated |

**Mechanics of delegation:** the orchestrating session spawns the lesser-model task with exactly three things — the SKILL.md, the template/example files, and the evidence packet — never "the whole repo context". Every skill above is written so its happy path is a checklist a haiku-class model can follow without inference; anything off-checklist is an explicit "STOP and escalate" instruction inside the skill (mirroring field-test's "verify, don't assume, if anything fails").

---

## c) Plan-document format spec (AI-first)

**File header (machine-parsable, top of every plan):**
```markdown
# <Title>
> **Status:** IN PROGRESS | NOT STARTED | BLOCKED(<on what>) | COMPLETE YYYY-MM-DD | SUPERSEDED(<by doc>)
> **Branch:** feat/<name>   **PR:** <#n or —>   **Phases:** 3/7 done
> **STATUS UPDATE — YYYY-MM-DD.** <newest correction banner; prepend, never rewrite history>
```
Allowed status values exactly those five; `DEFERRED` is per-item, not per-plan.

**Fixed section order** (the Doom-plan skeleton, made mandatory): §0 The one fact that shapes everything · §1 Key architectural decisions (each with reasoning — "so implementation doesn't re-litigate them mid-port"; open empirical questions are assigned to a phase, not resolved in prose) · §2 File-by-file map · §3 Phased plan · §4 Testing & verification (incl. "what CANNOT be verified headlessly") · §5 Explicit deviations ("not gaps — decisions") · §6 Risks · §7 Open questions for the repo owner · §8 What this research did and didn't verify.

**Per-phase block (§3), every field mandatory:**
```markdown
- [ ] **Phase N — <title>.**
  - **Objective:** one sentence, independently demoable.
  - **Inputs:** files/docs/decisions this phase consumes (§ refs into this plan).
  - **Steps:** numbered, concrete, no step requiring an unstated decision.
  - **Files touched:** exhaustive path list (create/modify per path).
  - **Verify:** the exact commands, in order, + expected observable
    (e.g. `Automation RunTests ReactiveUI.Core` → N checks green; boot check).
  - **Done when:** binary criteria (tests exist and pass, numbers measured, user confirmed X).
  - **Status:** `- [ ]` → on completion `- [x] … — DONE YYYY-MM-DD, <evidence: test counts, sha, ms numbers>`.
```
Per-item states inside phases: `SHIPPED <date>` / `SKIPPED (<measured reason>)` / `DEFERRED (<where tracked>)`. Cross-doc items carry stable IDs (`U-##` for findings, `UO-##` for perf, `TD-##` shared register).

**Resume protocol (an AI session joining mid-plan reads, in order):**
1. `plans/ROADMAP.md` — one table row per live plan: `| Plan | Status | Phases done | Next phase | Branch |`. This is the single living-status doc (PARITY_PLAN's role, formalized).
2. The plan's header banner stack (newest first) — corrections that override the body.
3. §1 decisions (never re-litigate) → the **first unchecked phase's** Inputs/Steps only.
4. `git log --oneline -15` + the phase's Verify commands to establish actual ground truth before writing anything (the "code-verified sweep" habit).
Never trust body prose over banners; never trust banners over running the Verify commands.

**Propagation rules:** phase done ⇒ checkbox+evidence (plan) + ROADMAP row + `plans/PENDING_CHANGELOG.md` staging bullet. Plan done ⇒ goal blockquote rewritten to COMPLETE form + `git mv plans/X.md plans/archive/` + row in `plans/archive/README.md` (`| Plan | Why archived |`, noting anything consciously deferred and where it's now tracked) + ROADMAP row removed/moved to a Shipped section. All of this is the `plan-progress` skill; it is text-edits-only and never commits.

---

## d) Autonomy boundary — least user intervention, encoded explicitly

**The AI does WITHOUT asking (the default-open set):** read/search anything in the repo and engine source; create/switch feature branches off `origin/dev` in the work tree; edit work-tree files; build; run every headless test/bench/boot command; create scratch/bench/probe files (probes removed before commit); web-research; write and update plan docs, ROADMAP, PENDING_CHANGELOG, skills, templates; stage (not commit) release prep; spawn lesser-model subtasks per the delegation matrix; open a PR *when the campaign's established flow calls for it*.

**The AI does ONLY when the task or an invoked skill explicitly establishes it:** `git commit` (MEMORY.md is categorical: **no auto-commit, ever — even mid-release-style work**; field-test-editor's loop is a documented standing ask, dev-process campaigns commit per milestone once the campaign is agreed); `git push` to the feature branch.

**The AI NEVER does (user-only actions):** merge PRs (user clicks); trigger `publish.yml` / any release workflow ("user runs it — never trigger it yourself"); push to `dev` or `master` directly except the sanctioned post-merge fast-forward `git push origin origin/dev:master` when the flow reaches that step; edit the live tree while the user's editor is open, or kill their editor process; weaken/skip/disable any lint, test, or CI gate; add `Co-Authored-By`; post to Discord/store (prepare text only); force-push or rewrite pushed history.

**Escalate instead of improvising (STOP conditions baked into every skill):** a Verify command fails twice on the same theory ("never re-try the same theory twice"); a contract-corpus diff appears during a "behavior-preserving" change (by definition a bug in the change); a §1 plan decision turns out wrong mid-phase (update the plan with a banner + ask, don't silently deviate — "report back and decide together"); anything public-facing (store listing, Discord, README claims) before it ships.

**Why this yields least intervention:** the user's remaining touchpoints are exactly four — approve a plan, playtest when a phase's Verify says "in-editor", merge the PR, press Publish. Everything else (research, implementation, tests, docs, changelogs, status bookkeeping, the widget production line) is skill-scripted, tiered to the cheapest capable model, and gated by CI tripwires rather than by asking the user.