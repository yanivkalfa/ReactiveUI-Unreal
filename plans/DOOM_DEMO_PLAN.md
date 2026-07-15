# Doom demo — the Unreal port (owner-decided 2026-07-15)

> **Status:** PLANNED (research complete — this file). **Branch (when it runs):** `feat/doom-demo`
> → `dev`, one PR, own campaign.
> **What it is:** the family's marquee stress showcase, ported third: a fully playable,
> software-rendered, sector/portal raycast Doom-style FPS whose **entire framebuffer is the
> ReactiveUI element tree**. Not a 3D scene with a HUD on top, and no custom-draw escape hatch:
> every wall strip, floor/ceiling band, sprite billboard, and tracer is a real, individually
> keyed host widget rebuilt and diffed by the reconciler **every tick**. It is a maximal,
> real-gameplay reconciler stress test — deliberately harder than the synthetic StressTest
> screen — and it becomes the demo video.

## 0. What the siblings shipped (research digest, 2026-07-15)

Both siblings ship the SAME game; Unity (`ReactiveUIToolKit/Samples/Components/DoomGame`,
~6,280 LOC) is the original, Godot (`ReactiveUI-Gadot/examples/demos/doom`, ~7,270 LOC) is a
faithful port that then added two Godot-only perf systems. Anatomy (both):

- **Markup (~0.9–1.3k LOC)**: `DoomGame` (menu↔game screen switch — plain `@if`, NO router by
  deliberate decision), `DoomMainMenu` (6 levels E1M1–E1M6 × 3 difficulties), `DoomGameScreen`
  (the raycast viewport + overlays), `DoomHUD` (8-panel status bar: AMMO/HEALTH/ARMS/FACE/
  ARMOR/KEYS/breakdown/INFO), `DoomFace` (8-state animated Doomguy face), `DoomMinimap`
  (auto-scaling automap), plus a `use_doom_game` hook owning the loop.
