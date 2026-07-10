# ReactiveUI-Unreal — Core Architecture

## 1. Plugin & module layout

`ReactiveUI.uplugin`, four modules:

| Module | Type | Deps | Contents |
|---|---|---|---|
| `ReactiveUICore` | Runtime | `Core` only — **no UObject, no CoreUObject** | VNode, fibers, reconciler, hooks, keys, callbacks, `IRUIHostConfig` (abstract host seam), signal store, context, router |
| `ReactiveUISlate` | Runtime | Core, `SlateCore`, `Slate`, `InputCore` | `FRUISlateHost : IRUIHostConfig`, element adapter registry, `SReactiveRoot`, widget pool, custom leaves (`SRUICanvas`) |
| `ReactiveUIUMG` | Runtime | above + `UMG`, `CoreUObject`, `Engine` | `UReactiveWidget : UWidget` embedding shim — the *only* UObject code |
| `ReactiveUIEditor` | Editor | above + editor modules | Nomad-tab mounting, debug/diagnostics UI |

This mirrors the Godot seam exactly: the reconciler touches the engine only through `IRUIHostConfig` (create/apply/insert/remove/reorder/pool-reset over an opaque `FRUIHostHandle`), so Core compiles in a standalone `SlateViewer`-style program target used as the CI test harness.

## 2. VNode & props

**Decision: strongly-typed per-element props structs with a set-bitmask; type-erased only at the tree level.** This is the Unity `ITypedElementAdapter` pipeline, which the port analysis already identifies as the cleaner seam.

```cpp
struct FRUIButtonProps : FRUIPropsBase {
    FText  Label;        // bit 0
    bool   bEnabled;     // bit 1
    FRUICallback OnClicked; // bit 2 (event class)
    uint64 SetBits = 0;  // which fields the render actually specified
};
```

- **VNode** (`FRUINode`) is a small transient value: `{ ETag Tag; FRUIElementTypeId Type; FRUIKey Key; TUniquePtr<FRUIPropsBase> Props; TArray<FRUINode> Children; void* ComponentId; }`. Heterogeneous children are uniform because props are erased behind `FRUIPropsBase`; only the element's adapter downcasts.
- **Diff** = `Adapter->ApplyDiff(Widget, Old, New)`: downcast both, memberwise compare-and-set — flat, branch-predictable, zero allocation, no FName hashing.
- **Removed-prop contract preserved via the bitmask**: a bit set last render but clear now is *ignored* for plain fields (matching the documented Godot/Unity semantic "removed plain props don't reset"), while event/ref/style bits *are* cleaned up.
- **Equality for bailout**: adapter-provided `Equals(Old,New)` compares set-bits + memberwise `==`, **excluding event fields** (`TFunction` has no `==`; events are always cheap trampoline-pointer updates, see §5, so they never force re-apply). Memoized components use `UseCallback` for identity-stable handles.
- Boilerplate is macro/codegen'd (`RUI_PROP(FText, Label, 0)` generates the field, builder setter, compare, and apply row).

**Rejected: `TMap<FName, FRUIValue(TVariant)>` prop bag.** Slate has no reflection — every key ends in a per-widget switch anyway, so the bag buys dynamism nowhere while costing hashing, boxing (`FSlateBrush`, `FText` behind variants), and no compile-time checking. **Rejected: `FInstancedStruct`** — drags CoreUObject/USTRUCT reflection into Core and reflected diffing is slower than memberwise `==`. A generic `FName→FRUIValue` adapter can be added later *behind the same adapter interface* when a `.guitkx`-style markup layer needs it.

## 3. Component model

**Decision: free functions + explicit context handle, identity = function pointer.**

```cpp
FRUINode Counter(FRUIContext& Ctx, const FCounterProps& Props);
// used as: RUI::FC(&Counter, FCounterProps{...})
```

