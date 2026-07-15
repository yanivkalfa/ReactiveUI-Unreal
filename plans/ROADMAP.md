# ReactiveUI-Unreal — Overview & Roadmap

> **STATUS UPDATE — 2026-07-15 (`feat/v1-gate-closeout`).** Localization SHIPPED (the last
> Phase-7 production gap): verified gather pipeline for markup text + culture-change → root
> re-render (`RuiCultureSync`), suite `ReactiveUI.Loc`; plugin 0.4.0. TD-020 (embedded-C++
> completion), TD-028 (host-widget props/VM channel), TD-029 (CommonUI desired focus) all
> RESOLVED; extensions 0.2.0. Router gallery demo added (17 screens). Battery 108/108 on
> UE 5.6. The consolidated backlog now lives in `plans/REMAINING.md`; finished campaign docs
> moved to `plans/archive/`.
>
> **STATUS RECONCILIATION — 2026-07-14 (audit second pass, `plans/archive/AUDIT_2026-07-14.md`).**
> Phase-4's row still claimed the interpreter/expression VM that **HMR v2 deleted**
> (`plans/archive/HMR_V2_PLAN.md`) — struck below; hot reload is Live-Coding-driven recompile.
> Phase-7's "media hooks" was really the `UseSfx` audio hook — reworded. Gallery count
> corrected 11 → 15 screens. The "80/80 battery" figure predates the current 100+-test
> battery. Docs site: guide/reference/integration content authored in the audit fix
> campaign; generated per-widget/per-hook references remain. The table below had lagged since Phase 0 (the scar
> the `plan-progress` skill warns about); reconciled to the delivered state. Phases 1–6 shipped
> across `feat/core-library` + `feat/uetkx-compiler` + `feat/uetkx-imports`; Phase 7 is
> substantially delivered (lists/focus/animation/portals/DnD/widget-batch-2 done — **localization
> deferred**); Phase 8 is in progress (gallery + benchmarks done, docs site in progress); Phase 9
> (release) is owner-gated. Evidence: the per-phase bullets in `plans/PENDING_CHANGELOG.md` +
> the 80/80 headless automation battery. **All work sits on `feat/uetkx-imports`, not yet merged.**
>
> **Status:** IN PROGRESS — the product is built end to end (reconciler → Slate host → `.uetkx`
> compiler → live hot reload → IDE tooling → Epic interop → production gaps); Phase 7's
> localization gap + Phase 8 docs + the Phase 9 release remain. One owner setup item still open:
> re-add the branch ruleset with THIS repo's check names (the imported one carried the Godot
> repo's and was deleted).
> **This document:** the plain-English picture of what we are building and where we stand.
> It is the **living status source of truth**: every time a phase in the master plan finishes,
> the matching row in the table at the bottom of this file is updated (see the `plan-progress` skill).
> **The detailed, AI-executable plan lives in [MASTER_PLAN.md](MASTER_PLAN.md). This file never
> contains instructions — only the picture.**

---

## 1. What we are building, in one sentence

**We are the layer that decides which Slate widgets exist each frame — everything of Epic's
(UMG, CommonUI, MVVM) stays in place, either feeding us data or hosting our output.**

Longer: a React-style reactive UI library for Unreal Engine, written in pure C++. You write UI as
functions that return a description of what the screen should look like. Our reconciler compares
this frame's description with last frame's and applies only the differences to real Slate widgets.
State lives in hooks (`UseState`, `UseEffect`, and 21 more — the same 23 the Godot and Unity
siblings have). On top of that sits `.uetkx` — a JSX-like markup file format, same grammar as
`.guitkx` (Godot) and `.uitkx` (Unity) — which you can edit **while the game is running in PIE**
and see the UI update in under a second, no C++ recompile.

This is the third sibling of a proven family:

| Sibling | Engine | Language | Status |
|---|---|---|---|
| ReactiveUIToolKit | Unity | C# | Shipped, the reference |
| ReactiveUI-Godot | Godot 4 | GDScript | Shipped (0.8.x), the process template |
| **ReactiveUI-Unreal** | Unreal 5.6+ | C++ | This plan |

## 2. What we are NOT building (and why that's the pitch)

We do not replace anything Epic ships. Each Epic technology has an exact role:

- **Slate** — the render target. Our output IS real Slate widgets, created with `SNew` and mutated
  with the same setters UMG itself uses. Widget Reflector, accessibility, and styling see ordinary
  widgets.
- **UMG** — a door in both directions. Designers can drop our UI inside their UserWidgets (one
  host widget), and we can use their UMG widgets as elements inside our tree. Nobody rewrites
  their existing screens to adopt us.
- **CommonUI** — we live inside it, never rebuild it. Menu stacks, gamepad input routing, platform
  glyphs, and focus navigation stay CommonUI's job; our screens are pushed onto their existing
  stacks.
