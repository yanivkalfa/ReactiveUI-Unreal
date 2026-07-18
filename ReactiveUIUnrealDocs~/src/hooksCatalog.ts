// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The per-hook reference catalog — 23 core hook entries + 17 router hook entries,
// authored from the real headers (RuiContext.h / RuiSignal.h / RuiPresence.h / RuiRouter.h).
// docs.tsx generates one reference page per entry (same model as the Components catalog);
// scripts/docs-drift.mjs compares these counts against the code registries, so the catalog
// can never silently lag the hooks that actually exist.

export interface HookEntry {
  name: string
  category: 'core' | 'router'
  /** The .uetkx authoring form (the compiler passes Ctx). */
  signature: string
  /** The C++ declaration, tidied. */
  cppSignature: string
  returns: string
  description: string
  example: string
  gotchas?: string[]
}

export const CORE_HOOKS: HookEntry[] = [
  {
    name: 'UseState',
    category: 'core',
    signature: 'auto [Value, SetValue] = UseState<T>(Initial)',
    cppSignature: 'TTuple<T, TRuiSetter<T>> FRuiContext::UseState<T>(T Initial = T())',
    returns: 'the current value and a stable setter',
    description: 'Gives the component a persistent value stored in its fiber. Calling the setter schedules a re-render with the new value; the setter identity is stable across renders, so it is safe to hand to children and effects. The setter also accepts a functional updater lambda that receives the freshest value.',
    example: `auto [Count, SetCount] = UseState<int32>(0);

<Button OnClicked={ SetCount(Count + 1) }>
	<TextBlock Text={ RUI::Fmt(TEXT("Count: {}"), Count) } />
</Button>`,
    gotchas: [
      'Setting a value equal (operator==) to the current one bails out — no re-render.',
      'Use the updater form SetCount([](const int32& Prev) { return Prev + 1; }) to read the freshest value from a stale closure.',
      'Rules of hooks apply: top level of the render function only, stable call order every render (rui.HookValidation diagnoses violations).',
    ],
  },
  {
    name: 'UseReducer',
    category: 'core',
    signature: 'auto [Value, Dispatch] = UseReducer<T, TAction>(Reducer, Initial)',
    cppSignature: 'TTuple<T, TFunction<void(TAction)>> FRuiContext::UseReducer<T, TAction>(TFunction<T(const T&, const TAction&)> Reducer, T Initial = T())',
    returns: 'the current state and a dispatch function',
    description: 'State driven by a reducer, for transitions too involved for a bare setter. Dispatch runs the reducer against the current state and re-renders when the result differs. The reducer closure is refreshed every render (family parity), so it always sees the current captures.',
    example: `auto [Cart, Dispatch] = UseReducer<FCartState, ECartAction>(&CartReducer, FCartState{});

<Button OnClicked={ Dispatch(ECartAction::Clear) }>
	<TextBlock Text="Clear cart" />
</Button>`,
    gotchas: [
      'A dispatch whose reducer returns a value equal (operator==) to the current state bails out without a re-render.',
      'Dispatch after the component has torn down is silently ignored.',
      'The reducer is re-captured every render — do not rely on the first-render closure surviving.',
    ],
  },
  {
    name: 'UseEffect',
    category: 'core',
    signature: 'UseEffect([=]() { ... }, RUI::Deps(A, B))',
    cppSignature: 'template <typename TFn> void FRuiContext::UseEffect(TFn&& Effect, FRuiDeps Deps) — Effect returns void or FRuiEffectCleanup',
    returns: 'nothing',
    description: 'Runs a passive side effect after the commit, in two passes across the tree: all pending cleanups first, then all setups. The dependency list gates re-runs: RUI::Deps(A, B) re-runs on change, empty RUI::Deps() runs once on mount, and RUI::EveryCommit() runs after every commit. Return an FRuiEffectCleanup to release what the effect acquired — it runs before the next setup and on unmount.',
    example: `UseEffect([=]() -> FRuiEffectCleanup
{
	auto Handle = Store->Subscribe(OnChange);
	return [=]() { Store->Unsubscribe(Handle); };
}, RUI::Deps(PlayerId));`,
    gotchas: [
      'Empty RUI::Deps() is mount-only; RUI::EveryCommit() is after every commit — they are different sentinels.',
      'The lambda may return void or an FRuiEffectCleanup; the dispatch is if-constexpr, so either form compiles unchanged.',
      'Deps comparison is shallow (family _deps_changed): unset deps on either side counts as changed.',
    ],
  },
  {
    name: 'UseLayoutEffect',
    category: 'core',
    signature: 'UseLayoutEffect([=]() { ... }, RUI::Deps(A, B))',
    cppSignature: 'template <typename TFn> void FRuiContext::UseLayoutEffect(TFn&& Effect, FRuiDeps Deps) — Effect returns void or FRuiEffectCleanup',
    returns: 'nothing',
    description: 'Like UseEffect, but runs synchronously during the commit, before the frame is painted — the slot for measurement and focus work that must land before the user sees the frame. Cleanup-then-setup runs per fiber rather than in the passive two-pass batch. Deps semantics are identical to UseEffect.',
    example: `UseLayoutEffect([=]() { SetMeasuredWidth(MeasurePanel()); }, RUI::EveryCommit());`,
    gotchas: [
      'Runs synchronously during commit (pre-paint) — unlike UseEffect, which runs after.',
      'Cleanup-then-setup happens per fiber, not in the two-pass batch passive effects use.',
    ],
  },
  {
    name: 'UseMemo',
    category: 'core',
    signature: 'const T& Value = UseMemo<T>(Factory, RUI::Deps(A, B))',
    cppSignature: 'const T& FRuiContext::UseMemo<T>(TFunction<T()> Factory, FRuiDeps Deps)',
    returns: 'a reference to the cached value',
    description: 'Caches an expensive computed value across renders, re-running the factory only when the dependency list changes. The comparison is shallow, per RUI::Deps semantics. The value lives in the hook cell and is returned by const reference, so unchanged renders pay no copy.',
    example: `const TArray<FString>& Sorted = UseMemo<TArray<FString>>(
	[&]() { return SortItems(Items, Filter); }, RUI::Deps(Filter));

<ListPanel Items={ Sorted } />`,
    gotchas: [
      'Deps comparison is shallow — C++ arrays and maps are values; wrap in TSharedPtr when you want identity semantics.',
    ],
  },
  {
    name: 'UseCallback',
    category: 'core',
    signature: 'FRuiCallback Cb = UseCallback(Fn, RUI::Deps(A, B))',
    cppSignature: 'FRuiCallback FRuiContext::UseCallback(TFunction<void()> Fn, FRuiDeps Deps) — payload overload takes TFunction<void(const FRuiValue&)>',
    returns: 'an FRuiCallback that keeps its identity while deps are unchanged',
    description: 'Caches a callback identity across renders by deps — the memo-friendly way to pass event handlers to children without breaking their prop equality. Two overloads accept a zero-argument lambda or one taking the FRuiValue event payload. Implemented directly over UseMemo.',
    example: `FRuiCallback OnSave = UseCallback([=]() { SaveDocument(DocumentId); }, RUI::Deps(DocumentId));

<SaveButton OnSave={ OnSave } />`,
    gotchas: [
      'The closure is only refreshed when deps change — for a permanent identity that always runs the latest body, use UseStableCallback instead.',
    ],
  },
  {
    name: 'UseRef',
    category: 'core',
    signature: 'auto Ref = UseRef<T>(Initial)',
    cppSignature: 'TSharedRef<TRuiRef<T>> FRuiContext::UseRef<T>(T Initial = T())',
    returns: 'a stable shared box whose Current field survives renders',
    description: 'A mutable box that persists for the life of the component — the same TSharedRef comes back every render. Mutating Current never triggers a re-render, which is exactly the point: it holds values the UI does not need to display, like timers, counters, and handles.',
    example: `auto ClickCount = UseRef<int32>(0);

<Button OnClicked={ ClickCount->Current++ }>
	<TextBlock Text="Counts silently" />
</Button>`,
    gotchas: [
      'Mutating Current never re-renders — use UseState for anything the UI must reflect.',
    ],
  },
  {
    name: 'UseImperativeHandle',
    category: 'core',
    signature: 'const T& Handle = UseImperativeHandle<T>(Factory, RUI::Deps(A, B))',
    cppSignature: 'const T& FRuiContext::UseImperativeHandle<T>(TFunction<T()> Factory, FRuiDeps Deps)',
    returns: 'a reference to the cached handle object',
    description: 'Builds an imperative handle a parent can call into, rebuilt only when deps change. Two forms: the memo alias (shown above), and the ref-PUBLISHING form UseImperativeHandle(TargetRef, Factory, Deps) — it writes the handle into a parent-supplied TRuiRef on mount and clears it on unmount (the React shape; both siblings ship it). Pair with RUI::Slate::WidgetFromHandle<TWidget>(Handle) to call live Slate APIs (ScrollToEnd()-class) through a captured host Ref.',
    example: `const FPanelHandle& Handle = UseImperativeHandle<FPanelHandle>(
	[=]() { return FPanelHandle{ FocusField, ScrollToTop }; }, RUI::Deps());

<InspectorHost Handle={ Handle } />

// Ref-publishing form: the parent passed us Props.PanelRef (TSharedRef<TRuiRef<FPanelHandle>>)
UseImperativeHandle<FPanelHandle>(Props.PanelRef,
	[=]() { return FPanelHandle{ FocusField, ScrollToTop }; }, RUI::Deps());`,
    gotchas: [
      'The memo form IS UseMemo under the hood (family alias) — identical deps and caching semantics.',
      'The ref-publishing form clears the target ref to a default-constructed handle on unmount — check validity before calling through a stored handle.',
    ],
  },
  {
    name: 'UseContext',
    category: 'core',
    signature: 'const T& Value = UseContext(Handle)',
    cppSignature: 'const T& FRuiContext::UseContext<T>(const TRuiContext<T>& Handle)',
    returns: 'the nearest provided value, or the handle\'s default',
    description: 'Reads the nearest value provided for the handle above this component — an O(1) lookup through the handle\'s render stack. When no provider is in scope it returns the handle\'s default rather than erroring. The read is recorded, so a provider change re-renders this consumer even through memo bailouts.',
    example: `const FLinearColor Theme = UseContext(GThemeCtx);

<TextBlock Text="Themed label" ColorAndOpacity={ Theme } />`,
    gotchas: [
      'Consumes no hook slot, but the read is recorded — any provider value change (operator==) re-renders the consumer.',
      'Without a provider above, you silently get the handle\'s default value.',
    ],
  },
  {
    name: 'ProvideContext',
    category: 'core',
    signature: 'ProvideContext(Handle, Value)',
    cppSignature: 'void FRuiContext::ProvideContext<T>(const TRuiContext<T>& Handle, T Value)',
    returns: 'nothing',
    description: 'Provides a value under a context handle to everything this component renders beneath it — a hook-style provider, matching the family API, with no wrapper element in the tree. On change against the previous commit it marks consuming descendants dirty so they re-render even through bailed-out intermediate components (eager propagation).',
    example: `auto [bDark, SetDark] = UseState<bool>(true);
ProvideContext(GThemeCtx, bDark ? DarkTheme : LightTheme);

<VerticalBox>
	<ThemedPanel />
</VerticalBox>`,
    gotchas: [
      'Change detection is operator== against the previous commit — an equal value does not dirty consumers.',
      'Propagation is eager: a changed value re-renders consumers even inside memoized subtrees.',
    ],
  },
  {
    name: 'UseStableCallback',
    category: 'core',
    signature: 'FRuiCallback Cb = UseStableCallback(Fn)',
    cppSignature: 'FRuiCallback FRuiContext::UseStableCallback(TFunction<void()> Fn)',
    returns: 'an FRuiCallback whose identity never changes for the component lifetime',
    description: 'Returns a callback whose identity is fixed for the life of the component while its body is swapped every render — callers always execute the latest closure. That makes it safe to register once with an external system in a mount-only effect and still see fresh state on every invocation.',
    example: `FRuiCallback OnPoll = UseStableCallback([=]() { SetStatus(QueryStatus(ServerId)); });

UseEffect([=]() -> FRuiEffectCleanup
{
	Poller->Register(OnPoll);
	return [=]() { Poller->Unregister(OnPoll); };
}, RUI::Deps());`,
    gotchas: [
      'Identity never changes and the body refreshes every render — the inverse trade-off of UseCallback.',
    ],
  },
  {
    name: 'UseStableFunc',
    category: 'core',
    signature: 'FRuiCallback Cb = UseStableFunc(Fn)',
    cppSignature: 'FRuiCallback FRuiContext::UseStableFunc(TFunction<void()> Fn)',
    returns: 'an FRuiCallback whose identity never changes for the component lifetime',
    description: 'A literal alias of UseStableCallback, kept for family API parity. The identity is stable for the component lifetime and the body is refreshed every render. Use whichever name reads better at the call site.',
    example: `FRuiCallback OnRefresh = UseStableFunc([=]() { SetTick(Tick + 1); });`,
    gotchas: [
      'Exactly UseStableCallback under another name — same slot kind, same semantics.',
    ],
  },
  {
    name: 'UseStableAction',
    category: 'core',
    signature: 'FRuiCallback Cb = UseStableAction(Fn)',
    cppSignature: 'FRuiCallback FRuiContext::UseStableAction(TFunction<void(const FRuiValue&)> Fn)',
    returns: 'an FRuiCallback whose identity never changes, invoked with the event payload',
    description: 'The payload-taking form of the stable-callback family: a fixed-identity FRuiCallback whose body receives the FRuiValue argument the caller passes. Like its siblings, the wrapper identity never changes while the inner closure is replaced every render. UseStableCallback and UseStableFunc are built on top of it.',
    example: `FRuiCallback OnPicked = UseStableAction([=](const FRuiValue& Value)
{
	SetChoice(Value.TextValue.ToString());
});`,
    gotchas: [
      'Identity never changes; the body is refreshed every render.',
    ],
  },
  {
    name: 'UseDeferredValue',
    category: 'core',
    signature: 'T Deferred = UseDeferredValue<T>(Value)',
    cppSignature: 'T FRuiContext::UseDeferredValue<T>(T Value, FRuiDeps Deps = FRuiDeps())',
    returns: 'a lagging copy of the value',
    description: 'Keeps interactions responsive by letting an expensive consumer trail one frame behind. On the render where the input changes it still returns the previous value, then commits the new target on a deferred next-frame tick — one extra re-render in which the deferred copy catches up.',
    example: `auto [Query, SetQuery] = UseState<FString>(FString());
const FString Deferred = UseDeferredValue<FString>(Query);

<EditableTextBox Text={ FText::FromString(Query) } OnTextChanged={ SetQuery(Value.TextValue.ToString()) } />
<SearchResults Query={ Deferred } />`,
    gotchas: [
      'Without explicit deps, change detection is operator== on the value itself; pass RUI::Deps(...) to control it.',
      'The catch-up is a real extra re-render scheduled on the next frame.',
    ],
  },
  {
    name: 'UseTransition',
    category: 'core',
    signature: 'auto [bIsPending, StartTransition] = UseTransition()',
    cppSignature: 'TTuple<bool, TFunction<void(TFunction<void()>)>> FRuiContext::UseTransition()',
    returns: 'a pending flag (always false) and a StartTransition function',
    description: 'Marks an update as non-urgent — API-compatible with the family, but synchronous by design on Unreal, where there is no concurrent renderer. StartTransition simply invokes the action immediately, and bIsPending is always false. It exists so components ported from the siblings compile and behave identically.',
    example: `auto [bIsPending, StartTransition] = UseTransition();

<Button OnClicked={ StartTransition([=]() { SetTab(ETab::Details); }) }>
	<TextBlock Text="Details" />
</Button>`,
    gotchas: [
      'Synchronous no-op, faithful to the family decision: the action runs immediately and bIsPending never becomes true.',
    ],
  },
  {
    name: 'UseSignal',
    category: 'core',
    signature: 'T Value = RUI::UseSignal<T>(Ctx, Sig)   // selector form: RUI::UseSignal<T, TSelected>(Ctx, Sig, Selector)',
    cppSignature: 'TSelected RUI::UseSignal<T, TSelected>(FRuiContext& Ctx, const TSharedRef<TRuiSignal<T>>& Sig, TFunction<TSelected(const T&)> Selector) — selectorless overload returns the whole T',
    returns: 'the signal\'s current value (or the selected slice)',
    description: 'Subscribes the component to a TRuiSignal — a reactive value store that lives outside the component tree. It follows React\'s useSyncExternalStore discipline: the render reads the snapshot directly from the store, and the subscription happens in a passive effect with a post-subscribe re-check, so a change landing between render and effect is never missed. With a selector, the component re-renders only when the selected slice changes.',
    example: `const int32 Score = RUI::UseSignal<int32>(Ctx, Game::GScoreSignal);

<TextBlock Text={ RUI::Fmt(TEXT("Score: {}"), Score) } />`,
    gotchas: [
      'Written with explicit Ctx in markup — RUI::UseSignal(Ctx, ...); only the built-in member hooks are called bare.',
      'Change detection is operator== on the (selected) value — signal values have value semantics; wrap containers in TSharedPtr for identity.',
      'The subscription is released by the hook cell\'s destructor on unmount, and re-bound automatically if the signal instance changes.',
    ],
  },
  {
    name: 'UseSignalKey',
    category: 'core',
    signature: 'T Value = RUI::UseSignalKey<T>(Ctx, Key, Initial)',
    cppSignature: 'T RUI::UseSignalKey<T>(FRuiContext& Ctx, FName Key, T Initial = T())',
    returns: 'the keyed signal\'s current value',
    description: 'UseSignal over the process-wide FName-keyed registry: the signal is created lazily on first use and shared by every reader of that key. Keyed signals deliberately outlive the components that read them — that is the point: shared app state that survives screens mounting and unmounting.',
    example: `const int32 Count = RUI::UseSignalKey<int32>(Ctx, TEXT("Demo.Counter"), 0);

<TextBlock Text={ RUI::Fmt(TEXT("Count: {}"), Count) } />`,
    gotchas: [
      'Keyed signals OUTLIVE their readers; RUI::ClearSignals() is the full session reset (subscribers are not notified).',
      'Reading one key as two different types trips a runtime type check — the registry is honest about misuse, never silent.',
      'Written with explicit Ctx in markup — RUI::UseSignalKey(Ctx, ...).',
    ],
  },
  {
    name: 'UseTween',
    category: 'core',
    signature: 'float Value = UseTween(Target, DurationSec, Ease)',
    cppSignature: 'float FRuiContext::UseTween(float Target, float DurationSec = 0.25f, ERuiEase Ease = ERuiEase::InOut)',
    returns: 'the eased float, updated every frame until it settles at the target',
    description: 'The float convenience over UseTweenValue: eases the returned value towards Target, re-rendering the component every frame until it settles. Retargeting mid-flight continues from the current value rather than snapping. Time comes from the host clock, so it is deterministic under the mock host in tests.',
    example: `const float Opacity = UseTween(bVisible ? 1.0f : 0.25f, 0.3f, ERuiEase::Out);

<Border RenderOpacity={ Opacity }>
	<TextBlock Text="Fades between states" />
</Border>`,
    gotchas: [
      'The first render primes at the target — no mount animation by design; use UseAnimate for enter transitions.',
      'While animating it re-renders every frame via a self-re-arming frame chain (one coalesced re-render per frame).',
    ],
  },
  {
    name: 'UseAnimate',
    category: 'core',
    signature: 'float Progress = UseAnimate(bIn, DurationSec, Ease)',
    cppSignature: 'float FRuiContext::UseAnimate(bool bIn, float DurationSec = 0.25f, ERuiEase Ease = ERuiEase::InOut)',
    returns: 'eased 0..1 progress towards bIn',
    description: 'The enter/exit-animation driver: returns eased progress towards 1 while bIn is true and towards 0 while it is false, re-rendering every frame until settled. Flipping bIn mid-flight continues from the current progress, so a cancelled exit re-enters smoothly. Pairs naturally with UsePresence for exit animations.',
    example: `FRuiPresenceState P = UsePresence(Ctx);
const float T = UseAnimate(P.bPresent, 0.25f);

<Border RenderOpacity={ T }>
	<TextBlock Text="Slides in and out" />
</Border>`,
    gotchas: [
      'The slot primes at the first render\'s target — an enter animation needs the first render to see bIn = false, flipping to true afterwards.',
      'Re-renders every frame while animating; time comes from the host clock (deterministic under the mock host).',
    ],
  },
  {
    name: 'UseTweenValue',
    category: 'core',
    signature: 'T Value = UseTweenValue<T>(Target, DurationSec, Ease)',
    cppSignature: 'T FRuiContext::UseTweenValue<T>(const T& Target, float DurationSec = 0.25f, ERuiEase Ease = ERuiEase::InOut)',
    returns: 'the eased value, updated every frame until it settles at the target',
    description: 'The generic tween: animates the returned value towards Target, re-rendering every frame until settled via a self-re-arming frame chain. Retargeting mid-flight starts from the CURRENT value, so nothing snaps. The first render primes at the target, so there is no mount animation; time comes from the host clock and is deterministic under the mock host.',
    example: `const FLinearColor Tint = UseTweenValue<FLinearColor>(
	bAlert ? FLinearColor::Red : FLinearColor::White, 0.4f);

<Border BorderImage="WhiteBrush" BorderBackgroundColor={ Tint }>
	<TextBlock Text="Eases between colors" />
</Border>`,
    gotchas: [
      'T needs an RUI::LerpTween overload — float, FVector2D, and FLinearColor ship in the box.',
      'No mount animation: the first render primes at the target (use UseAnimate for enters).',
      'Re-renders every frame while animating — one coalesced re-render per frame.',
    ],
  },
  {
    name: 'UseSfx',
    category: 'core',
    signature: 'FRuiCallback Play = UseSfx(Bus)',
    cppSignature: 'FRuiCallback FRuiContext::UseSfx(FName Bus = NAME_None)',
    returns: 'a play-sound callback bound to the named bus',
    description: 'Returns a callback that fires a sound request on a named bus. Dispatch goes through the process-wide sfx sink registered with RUI::SetSfxSink — the game decides how a bus maps to audio assets and world context, keeping the UI code declarative. The FRuiValue payload handed to the callback rides through to the sink untouched.',
    example: `FRuiCallback PlayClick = UseSfx(TEXT("UI.Click"));

<Button OnClicked={ PlayClick.Execute() }>
	<TextBlock Text="Click" />
</Button>`,
    gotchas: [
      'An unset sink is a quiet no-op — sound is opt-in game wiring, never a UI dependency.',
      'The callback payload rides through to the sink, so one bus can carry per-event data.',
    ],
  },
  {
    name: 'UseSafeArea',
    category: 'core',
    signature: 'FRuiSafeArea Insets = UseSafeArea()',
    cppSignature: 'FRuiSafeArea FRuiContext::UseSafeArea()',
    returns: 'the device safe-area insets in pixels (Left, Top, Right, Bottom)',
    description: 'Reads the display safe-area insets so content can stay clear of notches and rounded corners. The values are host-supplied through an owner chain: a mock test value, then Slate\'s FDisplayMetrics, then a CommonUI override when that module is active. Returns an FRuiSafeArea struct of four floats in pixels.',
    example: `FRuiSafeArea Safe = UseSafeArea();

<Border Padding={ FMargin(Safe.Left, Safe.Top, Safe.Right, Safe.Bottom) }>
	<TextBlock Text="Inside the notch-safe region" />
</Border>`,
    gotchas: [
      'Values come from the host chain (mock test value, then Slate FDisplayMetrics, then a CommonUI override) and are in pixels.',
    ],
  },
  {
    name: 'UsePresence',
    category: 'core',
    signature: 'FRuiPresenceState P = UsePresence(Ctx)',
    cppSignature: 'FRuiPresenceState UsePresence(FRuiContext& Ctx)   // global free function, not a Ctx member',
    returns: '{ bPresent, NotifyDone } from the nearest <Presence> boundary',
    description: 'Reads the nearest <Presence> boundary\'s signal for this child, enabling exit animations. bPresent flips to false once the child has been removed from the parent\'s render but is being kept mounted to animate out; call NotifyDone when the exit has settled and the boundary performs the real unmount. Outside any <Presence> it returns { true, unbound no-op }, so it is always safe to call.',
    example: `FRuiPresenceState P = UsePresence(Ctx);
const float T = UseAnimate(P.bPresent, 0.2f);
UseEffect([=]()
{
	if (!P.bPresent && T <= 0.0f) { P.NotifyDone.Execute(); }
}, RUI::EveryCommit());

<Border RenderOpacity={ T }>
	<TextBlock Text="Animates out before unmounting" />
</Border>`,
    gotchas: [
      'Written with explicit Ctx (UsePresence(Ctx)) — it is a free function, not a Ctx member and not in RUI::.',
      'Presence children must be keyed; an unkeyed child falls back to its positional index with a dev warning.',
      'Never calling NotifyDone costs the animation, not a leak — MaxExitSeconds (default 2.0) force-unmounts the child.',
    ],
  },
]

