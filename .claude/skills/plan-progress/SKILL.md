---
name: plan-progress
description: Mark a phase (or item) in a plan complete and propagate the status everywhere it must go — checkbox + evidence, superseded facts struck, ROADMAP row, pending-changelog bullet, and archive-on-complete. Deterministic text edits only; never commits. Use whenever a plan phase/milestone finishes or a plan-stated fact is proven wrong.
---

# Plan progress bookkeeping

Input: the plan file, the phase/item id, and the **evidence packet** (date; test suite name +
check counts; key commit sha; measured numbers if perf). Every step is a deterministic text edit
— this skill is haiku-class-executable by design.

## Procedure (phase completed)

1. **Flip the checkbox**: `- [ ] **Phase N — title.**` → `- [x] **Phase N — title.** — DONE
   YYYY-MM-DD, <evidence: suites + counts, sha, numbers>`. Set its `- **Status:**` line to match.
2. **Strike superseded facts in place** anywhere in the plan the phase invalidated:
   `~~old text~~ — **SUPERSEDED YYYY-MM-DD**` (never delete history).
3. **Prepend a banner** to the plan's banner stack:
   `> **STATUS UPDATE — YYYY-MM-DD.** Phase N complete: <one line>.` Update the header's
   `**Phases:** k/10 done` counter.
4. **Update the ROADMAP row** in `plans/ROADMAP.md` §status table (Status + Done/Next cells) —
   ROADMAP is the living status source of truth; it must never lag the plan.
5. **Stage the release note**: append one bullet to `plans/PENDING_CHANGELOG.md`
   (`- [lane A|B] [artifact] one-line summary (PR #n / sha)`) so `release-process` finds it.
6. **Bench numbers**, if any, land in `plans/BENCH_BASELINES.md` with the machine/config context
   that file requires — never only in the PR text.

## Procedure (LAST phase of a plan)

Everything above, plus: rewrite the plan's goal blockquote to
`**Status: COMPLETE YYYY-MM-DD — all N phases done; <deferred items> consciously deferred (tracked in <where>)**`;
`git mv plans/<PLAN>.md plans/archive/`; add its row to `plans/archive/README.md`'s
`| Plan | Why archived |` table (note outcomes and anything deferred + where it's tracked now);
move the ROADMAP row to a Shipped section.

## Item states vocabulary (use exactly these)

`SHIPPED <date>` · `SKIPPED (<measured reason>)` · `DEFERRED (<where tracked>)` ·
`~~struck~~ — SUPERSEDED <date>` · plan-level: `NOT STARTED / IN PROGRESS / BLOCKED(<on>) /
COMPLETE <date> / SUPERSEDED(<doc>)`.

## Boundaries

- **Never commits** — leaves the tree staged-ready and reports exactly what changed.
- Never edits §1 locked decisions (a wrong decision gets a banner + owner question, not a silent
  rewrite).
- STOP and escalate if the evidence packet is missing pieces (no invented check counts, ever).

## Scar tissue (why these steps exist)

- **Banners prepend, bodies stay**: the sibling repo's plans are readable histories precisely
  because corrections never rewrote the text they corrected.
- **ROADMAP updated in the same edit session**: a stale status table cost a full re-derivation of
  "where are we?" at the start of a later session — the table exists so resuming costs one read.
- **Evidence on the checkbox line, not in chat**: "done" claims without test counts/shas were
  exactly what the Godot repo's code-verified sweeps kept having to re-audit.
- **PENDING_CHANGELOG staging**: release notes reconstructed weeks later from git log missed
  user-facing changes; staging the bullet at phase-completion time is when the knowledge is fresh.
