# ReactiveUI-Unreal — Master Plan (AI-first)

> **Status:** IN PROGRESS
> **Branch:** `chore/post-merge-tidy` → `dev`   **PR:** pending owner merge   **Phases:** 1/10 done
> **STATUS UPDATE — 2026-07-10 (Phase 0 COMPLETE).** PR #4 merged (`657f279`), `main`
> fast-forwarded. The compile gate ran on the owner's fresh UE 5.6 install: **Development
> Editor build succeeded, 45/45 actions, 51 s** — all 8 plugin modules + both host modules
> compiled and linked, UHT clean under -WarningsAsErrors (MSVC 14.44, accepted with UE's
> "prefers 14.38" advisory — recorded in env facts). One Done-when clause is deferred to the
> owner's setup checklist: the "changelog verify as a REQUIRED check" designation — the
> imported ruleset was deleted (it carried the Godot repo's check names) and will be re-added
> with this repo's real check names (ROADMAP §5). Post-merge tidy: dependabot's action-pin
> bumps applied across all three workflows; docs deps minor-bumped (build+lint green); majors
> deliberately deferred family-wide via a dependabot ignore rule. **Phase 1 is next.**
> **STATUS UPDATE — 2026-07-10 (Phase 0 execution).** Steps 1–8 are BUILT and every engine-free
> gate is green (changelog verify · mirror byte-check · header lint · skills lint · docs-drift ·
> YAML lint · docs build+lint). Delivered: hygiene rails, the 8-module plugin skeleton +
> host project (boot banner in place), the scripts rail (changelog/bump/mirror/headers/skills/
> drift + the local publish rail), all nine skills, the plans ledgers (TECH_DEBT seeded
> TD-001..009, PENDING/DISCORD/BENCH/archive with formats pinned), the five production-line
> templates, the CI trio (self-guarded staged jobs; engine matrix unarmed), and the docs shell
> (builds + lints green, UE version selector). REMAINING for Phase 0 done: the
> Development-Editor compile Verify on the owner's machine (blocked on the UE 5.6 install) —
> then flip the checkbox via `plan-progress`. Step 9's PR is open for the owner.
> **STATUS UPDATE — 2026-07-10 (owner Q&A folded in).** All five §7 open questions are ANSWERED
> (see §7 for the decisions — naming kept, no engine CI for now, UE 5.6 installs first, Fab
> onboarding after the core milestone, demo at repo root; byte-compat confirmed HARD). The Godot
> sibling's 0.8.7 release is verified shipped (PR #69 merged, master fast-forwarded, all publish
> tags present) — this repo is the active front. Phase 0 steps 1–8 are engine-free and can start
> before Unreal is installed; only the compile Verify blocks on the UE 5.6 install.
> **STATUS UPDATE — 2026-07-10.** Initial version, written from the two-round research corpus in
> [`research/`](../research/). No code exists yet; the repo contains only `plans/` and `research/`.

**Audience: an AI session executing this project.** Humans should read
[ROADMAP.md](ROADMAP.md) instead. This document is written to be resumable, deterministic, and
non-relitigable: §1 decisions are settled (do not reopen them mid-phase — if one proves wrong,
banner it and ask the owner), §3 phases contain the exact steps, §4 the exact verification
commands, §8 the autonomy rules and the model-delegation matrix, §9 the skills to author.

**Resume protocol (read in this order when joining mid-project):**
1. [plans/ROADMAP.md](ROADMAP.md) — the status table (which phase is live).
2. This file's banner stack (newest first) — corrections override the body.
3. §1 decisions, then the **first unchecked phase's** block only (Inputs → Steps).
4. `git log --oneline -15` + that phase's Verify commands — establish ground truth by running,
   never by trusting prose. Never trust body text over banners; never trust banners over a green
   command.

---

## §0 — The one fact that shapes everything

**Slate widgets are construct-once C++ objects with no reflection.** There is no ClassDB, no
`node.set("property", value)`, no runtime signal discovery. Every widget type we support needs a
hand-or-codegen-written adapter: a factory, a typed props struct, a setter table, an event table,
and a "reconstruct-on-these-props" mask. This single fact drives the four biggest divergences from
the Godot port: typed props structs instead of Dictionaries (D-04), compile-time adapter registries
instead of reflective apply (D-11), an expression VM instead of interpreting the host language
(D-19), and clangd instead of a custom analyzer for IDE intelligence (D-23).

