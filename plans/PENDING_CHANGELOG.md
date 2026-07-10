# Pending changelog ledger

One bullet per merged user-relevant change, staged by the `plan-progress` skill at
phase/milestone completion — while the knowledge is fresh. **Drained by `release-process` §0**
into the real lanes (A: root CHANGELOG.md · B: `changelog.mjs add` · C: Discord) and then
EMPTIED. Bullets that never reach a lane are how release notes go missing — drain, don't append
around.

**Format:** `- [lane A|B|C] [artifact] one-line summary (PR #n / sha)`

---

- [lane A] [plugin] Phase 0: repository ecosystem + 8-module plugin skeleton (compiles on UE 5.6; CI gates, skills, templates, docs shell) (PR #4 / 657f279)
- [lane A] [plugin] chore: action pins v7/v6 across workflows, docs deps minor-bumped, dependabot major-ignore policy for the docs shell (chore/post-merge-tidy)
- [lane A] [plugin] Phase 1: the core library — fiber reconciler (subtree-skip, O(1) context, keyed diff, error-boundary latch, restart reclaim), 23 hooks, signals, Suspense; 23 mock-host automation tests + Bench.Core baselines (feat/core-library)
- [lane A] [plugin] Phase 2: Slate host — adapter registry, bind-once-swap-inner event proxy, SRuiRoot mount surfaces, 15 core widgets, style v1 (classes + removal-reset + never-reconstruct), GO-05 widget pool, focus fences, STATGROUP_ReactiveUI, demo gallery + PIE game mode; 38/38 suites (feat/core-library)
