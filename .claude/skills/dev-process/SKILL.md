---
name: dev-process
description: The house development methodology for ReactiveUI-Unreal — the research→develop→test→bughunt→fix→commit loop, the UE gate ladder (compile → suites → boot check → demo), branch/PR model, changelog/version tables, and the laws that never bend. Use for any code change in this repo.
---

# Development process & methodology (ReactiveUI-Unreal)

## The loop

**research → develop → test → bughunt → fix → commit → repeat.**

- **Research first**: read the actual code and docs, reproduce the problem, name the root cause.
  No fix ships on a theory that hasn't been observed. Never re-try the same theory twice — get
  more instrumentation instead (temporary probes are fine; remove before commit).
- **Production-grade, long-term solutions only. Never a patch, never a bandaid.** If the correct
  fix is deeper (shared infrastructure, an adapter contract, the codegen emitter), fix it there —
  special-casing on top of shared machinery is the smell that the fix is at the wrong altitude.
- **Never weaken a lint, test, or CI gate to get green.** If a gate fails, the code is wrong.
- **Plans govern**: locked decisions live in `plans/MASTER_PLAN.md` §1 — don't re-litigate them
  mid-phase. If one proves wrong, banner the plan and ask the owner. Phase finished ⇒ run the
  `plan-progress` skill. Behavior/UX changed ⇒ run the `docs-sync` skill.
- **Never commit engine-derived source or Epic-owned content** to this MIT repo (D-32d); demo
  `Content/` is original/redistributable assets only, each listed in `THIRD_PARTY_NOTICES.md`.

## The UE gate ladder (in order — there IS a compile step here, unlike Godot)

1. **Compiles**: `Development Editor` builds clean (and the packaged target when packaging is in
   scope). Commands in `CLAUDE.md` / the `test-run` skill.
2. **Markup drift gate** (Phase 3+): `-run=RUICompile -check` exits 0.
3. **Suites green**: the affected `ReactiveUI.*` filters headless (NullRHI).
4. **Boot check — never optional**: `Automation RunTests ReactiveUI.Boot`. Unit suites do NOT run
   `StartupModule()`/Slate registration — the exact analogue of the Godot rule "suites don't run
   `_enter_tree`".
5. **A demo renders** when the change touches anything user-visible (`ReactiveUI.Demos` headless;
   owner playtest for what headless can't see — the phase's Verify says which).
6. A change isn't done until the checks that would catch its regression exist (new test
   demonstrably red→green once).

## Branch / PR model

Campaigns are **1 branch, 1 PR**: `feat/<name>` off `dev` → PR into `dev` (PR title = squash
title; owner merges) → `master` is a **fast-forward** of dev (`git push origin origin/dev:master`) —
`master` is release-only. Git authorship belongs to the owner: no `Co-Authored-By`, no
commits/pushes beyond what the task established, never auto-commit.

## Changelogs — every artifact you touched gets one, before release

| Artifact | Changelog | How |
|---|---|---|
| ReactiveUI plugin | root `CHANGELOG.md` | Hand-write; **byte-identical mirror** into `Plugins/ReactiveUI/CHANGELOG.md` (`cp`, never re-type; `scripts/verify-mirror.mjs` enforces) |
| VS Code + VS2022 extensions | `ide-extensions/changelog.json` | `changelog.mjs add --message-file …` → `extract` each target → commit together; `verify` gates CI |
| Community | `plans/DISCORD_CHANGELOG.md` | Notable releases; ≤2000 chars/entry; owner pastes |
| Pending (between releases) | `plans/PENDING_CHANGELOG.md` | `plan-progress` stages bullets; `release-process` drains |

## Version bumps

**Patch by default** (bump Z). Perf/refactor-only changes to shipped bytes still bump + one
changelog line — shipped code never changes silently. Use `node scripts/bump.mjs
<plugin|vscode|vs2022> <ver>` (handles lockstep partners + prints the follow-ups). Any
`ide-extensions/lsp-server/src` change bumps BOTH extensions — repackaging is how server fixes
ship.

## Definition of done

1. Root cause named; fix at the right altitude.
2. Gate ladder green, incl. the boot check; regression coverage exists.
3. Changelog entry in every touched artifact + version bump staged.
4. Docs updated when behavior/UX changed (`docs-sync`).
5. Committed on the feature branch with a clear message; PR into `dev`; owner merges; `master`
   fast-forwarded.

## Scar tissue (why these steps exist)

- **The boot check exists** because unit suites never execute module startup — the Godot sibling
  shipped a plugin that loaded broken while every suite was green.
- **"Never re-try the same theory twice"**: repeated blind retries burned whole sessions in the
  sibling repo; instrumentation always found the real cause faster.
- **Byte-identical means `cp`, not re-typing** — a re-typed mirror once differed by whitespace
  and burned a release cycle.
- **Perf-only still bumps**: "no behavior change" perf work shipped silently once; users couldn't
  tell which version had the fix.
- **The branch-integrity check before committing** (verify `git branch --show-current` is your
  feature branch): during Phase 0 bootstrap, an external IDE branch switch mid-run emptied the
  working tree of committed files; the recovery was trivial ONLY because it was noticed
  immediately.
