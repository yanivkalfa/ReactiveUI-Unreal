## 1. INotifyFieldValueChanged — exact API (the interop surface)

Lives in the **engine runtime module `FieldNotification`** — header `/Engine/Source/Runtime/FieldNotification/Public/INotifyFieldValueChanged.h` (per the 5.7 API docs). It is a UInterface; the exact methods:

- `FDelegateHandle AddFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FFieldValueChangedDelegate InNewDelegate)` — "Add a delegate that will be notified when the FieldId value changed."
- `bool RemoveFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FDelegateHandle InHandle)`
- `int32 RemoveAllFieldValueChangedDelegates(FDelegateUserObjectConst InUserObject)` (+ overload taking `FFieldId` first) — bulk-remove by subscriber object.
- `void BroadcastFieldValueChanged(FFieldId InFieldId)`
- `const UE::FieldNotification::IClassDescriptor& GetFieldNotificationDescriptor() const`

`FFieldValueChangedDelegate` is `TDelegate<void(UObject*, UE::FieldNotification::FFieldId)>` with `FNotThreadSafeNotCheckedDelegateUserPolicy` (race detection disabled because the delegate container can reallocate during broadcast — i.e. **game-thread only**). `FFieldId` carries a `FName` + stable per-class index (`GetName()`, `GetIndex()`, `IsValid()`).

**Subscribing to a field by name**: UnrealHeaderTool generates, for every class with `FieldNotify` members, a nested `struct FFieldNotificationClassDescriptor : public Super::FFieldNotificationClassDescriptor` containing one `static const ::UE::FieldNotification::FFieldId <Name>` per field, an `enum EField { IndexOf_<Name> = SuperDescriptor::Max_IndexOf_ + 0, Max_IndexOf_ }` (indices chain across the class hierarchy), and a `ForEachField(const UClass*, callback)` override (verified from UHT output posted on the Epic forums). So a consumer can either use the compile-time constant, or resolve dynamically via the descriptor (`GetFieldNotificationDescriptor().GetField(Object->GetClass(), FName("Health"))` / iterate `ForEachField` — this is exactly how the MVVM plugin discovers bindable fields: the descriptor callback "is called from `UE::MVVM::Private::GetAvailableBindings`"). Canonical subscription (from the labutin.gg UE5.7 conventions site):

```cpp
VM->AddFieldValueChangedDelegate(
    UMyViewModel::FFieldNotificationClassDescriptor::bDropRequest,
    INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::OnDropRequested));
// handler: void OnDropRequested(UObject* Object, UE::FieldNotification::FFieldId FieldId)
```
`CreateUObject` binds weakly, so explicit unsubscription is unnecessary when lifetimes are shared. Blueprint/scripting mirror: `FFieldNotificationId` (FName wrapper, `GetFieldName`), BP nodes `AddFieldValueChangedDelegate`/`RemoveFieldValueChangedDelegate`, and `UFieldNotificationLibrary` (Engine module, `FieldNotificationLibrary.h`) with `BroadcastFieldValueChanged`, `BroadcastFieldsValueChanged`, `SetPropertyValueAndBroadcast`.

