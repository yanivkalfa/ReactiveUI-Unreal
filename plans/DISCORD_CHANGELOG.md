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

## [0.4.0] - 2026-07-16

**DOOM runs in the widget tree — and a whole game frame costs ~0.2 ms of CPU.** The family's
marquee demo lands on Unreal: a fully playable raycast FPS where every wall strip, sprite, and
tracer is a real keyed `<Image>` in ONE `<Canvas>`, diffed by the reconciler at 60+ Hz. Six
levels, 100% procedural textures, zero assets — playable from the gallery (WASD + mouse,
Ctrl+R for the FPS counter). Measured: sim 19 µs, geometry 34 µs, reconcile + Slate apply of
~470 live widgets ≈ 197 µs median per frame.

The last v1 production gap closes: **localization**. Markup text gathers through the stock
Localization Dashboard (`.uetkx` emits real `NSLOCTEXT`), and a culture switch re-renders every
mounted screen live. Epic-interop rounds out too — `URuiHostWidget` takes designer-set props +
a viewmodel straight from the Details panel, CommonUI screens answer `GetDesiredFocusTarget()`
for gamepad focus, and `UseOwnedViewModel` / marshaling helpers land from the research promises.

New in markup: the universal `Ref={ }` attribute, a `Canvas` widget (absolute placement, live
slot mutation), a universal `Clipping` style key, and `ScaleBox` alignment. Every hook now has
its own generated reference page (23 core + 17 router) that CI diffs against the code — the
docs can't claim hooks that don't exist.

Update to **ReactiveUI for Unreal 0.4.0** (GitHub release / Fab listing).
**Tooling:** UETKX 0.2.0 (VS Code + VS 2022) — completion inside embedded C++ is now
clangd-backed.

---
