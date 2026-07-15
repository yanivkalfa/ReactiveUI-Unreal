# PR: the Doom demo — a playable FPS whose framebuffer IS the widget tree (+ Canvas widget)

> Branch `feat/doom-demo` → `dev` (stacked on `feat/v1-gate-closeout` — merge that PR first;
> this one then shows only the Doom delta). Paste this file as the PR body.

The family's marquee stress showcase, ported third — like the Unity original and the Godot
port, this is a **fully playable software-raycast Doom-style FPS whose entire framebuffer is
the ReactiveUI element tree**: every wall strip, floor/ceiling band, sprite run, and tracer
is a real, individually keyed `<Image>` diffed by the reconciler every tick. No custom draw,
no 3D scene — the thesis, stress-tested by a real game.

## The headline number (`Bench.Doom`, committed baselines)

**~197 µs median per WHOLE game frame** — sim tick + geometry build + reconcile + real Slate
apply of the full ~168-quad framebuffer (walking E1M1 scene, M1, UE 5.6.1). The sim alone is
19 µs; geometry 34 µs. The retained-widget framebuffer costs ~0.2 ms of CPU per frame.

## What landed

- **`Canvas` widget** (plugin) — absolute placement over `SCanvas` (`Slot.Position`/`Slot.Size`);
  in-place live slot mutation (the per-frame-movement hot path); paint order = child order.
  Suite `ReactiveUI.Widgets.Canvas`. Plus **`<Image Image={ Brush } />`** as a markup attr
  (props field renamed `Brush`→`Image`, D-33) — which also made a latent uncompilable docs
  sample truthful.
- **The port (~7k LOC C++, from the Unity C# original with the Godot port's documented fixes)**:
  `DoomTypes` (LCG proven bit-identical over 10k iters; GO-03 pools), `DoomTextures`
  (67 procedural textures; .NET `System.Random` reproduced exactly — pixel-identical to the
  ORIGINAL), `DoomMaps` (six levels; 286/286 builder statements verified mechanically),
  `DoomRaycast`, `DoomGameLogic` (2,989 lines, C# section order 1:1), `DoomScreenGeometry`
  (11 painter-order passes; pre-tiled-texture UVRegion wall strips per the Phase-0 spike,
  answered from `ElementBatcher.cpp` source).
- **Six `.uetkx` screens** — menu (6 levels × 3 difficulties), the game framebuffer (11 `@for`
  passes in ONE `<Canvas>`), 8-panel HUD, animated face, automap, shell — all compiled markup,
  mounted green by the Demos suite.
- **`UseDoomGame`** — the loop hook: FTSTicker, per-tick input POLLING (no input preprocessor —
  the CommonUI promise holds), ESC-release/click-recapture mouse capture, version-bump
  coalesced re-render (one render per frame).
- **A compiler gap found & fixed**: `UseStableFunc`/`UseStableAction` were missing from the
  built-in hook table — uncallable from markup. Fixed in the C++ scanner AND the TS
  language-server mirror; schema re-exported (30 tags / 7 slot keys / 22 hooks).
- **`ReactiveUI.Doom` determinism suite** — 182 checks (vs the family's 179; 4 Godot-host-only
  checks replaced by 7 exit/boss-gate checks): every expectation matched the port on first
  execution. **Zero sim-port bugs found.**

## Gates (all green)

- **Battery 122/122, 0 failed** (UE 5.6; incl. the 10 Doom suites + Demos mounting the game)
- Drift gate 39 markup files / 0 drifted · contract goldens 10/10 · LSP 33/33 + smoke ·
  docs-drift 8/8 · headers/skills/mirror/corpus · clang-format full set
- `Bench.Doom` rows committed to BENCH_BASELINES.md with machine context

## What remains (the owner slice)

- **PIE playtest** — headless proves logic, not pixels: press Play → gallery → "Doom"
  (WASD + mouse, click to shoot, E for doors, 1–7 weapons, ESC frees the cursor)
- Then the demo video + showcase copy; the optional BSP renderer upgrade stays deferred
  (DOOM_DEMO_PLAN Phase 7)