Everything else about the family ports: the fiber reconciler algorithm, the 23 hooks, the markup
grammar, the two-lane changelog, the skills, the test discipline. **When in doubt about intended
semantics: ReactiveUIToolKit (Unity C#) is the behavioral reference, ReactiveUI-Godot is the
process reference, React itself is the algorithm reference for the parts neither sibling has yet.**
Owner-confirmed priority order (2026-07-10): **family for everything the developer sees; React
for the internals where the siblings diverge; Unreal reality overrides both at the engine seam.**

---

## §1 — Locked decisions (do not re-litigate mid-phase)

Each decision carries its reasoning so implementation doesn't reopen it. Deep dives live in
`research/round2-implementation/` (cited per decision). Corrections from the adversarial critique
round are **baked in** here — see also §6's DO-NOT-CLAIM list.

### Identity & naming

- **D-01 — Names & headers.** Plugin folder `Plugins/ReactiveUI/`, descriptor `ReactiveUI.uplugin`,
  FriendlyName "ReactiveUI for Unreal" (final store name is owner's call — §7 Q1, .NET-ReactiveUI
  collision noted). Type prefixes: `FRui*` structs/classes, `SRui*` Slate widgets, `URui*`
  UObjects, `IRui*` interfaces, `TRui*` templates. Free-function factories live in namespace
  `RUI::` (`RUI::VBox()`, `RUI::Button()`, `RUI::FC(&Counter, {...})`). Markup extension is
  **`.uetkx`** (settles the `.uitkx`/`.uetkx` drift found across research docs). UMG host widget
  is **`URuiHostWidget`** (settles `UReactiveWidget` vs `UReactiveHostWidget` drift). Root object
  `FRuiRoot::Create(...)` + Slate widget `SRuiRoot`. **Every source file carries the seller
  copyright line** (Fab review checks it): decided in Phase 0, baked into module scaffolds, all
  `templates/`, AND the codegen emitter (committed `.inl`/`.gen.cpp` files carry it too —
  retrofitting after generated code is committed is a full-tree sweep), enforced by an engine-free
  CI lint.
- **D-02 — The codegen contract is the fluent builder API.** `.uetkx` compiles to chained-setter
  builder calls (`RUI::Button().Label(X).OnClicked(H)`), NOT designated initializers — attribute
  order in markup must not matter, and builders give that for free. The builder API is therefore
  a **public, stable contract**: every prop the markup schema exposes exists as a builder method.
  Hand-written C++ uses the identical API; markup and hand-written components are
  indistinguishable to the reconciler.
- **D-03 — Grammar is byte-compatible with `.guitkx`/`.uitkx` — the FULL grammar, not just
  component markup.** The declaration inventory is the family's three kinds: `component`, `hook`
  (custom-hook companion files, the `.hooks.guitkx` convention), and `module` (shared
  hooks/components grouping) — with the family file-layout convention (one `component` per file,
  sub-components as siblings, `module` for shared hooks). Directives:
  `@if/@elif/@else/@for/@while/@match/@case/@default` (`@while` interprets with a hard iteration
  cap — see D-20); `@uss`/`@theme` are grammar-RECOGNIZED (corpus cases must parse) but emit a
  "stylesheet layer is post-v1" `UETKX####` diagnostic. Reserved props (`key, ref, style, classes,
  children, items, draw_fn, redraw_key, reuse_by_slot`) and event-prop conventions (`onClick`
  aliases + `on_<delegate>` escape hatch) are the family's. Structural primitives
  (Portal/Suspense/ErrorBoundary/Memo) are NOT markup tags — they are `RUI::` escape-hatch calls
  inside `{expr}`, mirroring the family's `V.*` convention. Only the embedded-language *lexis* is
  swapped (C++ instead of GDScript/C#) — exactly how `guitkx_lexer.gd` is described in its own
  header. A shared contract corpus pins all of this (D-22).

### Core architecture (full detail: `research/round2-implementation/distill-corpus.md` §1, `react-reconciler.md`)

- **D-04 — Typed per-element props structs + set-bitmask. No prop bags.**
  `FRuiButtonProps : FRuiPropsBase { FText Label; bool bEnabled; FRuiCallback OnClicked; uint64 SetBits; }`.
  Diff = adapter downcast + memberwise compare-and-set — flat, branch-predictable, zero FName
  hashing. `SetBits` records which fields this render specified; a bit set last render but clear
  now is **ignored for plain fields** (family semantic: removed plain props don't reset) but
  **cleaned up for event/ref/style bits**. Boilerplate via `RUI_PROP(Type, Name, Bit)` macro
  (field + builder setter + compare row + apply row). Rejected: `TMap<FName,FRuiValue>` bags and
  `FInstancedStruct` (reasons recorded in distill §1.2).
- **D-05 — Components are free functions + explicit context; identity is a registry FName, not a
  raw function pointer.** `FRuiNode Counter(FRuiContext& Ctx, const FCounterProps&)`, wrapped as
  `RUI::FC(&Counter, {...})`. **Correction from the React research:** Live Coding relocates code,
  so raw fn-pointer identity would destroy all state on every C++ hot patch. Components register
  a stable `FName` id (codegen emits it; hand-written components use `RUI_COMPONENT(Counter)`);
  `matches()` compares ids; the registry re-binds pointers after Live Coding (`ILiveCodingModule`'s
  patch-complete delegate). Lambda components are allowed with documented always-re-render semantics.
  `FRuiContext` is non-copyable, passed by reference; a debug-only thread-local asserts in-render
  use. (Honest claim only: this *narrows* the misuse class — capturing `Ctx` in a stored lambda
  still compiles and corrupts the cursor at runtime; StrictMode (D-14) is the runtime net.)
- **D-06 — Fiber memory: reconciler-owned slab allocator, raw intra-tree pointers, React-style
  double buffering.** Fibers live in paged slabs (fixed addresses, free-list); `Parent/Child/
  Sibling/Alternate` are raw `FRuiFiber*`; matching positions reuse `Old->Alternate` → zero
  allocation on stable frames and pointer stability for stored closures (mandatory without GC).
  Note: the Godot port ALSO double-buffers since commit `f300ee3` — its "fresh fibers each pass"
  header comment is stale; do not propagate it. `FRuiComponentState` lives outside the slab as
  `TSharedRef`, shared across alternates, back-pointer raw and re-pointed each render; closures
  hold `TWeakPtr<FRuiComponentState>`, never raw `this`. All Slate delegate binds use `CreateSP`.
- **D-07 — Reconciler phases port 1:1** (begin_work descend / complete_work ascend / commit:
  deletions → placement/update/layout → reorder (structural frames only) → swap → two-pass passive
  flush → deferred-update replay). Keep the React-17-style singly-linked effect list (right for a
  synchronous renderer). Keep restart-from-root on mid-render setState + 25-restart guard.
  Scheduling: hook setter → `RequestUpdate()` → one-shot `FSlateApplication::OnPreTick`
  registration → **one coalesced render+commit per frame before Slate prepass**. `FlushSync` entry
  for mount/HMR/tests.
- **D-08 — Adopt from React what the Godot port hasn't (the "copy more from React" list, ranked):**
  1. **Subtree-skip bailout** — bubble a dirty mask (`childLanes`-style; `SubtreeDirty` consumed
     for real), and on bailout with a clean subtree **return null and skip the whole subtree**
     (one-level `cloneChildFibers` adoption). Godot writes `subtree_has_updates` but never
     consumes it; this is the single biggest known gap. Makes a 60 Hz HUD O(dirty path).
  2. **React's two-pass child reconciliation + `lastPlacedIndex` minimal moves** instead of
     Godot's commit-time full `_enforce_child_order` — Slate slot ops fire ChildOrder
     invalidation (the most expensive reason), so move-minimization matters here in a way Godot's
     cheap `move_child` never did. Keep an enforce-order fallback per adapter for panels without
     indexed insert.
  3. **O(1) context via provider value stacks** (push in begin_work, pop in complete_work AND in
     every bailout-skip/unwind path — the classic bug source; the pop lives in the ascend loop).
     Keep the family's `provideContext`-as-a-call API and out-of-band context-dep recording (no
     hook slot). Keep Godot's **eager** propagation (it is what makes subtree-skip safe); React
     19's lazy propagation is a recorded later optimization.
  4. **React's ref lifecycle** — attach in layout phase, detach on deletion and on ref-identity
     change, support React-19 cleanup-returning ref callbacks. (Deliberate divergence FROM the
     Godot port's call-every-commit refs — §5.) Ref values are `TWeakPtr<SWidget>`, never strong.
  5. **`useSyncExternalStore` discipline for `UseSignal`** — render reads snapshot; **subscription
     happens in a passive effect** with a post-subscribe snapshot re-check. Fixes the Godot port's
     subscribe-during-render leak window (restart before commit leaks the subscription).
  6. **StrictMode dev double-render** (D-14).
  Keep from Godot (do NOT "upgrade" to React): synchronous loop, atomic commit, direct-mutate hook
  slots (no update queues, no lanes model — a 3-value `SyncLane/DefaultLane/DeferredLane` enum
  only), eager context propagation, structural error boundaries, and the three list fast paths
  (fast-leaf-list, keys-stable prepass, `reuse_by_slot`) plus the apply-plan inline cache and node
  pooling — React has no equivalent and they matter more on Slate.
- **D-09 — Keyed diff: three tiers port verbatim** (fast-leaf-list in-place → keys-stable
  positional → full keyed with persistent cleared-per-call `TMap<FRuiKey, FRuiFiber*>` +
  per-fiber `MatchedPass` mark-sweep). Unkeyed children get namespaced positional sentinel keys.
- **D-10 — Error handling without exceptions.** UE ships `bEnableExceptions=false`; there is no
  throw path. Keep the family's structural error boundaries (`eb_active`/`reset_key`/`fallback`/
  `on_error`) AND add a cooperative error latch: `RUI_RENDER_FAIL(...)` sets a thread-local
  `FRuiRenderError`; the reconciler checks it after each component render and unwinds the WIP to
  the nearest boundary (popping context stacks), discarding the partial subtree — safe pre-commit
  because of double buffering. Never `setjmp/longjmp`. Dev-only editor try/catch is a bonus, never
  shipping behavior.
- **D-11 — Host seam: `IRuiHostConfig` + per-element adapter registry** (`IRuiElementAdapter`
  keyed by element type id — the Unity `ITypedElementAdapter` shape, which is cleaner than
  Godot's two-file host). Adapter contract: `CreateWidget` (SNew with construct-only args →
  `ApplyFull`), `ApplyDiff` (memberwise setters), `Equals`, `GetChildContainer` (multi-slot panel
  ops / single-content `SetContent` with `warn_capacity` / leaf), an explicit
  **reconstruct-on-prop mask** (construct-only props force widget replacement via the type-change
  path), and an **event table** (bind once at construction to a per-node
  `TSharedRef<FRuiEventProxy>`; prop updates swap the proxy's inner `TFunction` — no rebinds, no
  `TFunction` equality problem). Slot layout props (padding/alignment/fill/ZOrder) are a nested
  `FRuiSlotProps` on the child vnode applied via UE 5.0 runtime slot setters — the `slot.*`
  namespace in markup. Raw `FText`/`FString` children auto-wrap to `STextBlock`. Port the
  apply-plan inline cache, GO-05 pooling (detach + stash last props + diff-on-reuse, cap 256/type,
  never stateful widgets, drained on unmount/`ReleaseSlateResources`) and GO-09 `reuse_by_slot`
  verbatim.
- **D-12 — Setters, never delegate-bound TAttributes** (critique verdict: CONFIRMED). Constant
  attributes at construction, engine setters on change — widgets stay non-volatile and fast-path
  valid under `Slate.EnableGlobalInvalidation`. Manual `Invalidate`/`MarkPrepassAsDirty` only on
  custom leaves (`SRuiCanvas`, the `draw_fn` analogue — register-once paint trampoline,
  invalidate(Paint) only on callback identity / `redraw_key` change).
- **D-13 — Style system v1 is load-bearing, not deferred** (critique gap 19). The hot-path style
  keys (colors, opacity, font size, padding, margins, visibility, render transform) map to
  **setters** and must NOT sit on any reconstruct mask — a style tweak never destroys a widget
  (this is what makes the HMR demo honest, since style tweaking is its primary use). Construct-only
  style structs (e.g. full `FButtonStyle` swaps) are explicitly reconstruct-mask props, documented
  per widget. The family's three-layer style model (inline `style` dict → `classes` →
  stylesheet) ships its first two layers in v1; the stylesheet layer is Phase 7+. Removal
  semantics: style/event/ref/draw props reset on removal, plain props don't (family contract).
- **D-14 — Dev diagnostics & naming conventions:** CVar `rui.StrictMode` (dev/editor only)
  double-invokes render functions discarding the first result (flushes impure renders and
  stale-capture bugs — deadlier in C++), plus the Godot port's hook-order signature validation and
  set-during-render warnings. `rui.Stats` (renders/frame, fibers, patch counts), `rui.DumpTree`,
  `STATGROUP_ReactiveUI`, and the editor Inspector tab (Phase 8) with flash-on-rerender.
  **Conventions (recorded in CLAUDE.md at Phase 0):** CVars are dotted PascalCase after the `rui.`
  prefix; log categories are per-module `LogRuiCore` / `LogRuiSlate` / `LogRuiUmg` / `LogRuiInterp`
  / `LogRuiEditor`; the MessageLog page name is `"ReactiveUI"` (used by the D-19 watcher de-dup
  and D-20 interpreter errors).
- **D-15 — Threading contract** (critique gap 8): every public entry point `checkf(IsInGameThread())`;
  one documented marshaling helper `RUI::PostToGameThread(TFunction<void()>)` for async/network
  callbacks. FieldNotify delegates are game-thread-only anyway.
- **D-16 — Commit reentrancy contract** (critique gap 7): Slate setters fire delegates
  synchronously (SetText→OnTextChanged), so hook setters called during commit are **queued into
  the deferred-update replay** (already ordered post-commit). Generalize the editable-text
  skip-when-equal guard to all self-notifying setters in adapter apply rows.
- **D-17 — Memory across the UObject boundary + lifetime/teardown contract:** fibers/widgets are
  non-UObject; every UObject we reference (brush textures/materials, `RUI::Umg` widgets, viewmodels)
  is held via `TWeakObjectPtr` (observe) or `TStrongObjectPtr`/single `FGCObject` root on the
  mount surface (own) — decision per case recorded in the phase that introduces it (brushes:
  critique gap 2 — one `FGCObject` root per mount surface owning all brush-referenced UObjects,
  plus an `FRuiAssetRef` soft-path + async-load-policy type). **World teardown (critique gap 17,
  decided):** `URuiSubsystem : UGameInstanceSubsystem` with per-world root tracking; the teardown
  order contract is *unmount all roots of a dying world + drain the widget pool BEFORE GC runs*
  (wired to world-cleanup delegates); PIE-end and level-travel are CI-tested against this contract
  (Phase 6 step 6).

### Toolchain & DX (full detail: `research/round2-implementation/uetkx-toolchain.md`)

- **D-18 — The `.uetkx` compiler is C++ inside an `UncookedOnly` module (`ReactiveUIToolchain`);
  TypeScript stays IDE-intelligence-only.** A Fab plugin that needs Node.js to build is dead on
  arrival. The C++ side is the single codegen/diagnostics authority; the TS LSP does light
  structural passes and reads the compiler's diagnostics sidecars (`Foo.uetkx.diags.json`,
  schema v2, same FNV-1a `src_hash` constants 2166136261/16777619, `UETKX####` code space
  mirroring `GUITKX####` — shared-semantics codes keep identical numbers; UE-only diagnostics
  (C++-lexis, aggregator, interp-fallback) are allocated from **UETKX3000+** so they never collide
  with family numbers). Port the entire staleness machinery: error-verdict sidecars, mtime-tie
  content-hash break, compiler fingerprint file (`Saved/ReactiveUI/compiler.fp`), environment
  hold (GUITKX2507 analogue), orphaned-output sweep, duplicate-binding incumbent rule.
  **Offset unit (decided — the mismatch that forced Godot's `codePoints.ts` retrofit):** all
  `off`/`len` sidecar fields and all `{input, at, expect}` corpus cases are **Unicode code
  points** (family precedent). The C++ side converts from UTF-8/TCHAR, the TS side reuses
  `codePoints.ts` for UTF-16 conversion; the corpus MUST include non-BMP (surrogate-pair) cases
  from the first commit.
- **D-19 — Build integration: committed generated code + one stable aggregator TU per module.
  PreBuildSteps never generate.** Output: `Foo.uetkx` → sibling `Foo.uetkx.inl` (reflection-free:
  NO UCLASS/UPROPERTY in generated code — avoids UHT ordering and keeps Live Coding on its safe
  path) + a stable, always-present `<Module>.Uetkx.gen.cpp` whose contents are a regenerated
  `#include` list. This defuses the UBT makefile-caching trap (files created during PreBuildSteps
  of the same invocation aren't in the already-loaded action graph — the critique-confirmed
  pitfall): the compiled file set is **constant**; only file contents change. Generated files are
  **committed** (inversion of Godot's gitignore — UE has no headless regenerate-from-nothing on a
  cold clone; Launcher customers can't add Program targets). CI runs
  `-run=RUICompile -check` and fails on drift. Editor compile-on-save ports plugin.gd's three
  triggers (directory watcher + 2s `HasStale` poll + window-activation) with busy-guard/30s
  deadman/first-sweep proof-of-life/MessageLog de-dup + green "resolved" line.
  **Placement & lifecycle contract:** `.uetkx` files live under a C++ module's `Source/` dir
  (convention: `Private/`; a `Content/`-placed file can never compile and the watcher warns on
  it); discovery roots = Source dirs of the project + enabled plugins; the FIRST successful
  compile in a module creates that module's aggregator (`<Module>.Uetkx.gen.cpp`) — a one-time
  new file, so the NEXT UBT invocation re-gathers its makefile once (expected, harmless; only
  mid-invocation creation is banned, which the PreBuildSteps ban covers); the aggregator is
  never deleted once created (empty include list if the last `.uetkx` goes) so the compiled file
  set stays constant; users never edit Build.cs for this. **Codegen always emits LF line endings
  regardless of host OS** — `-check` compares bytes under that guarantee, and `.gitattributes`
  pins `eol=lf` on `*.uetkx`, `*.uetkx.inl`, `*.Uetkx.gen.cpp`, corpus JSON, and `CHANGELOG.md`
  (otherwise CRLF normalization false-fails both the drift gate and the D-31 byte-mirror).
- **D-20 — Dev loop: interpret in dev, compile to ship.** `ReactiveUIInterp` (a `Runtime`-type
  module excluded from Shipping via the module descriptor's `TargetConfigurationDenyList:
  ["Shipping"]` — the verified UBT mechanism; see D-27's linkage rule) walks the markup AST and
  emits the SAME builder calls the generated C++ emits — one vtree format, one reconciler;
  interpreted and compiled components co-exist. Interpretable: structure, literal props, style
  dicts, `@if`/`@elif`/`@else`/`@for`/`@match` over the expression VM (`@while` interprets with a
  hard iteration cap — 10,000, exceeded → error leaf), hook calls (kind + order + initial values —
  e.g. `UseState(0)`), event handlers **re-bound by name** to compiled functions. The expression
  VM is restricted and side-effect-free (literals, prop/state/loop/context paths,
  arithmetic/comparison/logic/ternary, member/index access, whitelisted pure calls via
  `RUI::RegisterInterpFn`) — with ONE sanctioned side effect: **hook setters are callable from
  event-handler expressions** (`onClick={ SetCount(Count + 1) }` — the setter identifiers from
  the component's `UseState` calls resolve in the VM; this is the markup-only interactivity
  surface, and it participates in the hook-signature hash like any state slot). **Handler
  classes:** a component names its logic class via the reserved plain attribute
  `logic={UShopLogic}` (a plain attribute, so D-03 byte-compat is untouched); handler names then
  resolve on that UClass via `FindFunction`+`ProcessEvent` (Blueprint classes included —
  verified in Phase 4). NOT interpretable (falls back to last-compiled behavior + on-screen
  dev badge + "rebuild required" log, optional auto-Live-Coding): new C++ statements, new handler
  bodies, new types/includes. **Scope clarity (owner-discussed): the evaluator restricts only
  brace-expressions, only in dev mode — structure and component nesting are NEVER limited (nested
  custom components, `@for` children, conditional subtrees are all "structure", fully
  interpretable), and shipped builds run full compiled C++ with zero restrictions.**
  **State preservation is governed by the hook-signature hash**
  (FNV-1a over ordered hook kinds+shape; compiler bakes the same constant into the `.inl`): equal →
  hooks array preserved; different → deliberate reset, counted and reported in the exact Godot
  status-line format (`reloaded/refreshed/reset/linked/global/ms`). Parse error → per-file
  isolation, last-good spec stays live, never crash the session. Interpreted-mode expression
  errors are **total**: typed default + MessageLog diagnostic + visible dev-only error leaf
  (critique gap 16).
- **D-21 — Markup source of truth is the loose `.uetkx` text file** (critique gap 13 resolved —
  "text files you can diff" is the pitch). In-editor asset representation, import/cook rules:
  the cooked build ships generated C++ only by default; an opt-in "interpret in cooked dev
  builds" CVar exists for live-tuning, never for Shipping.
- **D-22 — Contract corpora keep every dual implementation honest** (the family's proven
  mechanism, adopted in FULL, not just the case files): (a) **the C++ compiler is the grammar of
  record in this repo** — the TS side reproduces it, never the reverse; (b) case corpora
  `uetkx-scanner-cases.json` + `uetkx-formatter-cases.json` `{input, at, expect}` run by BOTH the
  C++ automation tests and `node --test`; (c) a **full-file fixtures + goldens harness** (the
  `tests/contract/` analogue): fixture `.uetkx` files (flattened copies of every shipped demo +
  targeted edge cases) live under `Source/RuiHostTests/ContractFixtures/` — a path EXCLUDED from
  the `RUICompile` sweep and UBT (the `.gdignore` analogue), goldens dumped by
  `-run=RUIContractDump` with a `--check` drift mode wired into CI, TS side replays every golden;
  (d) the **`.pending` fixture convention** pins KNOWN divergences — the test asserts the
  divergence still exists, so fixing one forces the rename+regen; (e) the cross-repo grammar
  corpus pins byte-compatibility across **all three siblings** (Unity `.uitkx` is the origin,
  Godot `.guitkx`, this repo) — divergence = bug in whichever side changed last, and a
  **sibling-corpus drift check** (hash compare) runs at release time (§9.2) because the family
  grammar demonstrably still evolves (Godot 0.8.5/0.8.7 shipped grammar changes). Formatter
  invariants port verbatim: verbatim-on-parse-error, idempotent, `fellBack` flag. Config:
  `uetkx.config.json` Prettier-style walk-up; **default `indentStyle:"tab"`** (Epic C++ standard;
  C++ has no hard tab requirement — formatter default, overridable).
- **D-23 — IDE intelligence: third instantiation of the virtualDoc + length-preserving SourceMap
  pattern, with clangd as the embedded-language analyzer.** Virtual doc = real `.cpp` projection
  (`#include` preamble spliced verbatim, `#include "ReactiveUI/HookStubs.h"` — a REAL shipped
  header whose signatures are parity-tested against the runtime, real control-flow emitted so
  loop/branch vars are in scope, `{expr}` → `auto _eN = (expr);`). clangd runs as a child process
  proxied by the existing lsp-server; positions translate through the SourceMap both ways.
  **clangd binary discovery (never bundle — bundling would reintroduce the per-platform-VSIX
  pain):** `uetkx.clangd.path` setting → PATH → the vscode-clangd extension's auto-downloaded
  binary → VS2022's LLVM component (`VC\Tools\Llvm\x64\bin\clangd.exe`) → degrade with an
  actionable "install clangd or set uetkx.clangd.path" notice (distinct from the no-compile-DB
  notice). `compile_commands.json` comes from `UnrealBuildTool -mode=GenerateClangDatabase` —
  which writes to the **engine root by default** (read-only for Launcher installs): the documented
  recipe uses the project-output flag where the floor engine supports it, else a documented
  copy step, so the walk-up discovery finds it at the project root (gitignored). **Graceful
  degradation is the family rule:** no compile DB → markup intelligence fully works, embedded C++
  goes dark with a one-line actionable notice. Markup-side schema has two sources: a curated
  shipped Slate vocabulary (Slate is not reflected — no dump can produce it) + a reflection JSON
  exporter commandlet (`-run=RUIExportSchema`) for UMG wrappers/USTRUCT/UENUM/delegate signatures,
  with pre-generated engine-builtin schemas shipped per engine version. This design **removes**
  the per-platform-vsix pain (no napi addon for Unreal). Editor coverage: VS Code + VS2022 in v1;
  **Rider gets the TextMate grammar + sidecar-driven diagnostics day one** (Rider honors TextMate
  bundles and is where much of the UE audience lives); a thin JetBrains-LSP-API Rider plugin is
  DEFERRED (tracked in TECH_DEBT). Embedded-C++ reflow: when a `.clang-format` is found by
  walk-up, embedded blocks are formatted through clang-format (the `embeddedReflow` analogue),
  else left verbatim.

### Interop (full detail: `research/round2-implementation/distill-corpus.md` §3)

- **D-24 — UMG both directions.** (a) `URuiHostWidget : UWidget` — designer-placeable;
  `RebuildWidget()` returns `SRuiRoot`; `ReleaseSlateResources` unmounts; `SynchronizeProperties`
  forwards designer props; **design-time renders a placeholder, never runs user effects when
  `IsDesignTime()`** (critique gap 11). (b) `RUI::Umg(Class, Props)` — `CreateWidget` with the
  owning player threaded down from the mount surface (required for CommonInput/localization),
  `TakeWidget()`, `TStrongObjectPtr` held on the fiber's host handle, released in deletion commit;
  props via cached per-class `FProperty` maps preferring `Set<Name>` UFUNCTIONs; FieldNotify
  targets via `UFieldNotificationLibrary::SetPropertyValueAndBroadcast`; events via pooled UObject
  trampolines for dynamic multicast delegates.
- **D-25 — CommonUI: two rules, never rebuild.** Screens live inside
  `URuiActivatableScreen : UCommonActivatableWidget` pushed onto EXISTING activatable containers;
  `UseActivation()` surfaces activate/deactivate; `autofocus` maps to `GetDesiredFocusTarget()`.
  Common widgets wrap fine via `RUI::Umg`; wrapping activatable subclasses as plain children is
  rejected with a diagnostic. Never touch: stacks/back-handling, `UCommonUIActionRouter` (no
  custom input preprocessor ever), `UCommonInputSubsystem` (read + rebroadcast via
  `UseInputMethod()`), gamepad focus/analog cursor, platform traits/safe zones. Lives in an
  optional module gated on the CommonUI plugin (D-27).
- **D-26 — MVVM/FieldNotify: consume, don't author** (critique verdict: engine-level claim
  CONFIRMED). `UseField<T>(VM, Name)` / `UseViewModel(VM)` with read tracking; FFieldId resolved
  once and memoized per (UClass*, FName); subscribe on mount commit via `CreateSP` against the
  hook state + explicit remove on cleanup; on broadcast: re-read, `FProperty::Identical` compare,
  only-if-changed mark dirty → coalesced re-render. VM held `TWeakObjectPtr` (observe, never own)
  with a stale policy; `UseOwnedViewModel<T>` uses `TStrongObjectPtr` for UI-local VMs. Reverse
  direction: `URuiSignalViewModel` implements `INotifyFieldValueChanged` (engine-level) so classic
  UMG/MVVM views can bind to our state; global-collection registration lives in the tiny optional
  `ReactiveUIMVVMBridge` module (the only part needing the ModelViewViewModel plugin).

### Modules, versions, distribution

- **D-27 — Module map (8 plugin modules):**

  | Module | Type | Deps (beyond Core) | Contents |
  |---|---|---|---|
  | `ReactiveUICore` | Runtime | — (**no UObject/CoreUObject**) | vnode, fibers, reconciler, hooks, keys, context, signals, `IRuiHostConfig` |
  | `ReactiveUISlate` | Runtime | SlateCore, Slate, InputCore | `FRuiSlateHost`, adapter registry, widgets, `SRuiRoot`, pool, `SRuiCanvas`, style v1 |
  | `ReactiveUIUMG` | Runtime | CoreUObject, Engine, UMG, FieldNotification | `URuiHostWidget`, `URuiSubsystem`, `RUI::Umg`, `UseField`/`UseViewModel`, `UseSfx` glue, `URuiSignalViewModel`, brush GC root |
  | `ReactiveUICommonUI` | Runtime | + CommonUI (optional plugin ref) | `URuiActivatableScreen`, `UseActivation`, `UseInputMethod`, `UseSafeArea` platform override |
  | `ReactiveUIMVVMBridge` | Runtime | + ModelViewViewModel (optional plugin ref) | global viewmodel-collection registration only |
  | `ReactiveUIInterp` | Runtime + `TargetConfigurationDenyList: ["Shipping"]` | ReactiveUICore | **markup lexer + parser (the single grammar implementation — Toolchain consumes it)**, markup AST interpreter, expression VM, interp registry, HMR wiring |
  | `ReactiveUIToolchain` | UncookedOnly | Interp (parser) | codegen, formatter, sidecars, fingerprint, `UETKX####` catalog |
  | `ReactiveUIEditor` | Editor | + editor modules, DirectoryWatcher | watcher/compile-on-save, `RUICompile`/`RUIExportSchema` commandlets, asset actions, Inspector tab (later) |

  **Router is post-v1** (the family's 17 router hooks + `router_match`/`router_spine` suite ports
  are deliberately NOT in v1 scope — recorded in `plans/TECH_DEBT.md` at Phase 0 and in the ship
  gate's exclusion list; the `react-reconciler.md` digest's "must not forget" warning is hereby
  answered with a decision, not an omission). Placing the parser in `ReactiveUIInterp` (Runtime
  denied only for Shipping) rather than the UncookedOnly Toolchain is what makes D-21's opt-in
  "interpret in cooked dev builds" possible — UncookedOnly modules don't exist in cooked builds.
  **Interp exclusion mechanics:** "compiled out of Shipping" is NOT a module Type — the mechanism
  is the descriptor's per-module `TargetConfigurationDenyList: ["Shipping"]` (Test stays allowed,
  serving the D-21 CVar). The denylist creates a linkage rule: no always-compiled module may
  statically reference Interp symbols — Interp self-registers into Core registries from its
  `StartupModule`, and any direct cross-module use is gated by a `WITH_RUI_INTERP` define set in
  the dependent's Build.cs from `Target.Configuration != Shipping` (mirroring the denylist).
  Phase 0's empty-plugin packaged build proves the arrangement.
  **Dedicated servers:** modules stay present on Server targets (NO `TargetDenyList: ["Server"]` —
  it would break any user game module that lists us unconditionally in Build.cs). Instead:
  `URuiSubsystem::ShouldCreateSubsystem` returns false on dedicated servers
  (`IsRunningDedicatedServer()`), every mount entry point `checkf`s a non-server world, and the
  rule is documented in Notes & limitations. A server-target compile leg joins the engine CI
  matrix when armed.

  Automation tests live in the **host project** (`Source/RuiHostTests`), keeping the shipped
  plugin lean. Verify the optional-plugin-reference mechanism (`"Optional": true` in the
  `.uplugin` `Plugins` array) for CommonUI/MVVMBridge in Phase 6 — if it doesn't gate module
  loading cleanly, fall back to `WITH_COMMONUI`-style Build.cs conditionals.
