# ReactiveUI-Godot core analysis — grounding for an Unreal/Slate C++ port

## 1. Exact host contract (from `addons/reactive_ui/core/host_config.gd` + Unity `FiberHostConfig.cs`)

A new engine host must implement:

**Node lifecycle**
- `create_node(type: String) -> Node` — instantiate by type name (`ClassDB.instantiate`); unknown type warns and falls back to a plain container. Unity instead routes through an `ElementRegistry` of per-element adapters (`adapter.Create()`) — the right model for Slate, which has no runtime class registry.
- Destruction: reconciler calls `queue_free()` (deferred free) on the whole subtree root, OR pools childless leaves (see §5).
- Pool support: `reset_for_pool(node, props) -> bool` — detach node, stash last-applied props (meta `__rui_pool_old`); returns false for un-poolable nodes (item-model controls). `reset_removed_plain(node, old, new)` resets plain props absent in new props to *cached class defaults*.

**Child topology**
- `resolve_child_host(node) -> Node` — content-container indirection (meta `rui_content`; Unity: `adapter.ResolveChildHost` / `contentContainer`). Called on every append/insert/reorder.
- Placement: `parent.add_child(node)` under the *nearest ancestor host node* (fibers for components/fragments own no node; the host tree is the flattened projection). Reorder: post-commit `move_child(node, i)` against the collected in-order host children (not insertBefore); Unity's host config exposes `AppendChild / InsertBefore / RemoveChild / GetParent / ClearChildren`.
- `warn_capacity(node, count)` — dev warning for limited-child containers.

**`apply_props(node, old_props, new_props)`** — always a *diff* (fresh mount passes `old = {}`). Six ordered steps:
1. Disconnect events present in old, absent in new (gated on the node ever having events).
2. For each new prop: skip RESERVED (`key, ref, style, classes, children, items, draw_fn, redraw_key, reuse_by_slot`); events reconnect only on callback-identity change; plain props set only when value changed via `node.set(key, val)` (reflection).
3. `ref` — invoked **every commit** with the node; either `Callable(node)` or a `{current:}` box. Reconciler nulls refs on deletion/unmount.
4. `style` + `classes` — merge class styles left-to-right (stylesheet lookup) then overlay inline style; hand old/new effective style to `RUIStyle.apply` (itself a diff: resets removed keys, applies changed; StyleBox rebuilt only when box keys differ).
5. `items` — declarative item-model adapters via an extensible registry (`match_fn`/`apply_fn`); built-ins (ItemList/Tree/TabBar/OptionButton/PopupMenu) rebuild items but preserve runtime state (selection / current tab / expansion) by item **identity** ("tracker" pattern), multiset-counted for duplicate ids.
6. `draw_fn`/`redraw_key` — register-once trampoline connected to the `draw` signal reads the *latest* callback from node meta, so a fresh closure per render never re-subscribes; repaint only when callback identity or `redraw_key` changes.

**Event binding model** — purely syntactic classification (`on_` prefix or `on[A-Z]…`), then resolution: `on_<signal>` binds verbatim (escape hatch to any signal); alias table makes `onChange` polymorphic (first signal the node actually has among `item_selected/value_changed/text_changed/tab_changed/toggled`); otherwise camelCase→snake_case. Connections recorded in node meta with the *resolved* signal so disconnect never re-resolves.

**Text** — no text-node primitive; raw String children auto-wrap to `Label` vnodes. Controlled-input fix: setting `text` on LineEdit/TextEdit skips when equal and otherwise preserves caret line/column — mandatory for state-driven text fields.

**Documented semantic**: a *removed plain* prop is NOT reset to default between renders (events/style/refs/draw ARE cleaned up) — React-style "pass every prop every render". Preserve this contract.

## 2. Reconciler architecture (`reconciler.gd`, `fiber.gd`)

**Phases**: render — `begin_work` descending (reconcile children, run components with bailout), `complete_work` ascending (create host node + full apply on mount, or flag `EFFECT_UPDATE` when props differ; append to a post-order singly-linked effect list). Commit — deletions → effect list (`PLACEMENT / UPDATE / PORTAL_RETARGET / LAYOUT`; `PASSIVE` collected) → enforce child order for the `_reorder_set` → swap current↔wip → two-pass passive flush (all cleanups then all setups) → replay updates deferred during commit.