- **MVVM / FieldNotify** — a data source, not a competitor. `UseField(VM, "Health")` subscribes a
  component to an Epic viewmodel field; when it changes, we re-render. They own values, we own
  structure. (And our state can be exposed back so classic UMG bindings can read it.)
- **Live Coding** — still how your C++ *logic* edits get patched. Our markup hot-reload is what
  makes *structure and styling* edits not need it at all.

Nothing else in the market occupies this slot: existing React-for-Unreal attempts all carry a
JavaScript VM (dead React-UMG, alpha ReactorUMG, the closed-early-access RNUE — which is modern
JSI/Fabric React Native, but still a JS VM and three tree layers). Commercial middleware (Coherent
Gameface, NoesisGUI) costs four-to-five figures and renders its own UI stack. We are the only
pure-C++, zero-VM, native-widget, ahead-of-time-compiled option — the exact position
ReactiveUI-Godot occupies in Godot.

## 3. The product — what ships

Four deliverables, independently versioned, one repo (the model proven in the Godot repo):

1. **The runtime plugin** (`Plugins/ReactiveUI/`) — the reconciler, hooks, Slate host, UMG/CommonUI/
   MVVM interop, the `.uetkx` compiler and the in-editor hot-reload loop. Distributed as source on
   GitHub (MIT) and as a free Fab listing, one zip per supported engine version (5.6 / 5.7 / 5.8
   at launch).
2. **IDE extensions** (VS Code + Visual Studio 2022) — `.uetkx` syntax highlighting, completion,
   diagnostics, formatting; embedded C++ intelligence via clangd. Reuses the existing family
   language server.
3. **The docs site** — a sibling of the Godot/Unity docs sites, same shell, GitHub Pages.
4. **The demo project** — the repo root is itself an Unreal project you can open and press Play,
   exactly like the Godot repo.

### The ship gate — "we ain't going to ship half a product"

*(This list mirrors the canonical gate at the top of the master plan's §3 — that one governs;
this one is the readable copy.)* v1.0 does not go public until ALL of these are true:

- The reconciler + **all 23 hooks, fully working** (no placeholders), pass the ported test suites.
- **25 widgets wrapped**: 15 core ones early, 10 more later on the production line, plus the
  virtualized list (a 5,000-row list renders a handful of widgets, not 5,000).
- Styling works via setters — a style tweak never destroys and rebuilds a widget.
- `.uetkx` markup compiles to C++ for shipping builds AND updates live in dev builds:
  save a file mid-play, see the change in under a second, state preserved.
- **Both** IDE extensions ship (VS Code and Visual Studio 2022), with markup smarts working
  offline and C++ smarts when a project index is available; Rider gets syntax highlighting.
- **The interop story ships** — it's the whole thesis: our UI inside a designer's UMG widget,
  their UMG widgets inside ours, live data from an Epic viewmodel, and a screen living on a
  CommonUI menu stack — each with a demo. (In code these are `URuiHostWidget`, `RUI::Umg`,
  `UseField`, and `URuiActivatableScreen`.)
- The production essentials: localization, safe asset/texture lifetime, focus that survives
  re-renders, and popups/menus (portals).
- The demo project shows all of it, the docs site documents all of it, and every performance
  claim has a measured number behind it.

**Deliberately NOT in v1** (so nobody wonders): the router subsystem (the Godot sibling's 17
routing hooks — later release), exit animations, the third styling layer (stylesheets),
drag-and-drop helpers and keyboard-shortcut APIs, a full Rider plugin (v1 ships Rider syntax
highlighting only), an in-Unreal-editor markup tab, on-device remote reload, and
UnrealSharp/AngelScript adapters. All tracked, none forgotten.

Until the last gate item is green, the repo is public-but-unannounced at most.

## 4. How the work is organized

Ten phases. Each is independently verifiable, each has exact instructions in the master plan, and
each ends with tests that prove it. Plain-English meaning of each:

- **Phase 0 — The ecosystem (before any product code).** Repo skeleton, the Unreal plugin scaffold
  that compiles empty, CI pipelines, the changelog system (ported day-zero from the Godot repo,
  without its historical scars), version/publishing machinery, the Claude skills that encode our
  process (including the two you asked for: *mark a phase complete when it finishes* and *update
  docs when a feature lands*), templates for the repetitive production lines, and this pair of
  plan documents wired in. The point: from day one this is a production project, not a prototype.
- **Phase 1 — The core.** The reconciler and all hooks, in a module that doesn't know Slate
  exists (it talks to a fake test host). This is where we copy from BOTH references: the Godot
  port for the family behavior, and React itself for improvements the Godot port hasn't adopted
  yet — chiefly: skip re-rendering the parts of the screen where nothing changed, move list items
  with the fewest possible operations (moving widgets is expensive in Slate), and make
  shared-data reads cheap no matter how deep the tree.