Reference implementation of the interface (mirroring `UMVVMViewModelBase`): store a `UE::FieldNotification::FFieldMulticastDelegate` plus a `TBitArray<> EnabledFieldNotifications`; `Add` pads the bit array to `FieldId.GetIndex()+1`; `Broadcast` only fires if the bit is set (DoubleDeez's public gist shows the full pattern).

## 2. Macros, FieldNotify on functions, dependencies

- `UE_MVVM_SET_PROPERTY_VALUE(MemberName, NewValue)` — compares old/new, assigns, broadcasts the member's FieldId if changed, **returns bool** (changed or not). Resolves the FieldId from `ThisClass::FFieldNotificationClassDescriptor::MemberName`.
- `UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MemberName)` — unconditional broadcast; also used with **function** names.

FieldNotify **functions** (computed/derived fields) must be `UFUNCTION(FieldNotify, BlueprintPure)`, `const`, take no arguments, return a single value (Epic docs). **Dependencies are manual in C++**: the convention (labutin.gg) is to cascade inside the setter:

```cpp
void SetCurrentHealth(const float NewValue) {
    if (UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, NewValue)) {
        UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
        UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(IsAlive);
    }
}
```
In **Blueprint** viewmodels, `Set` nodes on FieldNotify variables become "Set w/ Broadcast" and dependent FieldNotify functions are notified automatically; in C++ "you must manually call UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED — forget this and your UI won't update". Circular dependencies between FieldNotify functions create notification loops — there is no built-in cycle guard.

## 3. Engine-level vs plugin-level

The notification system is **engine-level, fully usable without the MVVM plugin**: the `FieldNotification` runtime module, the `FieldNotify` UPROPERTY/UFUNCTION specifier (UHT), `UFieldNotificationLibrary`, and `FFieldNotificationId` all live in engine code. UMG's own widgets (`UEditableText`, `USlider`, `UImage`, …) implement `INotifyFieldValueChanged` so widget properties can be binding sources. `UMVVMViewModelBase` ("Base class for MVVM viewmodel"), the `UE_MVVM_*` macros, `UMVVMView`, and the subsystems live in the **plugin** `/Engine/Plugins/Runtime/ModelViewViewModel/`. A third-party store can implement the interface itself (gist pattern) and MVVM views will still consume it: `UMVVMView::SetViewModel(FName, TScriptInterface<INotifyFieldValueChanged>)` accepts any implementer, not just `UMVVMViewModelBase`.

## 4. MVVMGameSubsystem / GlobalViewModelCollection

`UMVVMGameSubsystem` is a `UGameInstanceSubsystem` (`MVVMGameSubsystem.h`) exposing one method: `UMVVMViewModelCollectionObject* GetViewModelCollection()` (BlueprintCallable), backed by a `Transient TObjectPtr` — so registered viewmodels are strongly referenced and live for the game-instance lifetime unless removed. `UMVVMViewModelCollectionObject` (`Types/MVVMViewModelCollection.h`) API: `bool AddViewModelInstance(FMVVMViewModelContext Context, UMVVMViewModelBase* ViewModel)`, `UMVVMViewModelBase* FindViewModelInstance(FMVVMViewModelContext) const`, `FindFirstViewModelInstanceOfType(TSubclassOf<UMVVMViewModelBase>)`, `bool RemoveViewModel(FMVVMViewModelContext)`, `int32 RemoveAllViewModelInstance(UMVVMViewModelBase*)`, `FSimpleMulticastDelegate& OnCollectionChanged()`. `FMVVMViewModelContext` = ContextClass + ContextName ("Global Viewmodel Identifier"; by convention the class name). Widgets whose viewmodel creation mode is "Global Viewmodel Collection" resolve by that identifier at view initialization. **Interop caveat**: the collection is typed to `UMVVMViewModelBase*`, so an interface-only store can be set on views directly but cannot be registered globally. Other creation modes: Create Instance, Manual, Property Path (e.g. `GetPlayerController.Vehicle.ViewModel`), and Resolver (`UMVVMViewModelContextResolver` subclasses).

## 5. Compiled bindings — event-driven, not polling

The UMG designer's View Bindings compile into `UMVVMViewClass` ("Shared between every instance of the same View class") holding `FMVVMViewClass_Source` ("Shared data to find or create a ViewModel at runtime") and an `FMVVMCompiledBindingLibrary` ("Library of all the compiled bindings"; `GetFieldId` returns `TValueOrError<UE::FieldNotification::FFieldId, void>`). Per widget instance, `UMVVMView` — a `UUserWidgetExtension` ("Instance UMVVMClassExtension_View for the UUserWidget", `View/MVVMView.h`) — runs `InitializeSources()` → `InitializeBindings()` ("Initializing the bindings will execute them"), registering `FFieldValueChangedDelegate`s on each source field and keeping delegate handles. Thereafter execution is **event-driven**: a viewmodel broadcast triggers only the bindings registered to that FieldId; the library reads the source via the compiled property path, runs the optional conversion function, writes the destination. Runtime methods: `SetViewModel`, `SetViewModelByClass`, `GetViewModel(FName)`, `ExecuteViewModelBindings(FName)`, `ExecuteTickBindings()`, `ExecuteDelayedBinding(FMVVMViewClass_BindingKey)`, `Uninitialize*`. `EMVVMExecutionMode`: **Immediate** (run on every change), **Delayed** (queued, flushed once at end of frame before drawing), **Tick** (every frame regardless of change — the only polling mode), **DelayedWhenSharedElseImmediate** (default, shown as "Auto": Delayed if the binding can fire from multiple fields, else Immediate). Community benchmark: 10,000 changes to one variable in a frame cost 976.7 ms Immediate vs 12.8 ms Delayed (tsubasamusu.com).

## 6. Adoption status 2025–2026

Still **Beta** in both 5.7 and 5.8 docs ("use caution when shipping with it"); community testing on 5.7.1 (Dec 2025) confirms. Epic's Cody Albert (forums, May 21, 2025): "We're using the Viewmodel plugin heavily in our own internal projects and don't have any significant feature work left", expects it "out of beta soon", "No breaking changes are planned, so it should be safe/stable enough to use." Concrete engine-internal adoption: Viewmodel is **natively part of UMG in UEFN/Fortnite** (replacing default Fortnite UI elements); 5.7 shipped MetaSound Viewmodels; 5.8 release notes: MetaSound template tabs are "built on Unreal's MVVM framework". Talks: Unreal Fest 2023 "UMG Viewmodels: Building More Robust and Testable UIs using MVVM" and Unreal Fest Orlando 2025 "Powering Dynamic UI with Viewmodels". Shipped third-party titles: no official list; miltoncandelero's community guide claims the new Tomb Raider uses the pattern (unverified). Plugin is free, in-engine since 5.1 (Experimental), "built-in since 5.3" per community docs.

## 7. MDViewModel (and other alternatives)

**DoubleDeez/MDViewModel** (Dylan Dumesnil): MIT, UE 5.1+, C++ projects only; repo shows 56 stars, 10 forks, 248 commits, single release v1.0 (Sep 8, 2024). Why it exists vs Epic's: Epic's plugin is **widget-centric** (views are UUserWidgets; you wire creation modes per widget), while MDViewModel binds **UMG Widget, Actor, and Object Blueprints** to viewmodels; it adds **View Model Providers** ("responsible for creating view model instances and/or setting them on Views" — Cached, Unique, Manual) that automatically fetch/create VMs from gameplay data sources, **caches VM instances with lifetimes tied to the model data source**, auto-generates Blueprint events from FieldNotify properties, and integrates with the Blueprint debugger (shows live VM values). Philosophy: views never touch models; VMs pull from external systems; one VM per model object. Also notable: **druhasu/UnrealMvvm** — a separate MVVM framework with its own `TBaseViewModel` and C++/BP bindings, marketed as unit-tested and "ready for production use", predating/bypassing FieldNotify entirely.

**Interop bottom line for an external state-store library**: implement `INotifyFieldValueChanged` (engine module, no plugin dependency) + a `FFieldNotificationClassDescriptor`; mark fields `FieldNotify`; broadcast on change. MVVM views subscribe event-driven via `AddFieldValueChangedDelegate`, and your store can be handed to any `UMVVMView` via `SetViewModel`; only the global collection requires deriving `UMVVMViewModelBase`.

## KEY FACTS
- INotifyFieldValueChanged lives in the engine runtime module FieldNotification (/Engine/Source/Runtime/FieldNotification/Public/INotifyFieldValueChanged.h) — usable with zero dependency on the ModelViewViewModel plugin.
- Exact subscription API: FDelegateHandle AddFieldValueChangedDelegate(FFieldId, FFieldValueChangedDelegate) / bool RemoveFieldValueChangedDelegate(FFieldId, FDelegateHandle) / int32 RemoveAllFieldValueChangedDelegates(UserObject) / void BroadcastFieldValueChanged(FFieldId) / const IClassDescriptor& GetFieldNotificationDescriptor().
- FFieldValueChangedDelegate = TDelegate<void(UObject*, UE::FieldNotification::FFieldId)> with FNotThreadSafeNotCheckedDelegateUserPolicy — game-thread only, no thread-safety.
- UHT generates per-class FFieldNotificationClassDescriptor with static const FFieldId members per FieldNotify field, an IndexOf_ enum chained from SuperDescriptor::Max_IndexOf_, and ForEachField() — the MVVM plugin discovers bindable fields via UE::MVVM::Private::GetAvailableBindings calling ForEachField.
- UE_MVVM_SET_PROPERTY_VALUE(Member, NewValue) compares+assigns+broadcasts and returns bool; UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Member) broadcasts unconditionally and is how dependent FieldNotify functions (UFUNCTION(FieldNotify, BlueprintPure), const, 0 args, 1 return) are cascaded — manually in C++, automatically in Blueprint 'Set w/ Broadcast'.
- UMVVMGameSubsystem is a UGameInstanceSubsystem with GetViewModelCollection() -> UMVVMViewModelCollectionObject; API: AddViewModelInstance(FMVVMViewModelContext{ContextClass, ContextName}, UMVVMViewModelBase*), FindViewModelInstance, FindFirstViewModelInstanceOfType, RemoveViewModel, RemoveAllViewModelInstance, OnCollectionChanged; strongly references VMs for game-instance lifetime.
- Global collection is typed to UMVVMViewModelBase*, but UMVVMView::SetViewModel takes TScriptInterface<INotifyFieldValueChanged> — any interface implementer works as a view's viewmodel; only global registration requires the base class.
- Compiled bindings are event-driven, not polled: UMVVMViewClass (shared per widget class) + FMVVMCompiledBindingLibrary; per-instance UMVVMView (a UUserWidgetExtension) registers field-changed delegates at InitializeBindings and executes only affected bindings; Tick mode is the sole polling mode.
- EMVVMExecutionMode = Immediate, Delayed (flushed once at end of frame before drawing), Tick, DelayedWhenSharedElseImmediate (default 'Auto'); community benchmark: 10,000 changes/frame = 976.7 ms Immediate vs 12.8 ms Delayed.
- UMG Viewmodel is still Beta in UE 5.7 and 5.8 docs (as of 2026): 'use caution when shipping with it'.
- Epic staff Cody Albert (forums, May 21, 2025): Epic uses the Viewmodel plugin 'heavily in our own internal projects', 'no significant feature work left', expects out of beta 'soon', 'No breaking changes are planned'.
- Engine-internal adoption: Viewmodel is native UMG in UEFN/Fortnite; UE 5.7 added MetaSound Viewmodels; UE 5.8 MetaSound templates ship custom UMG tabs 'built on Unreal's MVVM framework'.
- Many stock UMG widgets (UEditableText, USlider, UImage) implement INotifyFieldValueChanged, letting widget properties act as binding sources.
- MDViewModel (DoubleDeez, MIT, UE 5.1+, C++ projects; 56 stars / 10 forks / 248 commits / v1.0 Sep 8 2024 per repo page) exists because Epic's plugin is widget-centric: it binds Actor/Object Blueprints too, adds View Model Providers (Cached/Unique/Manual) that auto-fetch VMs from gameplay sources with lifetimes tied to the data source, auto-generates BP events from FieldNotify, and integrates the BP debugger.
- Another alternative, druhasu/UnrealMvvm, uses its own TBaseViewModel and bindings (not FieldNotify), positioned as unit-tested and production-ready.