**Fiber fields**: tree (`parent/child/sibling/index`), identity (`tag/key/type/component`), props (`props` committed / `pending_props` / `input_children`), host `node`, `portal_target`, error-boundary fields, `alternate` (double buffer), `effect_tag` bitmask + `next_effect`, `deletions`, `matched_pass` (mark-and-sweep flag), `provided_context`/`reads_context`, `has_pending_update`/`subtree_has_updates`, shared `state`, and a cached apply plan (`apply_size / apply_special / apply_plain`).

**Double buffering**: on type match, reuse `old_fiber.alternate` as the WIP — zero fiber allocations at stable positions; render-scoped fields reset each pass, committed baseline (node/state/props/context/plan) carried over. No per-frame tree sever; only deleted subtrees are cycle-severed and released.

**Keyed diff — three tiers**: (a) fast-leaf-list: same count/keys/order, all childless host leaves on both sides → reuse fibers *in place*, diff props per row, effect-list only changed rows (skips `_reconcile`/`_perform_unit`/`_complete_work` entirely). With `reuse_by_slot` on the parent, key matching is skipped: slot *i*'s node is reused for whatever leaf lands there (contract: stateless childless leaves, same type per slot). (b) `_keys_stable` positional scan → in-place keyed reconcile, no map. (c) full keyed: persistent per-reconciler `_key_map` (cleared per call) + per-fiber `matched_pass` mark-and-sweep; unkeyed children get control-char-namespaced positional keys. Unkeyed lists use the positional path. Any placement/move/deletion sets a `structural` flag → mark nearest host ancestor for one O(n) reorder at commit; pure-update frames skip reorder.

**Bailout** (function components): skip render when no pending update ∧ context deps unchanged ∧ props equal (identity `is_same` fast-path → optional `__memo_eq` custom comparer → deep `==`) ∧ children vnode-list identical; cached `state.last_output` is reused. Host UPDATE similarly: identity check then deep compare.

**Coalescing**: hook setters call `schedule_update_on_fiber` (marks fiber dirty + ancestor `subtree_has_updates`); one `call_deferred` tick per frame. Mid-commit updates queue; mid-render updates set `_restart` (rebuild from root, clearing the effect list — the invariant preventing stale Placement effects); >25 restarts aborts. Optional time-slicing parks work at a frame-budget boundary and resumes on `process_frame`.