- **Phase 2 — The Slate host.** The seam that turns "description" into real widgets: create, diff,
  reorder, pool. The first ~15 widget wrappers, events, and the v1 style system (setter-based, so
  hot-reloading a color never rebuilds a widget).
- **Phase 3 — The markup compiler.** `.uetkx` → generated C++ (committed, reflection-free, build
  integration that does not fight Unreal's build tool). Same grammar as the siblings, pinned by a
  shared contract-test corpus.
- **Phase 4 — The live dev loop.** File watcher + interpreter: save `.uetkx` mid-PIE, UI updates
  in place. State survives unless you change the shape of a component's hooks (then it resets
  deliberately and tells you). This is the headline feature.
- **Phase 5 — IDE tooling.** VS Code + VS2022 extensions on the shared language server: your
  editor understands `.uetkx` files — completion, errors, formatting — and the C++ written inside
  them gets the same intelligence a normal C++ file gets (a projection trick we've already built
  twice, for Godot and Unity).
- **Phase 6 — Epic interop.** The UMG host widget (our UI inside theirs), `RUI::Umg` (their widgets
  inside ours), CommonUI activatable screens, and the MVVM `UseField`/`UseViewModel` hooks.
- **Phase 7 — The hard production gaps.** List virtualization, localization (FText gathering from
  markup), asset/brush lifetime, focus preservation across re-renders, animation primitives,
  portals (menus/windows). These are what separate a demo from a shippable library.