- `RUI::FC` templates over `(FnPtr, TProps)`: stores `ComponentId = (void*)FnPtr` (stable, comparable — solves bailout identity, which `TFunction` alone cannot) plus a small invoker that owns the typed props. Lambda components are allowed but get per-instantiation identity — documented as "always re-renders", same as an unmemoized inline component in React.
- **Hooks find their fiber through `FRUIContext&`**, a thin wrapper over the current fiber's `FRUIComponentState*` passed into every component. This replaces Godot's static `Hooks._cur`: calling a hook outside render is a *compile* error, not a runtime assert, and there is no TLS lookup per hook. The reconciler still sets a debug-only thread-local to assert single-threaded, in-render usage.
- Hook storage ports 1:1: `FRUIComponentState` shared **by reference (`TSharedRef`) across fiber alternates**, positional `TArray<FRUIHookSlot>` (slot = `TVariant<FStateSlot, FMemoSlot, FRefSlot,…>`) with a cursor reset per render; separate arrays/cursors for passive and layout effects; `UseContext` records deps out-of-band. Setters are minted once per slot as `TSharedRef<FRUISetterCore>` closing over `(TWeakPtr<FRUIComponentState>, SlotIndex)`; `UseState<T>` returns a copyable typed `TRUISetter<T>` handle wrapping that ref → stable identity by pointer, safe after teardown via the weak ptr (replaces the index-bounds guard). Object.is-vs-deep-equality split (identity for containers in state, value equality for effect deps) is kept.

## 4. Fiber memory model

**Decision: reconciler-owned slab allocator, raw pointers between fibers, zero ref counting inside the tree.** The Godot lesson ("sever cycles explicitly") generalizes to: *don't create cycles at all*. `FRUIFiber` lives in paged chunks (fixed addresses, free-list reuse); `Parent/Child/Sibling/Alternate` are raw `FRUIFiber*`; deletion = explicit `Release()` to the free list after running teardown. Double-buffering ports 1:1 — matching positions reuse `Old->Alternate`, zero allocation on stable frames.

Three ownership edges cross the boundary:
- `Fiber → TSharedPtr<SWidget>` (strong): the shadow tree keeps detached/pooled widgets alive — exactly what enables pooling.
- `Fiber ↔ ComponentState`: state is `TSharedRef` on the fiber (shared across alternates); state's back-pointer to the live fiber is a raw `FRUIFiber*` re-pointed each render/commit and nulled on teardown.
- Closures (setters, event trampolines) hold `TWeakPtr<FRUIComponentState>` / `TWeakPtr` proxies — never raw `this`, never strong refs that would cycle. All Slate delegate binds use `CreateSP` per the research hazard list.

## 5. Slate host config

Per-element adapter (`IRUIElementAdapter`) registry keyed by `FRUIElementTypeId`:

- **Create**: `SNew(SButton)` with *construct-only* args (styles, list-view columns) pulled from props; then `ApplyFull` for everything else. Each adapter declares a **reconstruct-on-prop mask**: a diff touching those bits marks the fiber for widget replacement (same path as a type change) — the "per-widget reconstruct table" the research calls for.
- **Update**: constant values via setters (`SetText`, `SetVisibility`, `SetContent`, `SetImage`…). **Never bind delegate `TAttribute`s** — pass constants / call setters so widgets stay non-volatile and fast-path-valid under `Slate.EnableGlobalInvalidation`. Engine setters self-invalidate; the host calls `Invalidate`/`MarkPrepassAsDirty` only on custom leaves (`SRUICanvas`, the `draw_fn` analogue: paint-callback stored in a proxy, `Invalidate(Paint)` only when callback identity or `RedrawKey` changes — register-once trampoline ports 1:1).
- **Children**: `GetChildContainer()` per adapter (= `resolve_child_host`): panels expose slot ops (`AddSlot/InsertSlot(i)/RemoveSlot/NumSlots`); single-content widgets (`SBorder`, `SBox`, `SButton`) use `SetContent` and warn at >1 child (`warn_capacity`). Per-slot layout (padding, alignment, fill, ZOrder) is a nested `FRUISlotProps` on the child vnode, applied through UE 5.0 runtime slot setters.
- **Reorder**: no public MoveSlot → commit reorder pass **permutes children across existing slots via `DetachWidget`/`AttachWidget`** (no slot destruction), executed only when the frame's `structural` flag is set — inheriting reorder-only-on-structural-change verbatim, which also minimizes `ChildOrder` invalidations (the expensive reason).
- **Events**: bind **once at construction** to a per-node `TSharedRef<FRUIEventProxy>` (`SetOnClicked(FOnClicked::CreateSP(Proxy, &FRUIEventProxy::HandleClick))`); the proxy holds the current `FRUICallback`; diffs swap the callback pointer — no rebinding, generalizing Godot's draw trampoline. Godot's runtime signal-name resolution/alias table becomes compile-time per-adapter event tables. Handlers return `FReply` (default `Handled()` for void callbacks).
- **Text**: raw `FText`/`FString` children auto-wrap to `STextBlock` vnodes; editable-text adapters skip `SetText` when equal (controlled-input caret preservation).