**Engine leaks in the "agnostic" core**: `Node` types in reconciler signatures, `is_instance_valid`, `queue_free`, `call_deferred`/`process_frame` scheduling, `Time.get_ticks_msec`, pool keyed by `get_class`, `__rui_pool_old` meta handled in `_complete_work`, and String→Label auto-wrap. A C++ port should push all of these behind the host interface (Unity's `FiberHostConfig` + adapter registry + `ITypedElementAdapter` typed-props pipeline is the cleaner seam to copy).

## 3. Hooks storage model (`hooks.gd`, `component_state.gd`)

`RUIComponentState` per function fiber, **shared by reference across alternates** (survives the buffer swap); its `fiber` back-pointer is re-pointed each render/commit so setters always schedule on the live fiber. Storage: positional `hooks` array of dictionary slots with a `hook_index` cursor reset each render; **separate cursors/arrays** for passive effects and layout effects (`{factory, deps, last_deps, cleanup}`; factory/deps refreshed every render); context reads recorded out-of-band (`context_deps`) so `useContext` never consumes a slot. A static `_cur` pointer is set by `Hooks._begin/_end` around each render. Setters/dispatch are closures over `(state, slot_index)`, minted once → stable identity; late calls after teardown are guarded by index bounds. Change detection: `_ref_equal` (Object.is semantics — identity for collections) for state/signals; `_equal` (deep `==`) for deps. Teardown explicitly runs cleanups, calls `unsub` slots, and severs closure cycles.

## 4. Hardest GDScript/C# → C++ mappings

1. **Closures with identity** — hook setters, event handlers, and `draw_fn` all rely on stable *comparable* callable identity. `std::function` has no equality. Recommend a first-class `FRUICallback` = `TSharedRef` to a callable object; identity = pointer equality; hold `TWeakPtr` to component state inside setters (replaces the index-bounds guard).
2. **Variant prop bags** — GDScript `Dictionary` with deep `==` underpins props diffing/bailout. Recommend Unity's **typed-props pipeline** as primary (per-element `BaseProps` structs, `ApplyTypedDiff/ApplyTypedFull`, memberwise `operator==` — no boxing/hashing), keeping a `TMap<FName, FRUIValue>` generic path only for markup tooling.
3. **Reflection prop setting** — `node.set(key, val)` + `ClassDB` defaults have no Slate equivalent (SWidget attributes aren't reflected). Replace with per-widget adapters mapping prop keys to explicit setters (the Unity `ElementRegistry` model); adapters must also own pool-reset defaults. UMG's UObject reflection is an alternative if targeting UWidget.
4. **Ownership/cycles** — RefCounted + manual cycle severing maps naturally to an arena/free-list of reconciler-owned fibers (explicit release already exists); host widgets as `TSharedRef<SWidget>` strong-held by the tree; pooled nodes = detached shared refs; never let fiber↔state↔closure form shared_ptr cycles.
5. **Runtime signal discovery** — `has_signal`/verbatim `on_<signal>` binding can't exist in Slate (typed per-widget delegates). The polymorphic `onChange` alias table becomes compile-time per-adapter event maps; drop the generic escape hatch or expose it per adapter.

## 5. Perf lessons to inherit day one (all measured/annotated in code)

- **Double-buffered fibers**: zero alloc at stable positions; no per-frame sever [perf #1].
- **Fast-leaf-list path + per-row bailout**: the single biggest win for big dynamic lists.
- **`reuse_by_slot`**: key-churning, count/type-stable lists become pure prop updates — no create/free.
- **Node pool, *cheap-path* design**: eager reset-to-defaults costs as much as instantiate; instead detach + stash old props and reuse via a *diff* apply. Cap per class (256); exclude stateful item-model controls; drain on unmount.
- **Per-fiber apply plan (inline cache)**: classify props once (`apply_special` sticky); plain elements get a lean diff+write loop, no per-frame reclassification.
- **Identity-first comparisons**: `is_same` before deep compare (memoized props → O(1) bailout).
- **Allocation hygiene**: iterate map keys directly, shared immutable `_EMPTY` children, persistent `_key_map`, per-fiber `matched_pass` flag instead of a per-frame dict, null-default style-channel gets (GO-06: eliminated 12 empty-dict allocs per styled node per frame), reorder pass only on structural change [perf #2], `has_meta` gates before event/style/draw machinery [perf #4].
- **Coalesce to one render per frame**; restart-from-root on mid-render updates with a 25-restart guard.

## KEY FACTS
- The host seam is exactly two files: host_config.gd (RUIHost) + style.gd (RUIStyle); the reconciler touches engine nodes only through create_node, resolve_child_host, apply_props, reset_for_pool/reset_removed_plain, warn_capacity, plus add_child/move_child/queue_free calls that leak into reconciler.gd.
- Reserved props (never set as node properties): key, ref, style, classes, children, items, draw_fn, redraw_key, reuse_by_slot (RESERVED dict, host_config.gd:25).
- Removed plain props are deliberately NOT reset to defaults between renders (audit #23); events, refs, style, and draw_fn ARE cleaned up on removal — a contract a port must preserve.
- Event props are classified syntactically (on_ prefix or onCamel) and resolved to signals via a polymorphic alias table (onChange → first of item_selected/value_changed/text_changed/tab_changed/toggled the node has) with a camelCase→snake_case fallback and an on_<signal> verbatim escape hatch; resolved signal is cached in node meta for disconnect.
- The reconciler is a synchronous fiber loop: begin_work descending / complete_work ascending building a post-order effect list, then commit = deletions → placement/update/portal/layout effects → child-order enforcement via move_child → current↔wip swap → two-pass passive effects (all cleanups then all setups) → replay deferred updates.
- Fibers are double-buffered: a matching old fiber's .alternate is reused as the WIP, so stable tree positions allocate zero fibers per frame; only deleted subtrees are cycle-severed and released.
- Keyed diff has three tiers: fast-leaf-list in-place path (stable childless host leaves; reuse_by_slot variant ignores keys), a keys-stable positional path with no map, and a full keyed path using a persistent reused _key_map plus a per-fiber matched_pass mark-and-sweep flag.
- Component bailout condition: no pending update AND context deps unchanged AND props equal (is_same identity fast path → optional __memo_eq → deep ==) AND children vnode-list identical; bailed components reuse state.last_output without re-running.
- Hook state lives in RUIComponentState, shared BY REFERENCE across fiber alternates: a positional array of slot dictionaries with separate cursors for state hooks, passive effects, and layout effects; useContext records deps out-of-band and consumes no slot; setters are stable closures over (state, index).
- State/signal change detection uses Object.is semantics (identity for Array/Dictionary/Object, value for scalars); effect deps use deep value equality — an intentional, documented divergence from React.
- GO-05 node pool: pooled leaves are detached with last props stashed (__rui_pool_old) and reused via a diff apply, because measured eager reset-to-defaults cost about as much as the instantiate it avoided; pool is capped at 256 per class, excludes item-model controls, and drains on unmount.
- Per-fiber apply-plan inline cache (apply_size/apply_special/apply_plain) classifies props once; apply_special is sticky, and plain elements take a lean diff+write loop bypassing all event/ref/style/item machinery.
- Updates coalesce to one deferred render per frame; a setState mid-render restarts the pass from the root (clearing the effect list) with a 25-restart abort guard; optional time-slicing parks work on process_frame at a frame-budget boundary.
- The Unity sibling (FiberHostConfig.cs) is the cleaner seam for a C++ port: CreateElement/ApplyProperties/AppendChild/InsertBefore/RemoveChild/ClearChildren delegating to an ElementRegistry of per-element adapters, plus a typed-props pipeline (ITypedElementAdapter.ApplyTypedDiff/ApplyTypedFull over BaseProps structs) that avoids dictionary allocation entirely.
- Text handling: no text-node primitive — raw String children auto-wrap to Label vnodes; controlled text inputs skip the write when equal and otherwise preserve caret position (LineEdit column, TextEdit line+column).

## SOURCES
file:///c:/Yanivs/GameDev/ReactiveUI/ReactiveUI-Gadot/addons/reactive_ui/core/host_config.gd
file:///c:/Yanivs/GameDev/ReactiveUI/ReactiveUI-Gadot/addons/reactive_ui/core/reconciler.gd
file:///c:/Yanivs/GameDev/ReactiveUI/ReactiveUI-Gadot/addons/reactive_ui/core/fiber.gd
file:///c:/Yanivs/GameDev/ReactiveUI/ReactiveUI-Gadot/addons/reactive_ui/core/hooks.gd
file:///c:/Yanivs/GameDev/ReactiveUI/ReactiveUI-Gadot/addons/reactive_ui/core/component_state.gd
file:///c:/Yanivs/GameDev/ReactiveUI/ReactiveUI-Gadot/addons/reactive_ui/core/v.gd
file:///c:/Yanivs/GameDev/ReactiveUI/ReactiveUI-Gadot/addons/reactive_ui/core/style.gd
file:///c:/Yanivs/GameDev/ReactiveUI/ReactiveUI-Gadot/README.md
file:///C:/Yanivs/GameDev/UnityComponents/Assets/ReactiveUIToolKit/Shared/Core/Fiber/FiberHostConfig.cs
file:///C:/Yanivs/GameDev/UnityComponents/Assets/ReactiveUIToolKit/Shared/Elements/BaseElementAdapter.cs
file:///C:/Yanivs/GameDev/UnityComponents/Assets/ReactiveUIToolKit/Shared/Core/Fiber/FiberNode.cs