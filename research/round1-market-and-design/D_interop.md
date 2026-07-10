# ReactiveUI-Unreal — Ecosystem & Interop Strategy

Scope: the interop/ecosystem layer for a React-style reconciler over **Slate** (pure C++, no VM), sibling of ReactiveUIToolKit (Unity/C#) and ReactiveUI-Godot. Same architecture: function components → vtree → fiber reconciler → single engine-boundary host config (here `SlateHost`, the analog of `host_config.gd`). Markup (`.uitkx`) compiles ahead-of-time to C++ — never runtime codegen (console/iOS no-JIT safe).

## 1. MVVM interop — `UseViewModel` / `UseField`

Target the **engine-level `FieldNotification` module only** (`INotifyFieldValueChanged`, `FFieldId`, descriptors) — zero dependency on the ModelViewViewModel plugin. This makes any Epic-MVVM viewmodel, any `UMVVMViewModelBase`, MDViewModel VM, or stock UMG widget (they implement the interface) consumable as a ReactiveUI data source.

**Two granularities:**

```cpp
// (a) Field-granular — preferred. One subscription per FieldId.
float HP = RUI::UseField<float>(VM, "CurrentHealth");
// (b) Whole-VM with read tracking — auto fine-grained.
auto Reader = RUI::UseViewModel(VM);
float HP  = Reader.Get<float>("CurrentHealth");
FText Tag = Reader.Get<FText>("GetHealthLabel"); // FieldNotify function
```

**Subscription mechanics** (per hook cell, stored in the fiber's hooks array):

- **Resolve** `FFieldId` once: `VM->GetFieldNotificationDescriptor().GetField(VM->GetClass(), FName(Name))`; memoize per `(UClass*, FName)` in a static map alongside the resolved `FProperty*` (or `UFunction*` for FieldNotify functions — `const`, 0-arg, single return; read via `ProcessEvent` on a cached param buffer).
- **Subscribe on mount commit** (passive-effect phase, game thread — matches the delegate's `NotThreadSafeNotChecked` policy):
  `Handle = VM->AddFieldValueChangedDelegate(FieldId, FFieldValueChangedDelegate::CreateSP(HookState, &FVMSub::OnChanged));`
  `CreateSP` against the `TSharedRef` hook-state object means the delegate self-invalidates if the fiber dies first — belt *and* suspenders: cleanup still calls `RemoveFieldValueChangedDelegate(FieldId, Handle)` explicitly so the multicast list compacts.
- **On broadcast**: `OnChanged(UObject*, FFieldId)` re-reads the property, compares (`FProperty::Identical`), and only if changed marks the owning fiber dirty → `RequestUpdate()`. Renders coalesce to **one re-render per frame**, i.e. we natively give MVVM's "Delayed" execution semantics (the mode that benchmarks 76× faster than Immediate) — this is a headline interop claim.
- **Read tracking** for `UseViewModel`: the `Reader` records each FieldId touched during render; after commit we diff against the previous set — subscribe new, remove stale. Components automatically depend on exactly the fields they read, per render. (`UseField` remains for hot paths that want zero tracking overhead.)

**Lifetime (UObjects held from non-UObject fiber code):**

- Hook cells hold the VM as **`TWeakObjectPtr<UObject>` by default** — the UI *observes*, never owns; no GC extension, no root-set leaks, no cycles. Every access goes through `Get()`; if stale, `UseField` returns the prop-supplied default and the component may render a fallback (optional third arg `EStaleVMPolicy::RenderDefault | Unmount`).
- **`UseOwnedViewModel<T>()`** covers UI-local VMs: `NewObject<T>()` held in a **`TStrongObjectPtr`** inside the hook cell (adds to root set — safe from any thread-agnostic non-UObject holder), released in unmount cleanup. This is the `useState`-for-a-VM idiom.
- Unsubscribe path guards `VMWeak.IsValid()` — if GC already collected the VM the delegates died with it.

**Reverse direction:** our cross-component store (`RUISignal` analog) ships an optional adapter `URuiSignalViewModel` implementing `INotifyFieldValueChanged` (gist pattern: `FFieldMulticastDelegate` + `TBitArray` enabled-bits + hand-rolled descriptor), so **UMG MVVM views can bind to ReactiveUI state** via `UMVVMView::SetViewModel` (accepts any `TScriptInterface<INotifyFieldValueChanged>`). A separate, plugin-gated module provides `URuiViewModelBridge : UMVVMViewModelBase` solely for Global Viewmodel Collection registration (the collection is typed to the base class). Core stays plugin-free.

## 2. UMG interop, both directions

**(a) ReactiveUI inside a UserWidget** — `UReactiveHostWidget : public UWidget`, designer-placeable like any panel child:

```cpp
UCLASS() class UReactiveHostWidget : public UWidget {
  UPROPERTY(EditAnywhere) TSubclassOf<UReactiveComponentAsset> RootComponent; // or C++ factory
  virtual TSharedRef<SWidget> RebuildWidget() override {
    Root = FReactiveRoot::Create(RootFactory.Execute(), GetOwningLocalPlayer());
    return Root->GetSlateWidget();          // an SCompoundWidget the reconciler owns
  }
  virtual void ReleaseSlateResources(bool b) override { if (Root) Root->Unmount(); }
};
```

Designers drop it in any UMG layout; Blueprint can pass initial props and a VM (which flows into the tree via context → `UseField`). Pure-Slate hosts (editor tabs, `GameViewportClient::AddViewportWidgetContent`) get the same `FReactiveRoot::Create` directly.

**(b) UMG widgets as host elements** — `V::Umg(UMyWidget::StaticClass(), Props)`:

- Host config creates via `CreateWidget<UUserWidget>(OwningPlayer, Class)` (owning player threaded down from the root — required for CommonInput/localization), then `TakeWidget()` yields the `SObjectWidget` inserted into the Slate tree.
- The fiber's host-node handle holds the UUserWidget in a **`TStrongObjectPtr`** (the widget is UI-owned here), released in the deletion commit *after* removing the Slate child.
- **Props→widget**: reflection writes through `FProperty`, preferring a `Set<Name>` UFUNCTION when present; FieldNotify targets use `UFieldNotificationLibrary::SetPropertyValueAndBroadcast` so the widget's own bindings still fire. **Events**: React-style `OnClicked` props bind to the widget's dynamic multicast delegates through a pooled UObject trampoline (delegates require UObject targets); unbound on cleanup — consistent with the family rule that events *do* reset between renders.
- Cache per-class prop maps; diff props exactly like native Slate elements.

**CommonUI citizenship** — two rules:

1. **Our screens live *inside* activatables, never replace them.** Ship `URuiActivatableScreen : UCommonActivatableWidget` embedding a `UReactiveHostWidget`. Teams push it onto their existing `UCommonActivatableWidgetContainerBase` stacks — activation, input-mode switching, back-handling, and focus restoration all stay CommonUI's job. A `UseActivation()` hook surfaces `OnActivated/OnDeactivated` as state; an `autofocus` prop registers a Slate widget that `GetDesiredFocusTarget()` returns, so gamepad focus lands correctly on activation.
2. **Common widgets wrap fine as children** (`V::Umg(UCommonButtonBase...)`) because CommonUI subsystems discover them through the widget/player relationship, which rule (a)'s owning-player plumbing preserves. We do **not** allow wrapping `UCommonActivatableWidget` subclasses as plain children — the host config rejects it with a diagnostic pointing at `URuiActivatableScreen` (activation outside a container is undefined behavior we refuse to own).

## 3. Positioning (with "when not to use us")

**vs Coherent Gameface**: We are native Slate — no HTML engine, no V8/JSC, no atlas tuning, no per-seat middleware negotiation; source-available and console-buildable by you. **Don't use us** if your studio already staffs web engineers and designs UI in HTML/CSS/Figma-to-web pipelines, needs video/animation-heavy screens (Lottie-class motion), or ships a title where Gameface's AAA support contract is itself the feature. We won't match a full CSS engine's styling expressiveness.

**vs NoesisGUI**: Both are native-rendered and MVVM-friendly; Noesis brings the entire WPF/XAML toolchain and Blend authoring. **Don't use us** if your UI is artist-driven in a visual designer, you have WPF veterans, or you need vector-perfect scalable UI — Noesis's XAML/vector story is a decade deep and its €195 indie tier is cheap. We win when UI is programmer-authored, when you refuse a parallel widget world (we emit real Slate/UMG that CommonUI, focus, and accessibility already understand), and when license-per-project friction matters.

**vs React-UMG / JS-runtime approaches (ReactorUMG, RNUE)**: Same DX thesis, minus the VM: no V8/Hermes in shipping builds, no no-JIT contortions on console/iOS, no npm supply chain, one language (C++) end-to-end. **Don't use us** if your team is JavaScript-first and wants the actual React library, Fast Refresh, and npm — when RNUE ships publicly it will beat us on that axis; our hot-reload story (Live Coding + AOT markup recompile) is weaker than JS refresh.

**vs Epic MVVM alone**: Not either/or — we *consume* FieldNotify (§1). MVVM alone binds data into a widget tree you still build and restructure by hand in the Designer; we make the tree itself a function of state (conditional subtrees, keyed lists, composition). **Don't use us** if your UI is mostly static layouts with value bindings and a designer-centric team — the Designer + ViewModel (Beta but Fortnite-proven, free, first-party) is the lower-risk default, and adding any third-party reconciler is unjustified overhead there.

## 4. Distribution

- **Core on GitHub, MIT** (runtime, reconciler, `.uitkx` compiler): consoles require user-compiled source anyway (Fab can't ship NDA binaries), Blueprint-only friction is unavoidable for source plugins, and every OSS predecessor's failure mode was abandonment-fear — permissive license + public CI is the counter-signal.
- **Fab listing = convenience + tooling** (one-time price, ~$29.99–$59.99 anchored to Blueprint Assist/Dialogue Plugin): Epic-built Win64 binaries, one-click Launcher install, plus the **paid editor module** (live markup preview, `.uitkx` sensei/diagnostics — mirroring this repo's editor plugin + IDE extensions). This is the VRExpansion/Voxel "free code, paid convenience" model; no subscriptions exist on Fab, so recurring revenue is GitHub Sponsors/support tiers.
- **Engine-version policy**: support the **three latest UE versions** (Fab compliance window), CI matrix via `RunUAT BuildPlugin` per version (MuddyTerrain-style "develop low, upgrade high"). All Slate/engine churn (FAppStyle-class renames, 5.x widget regressions) is quarantined in the `SlateHost` boundary + one `RuiEngineCompat.h` of version defines — the reconciler/hooks never include engine UI headers, so a new engine release is a host-config diff, not a framework audit.
- **Console/iOS**: pure C++ + AOT-compiled markup ⇒ no JIT exposure at all; `.uplugin` leaves platform allow-lists open and licensees compile under their own SDK agreements (FMOD pattern).

## 5. Migration story (existing UMG project)

1. **Keep VMs, add nothing**: existing FieldNotify viewmodels work unmodified via `UseField` — migration never requires touching the data layer.
2. **Leaf islands**: pick one painful dynamic region (inventory grid, scoreboard) and drop a `UReactiveHostWidget` into the existing UserWidget; pass the same VM the surrounding Designer bindings use. Both systems react to the same broadcasts — no double-source-of-truth.
3. **Screen-level**: rebuild whole screens as `URuiActivatableScreen`s pushed onto the *existing* CommonUI stacks alongside legacy screens; reuse legacy Common widgets inside via `V::Umg` (buttons, styled text keep their skin).
4. **Inversion (optional)**: root the layout in ReactiveUI, keep stubborn designer-made pieces embedded as `V::Umg` islands indefinitely. Every step is shippable; rollback is deleting a widget.

## 6. CommonUI we must NOT rebuild — and delegation

| Feature | Delegation |
|---|---|
| Activatable stacks, back-stack, activation lifecycle | Host inside `URuiActivatableScreen`; never reimplement push/pop (§2) |
| Input routing / `UCommonUIActionRouter`, input-mode capture | Inherited via activatable hosting; we never install our own input preprocessor |
| Input-method detection + platform glyphs (`UCommonInputSubsystem`, controller icon data) | `UseInputMethod()` hook merely *reads* the subsystem + rebroadcasts changes as state; glyph lookup delegated to Common's data tables |
| Gamepad/keyboard focus navigation & analog cursor | Standard Slate navigation over our real SWidgets; `autofocus`→`GetDesiredFocusTarget`; zero custom focus manager |
| `UCommonButtonBase` states, held-input, text styles | Wrap via `V::Umg`; style props map onto Common style assets rather than bypassing them |
| Platform traits / safe zones | `UseSafeArea` (family API) reads `SSafeZone`/trait tags; no parallel config |

Rebuilding any of these forks console-cert-hardened, Fortnite-proven behavior — the framework's whole pitch is *emitting the widgets Epic's stack already governs*.

## OPEN QUESTIONS
- Markup compile target: generate C++ (best perf, but requires a C++ project and per-platform recompile) vs a data asset interpreted by the C++ runtime (Blueprint-only-project friendly, hot-reloadable in PIE) — or both, with the asset path as the dev-loop and codegen for shipping?
- Read-tracking UseViewModel does reflection reads every render — is FProperty::ContainerPtrToValuePtr fast enough for per-frame HUD components, or do we need a compile-time descriptor-constant fast path (UMyVM::FFieldNotificationClassDescriptor::CurrentHealth) exposed via a template UseField<&Descriptor::Field>?
- FieldNotify dependency cascades are manual in C++ and cycle-unguarded — should our subscription layer add a per-frame FieldId de-dup/cycle breaker, or is that overstepping into fixing Epic's semantics?
- Does UReactiveHostWidget need full UMG designer preview (rendering the reactive tree at design time), or is a placeholder box acceptable for v1? Design-time execution of user component code is a large safety/complexity surface.
- TStrongObjectPtr on V::Umg-hosted widgets vs relying on SObjectWidget's existing GC keep-alive — is the double-hold redundant, and does it risk delaying GC of large widget subtrees after unmount?
- Should the paid Fab tier include the MVVM bridge module (URuiViewModelBridge) or must all runtime-behavior code stay in the MIT core, with Fab strictly tooling-only, to avoid open-core resentment?
- CommonUI is itself only semi-documented and evolves (5.6 SlateIM signals Epic rethinking UI authoring) — do we hedge by keeping the CommonUI integration in a separate optional module so core has zero CommonUI includes?
- Naming/branding: does 'ReactiveUI' collide with the established .NET ReactiveUI project badly enough in the Unreal/C# adjacent space (UnrealSharp users) to warrant a different name for the Unreal port?