- **D-28 — Engine support: floor UE 5.6; launch matrix 5.6 / 5.7 / 5.8** (the "three latest" as
  of mid-2026; 5.8 is the final UE5 release, UE6 EA targeted end-2027). All engine-version churn
  is quarantined in the Slate host boundary + one `RuiEngineCompat.h` using
  `UE_VERSION_NEWER_THAN`/`UE_VERSION_OLDER_THAN` from `Misc/EngineVersionComparison.h`. Fab rule
  compliance: new products must support latest; maintain ≥1 of latest 3; one zip per engine
  version with matching `.uplugin` `EngineVersion`.
  **Target platforms (decided):** the code is pure C++ with no platform-specific paths — the
  `.uplugin` sets NO `PlatformAllowList`/`SupportedTargetPlatforms` restriction (so console
  builds are never blocked). CLAIMED-and-tested v1 platforms: **Win64** (primary dev + packaging)
  and **Linux** (CI headless when armed); **Mac** is declared supported, compile-verified when CI
  allows, best-effort otherwise; **mobile (iOS/Android)**: expected to work by architecture
  (pure C++/AOT — no JIT needed, which is exactly why the zero-VM design matters there; Slate/UMG
  ship on mobile routinely; `UseSafeArea` covers notches), untested in v1 and listed accordingly;
  on-device markup reload is the deferred remote-reload item. **Consoles**: compiles-from-source
  expected, explicitly listed as untested on the listing (buyers compile under their own
  SDKs/NDAs — the FMOD pattern). **Web**: not applicable — UE5 has no web target (Epic dropped
  HTML5 in UE4); not our gap. The Fab listing's supported-platforms declaration mirrors exactly
  this. **Owner install sequencing (2026-07-10):** no engines currently installed; owner installs
  **UE 5.6 (latest patch) first** via the Epic Games Launcher — author-on-the-floor, the proven
  "develop low, upgrade high" seller pattern — and adds **5.8** when packaging/release nears
  (each install ~50–100 GB; skip debug symbols). VS2022 + C++ workload already present (verify
  exact MSVC toolchain at Phase 0).