export const ROUTER_HOOKS: HookEntry[] = [
  {
    name: 'UseNavigate',
    category: 'router',
    signature: 'auto Navigate = UseNavigate()',
    cppSignature: 'TFunction<void(const FString& To, bool bReplace)> UseNavigate(FRuiContext& Ctx)',
    returns: 'a navigate callable: Navigate(TEXT("/path"), /*bReplace*/ false)',
    description: 'Returns the imperative navigation function from the nearest Router. To may be absolute (/settings) or relative (resolved against the current pathname as a base directory); bReplace swaps the top history entry instead of pushing, and any push or replace clears the forward stack. A ?query#hash tail on To is resolved once and never doubled. The callable identity is excluded from context equality, so consumers re-render on location change only, not on every Router render.',
    example: `auto Navigate = UseNavigate();\n\n<Button OnClicked={ Navigate(TEXT("/users/42"), false) }>Open user 42</Button>`,
    gotchas: [
      'Outside a Router the returned callable is a safe no-op — nothing navigates and nothing warns; check UseInRouterContext if you need to know.',
      'An active UseBlocker vetoes the navigation entirely: the location does not change and the blocker\'s OnBlocked fires with the attempted href instead.',
    ],
  },
  {
    name: 'UseGo',
    category: 'router',
    signature: 'auto Go = UseGo()',
    cppSignature: 'TFunction<void(int32 Delta)> UseGo(FRuiContext& Ctx)',
    returns: 'a relative-history callable: Go(-1) = back, Go(1) = forward',
    description: 'Returns the relative history-motion function: Go(-1) steps back, Go(1) steps forward, larger deltas walk multiple entries. A completed Go marks the navigation type as Pop. Back/forward motion is subject to navigation blockers too — the back button cannot bypass a UseBlocker guard that Navigate honors (a bughunt locked this in): the immediate target is vetoed before any stack is touched.',
    example: `auto Go = UseGo();\n\n<Button OnClicked={ Go(-1) }>Back</Button>`,
    gotchas: [
      'Go is a silent no-op when the target stack is empty (nothing to go back/forward to) or when a blocker is active.',
      'Outside a Router the returned callable is a no-op.',
    ],
  },
  {
    name: 'UseBackStack',
    category: 'router',
    signature: 'auto [bCanGoBack, GoBack] = UseBackStack()',
    cppSignature: 'TTuple<bool, TFunction<void()>> UseBackStack(FRuiContext& Ctx)',
    returns: '{ bCanGoBack, GoBack() } — a bool plus a zero-argument back callable',
    description: 'The common back-button surface: a bool telling you whether there is anywhere to go back to, and a GoBack() callable that is equivalent to Go(-1). Use bCanGoBack to disable or hide the back button; the consumer re-renders when the back stack transitions between empty and non-empty, because bCanGoBack participates in the router context\'s equality.',
    example: `auto BackStack = UseBackStack();\n\n<Button bEnabled={ BackStack.Get<0>() } OnClicked={ BackStack.Get<1>()() }>Back</Button>`,
    gotchas: [
      'GoBack goes through the same blocker gate as Go(-1) — an active UseBlocker intercepts it.',
    ],
  },
  {
    name: 'UseLocation',
    category: 'router',
    signature: 'const FRuiLocation& Loc = UseLocation()',
    cppSignature: 'const FRuiLocation& UseLocation(FRuiContext& Ctx)',
    returns: 'a const reference to the current FRuiLocation (Pathname, Search, Hash, Key)',
    description: 'The current parsed location: Pathname (slash-normalized), Search (including the leading ?, or empty), Hash (including the leading #, or empty), plus a per-entry history Key. Routing is in-memory, so there is no origin/URL bar. Consumers re-render only when the location or navigation data actually changes — the navigation functions are excluded from the context value\'s equality.',
    example: `const FRuiLocation& Loc = UseLocation();\n\n<TextBlock Text={ Loc.ToHref() } />`,
    gotchas: [
      'Outside a Router you get the default location ("/" with empty search/hash), not an error.',
      'Location equality ignores Key — two entries with the same pathname/search/hash compare equal.',
    ],
  },
  {
    name: 'UsePathname',
    category: 'router',
    signature: 'const FString Path = UsePathname()',
    cppSignature: 'FString UsePathname(FRuiContext& Ctx)',
    returns: 'the current pathname as an FString (e.g. "/users/42")',
    description: 'Convenience over UseLocation: just the current pathname, slash-normalized (leading slash, no duplicate or trailing slashes). Re-renders the consumer on location change only, like every router read.',
    example: `const FString Path = UsePathname();\n\n<TextBlock Text={ Path } />`,
    gotchas: [],
  },
  {
    name: 'UseSearch',
    category: 'router',
    signature: 'const FString Search = UseSearch()',
    cppSignature: 'FString UseSearch(FRuiContext& Ctx)',
    returns: 'the current search string including the leading "?" (or empty)',
    description: 'Convenience over UseLocation: the current raw search string, with the leading ? included (empty when there is no query). Pair it with the pure helper RUI::ParseSearch to get a key-value map, or reach straight for UseSearchParams for the parsed read-write surface.',
    example: `const FString Search = UseSearch();\n\n<TextBlock Text={ Search } />`,
    gotchas: [],
  },
  {
    name: 'UseParams',
    category: 'router',
    signature: 'const TMap<FString, FString>& Params = UseParams()',
    cppSignature: 'const TMap<FString, FString>& UseParams(FRuiContext& Ctx)',
    returns: 'a const reference to the param map captured by the nearest matched route',
    description: 'The params captured by the nearest matched route: :name segments land under their name, a trailing * splat lands under the "*" key with the rest of the path joined by slashes. In a nested match, every level of the chain provides the same merged param map, so a layout route sees its children\'s params too.',
    example: `// route pattern: /users/:id\nconst TMap<FString, FString>& Params = UseParams();\n\n<TextBlock Text={ Params[TEXT("id")] } />`,
    gotchas: [
      'Outside a matched route (or outside Routes/UseRoutes entirely) the map is empty — indexing a missing key with operator[] on an empty map is on you; prefer Find.',
      'Matching is case-sensitive.',
    ],
  },
  {
    name: 'UseSearchParams',
    category: 'router',
    signature: 'auto [Params, SetParams] = UseSearchParams()',
    cppSignature: 'TTuple<TMap<FString, FString>, TFunction<void(TMap<FString, FString>)>> UseSearchParams(FRuiContext& Ctx)',
    returns: '[ params map, SetParams(NextMap) ] over the location\'s search string',
    description: 'The read-write surface over the location\'s query string: the current search parsed into a key-value map, plus a setter that rebuilds the search (stable, sorted key order) and navigates to the current pathname with it. SetParams PUSHES a new history entry — React Router\'s setSearchParams default — so going back restores the prior search state instead of losing it to an in-place replace (a bughunt locked this in). When parsing, the first value wins for a repeated key.',
    example: `auto SearchParams = UseSearchParams();\nTMap<FString, FString> Next = SearchParams.Get<0>();\nNext.Add(TEXT("tab"), TEXT("stats"));\n\n<Button OnClicked={ SearchParams.Get<1>()(Next) }>Stats tab</Button>`,
    gotchas: [
      'SetParams replaces the WHOLE search string with the map you pass — merge the current map yourself if you only want to change one key.',
      'SetParams navigates to pathname + search only; an existing #hash is dropped.',
      'Each SetParams call pushes a history entry — a slider bound to it will flood the back stack.',
    ],
  },
  {
    name: 'UseMatch',
    category: 'router',
    signature: 'FRuiPathMatch M = UseMatch(TEXT("/users/:id"))',
    cppSignature: 'FRuiPathMatch UseMatch(FRuiContext& Ctx, const FString& Pattern)',
    returns: 'an FRuiPathMatch: bMatched (also via explicit operator bool), Params, the matched Pathname',
    description: 'Matches Pattern against the current pathname and returns the full match result: whether it matched, the captured params, and the portion of the pathname that matched. The match is a FULL match (bEnd = true) — the pattern must consume the whole pathname, like a leaf route. Patterns support literals, :name params (one segment each), and a trailing * splat captured as the "*" param.',
    example: `FRuiPathMatch M = UseMatch(TEXT("/users/:id"));\n\n{ M ? <TextBlock Text={ M.Params[TEXT("id")] } /> : <TextBlock Text="no user" /> }`,
    gotchas: [
      'It is a whole-pathname match — use UseIsActive (default bEnd = false) if you want prefix semantics.',
      'Matching is case-sensitive and slash-normalized.',
    ],
  },
  {
    name: 'UseIsActive',
    category: 'router',
    signature: 'const bool bActive = UseIsActive(TEXT("/settings"))',
    cppSignature: 'bool UseIsActive(FRuiContext& Ctx, const FString& Pattern, bool bEnd = false)',
    returns: 'true when Pattern matches the current pathname',
    description: 'True when Pattern matches the current pathname — the NavLink active-state test. By default bEnd is false, so a PREFIX match counts: /settings is active while you are on /settings/audio. Pass bEnd = true to require the pattern to consume the whole pathname (exact-leaf semantics).',
    example: `const bool bActive = UseIsActive(TEXT("/settings"));\n\n<Button style:hue={ bActive ? Active : Idle } OnClicked={ Navigate(TEXT("/settings"), false) }>Settings</Button>`,
    gotchas: [
      'The default is prefix matching (bEnd = false) — the opposite default from UseMatch, which always matches the full pathname.',
    ],
  },
  {
    name: 'UseResolvedPath',
    category: 'router',
    signature: 'const FString Resolved = UseResolvedPath(TEXT("details"))',
    cppSignature: 'FString UseResolvedPath(FRuiContext& Ctx, const FString& To)',
    returns: 'the pathname To resolves to, as a normalized FString',
    description: 'Resolves To against the current location\'s pathname. An absolute To (leading /) wins as-is; a relative To appends onto the current pathname treated as a base directory — "details" from /users/42 resolves to /users/42/details (the current location\'s last segment is intentionally KEPT, not dropped). ".." pops a segment and "." is skipped; the result is slash-normalized.',
    example: `const FString Resolved = UseResolvedPath(TEXT("../inbox"));\n\n<TextBlock Text={ Resolved } />`,
    gotchas: [
      'Relative resolution anchors on the current pathname as a directory — it does NOT drop the last segment like a browser URL would; this is a locked, test-covered decision.',
      'Pass a pure path — resolution segments on "/" only; use UseHref when To carries a ?query or #hash.',
    ],
  },
  {
    name: 'UseHref',
    category: 'router',
    signature: 'const FString Href = UseHref(TEXT("details?tab=2"))',
    cppSignature: 'FString UseHref(FRuiContext& Ctx, const FString& To)',
    returns: 'the full href a link to To would point at: resolved pathname + ?search + #hash',
    description: 'The href a link to To would point at: the pathname part is resolved against the current location (same rules as UseResolvedPath) and the ?query#hash tail is re-attached once. Only the pathname is fed through resolution — a bughunt fixed a doubling where the tail was embedded into the resolved path AND appended again. Absolute To is just parsed and reassembled.',
    example: `const FString Href = UseHref(TEXT("details?tab=2"));\n\n<TextBlock Text={ Href } />`,
    gotchas: [],
  },
  {
    name: 'UseOutlet',
    category: 'router',
    signature: 'FRuiNode Child = UseOutlet()',
    cppSignature: 'FRuiNode UseOutlet(FRuiContext& Ctx)',
    returns: 'the nested matched child element as an FRuiNode (an empty Fragment when there is none)',
    description: 'The nested matched child element — a layout route\'s Element calls this to decide where its matched child renders. When the route chain has no deeper match (you are at the leaf, or the layout\'s own index position), it returns an empty Fragment, so embedding it unconditionally is always safe.',
    example: `export FRuiNode SettingsLayout() {\n\treturn (\n\t\t<VerticalBox>\n\t\t\t<TextBlock Text="Settings" />\n\t\t\t{ UseOutlet() }\n\t\t</VerticalBox>\n\t);\n}`,
    gotchas: [
      'Only meaningful inside an element rendered by Routes/UseRoutes — elsewhere the route context is the default and you get an empty Fragment.',
    ],
  },
  {
    name: 'UseRoutes',
    category: 'router',
    signature: 'FRuiNode Matched = UseRoutes(RouteList)',
    cppSignature: 'FRuiNode UseRoutes(FRuiContext& Ctx, const TArray<RUI::FRuiRoute>& RouteList)',
    returns: 'the rendered best-match element for the current location (an empty Fragment on no match)',
    description: 'Declarative routing as a hook: matches the current location against RouteList and returns the best match\'s element, with nesting wired so each matched layout renders its child at UseOutlet. Ranking prefers static segments over :params over * splats, longer patterns first, with bIndex marking a parent\'s default child. A layout route with leftover path that no child consumed does not match — a sibling catch-all * gets its turn instead (a bughunt fixed the shadowing). Returns an empty Fragment when nothing matches.',
    example: `TArray<RUI::FRuiRoute> RouteList;\nRouteList.Add(RUI::FRuiRoute{ TEXT("/"),          HomeScreen() });\nRouteList.Add(RUI::FRuiRoute{ TEXT("/users/:id"), UserScreen() });\n\n<Overlay>{ UseRoutes(RouteList) }</Overlay>`,
    gotchas: [
      'Must run under a Router — without one the location is the default "/" and only a route matching "/" can render.',
      'RUI::Routes(RouteList) is the node-shaped wrapper over this hook; use whichever fits the call site.',
    ],
  },
  {
    name: 'UseNavigationType',
    category: 'router',
    signature: 'ERuiNavigationType NavType = UseNavigationType()',
    cppSignature: 'ERuiNavigationType UseNavigationType(FRuiContext& Ctx)',
    returns: 'ERuiNavigationType::Push, ::Replace, or ::Pop',
    description: 'How the current location was reached — React Router\'s NavigationType. Push for a normal navigate, Replace when bReplace swapped the top entry, Pop when Go/GoBack walked the history. Useful for direction-aware screen transitions (slide left on Push, slide right on Pop).',
    example: `ERuiNavigationType NavType = UseNavigationType();\n\n<TextBlock Text={ NavType == ERuiNavigationType::Pop ? TEXT("went back") : TEXT("went forward") } />`,
    gotchas: [
      'The initial location reports Push (the enum default) — no navigation has happened yet.',
    ],
  },
  {
    name: 'UseInRouterContext',
    category: 'router',
    signature: 'const bool bRouted = UseInRouterContext()',
    cppSignature: 'bool UseInRouterContext(FRuiContext& Ctx)',
    returns: 'true when called inside a <Router>',
    description: 'True when the component renders under a Router. Every router hook degrades gracefully without one (default location, no-op navigate), so this is the explicit probe for components that must behave differently when routing is available — a shared widget that shows a back button only inside routed flows, for example.',
    example: `const bool bRouted = UseInRouterContext();\n\n{ bRouted ? <BackButton /> : <Fragment /> }`,
    gotchas: [],
  },
  {
    name: 'UseBlocker',
    category: 'router',
    signature: 'UseBlocker(bDirty, OnBlocked)',
    cppSignature: 'void UseBlocker(FRuiContext& Ctx, bool bBlock, TFunction<void(const FString& AttemptedHref)> OnBlocked)',
    returns: 'nothing — a fire-and-forget navigation guard',
    description: 'Registers a navigation blocker: while bBlock is true, every navigation is intercepted and OnBlocked fires with the attempted href instead of the location committing — the unsaved-changes guard. It blocks back/forward (POP) navigations too, not just Navigate; a bughunt fixed the back button bypassing the guard. The entry registers on mount and unregisters on unmount; bBlock and OnBlocked are refreshed in place every render, so flipping bBlock takes effect immediately without re-registering.',
    example: `auto [bDirty, SetDirty] = UseState(false);\nUseBlocker(bDirty, [](const FString& AttemptedHref) {\n\tUE_LOG(LogTemp, Display, TEXT("blocked nav to %s"), *AttemptedHref);\n});`,
    gotchas: [
      'The first ACTIVE blocker in the list wins: it fires its OnBlocked and vetoes the navigation; later blockers are not consulted for that attempt.',
      'There is no built-in "proceed anyway" token — to let the user continue, stash AttemptedHref, flip bBlock to false, and Navigate to it yourself.',
      'Outside a Router it is a no-op (there is no blocker list to join).',
    ],
  },
]