## 6. Keyed diff, bailout, reuse_by_slot, pooling

All four port algorithmically 1:1; only the containers change:

- Three-tier keyed diff: fast-leaf-list in-place path → keys-stable positional scan → full keyed with a persistent, cleared-per-call `TMap<FRUIKey, FRUIFiber*>` plus the per-fiber `MatchedPass` mark-and-sweep counter. Unkeyed children get sentinel positional keys.
- Bailout: `ComponentId` pointer equality ∧ no pending update ∧ context deps unchanged ∧ typed-props `Equals` (identity-first: same props object pointer → skip compare) ∧ children array identity; reuse cached `LastOutput`.
- `reuse_by_slot` on a parent: slot *i*'s widget reused for whatever leaf lands there (contract unchanged: stateless childless leaves, stable type per slot) — churn becomes pure setter writes, the ideal Slate citizen.
- `FRUIWidgetPool`, keyed by element type id: unmounted poolable leaves are `DetachWidget`-ed with their last-applied props struct stashed; reuse is a *diff apply* against stashed props (the measured cheap-path lesson — never eager reset-to-defaults). Cap 256/type, exclude stateful widgets (editable text, list views), drain on root unmount and in `ReleaseSlateResources`. Precedent: `SListView` row recycling / `FUserWidgetPool`.
- Scheduling: setter → `RequestUpdate()` → one-shot `FSlateApplication::OnPreTick` registration → single coalesced render+commit per frame before Slate prepass; mid-render setState restarts from root with the 25-restart guard.

## 7. API sketch (recommended style)

```cpp
FRUINode TodoList(FRUIContext& Ctx, const FEmptyProps&)
{
    auto [Items, SetItems] = Ctx.UseState<TArray<FString>>({ TEXT("Ship v1") });
    auto [Draft, SetDraft] = Ctx.UseState<FString>(TEXT(""));

    TArray<FRUINode> Rows;
    for (const FString& Item : Items)
        Rows.Add(RUI::Text().Content(FText::FromString(Item)).Key(Item));

    return RUI::VBox().Children({
        RUI::HBox().Children({
            RUI::EditableText().Text(Draft)
                .OnTextChanged([SetDraft](const FText& T){ SetDraft(T.ToString()); }),
            RUI::Button().Label(INVTEXT("Add"))
                .OnClicked([=]{
                    TArray<FString> Next = Items;
                    Next.Add(Draft);
                    SetItems(MoveTemp(Next));
                    SetDraft(TEXT(""));
                    return FReply::Handled();
                }),
        }),
        RUI::VBox().Key(TEXT("list")).ReuseBySlot(false).Children(MoveTemp(Rows)),
    });
}
// FReactiveRoot Root = FReactiveRoot::Create(ContainerWidget, RUI::FC(&TodoList));
```

Builders (`RUI::Button()`) are the typed props structs' fluent fronts; each setter assigns the field and flips its bit — `SLATE_ARGUMENT` ergonomics without construct-only limits.

**Rejected alternative — CRTP class components with member state:**

```cpp
class FTodoList : public TRUIComponent<FTodoList, FEmptyProps>
{
    TRUIStateVar<TArray<FString>> Items{ this, { TEXT("Ship v1") } };
    TRUIStateVar<FString>         Draft{ this, TEXT("") };

    virtual FRUINode Render() override
    {
        TArray<FRUINode> Rows;
        for (const FString& I : Items.Get())
            Rows.Add(RUI::Text().Content(FText::FromString(I)).Key(I));
        return RUI::VBox().Children({ /* …as above, using Items.Set(…) … */ });
    }
};
```

