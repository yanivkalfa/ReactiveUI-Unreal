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
