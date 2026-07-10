# Discord changelog

Community-facing release notes, newest first, pasted MANUALLY by the owner into the family
Discord server's `#unreal` channel after publishing — never posted by the AI.

**Entry format** (pinned here because a fresh repo has no incumbents to imitate):
`## [X.Y.Z] - YYYY-MM-DD`, a **bold hook line** (what + why it matters, with real numbers),
short prose paragraphs, an `Update to **ReactiveUI for Unreal X.Y.Z** (GitHub release / Fab
listing).` line, a `**Tooling:** UETKX <ver> (VS Code + VS 2022)` trailer when extensions
shipped, then a `---` separator line.

**Hard limit: ≤ 2000 characters per entry** (Discord message cap). Count before committing:
`awk '/^---$/{exit} {n+=length($0)+1} END{print n}' plans/DISCORD_CHANGELOG.md`

---