## SOURCES
https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/FieldNotification/INotifyFieldValueChanged
https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-viewmodel-for-unreal-engine
https://mvvm.labutin.gg/advanced/mvvm-conventions-for-ue5.7
https://gist.github.com/DoubleDeez/cc0ed2dfe67f5fb971908053acbc9596
https://forums.unrealengine.com/t/new-uht-specifiers-fieldnotify-getter-setter-etc/1184997
https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/ModelViewViewModel
https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/ModelViewViewModel/UMVVMViewModelCollectionObject
https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/ModelViewViewModel/UMVVMGameSubsystem
https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/ModelViewViewModel/UMVVMView
https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MVVMView?application_version=5.3
https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/FieldNotificationLibrary?application_version=5.3
https://tsubasamusu.com/umg-viewmodel-execution-mode/
https://forums.unrealengine.com/t/questions-about-umg-viewmodel-roadmap-support-and-timeline/2567457
https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes
https://dev.epicgames.com/documentation/fortnite/using-the-viewmodel-in-umg-in-unreal-editor-for-fortnite?lang=en-US
https://github.com/DoubleDeez/MDViewModel
https://github.com/DoubleDeez/MDViewModel/wiki
https://github.com/DoubleDeez/MDViewModel/wiki/View-Model-Providers
https://github.com/druhasu/UnrealMvvm
https://blog.rushdownstudio.com/wrangle-your-ui-data-intro-to-unreal-engines-umg-viewmodel-plugin/
https://miltoncandelero.github.io/unreal-viewmodel
https://www.dreamawake.io/blog/unreal-engine-viewmodel-plugin-guide
https://dev.epicgames.com/community/learning/talks-and-demos/pw3Y/unreal-engine-umg-viewmodels-building-more-robust-and-testable-uis-using-mvvm-unreal-fest-2023
https://www.youtube.com/watch?v=xOTZ-DVNc9U
https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/1465-umg-viewmodel-beta-
https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UWidget