- **Phase 8 — Demos, docs, benchmarks.** The gallery project, the docs site content, and honest
  measured numbers against plain UMG (we make no performance claim we haven't measured).
- **Phase 9 — Release.** Packaging per engine version, the Publish-button workflow, Fab listing,
  Discord announcement.

Phases 1–2 and 3 can overlap (the compiler's final step waits for them); 4 needs 2+3; 5 needs
1+3; 6 needs 2; 7 needs 2+3. The master plan's §3 dependency block is the authoritative version.

## 5. Who does what

**Your recurring touchpoints are four** (day-to-day, everything else is designed to not need
you):

1. Approve a plan (this one, once).
2. Playtest when a phase's verification says "needs eyes in the editor" (the AI cannot see PIE).
3. Merge PRs.
4. Press the Publish button (and do the manual Fab upload — Fab has no API).

**Plus a one-time setup checklist, honestly listed** (mostly at the very start and at release —
accounts and money can't be delegated): ~~answer the master plan's open questions~~ (done
2026-07-10 — §7 there records the decisions); install UE 5.6 (and 5.8 later, near release);
apply the GitHub repo admin settings (branch protection, required checks, Pages) when Phase 0
lands; link your GitHub account to the Epic org and create the CI secrets if/when you want
engine-CI (there's a secrets table in the master plan — until then, tests run locally and CI
covers the engine-free parts); decide on a self-hosted build machine for packaging (or skip it
and package locally); create the marketplace publisher tokens when the extensions first ship; do
the Fab seller onboarding (identity verification has lead time — start early); record or approve
the demo video (the AI storyboards and drives the scenes; it can't record); sign off on public
art and listing text; and paste the Discord announcements into the family server's #unreal
channel (the AI drafts, you post).

**The AI does everything else** — research, implementation, tests, docs, changelogs, status
bookkeeping — under the autonomy rules written into the master plan (never commits without it
being an established ask, never touches your open editor's tree, never weakens a failing gate).

**Repetitive work runs on cheaper models.** The master plan defines production lines with
templates and checklists so that a small model can execute them and CI verifies the result:
widget wrappers (after the first few establish the pattern), style keys, docs pages, changelog
entries, contract-test cases, web-research sweeps for widget properties. Architecture, the
reconciler core, and anything touching a CI gate always stay on the strongest model.

## 6. Honest posture — market and risks

- **Money:** Fab's economics are modest (88/12 split, ~$1.2K/yr mean per publisher, no
  subscriptions). The model is OSS-core (MIT) + free Fab listing as the funnel; revenue, if any,
  comes later from tooling/support — treat it as reputation, not income. We build this because the
  slot is real and empty, not because Fab pays.
- **Audience honesty:** Fab's largest demographic writes Blueprints, not C++. They can use our
  markup, its built-in local state, and Blueprint logic classes — but anything needing a
  hand-written C++ handler excludes them. Our store copy will say what they can do, not imply
  everything.
- **Epic risk:** UE6 hits Early Access ~end-2027 with UMG/Slate expected to carry over; Epic's
  MVVM plugin is in maintenance mode; SlateIM (their immediate-mode experiment) is the nearest
  first-party encroachment. Our mitigation is architectural: everything engine-touching lives
  behind one seam (the host config + one compat header), so engine churn stays quarantined.
- **The two claims we will NOT make until measured:** that our diffing beats MVVM compiled
  bindings, and that widget reordering is cheap. Both have a benchmark phase before any marketing
  copy. (The research record of every corrected overclaim is in `research/`.)
- **Scope risk is the real one:** this is multiple engineer-years of surface. The phase gates and
  the v1 cut-line exist precisely so we never sit on a half-built everything. What's in and out of
  v1 is spelled out in the ship gate above — the "not in v1" list is the scope valve.

## 7. Current status

| # | Phase | Status | Done / Next |
|---|---|---|---|
| 0 | Ecosystem & repo bootstrap | COMPLETE 2026-07-10 | PR #4 merged; UE 5.6 editor build 45/45 green; all engine-free gates live (owner: re-add the ruleset with this repo's check names) |
| 1 | Core reconciler + hooks | COMPLETE | Fiber reconciler (subtree-skip, O(1) context, keyed diff, error-boundary latch) + all 23 hooks + signals/Suspense; 23 mock-host suites + Bench.Core (feat/core-library) |
| 2 | Slate host + first widgets | COMPLETE | Adapter registry, bind-once-swap-inner event proxy, SRuiRoot mount surfaces, 15 core widgets, style v1, GO-05 pool, focus fences, demo gallery (feat/core-library) |
| 3 | `.uetkx` compiler + build integration | COMPLETE | Lexer/parser/scan → committed reflection-free `.inl` + aggregators, schema-v2 sidecars, staleness machinery, RUICompile/RUIExportSchema/RUIContractDump commandlets, formatter (feat/uetkx-compiler) |
| 4 | ~~Interpreter +~~ hot reload | COMPLETE (re-scoped by HMR v2) | ~~D-20 expression VM + markup interpreter, FRuiHmr swap/link/reset + status line, editor watcher (3 triggers, MessageLog), rui.Hmr.AutoLiveCoding~~ — **SUPERSEDED 2026-07-14 (audit):** HMR v2 (`plans/archive/HMR_V2_PLAN.md`) deleted the interpreter/expression VM; hot reload = Live-Coding recompile via `FUetkxHmrController` + `FUetkxWatcher`, the `ReactiveUetkx` menu/window/settings, `ReactiveUetkx.HMR.*` console cmds (no `rui.Hmr.AutoLiveCoding` CVar exists) |
| 5 | IDE extensions (LSP, VS Code, VS2022) | COMPLETE | uetkx-language-server (schema completions/hover, sidecar diags, formatting) + VS Code + VS2022 extensions; embedded-C++ clangd proxy + VS2022 polish added (feat/uetkx-*) |
| 6 | UMG / CommonUI / MVVM interop | COMPLETE | Phase-6 core (URuiHostWidget, RUI::Umg::UserWidget, URuiWorldSubsystem, UseField) + TD-021 CommonUI activatables, MVVM global collection, UMG prop-map bridge (feat/uetkx-*) |
| 7 | Production gaps (lists, loc, focus, animation, portals, widget batch 2) | COMPLETE 2026-07-15 | Lists (virtualized ListView/TileView), focus, animation + SFX hooks (~~media~~ — only `UseSfx` exists, audit 2026-07-14), portals, widget batch-2 + specials, drag-and-drop, exit animations — all shipped. ~~Localization (FText gathering) DEFERRED~~ — **SHIPPED 2026-07-15** (gather from `*.uetkx.inl` + culture-change re-render; suite `ReactiveUI.Loc`; 0.4.0) |
| 8 | Demos, docs site, benchmarks | IN PROGRESS | Gallery ✅ (17 screens on the compiled path, incl. RouterDemo), Bench baselines ✅ (re-run 2026-07-15); docs site: guides + reference + Integration (four-pillar) + Localization pages ✅; generated per-widget catalog ✅; generated per-hook references ✅ (23 core + 17 router, drift-gated 2026-07-15). Doom demo ✅ BUILT 2026-07-15 (playable, 182-check suite, ~197 µs/frame — feat/doom-demo; owner playtest + demo video remain) |
| 9 | Release & publishing | NOT STARTED | Owner-gated — needs the ship gate (§3) + the `feat/uetkx-imports` → `dev` → `master` merge + Fab/Discord |

*Rows are updated by the `plan-progress` skill whenever a phase in
[MASTER_PLAN.md](MASTER_PLAN.md) is marked done. Phases 1–7 were delivered across earlier branches
but the table lagged; reconciled 2026-07-12 from the PENDING_CHANGELOG evidence (see the banner).*