Why rejected: member state resurrects class components — hooks stop composing (custom hooks are plain functions taking `FRUIContext&`; members can't be extracted that way), the reconciler must manage a second stateful object tree with construction/identity/lifetime rules parallel to fibers, and it diverges from *both* proven references, forfeiting the "twice-proven port" property. The positional-slot hook model needs no instance at all.

## 8. Mount surfaces

Everything mounts through one widget: `SReactiveRoot : SCompoundWidget` — the reconciler owns its `ChildSlot`; the root object holds the reconciler, host, pool, and pre-tick handle; `.Unmount()` runs cleanups and drains the pool.

- **Game viewport**: `GEngine->GameViewport->AddViewportWidgetContent(SNew(SReactiveRoot)…, ZOrder)` — plain Slate-in-game, no HUD/UMG required.
- **UMG embedding**: `UReactiveWidget::RebuildWidget()` returns the `SReactiveRoot`; `SynchronizeProperties` forwards designer props; `ReleaseSlateResources` unmounts (UObject code confined to `ReactiveUIUMG`).
- **Editor**: `FGlobalTabmanager` nomad tab (`SDockTab [ SNew(SReactiveRoot) ]`) or raw `FSlateApplication::Get().AddWindow(SNew(SWindow)[…])`. Widget Reflector works natively since real `SNew` calls happen inside adapters.
- **Standalone program** (SlateViewer pattern): `InitializeAsStandaloneApplication` + pump loop — doubles as the automated test host for the ported `core_test`/`update_test` suites.

## 9. Ports 1:1 vs rework

**1:1** (algorithms are engine-free already): reconciler phases + post-order effect list + commit order (deletions → placement/update/layout → reorder → swap → two-pass passive flush → replay deferred); double-buffered fibers; three-tier keyed diff + mark-and-sweep; bailout conditions; hook slot/cursor model + state-shared-across-alternates; coalescing + restart guard + optional time-slicing; pool cheap-path; per-fiber apply-plan idea (subsumed by typed adapters); removed-plain-prop semantic (via bitmask); structural error boundaries (UE builds without exceptions — same constraint as GDScript, unchanged design).

**Rework**: props (Dictionary → typed structs + bitmask + builders); prop application (reflective `node.set` → adapter setter tables + reconstruct-on-prop masks); events (runtime signal discovery/alias table → compile-time delegate maps + `CreateSP` trampoline proxies); callable identity (Callable → `FRUICallback` shared-ref, `TWeakPtr` guards); memory (RefCounted + manual sever → slab + raw intra-tree pointers); scheduling (`call_deferred` → `OnPreTick`); reorder (`move_child` → slot `DetachWidget/AttachWidget` permutation); text (`Label`/String → `STextBlock`/FText); **style/classes system needs the deepest rethink** — Slate styles are construct-heavy structs, not per-property channels, so v1 ships per-widget style props + a reconstruct path, with a stylesheet layer deferred.

## OPEN QUESTIONS
- Style/classes scope for v1: is a CSS-ish stylesheet layer (Godot RUIStyle/classes) required at launch, or are per-widget typed style props plus widget-reconstruction on style change acceptable until a Slate-native theming design exists?
- Minimum engine version: UE 5.1+ (MarkPrepassAsDirty, mature slot setters, TSlateAttribute) assumed — is 5.0 or 4.27 support needed, which would change the invalidation and slot-API surface?
- Should the element set include UMG UWidget-backed adapters (UObject inside ReactiveUISlate's tree) for designer-made UserWidgets as leaves, or stay pure-Slate with UMG only as an outer mount surface?
- Codegen for adapter/props boilerplate: macros only, or a small offline generator (the future .guitkx-for-Unreal compiler could emit both markup output and adapter tables) — decides how much hand-written per-element code v1 tolerates?
- Reorder strategy under Slate.EnableGlobalInvalidation: is detach/attach permutation measurably safer than remove+insert given the UE 5.7 FSlateInvalidationRoot fast-path crash reports, or should reordered regions auto-wrap in an invalidation boundary? Needs a benchmark spike.
- Threading stance: hard-assert game-thread-only (Slate's model) or leave the render phase structurally thread-agnostic for a future time-sliced/async mode?