- **D-29 — Distribution: GitHub-first (MIT), Fab free listing as the funnel** (Cesium/Sentry
  precedent; Fab Distribution Agreement is non-exclusive). CI-built
  `ReactiveUI-<ver>-UE<engine>.zip` per engine version via `RunUAT BuildPlugin -StrictIncludes`
  (strip `Binaries/`/`Intermediate/` for Fab — Epic compiles binaries); GitHub Releases are the
  primary channel while Fab review (days→3 weeks) runs. **No Fab API exists — the manual upload
  checklist is a first-class release-process section**, and it MUST include the four
  review-enforced rules: (1) **AI-generated-content disclosure** in the listing (undisclosed →
  delisting; maximally relevant to this project — the Godot store template's "Developed with AI
  assistance under human direction" line is the model); (2) an **updated example project per
  supported engine version** accompanies every code-plugin submission (CI builds these from the
  demo host project); (3) **per-file copyright lines** (D-01); (4) **no trademark-adjacent
  naming** in the listing title (§7 Q1). Revenue expectations per §6.
- **D-30 — Versioning & branches.** Default branch is `main` (this repo's existing default);
  integration branch `dev`; campaigns are 1-branch-1-PR `feat/<name>` → `dev`; `main` is
  release-only, fast-forwarded via `git push origin origin/dev:main`. Patch-by-default versioning;
  perf-only changes to shipped bytes still bump. Version sources: plugin →
  `ReactiveUI.uplugin` `"VersionName"` (+ integer `"Version"` monotonic); VS Code ext →
  `ide-extensions/vscode-uetkx/package.json`; **lsp-server is a VENDORED copy in this repo**
  (settled): it version-locks to `vscode-uetkx/package.json` and carries no own tag (the Godot
  arrangement); family-shared files (`markup.ts`, `sourceMap.ts`, formatter, corpora) sync across
  repos via the `grammar-contract` skill's procedure. VS2022 → vsixmanifest; docs → not
  version-gated. Tags: `v*` (plugin — **one tag carries all per-engine-version zips as release
  assets**; no per-leg tags unless a true per-platform artifact matrix ever appears, in which case
  per-leg idempotency tags are mandatory from its first release — the Godot stranded-platforms
  scar), `vscode-v*`, `vs2022-v*`. Publish = user-pressed `workflow_dispatch` with idempotent
  tag-gated jobs. **Deprecation policy (post-1.0):** builder methods/props (the D-02 public
  contract) are removed only after ≥1 minor release carrying `UE_DEPRECATED(...)` + a changelog
  notice; the prop-map schema carries a `deprecated` field the LSP surfaces; grammar changes
  always go through the cross-repo corpus procedure (D-22).
- **D-31 — Changelog system from day zero, no cutover scars.** Lane A (runtime plugin):
  hand-written root `CHANGELOG.md` + byte-identical mirror at `Plugins/ReactiveUI/CHANGELOG.md`
  (resync via `cp`, tripwire test). Lane B (tooling: extensions): `ide-extensions/changelog.json`
  + `changelog.mjs add --message-file … / extract / verify`, `verify` as a required CI check from
  the first commit; `--message-file` (never `--message`) for anything non-ASCII. Lane B targets:
  `vscode`, `vs2022` (the vendored lsp-server has no separate lane — its changes are extension
  changes, per D-30's lockstep rule). Lane C: `plans/DISCORD_CHANGELOG.md`, ≤2000 chars/entry,
  manual paste. A new `scripts/bump.mjs` bumps an artifact + its lockstep partners + scaffolds
  the changelog entry in one command (Godot improvement #4).
- **D-32 — Legal & licensing.** (a) **Generated code in USER projects belongs to the user**: the
  codegen emitter is context-aware — files inside THIS repo get the D-01 seller copyright line;
  output generated in a customer's project gets a neutral
  `// Generated by ReactiveUI from Foo.uetkx — this generated code belongs to your project.`
  banner (ownership also stated in docs/FAQ). (b) **Dual distribution stated plainly**: GitHub
  source is MIT; Fab downloads are additionally governed by the Fab EULA — README + listing carry
  the sentence. (c) `THIRD_PARTY_NOTICES.md` at the repo root (and in `FilterPlugin.ini`) for any
  demo fonts/assets/tools. (d) **House law (goes in `dev-process`): never commit engine-derived
  source or Epic-owned content to the MIT repo** — demo `Content/` is original or
  clearly-redistributable assets only. Phase 0 scaffolds (c)+(d); Phase 3's emitter implements
  (a); Phase 9's checklist re-verifies all four.

---

## §2 — Repo layout (target state)

```
ReactiveUI-Unreal/                          # repo root IS the demo host project (Godot-repo pattern)
  ReactiveUIUnrealDemo.uproject             # host project; enables the plugin; demo maps
  Source/
    RuiDemo/                                # demo game module (gallery screens, .uetkx demos)
    RuiHostTests/                           # ALL automation tests (ReactiveUI.* hierarchy) — not shipped in plugin
  Content/                                  # demo maps + minimal assets
  Config/                                   # DefaultEngine.ini etc.
  Plugins/ReactiveUI/                       # DELIVERABLE 1 — the plugin (self-contained: own README/CHANGELOG/LICENSE)
    ReactiveUI.uplugin                      # FileVersion 3; Modules per D-27; VersionName; PlatformAllowList
    Config/FilterPlugin.ini                 # packaging allowlist (Docs/, README, LICENSE, templates)
    Source/ReactiveUICore/                  # D-27 table; each module: Public/, Private/, <Module>.Build.cs
    Source/ReactiveUISlate/
    Source/ReactiveUIUMG/
    Source/ReactiveUICommonUI/
    Source/ReactiveUIMVVMBridge/
    Source/ReactiveUIInterp/
    Source/ReactiveUIToolchain/
    Source/ReactiveUIEditor/
    Content/                                # small demo widgets if any (CanContainContent)
    README.md  CHANGELOG.md  LICENSE        # CHANGELOG.md = byte-identical mirror of root (tripwire)
  ide-extensions/                           # DELIVERABLE 2 — extends the FAMILY monorepo pattern
    changelog.json                          # Lane B source of truth (day-zero, no legacy tails)
    scripts/changelog.mjs                   # ported from Godot repo; scripts/bump.mjs + scripts/docs-drift.mjs new
    lsp-server/                             # VENDORED copy of the family server (D-30); new src/uetkx/{cppScanner,uetkxVirtualDoc,clangdProxy,uetkxSchema}.ts
    lsp-server/test-fixtures/uetkx-scanner-cases.json + uetkx-formatter-cases.json
    grammar/uetkx.tmLanguage.json           # TextMate; C++ embedded-scope injection (source.cpp)
    vscode-uetkx/                           # VS Code extension (language id uetkx)
    visual-studio/                          # VS2022 VSIX reusing the ILanguageClient stdio host
  docs/                                     # DELIVERABLE 3 — docs site (React+Vite, cloned family shell)
                                            # NOTE: no trailing-~ needed — UE only scans Source/ and Content/
  plans/                                    # ROADMAP.md, MASTER_PLAN.md (this), DISCORD_CHANGELOG.md,
                                            # PENDING_CHANGELOG.md, TECH_DEBT.md, archive/
  research/                                 # the two-round research corpus (committed evidence)
  templates/                                # production-line templates: widget_wrapper.template.cpp,
                                            # widget_test.template.cpp, hook_doc.template.tsx, prop-map schema
  .claude/skills/                           # §9 roster
  .github/workflows/                        # test.yml, ide-extensions.yml, publish.yml (§3 Phase 0)
  .gitattributes                            # export ALLOWLIST (Godot lesson: denylists leak)
  .gitignore                                # Intermediate/, Binaries/, Saved/, DerivedDataCache/, .vs/,
                                            # compile_commands.json, *.uetkx.diags.json (machine-local sidecars),
                                            # publisher-secrets.json (local-publish token mirror)
  .clang-format  .editorconfig              # Epic C++ style — the enforcement rail for production-line codegen
  .github/ISSUE_TEMPLATE/ + PULL_REQUEST_TEMPLATE.md + dependabot.yml   # community files (OSS funnel hygiene)
  CONTRIBUTING.md  SECURITY.md              # + designated support channel (GitHub Issues + the EXISTING family
                                            # Discord server — add an #unreal channel, never a new server)
  THIRD_PARTY_NOTICES.md                    # demo fonts/assets/tools attribution (D-32; also in FilterPlugin.ini)
  CHANGELOG.md  CLAUDE.md  README.md  LICENSE  icon files
  Plugins/ReactiveUI/Resources/Icon128.png  # plugin-browser tile (packaged; FilterPlugin-aware)
```

Generated-per-component (COMMITTED, per D-19): `Foo.uetkx` → `Foo.uetkx.inl` +
`Foo.uetkx.diags.json` (sidecar, committed? **No** — sidecars are machine-local, gitignored);
per module: stable `<Module>.Uetkx.gen.cpp` (committed). Machine-local:
`Saved/ReactiveUI/compiler.fp`.

---

## §3 — Phased plan

Format per phase: Objective / Inputs / Steps / Files touched / Verify / Done when / Status.
Statuses: `- [ ]` → `- [x] … — DONE YYYY-MM-DD, <evidence>`. Item states inside phases:
`SHIPPED <date>` / `SKIPPED (<measured reason>)` / `DEFERRED (<where tracked>)`. On completing a
phase, run the `plan-progress` skill (§9.3). Model tiers per step follow §8's matrix; steps marked
**[production-line]** are lesser-model-executable once their template exists.

**Phase dependencies (the single authoritative statement — ROADMAP's table mirrors this):**
1 needs 0 · 2 needs 1 · 3 starts after 0 (steps 1–8), completion needs 1–2 (step 9 + Verify run
demos through the compiled path) · 4 needs 2+3 · 5 needs 1+3 (HookStubs parity needs the Phase 1
hooks header) · 6 needs 2 · 7 needs 2+3 (the `loc()` form needs the compiler) · 8 needs 4–7 ·
9 needs the ship gate below.

### The v1 ship gate (canonical — Phase 9 step 1 audits EXACTLY this list; ROADMAP §3 is a mirror)

IN v1 (all must be green, verified by running):
- [ ] Core reconciler + **all 23 hooks fully wired** (no stubs — the five late-wired hooks
      UseSafeArea/UseTween/UseTweenValue/UseAnimate/UseSfx included) passing the ported suites.
- [ ] **25 widgets** (the 15 core widgets of Phase 2 + the 10 batch-2 widgets of Phase 7 step 8)
      **plus** the virtualized `SListView` items adapter — each through the full component
      pipeline.
- [ ] Style v1: setter-based hot-path keys (style tweak never reconstructs), `classes` layer.
- [ ] `.uetkx`: compiles to committed C++ (drift-gated in CI) AND interprets live —
      save mid-PIE, sub-second update, hook-signature state rule working and reported.
- [ ] IDE tooling: **VS Code AND VS2022** extensions (shared server), markup intelligence
      offline, embedded-C++ via clangd with graceful degradation; Rider TextMate grammar.
- [ ] Interop (the thesis — v1 ships it or the tagline is false): `URuiHostWidget`, `RUI::Umg`,
      `UseField`/`UseViewModel`, `URuiActivatableScreen` on a CommonUI stack — each with a demo.
- [ ] Production gaps closed: virtualization, localization, brush/asset GC, focus restore,
      portals.
- [ ] Demo gallery + docs site + measured benchmark table (no unmeasured perf claims).

OUT of v1 (deliberate, tracked in `plans/TECH_DEBT.md` from Phase 0 — not "demand-gated where
marked" hand-waving; THIS is the list): the router subsystem (17 family router hooks + both
router suites), the stylesheet third style layer, exit-animation protocol (designed in Phase 7,
shipped v1.x), drag-and-drop + keyboard-shortcut APIs, the Rider LSP plugin (grammar-only in v1),
the in-Unreal-editor `.uetkx` tab (and its cheaper cousin, a read-only live-preview panel — the
realistic someday-step toward designer-ish workflows), Phase-4-style on-device remote reload,
scripting adapters (UnrealSharp/AngelScript integration — or, owner's stated worst case, our own
adapter someday: if so, plugin-level like UnrealSharp, NEVER an engine fork, which would kill Fab
distribution, and always opt-in — never a mandatory VM under the shipped UI, per §6).

### - [x] Phase 0 — Ecosystem & repo bootstrap ("step 0": the production project before the product) — DONE 2026-07-10 (UE 5.6 editor build 45/45 green; all engine-free gates green; PR #4 `657f279`)

- **Objective:** a repo where every later phase has rails: compiling empty plugin, CI, changelog
  machinery, skills, templates, docs scaffold — such that from the first feature commit onward,
  process is enforced by tooling, not memory.
- **Inputs:** `research/round2-implementation/godot-ecosystem.md` (the full reference inventory +
  its 12 "IMPROVE FOR UNREAL" items — all 12 are owned: #1's offset-unit tail is decided in D-18,
  #11's editor-verify need is covered by the Boot suite in `test-run` + `field-test-editor`; the
  rest map to steps below); `ue-production.md` §§1–5; D-27..D-31. **Owner inputs — ALL ANSWERED
  2026-07-10 (§7):** Q2 → engine CI stays unarmed, everything engine-bound runs locally via
  `test-run` (revisit at release time); Q3 → owner installs **UE 5.6** (none installed today);
  discover the path via `Get-ChildItem "C:\Program Files\Epic Games\UE_5.*"` and record it in the
  skills' environment-facts section; Q5 → project name `ReactiveUIUnrealDemo`, at the repo root.
  **Execution note: steps 1–8 are pure text/scaffold work and need NO engine installed; the
  Verify compile (and everything after Phase 0) blocks until the UE 5.6 install finishes — start
  the ~50–100 GB download in parallel with this phase.**
- **Steps:**
  1. Scaffold the repo tree of §2: `.uproject` (name per Inputs default, floor engine
     association 5.6), plugin skeleton with all 8 modules compiling EMPTY (each module: Build.cs
     with `PCHUsage = UseExplicitOrSharedPCHs`, IWYU-clean, correct Type per D-27, `_API` macros),
     `.gitignore` (incl. `*.uetkx.diags.json`), `.gitattributes` allowlist,
     `Config/FilterPlugin.ini`, `Plugins/ReactiveUI/Resources/Icon128.png` (placeholder art —
     final art is an owner sign-off item in Phase 9), `.clang-format` (Epic style) +
     `.editorconfig`, and the **copyright header line** (D-01 — decided here, applied to every
     scaffolded file, encoded in templates and later in the codegen emitter).
  2. Write `CLAUDE.md` (repo map table, exact test commands in CI order, architecture summary,
     "known constraints — preserve these" list — clone the Godot CLAUDE.md's shape). Add GitHub
     community files: issue/PR templates, `CONTRIBUTING.md`, `SECURITY.md`, `dependabot.yml`
     covering the two npm trees; designate the support channel (GitHub Issues + Discord) — the
     Fab listing requires one.
  3. Port `changelog.mjs` (Lane B day-zero: targets `vscode`, `vs2022`; no legacy-tail machinery
     needed but KEEP the mojibake guard and `verify`); write `scripts/bump.mjs` (bump + lockstep
     partners + changelog scaffold), `scripts/docs-drift.mjs` (the docs-drift tripwire: greps
     README/docs claimed counts — "N hooks / M widgets" — against the actual registries; trivial
     at first, grows with the registries; this is the tool `docs-sync` (§9.4) and Phase 8's
     Verify invoke), `scripts/verify-mirror.mjs` (byte-compare root `CHANGELOG.md` vs the plugin
     mirror — the tripwire must be ENGINE-FREE here, unlike Godot's in-suite test, because engine
     CI is default-unarmed), `scripts/check-headers.mjs` (D-01 copyright-line lint over
     Source/templates/ide-extensions) and `scripts/lint-skills.mjs` (frontmatter + scar-tissue
     section present). Also the **local publish/package rail** (the unarmed-CI fallback the §7 Q2
     default makes the expected v1 path): `scripts/package-plugin.ps1` (BuildPlugin per engine
     version → staging dir with per-version zips) + ports of the Godot repo's two extension
     publish scripts, all resolving tokens param → env → git-ignored `publisher-secrets.json`
     (the Godot pattern). Seed root `CHANGELOG.md` (+ plugin mirror) at `0.1.0-dev`.
  4. Author the §9 skills (all nine), `plans/PENDING_CHANGELOG.md`, `plans/TECH_DEBT.md`
     (seeded with the ship gate's OUT-of-v1 list), `plans/DISCORD_CHANGELOG.md`,
     `plans/BENCH_BASELINES.md`, `plans/archive/README.md`. **Pin each artifact's format in the
     file itself** (a fresh repo has no incumbent entries to imitate): TECH_DEBT uses the family
     `## TD-## — title` template (Where: file:line · the defect · failure mode · why still open ·
     what a production-grade fix looks like); archive README is the `| Plan | Why archived |`
     table + a "Still live in plans/" trailer; DISCORD_CHANGELOG documents the entry shape
     (`## [ver] - date`, bold hook line, prose, "Update to **X**" line, `**Tooling:**` trailer,
     `---` separator) + the awk ≤2000-char count command; PENDING_CHANGELOG's format is one
     bullet per merged change (lane · artifact · one-line summary · PR/sha) and it is **drained
     by `release-process` into Lanes A/B/C then emptied** (§9.2); BENCH_BASELINES requires every
     row to carry machine spec (CPU/RAM/OS), engine version, build config, and
     `Slate.EnableGlobalInvalidation` state — cross-machine comparisons are invalid.
  5. Create `templates/`: `widget_wrapper.template.cpp`, `widget_test.template.cpp`, the prop-map
     JSON schema (the machine-readable per-widget schema BOTH the adapter codegen/docs AND the
     LSP vocabulary consume — single source), `hook_doc.template.tsx` — all carrying the D-01
     header.
  6. CI `test.yml`: job matrix armed by repo variables (the Godot "inert until armed" pattern):
     always-on jobs that need no engine (changelog verify, **changelog-mirror byte check**
     (`verify-mirror.mjs`), copyright-header lint, skills lint, clang-format check, lsp-server
     build+tests, contract-corpus TS side, docs build); **staged jobs self-guard inside the job**
     — first step checks whether the consuming directory exists yet (e.g. `ide-extensions/lsp-server`
     before Phase 5, `docs/` content before Phase 8) and exits 0 with a "not armed yet" log line,
     so required checks always report from commit one with NO paths filters. Engine jobs
     (`build-and-test-5.6/5.7/5.8` on `ghcr.io/epicgames/unreal-engine:dev-slim-<ver>` Linux
     containers, needs Epic-org-linked PAT secret; a DedicatedServer-target compile leg per D-27;
     Windows packaging job on self-hosted runner) gated behind `RUI_CI_ENGINE_ARMED`.
     **Required-checks trap:** PR triggers carry NO paths filter (Godot lesson — required checks
     must always report). Write the reason as an in-file comment (house style: incident
     commentary in workflows). Commit a **secrets inventory table** into `CLAUDE.md`
     (secret/variable name → consuming workflow → local-mirror key in `publisher-secrets.json` →
     who creates it): `RUI_CI_ENGINE_ARMED`, `EPIC_GHCR_PAT`, `VSCE_PAT`, `OVSX_TOKEN`,
     `VS_MARKETPLACE_TOKEN` — all owner-created; none block unarmed CI.
  7. CI `publish.yml`: workflow_dispatch; per-artifact version-read → tag-exists check → skip or
     build+tag+GitHub-Release (idempotent); plugin job runs BuildPlugin per engine version and
     attaches per-version zips; docs job deploys Pages; extension jobs publish marketplace(s).
     Factor the test-suite list into one reusable workflow consumed by both test.yml and
     publish.yml (Godot improvement #5 — no drift).
  8. Docs scaffold: clone the family docs-site shell into `docs/` (routing/theme/search), landing
     page only; wire `vite.config.ts` to read the plugin version + prop-map schema from day one
     (Godot improvement #9); port `versionManifest.ts` + the TopBar version selector with
     `SUPPORTED_VERSIONS` = the D-28 engine matrix (floor first) — feature maps fill in as
     version-gated features appear.
  9. Branch flow (must itself obey §8's law): create `dev` from `main` and push it — the
     sanctioned one-time bootstrap push; branch `feat/phase-0-bootstrap` off `dev`; all Phase 0
     commits land there; PR `feat/phase-0-bootstrap` → `dev`; owner merges; fast-forward `main`
     via `git push origin origin/dev:main`. (Commits within this phase are the established ask
     once the owner approves the plan.)
- **Files touched:** everything in §2 except module internals; `.github/workflows/*`;
  `.claude/skills/*`; `templates/*`; `docs/*` shell.
- **Verify:** `Build.bat ReactiveUIUnrealDemoEditor Win64 Development -Project=<abs>.uproject`
  compiles the empty plugin warning-clean; `node ide-extensions/scripts/changelog.mjs verify`
  green; skills lint (each SKILL.md has frontmatter + scar-tissue section); CI runs green on the
  PR (engine jobs may be unarmed — the armed/unarmed state is recorded in ROADMAP).
- **Done when:** empty plugin compiles on the owner's machine AND in whatever CI legs are armed;
  all nine skills exist; changelog verify is a required check; ROADMAP row flipped by
  `plan-progress`.
- **Status:** COMPLETE 2026-07-10 — evidence: Development Editor build on UE 5.6 succeeded
  (45/45 actions, 51 s; MSVC 14.44 advisory noted); engine-free gates green (changelog verify ·
  mirror · headers · skills · docs-drift · YAML · docs build+lint · clang-format after its
  first-run catch); PR #4 merged `657f279`; `main` fast-forwarded. Deferred to the owner's
  one-time checklist (not blocking): re-adding the ruleset with THIS repo's check names so
  the gates become REQUIRED checks (the imported Godot-named ruleset was deleted).

### - [ ] Phase 1 — Core reconciler + hooks (`ReactiveUICore`)

- **Objective:** the engine-blind core: vnode/builder API, fiber slab, full reconciler, all 23
  hooks, signals, context, structural error boundaries — tested against a mock host with ports of
  `core_test.gd` + `update_test.gd`.
- **Inputs:** D-04..D-10, D-14..D-16; `react-reconciler.md` (the entire gap table + the hooks
  table with per-hook C++ trickiness flags); Godot sources
  `addons/reactive_ui/core/*.gd` (algorithm reference — read them, they are the spec);
  Unity `FiberHostConfig.cs`/`ElementRegistry` shape for the adapter seam.
- **Steps:**
  1. `FRuiNode`/`FRuiPropsBase`/`RUI_PROP` macro/`FRuiKey`/`FRuiCallback` + the builder base.
     Per-frame **vnode arena** (frame allocator reset post-commit — critique gap 9) from day one.
  2. Fiber slab allocator + `FRuiFiber` (fields per the Godot fiber inventory: parent/child/
     sibling/index, tag, key, type id, component id, props/pending/input_children, host handle,
     portal target, alternate, effect flags + next_effect, deletions, matched_pass, context
     fields, dirty flags, state ref, apply-plan cache, eb fields).
  3. Work loop: begin/complete, three-tier keyed diff (D-09) + React two-pass with
     `lastPlacedIndex` (D-08.2), bailout incl. **subtree-skip** (D-08.1), context stacks (D-08.3)
     + eager propagation, commit sub-phases (D-07), deletion tracking, portals, error latch
     (D-10), scheduling on a host-provided tick delegate (Core stays Slate-free; `OnPreTick`
     wiring happens in Phase 2).
  4. Hook infrastructure: positional slots with three cursors, type-erased small-buffer cells,
     setter handles `{TWeakPtr<state>, slot}`, teardown order (`_dispose_fiber_state` verbatim —
     this is where TFunction capture cycles are severed). Then all 23 hooks per the
     react-reconciler digest's table (UseState/UseReducer/UseRef/UseMemo/UseCallback/
     UseImperativeHandle/UseEffect/UseLayoutEffect/CreateContext/UseContext/ProvideContext/
     UseDeferredValue (DeferredLane)/UseTransition (sync no-op)/UseStableCallback/UseStableFunc/
     UseStableAction/UseSafeArea/UseSignal (subscribe-in-effect per D-08.5)/UseSignalKey/UseTween/
     UseTweenValue/UseAnimate/UseSfx — **five of these are host-dependent stubs in this phase**,
     each with its wiring owner: UseSafeArea reads a value the host config supplies (mock host
     returns a test value now; the Slate host supplies `FDisplayMetrics` safe zones in Phase 2;
     CommonUI overrides it in Phase 6), UseTween/UseTweenValue/UseAnimate get their curve/timer
     drive over the adapter setter tables in Phase 7 step 5, UseSfx gets its
     `UGameplayStatics::PlaySound2D` world-context glue in Phase 7 step 5 (lives in
     `ReactiveUIUMG`). All five are FULLY WIRED before the ship gate — the gate forbids stubs.
     Deps equality decision: value-equality for value types, identity for shared refs —
     documented (§5).
  5. `FRuiMockHost : IRuiHostConfig` recording ops; port `core_test.gd`'s ~36 test funcs and
     `update_test.gd` as Automation Specs under `ReactiveUI.Core` / `ReactiveUI.Update` in
     `RuiHostTests`; **create the `ReactiveUI.Boot` spec** (module `StartupModule` runs, a root
     mounts on the mock host — the gate ladder's boot check exists from here on); port the bench
     harness shape (`ReactiveUI.Bench`, not pass/fail) with its first baseline rows committed to
     `plans/BENCH_BASELINES.md`.
  6. StrictMode/hook-order-validation/set-during-render diagnostics behind CVars (D-14).
- **Files touched:** `Plugins/ReactiveUI/Source/ReactiveUICore/**`, `Source/RuiHostTests/**`.
- **Verify:** `test-run` skill: build + `Automation RunTests ReactiveUI.Core; ReactiveUI.Update`
  headless (NullRHI) green with per-section markers; bench numbers recorded; boot check
  (`ReactiveUI.Boot` smoke: module starts, root mounts on mock host).
- **Done when:** every ported test green; subtree-skip demonstrably skips (a counter test asserts
  render counts); zero allocations on a stable frame (bench asserts slab/arena reuse).
- **Status:** NOT STARTED.

### - [ ] Phase 2 — Slate host + first widgets (`ReactiveUISlate`)

- **Objective:** real pixels: `FRuiSlateHost`, adapter registry, `SRuiRoot`, ~15 core widgets with
  events + v1 styles + slot props, pooling, `reuse_by_slot`; the demo host project shows a live
  hooks-driven screen in PIE.
- **Inputs:** D-11..D-13, D-16, D-17; `distill-corpus.md` §§1.5–1.6, 2 (Slate ground truth);
  the prop-map schema from Phase 0.
- **Steps:**
  1. `FRuiSlateHost : IRuiHostConfig`; adapter interface + registry; event proxy
     (`FRuiEventProxy`, bind-once-swap-inner). **Event-handler API decided HERE, not in Phase 6**
     (15 widgets ship on this signature): user callbacks are `void`-returning by default with the
     proxy returning `FReply::Handled()`; per-event an optional `FReply`-returning overload exists
     where pass-through matters (mouse/key events); the typedefs live in `ReactiveUISlate`
     (`FReply` is a SlateCore type — never in Core). Child adapters for the three kinds
     (multi-slot / single-content / leaf); `slot.*` props; reorder pass (permute via
     Detach/AttachWidget on structural frames — and **run the reorder benchmark spike now**:
     detach/attach vs remove+insert vs invalidation-boundary wrap, per the critique's downgrade of
     the invalidation claim; numbers land in `plans/BENCH_BASELINES.md`, the decision in
     `plans/TECH_DEBT.md`).
  2. `SRuiRoot` + `FRuiRoot::Create/Unmount`; mount surfaces: game viewport
     (`AddViewportWidgetContent`), a plain `SWindow`, and the standalone test path (the editor
     *tab* spawner is editor-only code — it ships with Phase 8's Inspector in `ReactiveUIEditor`);
     `OnPreTick` scheduling wiring; `FlushSync`.
  3. First widgets, hand-written to establish the pattern (opus-class): `STextBlock`, `SImage`,
     `SButton`, `SBorder`, `SBox`, `SVerticalBox`, `SHorizontalBox`, `SOverlay`, `SScrollBox`,
     `SSpacer`, `SEditableTextBox` (controlled-input caret rule), `SCheckBox`, `SSlider`,
     `SProgressBar`, `SRuiCanvas` (draw_fn trampoline). Each: prop-map entry → props struct →
     adapter → contract case → per-widget test → docs row. After the first five, the remaining
     ten run as **[production-line]** via the `component-pipeline` skill.
  4. Style v1 (D-13): central style-key registry (key → type → setter → reset behavior), hot-path
     keys OFF every reconstruct mask; `classes` merge layer; removal-reset semantics tests.
  5. Node pool + `reuse_by_slot` + apply-plan cache ports; pooling excluded for stateful widgets.
  6. Port `style_test.gd` → `ReactiveUI.Style`; widget tests under `ReactiveUI.Widgets.*`; a
     `ReactiveUI.Demos` suite that mounts every demo screen headlessly (NullRHI) — the analogue of
     `demos_test.gd`; focus capture/restore pre/post-commit per FSlateUser (critique gap 6) with a
     test; ship the `rui.Stats` console overlay (renders/frame, fibers, patch counts) and the
     `STATGROUP_ReactiveUI` counters (D-14) — the Doom-demo perf-overlay pattern, needed for every
     later benchmark.
  7. Demo screen in the host project (hand-written C++ builder API — markup arrives Phase 3/4):
     a counter + a dynamic keyed list + styled panels. Owner playtests in PIE (**field-test
     touchpoint**).
- **Files touched:** `ReactiveUISlate/**`, `RuiHostTests/**`, `Source/RuiDemo/**`, `templates/*`
  refinements, prop-map schema entries.
- **Verify:** `ReactiveUI.Core|Update|Style|Widgets|Demos` green headless; reorder-spike numbers
  recorded; owner confirms the PIE demo (visuals can't be verified headlessly — §4).
- **Done when:** 15 widgets shipped through the full pipeline; style tweak provably does not
  reconstruct (test asserts widget pointer identity across a style-only re-render); pool
  cheap-path benchmarked vs SNew.
- **Status:** NOT STARTED.

### - [ ] Phase 3 — `.uetkx` compiler + build integration (`ReactiveUIToolchain` + parser in `ReactiveUIInterp`)

- **Objective:** `Foo.uetkx` → committed `Foo.uetkx.inl` + stable aggregator TU, byte-compatible
  grammar, sidecar diagnostics, staleness machinery, commandlets — CI-gated for drift. (Steps 1–8
  start right after Phase 0; step 9 and the Verify need Phases 1–2 — see the dependency block.
  The lexer + markup parser land in `ReactiveUIInterp` per D-27 so the interpreter and cooked-dev
  builds can reach them; Toolchain consumes them.)
- **Inputs:** D-18, D-19, D-21, D-22; `uetkx-toolchain.md` PARTS 1–2 in full (it is the spec);
  the Godot compiler sources (`addons/reactive_ui/guitkx/*.gd`) as the algorithm reference.
- **Steps:**
  1. C++ lexer (`skip_noncode` state machine with C++ lexis: `//`, `/* */`, preprocessor lines,
     `"…"`, `'c'`, `R"delim(…)delim"`, `L/u8/u/U` prefixes) — corpus-pinned from the first
     function (`uetkx-scanner-cases.json`, D-22).
  2. Markup parser + jsx scan (byte-compatible grammar; port the `GUITKX####` catalog as
     `UETKX####` including 2101/2106/2107/2507/0304 semantics).
  3. Codegen: reflection-free `.inl` (builder calls + `RUI_COMPONENT` registration + baked
     hook-signature hash constant; `FText` literals emit as `NSLOCTEXT` — self-namespaced, so
     aggregated `.inl`s need no `LOCTEXT_NAMESPACE` pairs, and the D-32 context-aware
     copyright/generated banner) + codegen shapes for **`hook` and `module` companion files**
     (free functions in the `.inl` — D-03's declaration inventory) + `<Module>.Uetkx.gen.cpp`
     aggregator management + post-write parse verification (compile-check the emitted TU cheaply —
     at minimum a re-lex/parse of the output; full compile verification is CI's job).
  4. Sidecars (schema v2, FNV-1a constants per D-18), compiler fingerprint, staleness rules
     (every field-capture-hardened rule from the toolchain digest §1.3 ports: error-verdict
     sidecars, env-hold GUITKX2507→UETKX2507 with outputs KEPT, mtime-tie hash break, orphan +
     refs/2107 sweeps, duplicate-binding incumbent).
  5. Commandlets in `ReactiveUIEditor`: `-run=RUICompile [-full] [-check]` (CI drift gate),
     `-run=RUIExportSchema`.
  6. Formatter (C++ side) + `uetkx.config.json` walk-up loader; `uetkx-formatter-cases.json`
     goldens.
  7. Cross-repo grammar corpus: copy the `.guitkx` corpus cases that are host-language-agnostic,
     add C++-lexis cases; wire the `grammar-contract` skill's sync procedure; the Godot repo
     gains the mirrored cases in a follow-up PR there (tracked in TECH_DEBT until done).
  8. Full-file contract harness per D-22(c)–(d): fixtures under
     `Source/RuiHostTests/ContractFixtures/` (excluded from the `RUICompile` sweep + UBT),
     `-run=RUIContractDump` commandlet with `--check` wired into CI, `.pending` convention
     documented in `grammar-contract`.
  9. Convert the Phase 2 demo screens to `.uetkx` (compiled path); `ReactiveUI.Uetkx` +
     `ReactiveUI.Contract` suites.
- **Files touched:** `ReactiveUIToolchain/**`, `ReactiveUIEditor/Private/RUI*Commandlet.cpp`,
  `RuiHostTests/**`, `ide-extensions/lsp-server/test-fixtures/uetkx-*.json`, demo `.uetkx` files
  + committed `.inl`/aggregators.
- **Verify:** `RUICompile -check` exits 0 on clean tree, non-zero after touching a `.uetkx`
  without recompile (test both); `ReactiveUI.Uetkx|Contract` green; scanner corpus green on the
  C++ side (TS side arrives Phase 5); generated demo `.inl` compiles and `ReactiveUI.Demos` still
  green through the compiled path.
- **Done when:** demos run from committed codegen; drift gate is a required CI check; corpus is
  the compiler's regression suite.
- **Status:** NOT STARTED.

### - [ ] Phase 4 — Interpreter + hot reload (`ReactiveUIInterp` + watcher)

- **Objective:** the headline: save `.uetkx` mid-PIE → UI updates in place <1 s, state preserved
  per the hook-signature rule, honest fallbacks for non-interpretable edits.
- **Inputs:** D-20; toolchain digest §§1.4–1.5, 2(b)5–6, 2(c); Godot `plugin.gd` + `hmr.gd` +
  `hmr_debugger.gd` as behavior reference (in-process here — no debugger channel needed).
- **Steps:**
  1. Expression VM over `FRuiValue` (the restricted set per D-20; whitelist registry
     `RUI::RegisterInterpFn` shipping fmt/math/LexToString helpers). Total evaluation: errors →
     typed default + MessageLog + dev error-leaf widget.
  2. AST → interp component: emits builder calls; hook calls interpreted structurally; handlers
     resolved by name (compiled functions via the component/handler registry; `UFUNCTION` logic
     classes via `FindFunction`+`ProcessEvent` — verify Blueprint-class handlers resolve too,
     critique gap 12).
  3. Hook-signature hash: compiler-baked constant vs interpreter-computed; equal → preserve;
     different → reset, counted, reported in the Godot status-line format. Name-based migration
     where declared names+types match (critique gap 21).
  4. Watcher in `ReactiveUIEditor` (three triggers + busy/deadman + proof-of-life + MessageLog
     de-dup + green resolved line, per D-19); swap definitions under the same component key;
     `HmrRefreshAll` defeats bailout caches; per-file isolation on parse error; the full
     `reloaded/refreshed/reset/linked/global/ms` status line.
  5. Optional auto-Live-Coding setting (`ILiveCodingModule::Compile()`) for fallback regions.
  6. **Editor UX / onboarding surface** (the D-27 "asset actions" get built here — Phase 8's
     Getting Started depends on them): content-browser visibility for `.uetkx`, right-click →
     ReactiveUI → New Component (writes the template `.uetkx` + triggers the sweep), double-click
     → open in external editor, MessageLog deep links; plus the convenience mount path —
     `URuiSubsystem::Mount(<component or .uetkx path>)` and a drop-in mount actor for
     "see it on screen in one step".
  7. `ReactiveUI.Hmr` suite (port `hmr_test.gd` semantics: targeted vs global refresh, signature
     reset, isolation); owner field-tests the mid-PIE loop (**cannot be verified headlessly**).
- **Files touched:** `ReactiveUIInterp/**`, `ReactiveUIEditor/Private/UetkxWatcher.cpp`,
  `RuiHostTests/**`.
- **Verify:** `ReactiveUI.Hmr` green; scripted end-to-end: launch editor commandlet-driven PIE?
  — no: the PIE loop is the owner's playtest; the headless proxy is the suite + a
  watcher-simulation test (inject file-change events, assert re-render + state rules).
- **Done when:** owner confirms: style edit <1 s state-preserved; structure edit <1 s; hook-shape
  edit resets that component only, with the status line saying so; parse error leaves the old UI
  live.
- **Status:** NOT STARTED.

### - [ ] Phase 5 — IDE tooling (lsp-server + VS Code + VS2022)

- **Objective:** `.uetkx` first-class in VS Code and VS2022: markup intelligence offline from the
  schema, embedded-C++ intelligence via clangd, diagnostics from sidecars, formatter parity.
- **Inputs:** D-22, D-23; toolchain digest §§1.6, 2(d)-(e); the existing family lsp-server
  (reused wholesale — `markup.ts`, `jsxScan.ts`, `context.ts`, `sourceMap.ts` byte-for-byte,
  `semanticTokens.ts`, schema machinery, `formatGuitkx.ts` port).
- **Steps:**
  1. `cppScanner.ts` (C++ lexis, corpus-pinned against the C++ lexer via
     `uetkx-scanner-cases.json` — run in `node --test` AND the C++ automation suite).
  2. `uetkxSchema.ts`: shipped Slate vocabulary generated FROM the prop-map schema (single
     source) + `Saved/ReactiveUI/schema.json` reader (RUIExportSchema output) + pre-generated
     engine-builtin schemas per engine version.
  3. `uetkxVirtualDoc.ts` + `HookStubs.h` (real shipped header; signature parity test vs the
     runtime hooks header); `clangdProxy.ts` (child process, request forwarding through the
     SourceMap, compile_commands.json walk-up discovery, graceful degradation notice).
  4. TS formatter port + `uetkx-formatter-cases.json` both-sides green; embedded-C++ reflow via
     clang-format when a `.clang-format` walk-up hit exists (the `embeddedReflow` analogue), else
     verbatim (D-23).
  5. `grammar/uetkx.tmLanguage.json` (embedded `source.cpp` injection); VS Code extension
     (`vscode-uetkx`): language id, client, sidecar watcher; VS2022: reuse the ILanguageClient
     stdio host (no napi addon → single cross-platform VSIX — verify and celebrate in the
     changelog); **Rider day-one tier:** ship the TextMate grammar as a Rider-consumable bundle +
     document sidecar-diagnostics visibility (the JetBrains-LSP-API plugin is OUT of v1, tracked
     in TECH_DEBT).
  6. e2e smoke (`scripts/smoke.js` analogue): stdio handshake + markup completion + a clangd
     round-trip behind a flag (CI may lack an engine/compile DB — the degraded path is also
     tested).
- **Files touched:** `ide-extensions/**`.
- **Verify:** `npm ci && npm run build && node --test out/test/*.test.js && node scripts/smoke.js`
  in lsp-server; corpus green on BOTH sides; `vsce package` produces installable vsix; manual
  check in VS Code (owner or screenshot-verified).
- **Done when:** editing the demo `.uetkx` files in VS Code gives completion/diagnostics/format;
  embedded C++ hovers resolve UE types when compile_commands.json exists; degradation notice
  correct without it.
- **Status:** NOT STARTED.

### - [ ] Phase 6 — Epic interop (UMG / CommonUI / MVVM)

- **Objective:** the thesis made real: our UI inside theirs, theirs inside ours, their data
  feeding ours — with the migration story shippable at every step.
- **Inputs:** D-24..D-27; `distill-corpus.md` §3 in full.
- **Steps:**
  1. `URuiHostWidget` (design-time placeholder rule; `ReleaseSlateResources` unmount; pool drain).
  2. `RUI::Umg` (owning-player plumbing, `TStrongObjectPtr` handle lifecycle, per-class prop maps,
     delegate trampoline pool, unbind on cleanup).
  3. `UseField`/`UseViewModel` (+ read-tracking), stale-VM policy, `UseOwnedViewModel`;
     `URuiSignalViewModel` reverse bridge; `ReactiveUIMVVMBridge` global-collection registration.
  4. `ReactiveUICommonUI`: `URuiActivatableScreen`, `UseActivation`, `UseInputMethod`,
     activatable-as-child rejection diagnostic; verify optional-plugin gating (D-27 fallback if
     needed).
  5. Input policy, CommonUI half (the handler-signature decision itself moved to Phase 2 step 1):
     document how handled-vs-unhandled interacts with CommonUI's action router and gamepad
     navigation; drag-drop + shortcuts recorded in TECH_DEBT as post-v1. Verify Editor Utility
     Widgets work implicitly by placing `URuiHostWidget` in one (no separate surface — one test +
     a docs note).
  6. Suites: `ReactiveUI.Umg`, `ReactiveUI.Mvvm` (FieldNotify round-trip on a test VM),
     `ReactiveUI.CommonUI` (activation lifecycle headless where possible); world-space UI
     (`UWidgetComponent`) verified + demo (critique gap 10); PIE-end/level-travel teardown-order
     tests verifying the D-17 contract (`URuiSubsystem` per-world tracking; unmount + pool drain
     BEFORE GC); `UseSafeArea` CommonUI override wired (the Phase 1 stub's final owner).
- **Files touched:** `ReactiveUIUMG/**`, `ReactiveUICommonUI/**`, `ReactiveUIMVVMBridge/**`,
  `RuiHostTests/**`, demo screens.
- **Verify:** suites green; owner playtests the mixed demo (UMG screen hosting Rui island +
  Rui screen hosting a Common button; gamepad focus walks through both).
- **Done when:** the four-step migration story (§3.5 of distill) each demonstrated by a demo.
- **Status:** NOT STARTED.

### - [ ] Phase 7 — Production gaps (the critique's hard list)

- **Objective:** close the gaps that separate a demo from a shippable library. Each item is a
  critique gap with its resolution decided in §1/D-refs; this phase implements them.
- **Inputs:** `distill-corpus.md` §6.1 (the 21 gaps — each names its resolution).
- **Steps:** (each lands with tests + docs; order by dependency then risk)
  1. **List virtualization** (gap 4): the `items`-seam adapter mapping declarative rows onto
     `SListView` `OnGenerateRow`/recycling with identity-based tracker state (the Godot `items`
     registry is the template). Named hard deliverable of v1.
  2. **Localization** (gap 1): `loc("NS","Key","Text")` markup form → `NSLOCTEXT`/string-table
     refs in the generated `.inl` (Phase 3 already emits NSLOCTEXT — no custom `.uetkx` gatherer
     needed: add `*.inl` to the project's `GatherTextFromSource` file masks and verify entries
     appear); culture-change event → root re-render.
  3. **Brush/asset lifetime** (gap 2): `FRuiAssetRef` + mount-surface `FGCObject` root; async
     load policy; tests that GC never collects a displayed texture.
  4. **Focus** (gap 6): implemented in Phase 2 — extend to keyed-reorder restore + FSlateUser
     multiplicity; document reconstruct⇒focus-loss rule.
  5. **Animation + media hooks fully wired** (gap 3 — the Phase 1 stubs come due; the ship gate
     forbids stubs): `UseTweenValue` (primary, steer docs here) + `UseTween`/`UseAnimate` over the
     adapter setter tables + `FCurveSequence`/active timers; `UseSfx` via
     `UGameplayStatics::PlaySound2D` with the world context carried by the mount surface (glue in
     `ReactiveUIUMG`), bus → `USoundClass` mapping. Only the **exit-animation protocol** (delayed
     unmount) may be deferred: DESIGNED now, shipped if time allows, else DEFERRED with the design
     doc committed (it is on the gate's OUT list).
  6. **Portals** (gap 5): portal fiber owning out-of-tree content (overlay slot / `SWindow` /
     `PushMenu` targets as opaque handles), teardown on unmount.
  7. **Accessibility** (gap 18): verify real-SWidget inheritance of the accessibility layer with
     a screen-reader pass; substantiate or soften every claim in docs.
  8. **Widget batch 2 [production-line]** (closes the gate's 25-widget line): ~10 more widgets
     through the `component-pipeline` skill — `SComboBox`, `SSpinBox`, `SMultiLineEditableTextBox`,
     `SExpandableArea`, `SSeparator`, `SWrapBox`, `SGridPanel`, `SUniformGridPanel`, `SScaleBox`,
     `SThrobber`/`SCircularThrobber` (exact list may trade one-for-one with demand discovered in
     Phases 6–8; the COUNT is the gate).
- **Files touched:** `ReactiveUISlate/**`, `ReactiveUICore/**` (portals/exit-anim touch the
  reconciler), `ReactiveUIUMG/**` (assets, UseSfx), docs, demos, prop-map schema entries.
- **Verify:** per-item suites (`ReactiveUI.Widgets.ListView` with a 5,000-row source asserting
  live-widget count; loc gather produces entries; GC test; focus tests; portal lifecycle;
  `ReactiveUI.Widgets.<Name>` per batch-2 widget; tween/sfx hook tests); bench: virtualized list
  scroll numbers.
- **Done when:** items 1–6 + 8 shipped (item 5's exit-animation sub-part ship-or-defer); 7
  verified; all 23 hooks now stub-free.
- **Status:** NOT STARTED.

### - [ ] Phase 8 — Demos, docs site, benchmarks

- **Objective:** the gallery, the docs, and honest numbers.
- **Inputs:** family docs sites (shell already scaffolded Phase 0); Godot demo gallery structure;
  critique gap 9 (perf claims need baselines).
- **Steps:**
  1. Demo gallery: counter, todo, styled panels, keyed-list stress, virtualized inventory (5k),
     HUD-over-game, CommonUI menu stack, MVVM-bound screen, world-space panel, HMR playground.
     All authored in `.uetkx` [production-line for the simple ones]. **Marquee demo decision**
     (the family precedent: the Godot Doom clone drove `reuse_by_slot`/pooling/keyed-diff perf
     work, fed the bench suite, AND became the demo video): one flagship stress showcase,
     ship-or-defer decided here — leading candidate is porting the family Doom demo's markup to
     `.uetkx` (grammar is byte-compatible; the `.guitkx` sources are the starting corpus). If
     deferred: seeded in TECH_DEBT with the dual role (perf-workload driver + marketing
     centerpiece) recorded.
  2. Docs content — the checklist is the FAMILY site inventory (minus Router), not a shortlist:
     Getting Started (the "first five minutes": install → New Component template → mount → PIE →
     edit-and-watch), Concepts, Hooks reference (23 pages [production-line]), Elements reference
     generated from the prop-map schema (with per-engine-version feature annotations), `.uetkx`
     syntax, **Companion files** (hook/module declarations, file-layout convention), HMR
     semantics incl. the honest limitation set (D-20 verbatim), **Diagnostics** (the `UETKX####`
     code catalog — GENERATED from the Phase 3 catalog source, docs-drift-checkable), **Config**
     (`uetkx.config.json`, CVars, settings), **Styling**, **Events**, **Signals**, **Context**,
     **Custom rendering** (`SRuiCanvas`/draw_fn), **Portals**, **Suspense**, **Assets**
     (`FRuiAssetRef`/brush lifetime), **Debugging** (`rui.Stats`/`rui.DumpTree`/Inspector),
     Interop guides (UMG/CommonUI/MVVM), Differences vs siblings, Notes & limitations,
     **Known issues**, FAQ, Roadmap.
  3. Editor Inspector tab (fiber tree, hook slots, render counts, flash-on-rerender,
     click-through to Widget Reflector) — the `rui.Stats` overlay shipped in Phase 2.
  4. Benchmarks vs baselines: same screen in pure UMG + MVVM bindings vs ours (update latency,
     memory, widget counts); the reorder-spike and pool numbers consolidated into
     `plans/BENCH_BASELINES.md`. Public marketing numbers come from packaged **Test** builds,
     with machine/config context recorded. Publish ONLY measured claims (§6).
  5. README(s) per the family conventions (Discord invite on the first line; status blockquote
     with real counts; honest limitations; markup-first authoring examples); store listing
     authored as a committed template — `templates/fab-listing.template.md` (title, description
     with the AI-content disclosure line (D-29), the BP-only-surface honesty (§6), the MIT/Fab
     dual-license sentence (D-32), engine-version + platform placeholders, media list) — the
     Phase 9 manual checklist renders from it; `discordpost` evergreen pitch; Fab listing art
     list drafted (gallery/feature images at Fab's required sizes — creative sign-off is the
     owner's, §8); **demo/trailer video**: AI writes the storyboard + drives the demo scenes,
     owner records or approves the capture — linked from README, docs landing, `discordpost`,
     and the Fab listing (Fab listings live or die on media; the AI cannot record video, so this
     is a scheduled owner dependency, not an afterthought).
- **Files touched:** `Source/RuiDemo/**`, `Content/**` (demo maps), `docs/**`,
  `ReactiveUIEditor/**` (Inspector tab), root + plugin READMEs, `discordpost`, `plans/`.
- **Verify:** `ReactiveUI.Demos` covers every gallery screen; docs `npm run build && npm run lint`;
  `node ide-extensions/scripts/docs-drift.mjs` (claimed counts vs registries) green; bench table
  committed.
- **Done when:** a newcomer can go install→hello-world→hot-edit in five minutes following only
  the docs (owner or a fresh session validates by literally doing it).
- **Status:** NOT STARTED.

### - [ ] Phase 9 — Release & publishing

- **Objective:** v1.0 out: GitHub release with per-engine zips, Fab listing live, announcement.
- **Inputs:** D-28..D-31; `ue-production.md` §§3, 6; **the canonical v1 ship gate at the top of
  §3** (ROADMAP §3 is its mirror).
- **Steps:**
  1. Ship-gate audit: every item of the §3 gate checklist green, re-verified by running (not by
     reading plans).
  2. Version sweep to 1.0.0 (`bump.mjs`); changelogs all three lanes; README/store text final.
  3. `RunUAT BuildPlugin -StrictIncludes` per engine version (5.6/5.7/5.8) on the packaging
     runner; **per-engine-version example project** built from the demo host (Fab requires one
     per submission, D-29); packaged-fidelity test: install each zip into a FRESH project,
     enable, expect the startup banner + demo map renders (the `field-test-editor` skill's
     store-zip rule).
  4. PR → dev → owner merges → fast-forward `main` → owner presses Publish → GitHub releases +
     extension marketplaces publish.
  5. Fab: owner performs seller onboarding (once; Trader Verification has lead time — §7 Q4) +
     manual listing/upload per version following the `release-process` checklist **including the
     D-29 compliance items (AI disclosure, example projects, copyright lines, naming)**; final
     listing art gets owner sign-off; review latency is days→weeks — GitHub is already live
     meanwhile.
  6. Discord entry ≤2000 chars posted to the family server's #unreal channel (owner posts it —
     AI prepares only); docs site deployed; **GitHub repo metadata pass** (About/description,
     topics — `unreal-engine`, `slate`, `umg`, `react`, `ui`, `hooks` —, social-preview image,
     links to docs + Discord; AI drafts, owner applies — admin-only); ROADMAP flipped to shipped.
- **Files touched:** `.github/workflows/publish.yml` refinements, all version manifests,
  `CHANGELOG.md` ×2 + `changelog.json` + `plans/DISCORD_CHANGELOG.md`, READMEs/store text,
  `plans/ROADMAP.md`.
- **Verify:** publish.yml green all legs; fresh-project fidelity test per engine version; Fab
  submission confirmed (owner).
- **Done when:** a stranger can install from Fab or GitHub and reach hello-world.
- **Status:** NOT STARTED.

---

## §4 — Testing & verification

**Suite hierarchy** (Automation framework + Specs; names are prefix-filterable):

| Suite | Ports / covers | Phase |
|---|---|---|
| `ReactiveUI.Boot` | module startup, root mount smoke (the "_enter_tree analogue" — unit suites don't run `StartupModule`) | 1 |
| `ReactiveUI.Core` | `core_test.gd` (~36 funcs: hooks, effects, bailout, context, keyed, suspense, portals, error latch) | 1 |
| `ReactiveUI.Update` | `update_test.gd` (diff correctness) | 1 |
| `ReactiveUI.Style` | `style_test.gd` + removal-reset + no-reconstruct-on-style | 2 |
| `ReactiveUI.Widgets.*` | per-widget adapter tests | 2+ |
| `ReactiveUI.Demos` | mounts every demo headlessly — the real "generated code runs" check | 2+ |
| `ReactiveUI.Uetkx` | compiler/codegen (`guitkx_test.gd` analogue) | 3 |
| `ReactiveUI.Contract` | scanner/formatter corpora (C++ side) + cross-repo grammar cases | 3 |
| `ReactiveUI.Hmr` | `hmr_test.gd` analogue: targeted/global refresh, signature reset, isolation | 4 |
| `ReactiveUI.Umg` / `.Mvvm` / `.CommonUI` | interop + teardown-order + world-space | 6 |
| `ReactiveUI.Bench` | NOT pass/fail; committed baseline tables | 1+ |

(No router suites: the router subsystem is post-v1 by decision — see the ship gate's OUT list.
When it lands, `router_match_test.gd` + `router_spine_test.gd` port as `ReactiveUI.Router.*`.)

**The canonical local/CI run (this order — order is load-bearing):**

```bat
:: 0. Build (compile step — Godot never had one; UE does)
<Engine>\Engine\Build\BatchFiles\Build.bat ReactiveUIUnrealDemoEditor Win64 Development -Project=<abs>\ReactiveUIUnrealDemo.uproject -WaitMutex

:: 1. Markup compile sweep + drift gate (mirror of tests/guitkx_build.gd)
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject -run=RUICompile -check

:: 2. Suites (headless; ALWAYS redirect output to a file — console buffering lies)
<Engine>\UnrealEditor-Cmd.exe <abs>\ReactiveUIUnrealDemo.uproject ^
  -ExecCmds="Automation RunTests ReactiveUI; Quit" ^
  -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput ^
  -ReportExportPath=<scratch>\report
:: pass/fail: parse <scratch>\report\index.json (exit code alone is unreliable)
```

Single suite: same command with `Automation RunTests ReactiveUI.Core;` (prefix match).
Extensions: `cd ide-extensions/lsp-server && npm ci && npm run build && node --test out/test/*.test.js && node scripts/smoke.js`.
Docs: `cd docs && npm ci && npm run build && npm run lint`.

**Rules (house law, enforced):** every suite prints per-section markers so hangs name their
culprit; benches are separate from tests and their numbers live in **`plans/BENCH_BASELINES.md`**
(every row: machine spec, engine version, build config, global-invalidation state — cross-machine
comparisons are invalid; marketing numbers only from packaged Test builds); perf changes require
before/after numbers in the PR against the permanent harness; contract diffs during
"behavior-preserving" changes are by definition bugs in the change; the byte-identical changelog
mirror is enforced engine-free by `scripts/verify-mirror.mjs` in the always-on CI job; the
docs-drift tripwire (`scripts/docs-drift.mjs`) greps claimed counts against registries.

**What CANNOT be verified headlessly (owner playtest checkpoints):** actual visuals/layout
correctness in PIE (Phases 2, 6, 8), the mid-PIE HMR feel and <1 s latency (Phase 4), gamepad
focus walking through mixed CommonUI/Rui trees (Phase 6), screen-reader pass (Phase 7),
fresh-project install fidelity on a machine without the repo (Phase 9), and anything requiring a
GPU. Each phase's Done-when says which of these it needs.

---

## §5 — Explicit deviations (decisions, not gaps)

Family semantics preserved on purpose (do not "fix"):
- Removed **plain** props don't reset to defaults; style/event/ref/draw props DO reset.
- `UseTransition` synchronous no-op; `UseDeferredValue` via DeferredLane (upgraded mechanism,
  same observable semantics).
- Error boundaries are structural (+ the D-10 latch — an addition, not a semantic change).
- `provideContext` is a hook-style call, not a wrapper element.
- Suspense is a function-component polyfill (delegate/poll readiness; optional Collapsed
  keep-mounted mode as an upgrade — if adopted, document as a divergence-from-siblings).

Deliberate divergences FROM the Godot port (toward React or C++ reality — document in
"Differences" docs):
- **Refs**: React lifecycle (attach/detach + cleanup callbacks), NOT call-every-commit.
- **`UseSignal`**: subscribe-in-effect (`useSyncExternalStore` discipline), NOT during render.
- **Deps equality**: value-equality for value types, identity for shared refs (between Godot's
  deep-== and React's Object.is; rationale G-09 recorded).
- **Component identity**: registry FName, not Callable/pointer equality (Live Coding reality).
- **Child reorder**: React two-pass + lastPlacedIndex, not commit-time enforce-order (Slate slot
  costs), with per-adapter fallback.
- **Generated code is committed**, not gitignored (cold-clone/Fab reality).
- Formatter default `tab` (Epic convention), not the hard tab *requirement* GDScript imposed.

---

## §6 — Risks & the DO-NOT-CLAIM list

Risks (mitigations in parentheses):
- **UE 5.7+ fast-path invalidation crashes under heavy child churn** (`FSlateInvalidationRoot`
  reports) — the fragile spot for a reconciler (reorder benchmark spike Phase 2; minimal-move
  diff D-08.2; invalidation-boundary wrapping as measured fallback).
- **UE6 transition** (EA end-2027) inside our support window; Slate carry-over expected but not
  guaranteed (engine churn quarantined in host boundary + `RuiEngineCompat.h`; support-window
  policy is "three latest").
- **SlateIM / Viewmodel+Verse data-binding "good enough" erosion** over 12–24 months (our moat is
  the full DX loop: markup+HMR+LSP, which no first-party effort has signaled).
- **Scope vs team-size** (critique gap 20): phases are demand-gated past the v1 cut-line; the
  ship gate is fixed; everything else can wait.
- **Fab economics are modest** — plan as reputation/funnel, not income (88/12, ~$1.2K/yr mean,
  no subscriptions, weeks-long reviews, support burden precedents).
- **The Blueprint-only segment — Fab's largest demographic — is only partially addressable**
  (critique gap 12b): their surface is markup-only components + `set()` local state + `@logic`
  Blueprint classes (verified to resolve in Phase 4); anything needing hand-written C++ handlers
  excludes them. Positioning and store copy must size this honestly (Phase 8 step 5 gates on it).

**DO-NOT-CLAIM (critique-downgraded statements — never in docs/marketing until measured/fixed):**
1. "Detach/AttachWidget reorder minimizes ChildOrder invalidation" — FALSE as stated; it saves
   slot construction only. Claim requires the Phase 2 benchmark.
2. "76× faster than Immediate MVVM bindings" — benchmark misapplication. Defensible: "we batch
   to one pass per frame like Delayed mode." Anything stronger needs our own Phase 8 numbers.
3. "`FRuiContext&` makes out-of-render hooks a compile error" — overclaim; it narrows the misuse
   class. StrictMode is the runtime net.
4. "PreBuildSteps codegen is solved/proven" — it is NOT the design (D-19 exists because of this).
5. "<1 s state-preserving hot reload" unqualified — true only within the hook-signature rule and
   the interpretable set; docs state the honest limitation set verbatim (D-20).
6. Unmeasured performance superiority over UMG/MVVM of any kind (Phase 8 gates all perf copy).

---

## §7 — Open questions for the repo owner — ALL ANSWERED 2026-07-10

Kept as the decision record; new questions get appended below as they arise.

1. **Naming — ANSWERED: "ReactiveUI-Unreal" stays** (repo name already is; plugin FriendlyName
   "ReactiveUI for Unreal"). The family has shipped twice under this name (Unity, Godot) without
   incident. Residual caution: Fab review screens trademark-adjacent LISTING titles — Phase 8
   drafts 2–3 title variants for the owner to pick at listing time.
2. **CI budget & Epic org — ANSWERED: no engine CI for now.** Local-first (the plan's unarmed
   default); the free engine-less CI legs still run. Revisit exactly once: at release time, when
   multi-engine-version compile checks would otherwise mean juggling three local installs.
3. **Engine on the owner's machine — ANSWERED: none installed today.** Owner installs **UE 5.6
   first** (the floor; author-low), adds **5.8** near packaging/release. VS2022 + C++ workload
   already present (from the analyzer work); Phase 0 verifies the MSVC toolchain and records all
   paths as environment facts in the skills.
4. **Fab seller account — ANSWERED: after the core milestone** — when the reconciler/hooks are
   proven working and performant (post Phases 1–2) and what remains is tooling. Trader
   Verification lead time makes that timing comfortable.
5. **Demo host project — ANSWERED: `ReactiveUIUnrealDemo`, at the repo root** (the repo IS the
   host project — the family's single-repo pattern, and the Fab example-project requirement is
   satisfied from it).
6. **Byte-compat — ANSWERED: HARD.** The cross-repo corpus machinery (D-22) stays as specified.
7. **Sequencing — RESOLVED:** the Godot 0.8.7 release is verified shipped (PR #69 merged, master
   fast-forwarded, all publish tags present across all legs). This repo is the next active front;
   Phase 0 starts on the owner's word.

(Everything else that was an open question in the research is now a §1 decision or a phase step.)

---

## §8 — AI execution: autonomy, delegation, production lines

### Autonomy boundary (least user intervention, explicitly)

**Do WITHOUT asking:** read/search repo + engine source; branch off `dev` in the work tree; edit
work-tree files; build; run every headless test/bench/commandlet; scratch/probe files (removed
before commit); web research; update plans/ROADMAP/PENDING_CHANGELOG/skills/templates; stage
release prep; spawn lesser-model subtasks per the matrix; open PRs when the campaign's flow calls
for it.

**Only when established by the task or an invoked skill:** `git commit` (**never auto-commit — a
standing user rule**; a campaign's per-milestone commits become established once the owner
approves the campaign), `git push` to the feature branch.

**NEVER (user-only):** merge PRs; trigger publish.yml or any release workflow; push to `dev`/
`main` directly except the sanctioned post-merge fast-forward; edit the live tree while the
owner's editor is open, or kill their editor (UE locks DLLs anyway); weaken/skip/disable any
lint/test/CI gate; add Co-Authored-By; post to Discord/Fab (prepare text only); force-push shared
history.

**STOP-and-escalate conditions (baked into every skill):** a Verify command fails twice on the
same theory (get more instrumentation, never re-try the same theory); a contract-corpus diff
appears during a behavior-preserving change; a §1 decision proves wrong mid-phase (banner + ask,
never silently deviate); anything public-facing before it ships.

### Delegation matrix (the "lesser model" production lines)

Tiers: **haiku-class** = mechanical, template-in/template-out, zero design decisions;
**sonnet-class** = bounded implementation inside fixed rails; **opus-class** = architecture,
root-causing, anything touching the reconciler core or a CI gate. **Tier is chosen by blast
radius, not size** — anything that can silently corrupt shared machinery escalates. The
orchestrator hands a lesser-model task exactly three things: the SKILL.md, the template/example
files, and the evidence packet — never whole-repo context. Off-checklist ⇒ STOP and escalate.

| Recurring task | Tier | Inputs handed over | Verification gate |
|---|---|---|---|
| Slate widget wrapper (after pattern established, Phase 2.3+) | sonnet (research station) / haiku (stations 2–6) | `component-pipeline` skill; `S<Name>.h` path; templates; prop-map schema; traps list (SLATE_ARGUMENT=construct-only vs SLATE_ATTRIBUTE=bindable; slot props live on slots; events return FReply) | build + `ReactiveUI.Widgets.<Name>` + contract suite; opus review only if a new host-config *mechanism* was needed |
| Style key addition | haiku | style registry + one worked key; reset-behavior rule; contract-case template | `ReactiveUI.Style` + apply→remove→default case + changelog line |
| Hook/element doc page | haiku | `docs-sync` skill; source + doc comment; an existing page as template | docs build+lint; docs-drift tripwire; sonnet skim for semantic claims |
| Changelog entry | haiku | `release-process` §lanes; the diff/PR title; `--message-file` rule | `changelog.mjs verify` + mirror tripwire |
| Contract-corpus case | haiku | `grammar-contract` skill; corpus schema; 3 examples; the behavior sentence to pin | corpus green BOTH sides; cross-repo sync confirmed |
| Widget-prop web-research sweep | haiku execute / sonnet synthesize | current prop-map; target `S*` class list; **the header outranks the docs** rule; output = diff proposals | sonnet verifies each prop against the actual header; contract suite catches wrong types |
| README/store refresh | haiku draft | `docs-sync`; delta list from ROADMAP+changelogs; counts tripwire | tripwire; opus pass for claims accuracy vs §6; owner approves public text |
| Test boilerplate | haiku | test templates; naming convention; per-section-marker rule | new test demonstrably red→green once |
| Plan status bookkeeping | haiku | `plan-progress` skill; evidence packet (counts, shas, numbers) | the skill's own checklist; ROADMAP diff reviewed at PR |
| Reconciler/host core, perf, gates, build integration | **opus, always** | full plan + measured baseline + bench harness | measured before/after in PR; full suite; never delegated |

### Plan-document rules (for any future plan in this repo)

Use this file's format: machine-parsable header banner; §-order (one-fact → decisions → map →
phases → testing → deviations → risks → open questions); the mandatory per-phase block; stable
IDs; status vocabulary (`NOT STARTED / IN PROGRESS / BLOCKED(x) / COMPLETE date / SUPERSEDED(doc)`
at plan level; `SHIPPED/SKIPPED(reason)/DEFERRED(where)` per item; `~~struck~~ — SUPERSEDED date`
for corrected facts). Finished plans `git mv` to `plans/archive/` + row in `archive/README.md` +
ROADMAP update — all via `plan-progress`.

---

## §9 — Skills to author in Phase 0 (`.claude/skills/<name>/SKILL.md`)

Each ports or extends a proven Godot-repo skill; each carries a "Scar tissue (why these steps
exist)" section; each encodes its STOP conditions. Full specs in
`research/round2-implementation/ai-delegation.md` §a — summary:

1. **`dev-process`** (port) — the loop (`research → develop → test → bughunt → fix → commit →
   repeat`), the laws (root cause only; production-grade only; never weaken gates; 1-branch-1-PR
   feat→dev; main is fast-forward release-only; no Co-Authored-By; user merges; **never commit
   engine-derived source or Epic-owned content to the MIT repo** — D-32d), PLUS the UE
   deltas: the compile-step gate ladder (compiles Development Editor + packaged → suites green →
   **boot check** `Automation RunTests ReactiveUI.Boot` because unit suites don't run
   `StartupModule` → demo renders), the changelog/version tables (D-30/D-31).
2. **`release-process`** (port) — the runbook, in order: **drain `plans/PENDING_CHANGELOG.md`
   into Lanes A/B/C, then empty it**; what-bumps diff check; patch-by-default incl. perf-only;
   Lane A/B/C commands; **sibling-corpus drift check** (hash-compare the grammar corpus against
   the Unity + Godot repos — D-22e); BuildPlugin per engine version (CI, or the
   `scripts/package-plugin.ps1` local rail when engine CI is unarmed — token resolution via
   `publisher-secrets.json`); packaged-fidelity fresh-project test; commit-message shape;
   PR→dev→ff-main; user presses Publish; **manual Fab checklist rendered from
   `templates/fab-listing.template.md`** incl. the D-29 compliance items (no API); Discord
   ≤2000 chars.
3. **`plan-progress`** ⭐ (the owner's "mark phase complete" ask) — deterministic text-edit
   procedure: flip checkbox + append evidence (date, test counts, shas, numbers) → strike
   superseded facts in place → prepend STATUS banner → update ROADMAP row → stage bullet in
   `plans/PENDING_CHANGELOG.md` → on last phase: rewrite goal blockquote COMPLETE, `git mv` to
   archive, archive-README row, ROADMAP move to Shipped. **Never commits.** Haiku-executable.
4. **`docs-sync`** ⭐ (the owner's "feature lands → update docs" ask) — change-type → artifact
   checklist table (new hook → 4 places; new widget → 3 places; grammar change → syntax docs +
   LSP schema + **cross-repo byte-compat note**; behavior change → Notes & limitations + store
   text; perf change → measured numbers only). Ends by running the docs-drift tripwire
   (`node ide-extensions/scripts/docs-drift.mjs`, created Phase 0 step 3). Haiku-executable.
5. **`component-pipeline`** ⭐ — the widget production line, 6 stations (research the real
   `S<Name>.h` → wrapper from template → style keys → contract case → test from template →
   docs-sync), the worked SButton example, the traps list, exit gate = build + widget suite +
   contract green.
6. **`test-run`** ⭐ — the §4 command block verbatim + environment facts (engine paths per §7 Q3,
   ALWAYS redirect output to a file, index.json parsing, prefix-match filters, single-test
   invocation, the RUICompile pre-step).
7. **`field-test-editor`** (port) — live tree vs work tree; never edit live tree while the
   owner's editor is open; Live Coding patches `.cpp` bodies only — header/UPROPERTY/module
   changes ⇒ ask owner to close editor + rebuild; ask for `Saved/Logs/<Project>.log` + Message
   Log; packaged-fidelity test into a fresh project; "never re-try the same theory twice".
8. **`new-component`** (port) — `.uetkx` authoring guide: grammar (byte-compatible, pointer to
   corpus), the declaration kinds + companion-file layout convention (one `component` per file,
   sub-components as siblings, `hook`/`module` files for shared hooks — D-03), hooks list, events,
   the `logic={UClass}` attribute + setter calls in event exprs (D-20), style keys, directives,
   keyed lists, what the compiler generates (committed `.inl`), done-checklist (compiles, no
   `UETKX####`, renders, doc comment).
9. **`grammar-contract`** ⭐ — every lexer/parser/formatter change lands with a corpus case; the
   corpus runs on BOTH in-repo implementations AND syncs across **all three sibling repos**
   (Unity `.uitkx` — the origin —, Godot `.guitkx`, this repo); divergence = bug in whichever
   side changed last; the sync procedure + the `.pending` divergence convention + the
   release-time sibling drift check (D-22e); also covers syncing the vendored lsp-server's
   family-shared files (D-30).

---

## §10 — Provenance: what this plan rests on

- **Two research rounds, committed in-repo:** `research/round1-market-and-design/` (12 docs:
  Slate ground truth, MVVM, competition, demand, Fab economics, Epic risk, core architecture,
  interop, DX designs, adversarial CRITIQUE) and `research/round2-implementation/` (6 digests:
  Godot-repo ecosystem inventory, corpus distillation, React-reconciler gap study, UE production
  research, `.uetkx` toolchain design, AI-delegation design). Where this plan compresses, those
  files hold the full detail — they are part of the plan.
- **Two adversarial audit rounds over THIS document** (both same-day, all findings applied):
  a three-critic panel (completeness vs corpus / internal consistency / owner requirements) and
  a second deep+wide pass — ledger in `research/round2-implementation/second-pass-audit.md`
  (41 applied findings, incl. the Interp `TargetConfigurationDenyList` mechanism, LF pinning for
  the byte-drift gates, the full grammar declaration inventory, D-32 legal, and the
  platform/server policies).
- **Verified by reading code:** everything cited from the Godot repo (reconciler internals incl.
  the double-buffering correction, toolchain machinery, CI, skills, changelog system).
- **Verified by primary sources (web, July 2026):** UE version timeline (5.8 final UE5, UE6 EA
  end-2027), Fab rules/economics, BuildPlugin/StrictIncludes behavior, container/CI reality,
  Live Coding limits, FieldNotify engine-level API, React 18/19 internals (lazy context
  propagation, prepareUpdate removal, two-pass reconciliation).
- **Assumed, to be verified in-phase (each is assigned):** optional-plugin module gating (Ph 6),
  Blueprint `@logic` handler resolution (Ph 4), reorder-strategy costs (Ph 2 spike), clangd flag
  inference on virtual docs (Ph 5), accessibility inheritance (Ph 7), `ghcr` container tag
  availability for our matrix (Ph 0, armed legs).
- **Nothing in this plan asserts a performance number we have not measured.** (§6.)