- **Logic (~4.9–6.4k LOC)**: `GameLogic` (sim + sector/portal raycaster, ~2k), `DoomTypes`
  (enums/structs/~70 tunables/**object pools**), `DoomMaps` (a MapBuilder DSL + 6 levels),
  `DoomTextures` (**everything procedural, zero image assets** — 12 walls, 7 floors, ~25
  sprites, sky, 8 faces, weapons, generated per-pixel), `Raycast` (portal-walking column
  caster), `DoomGameScreenLogic` (pure per-tick geometry-list builders feeding the `@for`s),
  a bootstrap (mount + raw input + perf overlay), and a shared input-state accumulator.
- **The load**: `VIEW_W = 160` columns → 160 wall strips + merged floor/ceiling bands + extra
  portal segs + sprites + tracers ≈ **several hundred live keyed widgets churning per frame**
  at a target 60 Hz sim.
- **Perf patterns that made it fast** (all must port): per-frame linear-allocator pools for
  WallSeg/FloorBand/CeilingBand (`reset_pools()` rewinds cursors each frame), a WallHit
  ring-pool in the caster, a fixed 256-slot Mobj pool with free-slot sentinel, ONE shared
  white texture so bands/sprites are the same node type (no type churn), **stable build order**
  (never re-sort the emitted list — the reconciler then rewrites only changed props), keyed
  quads (`"w%d"`, `"fb%d_%d"`, `"s%d_%d"`…) vs deliberately-unkeyed uniform groups (slot reuse).
- **State flow**: one monolithic `GameState` in the hook, **100% prop-drilling** (no
  signals/context by decision); input and the live state copy live in refs (never re-render
  triggers); the sim mutates in place, then hands the setter a fresh identity to defeat bailout.
- **What it deliberately does NOT use**: the router, virtualized lists, portals, tween/animate
  hooks, audio (all "animation" is per-frame math in style values).
- **Godot-only upgrades worth stealing later**: a 2D BSP visible-surface renderer
  (1.5–2.5× faster/frame, pixel-identical, own cmp+bench harness) and a per-key pooled wall
  material for vertical texture tiling.
- **Assets**: none. 100% procedural pixels — no third-party notices needed (D-32d clean).
  "DOOM"/level names are homage text only.

## 1. How Slate layering ACTUALLY works (verified against 5.6 source, 2026-07-15)

Slate has **no global z-index**. Paint order is hierarchical: every panel paints its children
in **arrangement order** — a later sibling paints on a higher layer ID, full stop. The three
exceptions/levers, verified in engine headers:

- **`SOverlay`** — the one panel with per-slot z: an `int32 ZOrder` per slot; "slots with
  larger ZOrder draw above; slots with the SAME ZOrder draw in the order they were added"
  (`SOverlay.h:65`). Slots are kept z-sorted at insertion (`AddSlot(ZOrder)`), and 5.6 has a
  `FOverlaySlot::SetZOrder` setter. **Our markup already exposes this** — `Slot.ZOrder` is in
  the shipped slot vocabulary and the Overlay adapter consumes it. Caveat: our adapter's
  slot-prop UPDATE path currently does RemoveSlot+AddSlot; if the demo ever mutates ZOrder
  per frame, switch that path to `SetZOrder` first (recorded as a Phase-0 adapter task).
- **`SCanvas`** — absolute placement: per-slot `Position` + `Size` attributes (+ alignment
  anchoring; FILL unsupported). **No per-slot z** — paints purely in child order. Not wrapped
  by us yet (note: our `RuiCanvas` tag is the CUSTOM-DRAW canvas — DrawFn/RedrawKey, no
  children — a completely different thing).
- **`SConstraintCanvas`** — UMG's canvas: per-slot Offset/Anchors + a `float ZOrder`,
  z-sorted at paint. Heavier than we need.
- Style keys `RenderTranslation`/`RenderScale`/`RenderTransformAngle`/`RenderTransformPivot`
  move/transform PIXELS (render transforms), not layout or paint order.
- The viewport-level `ZOrder` on `MountNamed`/`AddViewportWidgetContent` orders whole mounted
  ROOTS against other viewport content — orthogonal to intra-tree layering.

**The consequence for the demo:** the Godot port's per-quad `z_index` model has no cheap
Slate equivalent (per-frame ZOrder mutation on hundreds of Overlay slots would churn slots).
The **Unity model maps exactly**: one absolute-position panel, quads emitted in painter's
order (sky → ceiling bands → floor bands → rims → segs → walls → sprites → tracers →
weapon/flash), **stable build order within each pass**, and occlusion decided in the geometry
builders (only visible spans are emitted — how the sector engine already works). That also
preserves the Godot port's real perf lesson — never re-sort the emitted list.

## 1b. Unreal mapping — the decisions

| Sibling concept | Unreal/Slate answer |
|---|---|
| Host quad (TextureRect/VisualElement) | `Image` (SImage) with a brush; the shared-white-texture trick → one `FSlateBrush` reused so all bands are the same widget type |
| Viewport container + absolute placement | **a new `Canvas` widget wrapping `SCanvas`** (tag `Canvas`, slots `Slot.Position`/`Slot.Size` — one component-pipeline run, Phase 0). Paint = child order = the stable build order the plan needs. Fallback that works TODAY with zero new widgets: `Overlay` + `Slot.ZOrder` per PASS (fixed constants, never mutated) + `Slot.Padding` as position + fixed child sizes |
| Painter's order | the Unity model (ordered emission passes, stable within pass — §1); occlusion in the geometry builders, never via z mutation |
| Wall strip vertical texture tiling | Phase-0 SPIKE, three candidates: (a) `FSlateBrush` `Tiling=Vertical` + `UVRegion` per column — UVRegion exists (`SlateBrush.h:388`) but whether it COMPOSES with tiling is the open question; (b) pre-split per-column strip textures at texture-gen time (12 walls × 64 columns = 768 tiny 1×64 wrap-addressed textures — pure engine, memory-trivial, guaranteed to work); (c) pooled per-key brushes over `UMaterialInstanceDynamic` (brushes take a `UMaterialInterface` — the direct analogue of the Godot port's pooled ShaderMaterial) |
| Rotated tracer rects | style keys `RenderTransformAngle` + `RenderTransformPivot` (already shipped) |
| Procedural textures (SetPixels32/Apply) | `UTexture2D::CreateTransient` + mip write + `UpdateResource`, wrapped by `RUI::Umg::MakeAssetBrush` (GC-rooted — D-17 machinery already ships) |
| 16 ms scheduler / process_frame hook | a `UseDoomGame` hook: `UseEffect` registers an `FTSTicker` (dt-clamped); tick = consume input → `GameLogic::Tick(State, dt, Cmd)` → bump a version `UseState` (our coalescing then renders once/frame, the Godot lesson) |
| Raw input under mouse capture | the demo host project: a `ADoomDemoPlayerController`/game-mode leg capturing WASD/mouse-delta with `SetInputMode(GameOnly)` + mouse capture, writing a shared `FDoomInputState` (the Godot `_unhandled_input` pattern); the hook consumes+clears per tick |
| Language for the sim | **C++, ported from the Unity C# original** (closest source: value-struct semantics map 1:1; keep the Godot port's single-LCG `frand()` determinism fix) |
| Where it lives | the demo host project (`Source/RuiDemo/Doom/` + `Screens/DoomDemo/*.uetkx`) — NOT the shipped plugin; gallery gets a "Doom" entry, and a standalone map/game-mode gives the full-bleed + input-capture experience |
| Perf instrumentation | `stat ReactiveUI` + an F3-style overlay (sim ms vs reconcile ms — port the Godot bootstrap's classifier); numbers land in `plans/BENCH_BASELINES.md` as a new `Bench.Doom` workload |

## 2. Phases (each lands with tests; order by risk)

0. **SPIKE — the column renderer + the Canvas widget** *(the open technical questions, settled
   before any port work)*:
   - wrap `SCanvas` as the `Canvas` tag (component-pipeline run: adapter + `Slot.Position`/
     `Slot.Size` slot keys + schema + docs page) — the framebuffer container;
   - 160 `Image` strips on it, testing the three texture-tiling candidates from §1b (UVRegion
     × Tiling composition test first; pre-split column textures as the guaranteed fallback;
     MID-brush pool as the Godot-parity option);
   - measure render+diff ms at 800×500 with per-frame position/size churn; if the Overlay
     fallback is ever preferred, first switch the Overlay adapter's slot-prop update path
     from RemoveSlot+AddSlot to `FOverlaySlot::SetZOrder`/in-place mutation.
   Exit: a chosen renderer + ≥60 fps headroom on the owner machine, recorded here.
1. **Data + textures**: `DoomTypes` (enums/structs/tunables/pools), `DoomMaps` (MapBuilder DSL
   + E1M1–E1M6), `DoomTextures` (procedural gen). Verify: `ReactiveUI.Doom.Types/Maps/Textures`
   unit suites (pool rewind semantics, map integrity, texture dimensions/determinism).
2. **Sim core**: `GameLogic` + `Raycast` ported from C# (sector/portal engine incl. variable
   heights, lighting, doors-as-moving-sectors, 3D floors). Verify: port the Godot suite's
   **179-check** `doom_game_test` (determinism via the shared LCG; combat/pickup/door/exit
   scenarios), headless.
3. **Geometry builders**: `DoomGameScreenLogic` — pure per-tick builders emitting keyed quad
   lists (walls/bands/segs/sprites/tracers) in stable order. Verify: golden-frame tests (fixed
   seed + fixed input script → identical quad lists).
4. **Markup screens**: `DoomGame`/`DoomMainMenu`/`DoomGameScreen`/`DoomHUD`/`DoomFace`/
   `DoomMinimap` + `use_doom_game` hook in `.uetkx` (compiled path; HMR works on it like any
   screen). Verify: `ReactiveUI.Demos` mounts; screen-level assertions (menu→game flow, HUD
   values mirror state).
5. **Input + bootstrap**: the standalone map/game mode, mouse capture, the input accumulator,
   pause/died/level-complete overlays. Verify: owner PIE playtest (the checklist item).
6. **Perf pass**: pools wired end to end, `Bench.Doom` rows committed with machine context,
   the F3 overlay, `rui.TimeSlicing` interaction documented. Target: the Godot demo's ~80 fps
   CPU-bound baseline class, measured honestly.
7. *(optional, post-ship)* **BSP renderer**: port `doom_bsp` + its pixel-identical cmp harness
   and bench — the Godot-only 1.5–2.5× upgrade.
8. **Showcase**: demo-video storyboard (AI) + owner capture; Discord/release copy.

## 3. Risks (from the siblings' own records)

- **Perf is the point and the risk** (Godot plan §7 said the same): if Slate's per-widget cost
  makes 160-column churn slow, that is a *finding* to publish and attack (time-slicing, pool
  tuning, the BSP phase) — never something to hide by shrinking the demo.
- **Slate brush/UV tiling** is the one mechanism with no proven sibling analogue → it is
  Phase 0, before any port work.
- **Scale**: ~6–7k LOC, "the single largest single feature" in both sibling repos. Runs as its
  own campaign with per-phase commits; Phases 1–3 are engine-light (unit-testable headless) and
  parallelize well on the delegation matrix.

## 4. Explicit non-goals (family decisions, kept)

No router, no virtualized lists, no portals, no tween hooks, no audio in v1 of the demo; no
image assets ever (procedural only — keeps Fab/licensing trivially clean); no engine forks of
the render path (ordinary widgets, that's the thesis).
