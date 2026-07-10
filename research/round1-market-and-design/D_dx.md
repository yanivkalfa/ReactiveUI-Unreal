# ReactiveUI-Unreal — Developer-Experience Design

Core premise carried over from the Godot/Unity siblings: the reconciler core never touches concrete widgets directly — everything goes through a host seam (`FRuiSlateHost`, the Slate analogue of `addons/reactive_ui/core/host_config.gd`). That seam is what makes every DX option below pluggable.

## 1. Iteration-loop options, ranked

| Rank | Option | Round-trip | What breaks it | Verdict |
|---|---|---|---|---|
| 1 | **(c) Runtime-interpreted markup** | **<1 s, state-preserving, works mid-PIE and on device** | Can't express arbitrary C++ logic | **Primary dev loop** |
| 2 | **(b) Markup → codegen'd C++ (PreBuildSteps)** | 5–20 s via Live Coding *if* changes stay in `.cpp` bodies; minutes on header churn | New component/prop = new header = Live Coding's documented worst case; UE 5.8 header-edit recompile-loop regression | **Ship path, not dev loop** |
| 3 | **(a) Pure C++ + Live Coding** | 3–15 s for `.cpp` body edits | Any `UPROPERTY`/`UFUNCTION`/signature/header change → restart in practice; `.cpp`-constructor defaults never propagate; Object Reinstancing needs manual pointer cleanup or shutdown crashes | Baseline for *logic*; unacceptable as the *UI structure* loop (competing DX bar is Blueprint's 1–3 s + WYSIWYG) |
| 4 | **(e) UnrealSharp host** | Seconds-scale C# reload, vanilla engine | Pre-1.0, no tagged releases, Win/Mac only, jam-scale track record | Optional community adapter, Phase 3+. Never a foundation for a Fab-distributed plugin |
| 5 | **(d) AngelScript host** | Best-in-class (save → instant, mid-PIE) | Requires Hazelight's engine fork; fork is incompatible with pre-built binary plugins → kills Fab distribution outright | Optional adapter for fork-committed studios (Embark/Hazelight class), community-maintained |

**Recommended phased combination — "interpret in dev, compile to ship":**
- **Phase 1:** C++ core (fiber reconciler + hooks + `FRuiSlateHost`) with a `V.*`-style C++ factory API. Usable standalone; Live Coding covers logic-body iteration.
- **Phase 2 (the DX headline):** `.uetkx` markup, **interpreted at runtime** in editor/dev builds — file-watch → reparse → reconciler diff → sub-second state-preserving patch, exactly the loop Noesis/Gameface proved commercially.
- **Phase 3:** codegen backend — same `.uetkx` → plain C++ `Render()` for cooked builds (zero parse cost, compile-time prop typing). One source file, two backends, with a CI parity test that both produce identical trees.
- **Phase 4:** on-device remote reload (editor pushes specs over TCP); optional UnrealSharp/AngelScript adapters if demand materializes.

## 2. Markup path (`.uetkx`)

**Element names are abstract** (same as Godot's `V.*`): `Border/VBox/HBox/Text/Button/Image/ScrollBox/List…` mapped by the host config to `SBorder/SVerticalBox/…`, with an escape hatch `<slate:SConstraintCanvas>`. Abstract names keep files portable across the family and let the host swap Slate widget choices without breaking user markup.

**The two-backend constraint shapes the embedded language.** Unlike `.guitkx` (embedded GDScript runs natively), embedded raw C++ can't be interpreted. So markup expressions are a small **bindable expression DSL** (literals, prop/state paths, arithmetic/comparison, indexing, `fmt()`), and **imperative logic lives in a C++ logic class referenced by name** — the same split as the repo's `.hooks.guitkx` pattern and benui's `BindWidget` convention:

```text
@class_name ShopPanel
@logic UShopLogic            // C++ class hosting handlers

component ShopPanel(items: TArray<FShopItem>, gold: int32 = 0) {
	state selected: int32 = -1

	return (
		<Border style={ {"bg_color": "#1A140C", "padding": 8} }>
			<VBox>
				<Text text={ fmt("Gold: {0}", gold) } style={ {"font_size": 24, "color": "#F2C14E"} } />
				@for item in items key={item.Id}
					<Button text={item.Name} enabled={item.Cost <= gold}
					        onClick={ OnBuy(item.Id) } />
				@if selected >= 0
					<Text text={items[selected].Description} />
			</VBox>
		</Border>
	)
}
```

**Codegen output shape (Phase 3).** Deliberately **plain C++, zero UHT surface** — no `UCLASS`/`UPROPERTY` in generated files, so regeneration never triggers reflection/reinstancing, and Live Coding stays on its safe path:

```cpp
// ShopPanel.uetkx.g.h — shape-stable; regenerated only on prop-signature change
struct FShopPanelProps { TArray<FShopItem> Items; int32 Gold = 0; };
struct FShopPanel : FRuiComponent<FShopPanelProps> {
	FRuiNode Render(FRuiCtx& Ctx) override;   // body lives in .g.cpp
};

// ShopPanel.uetkx.g.cpp — where all expression edits land (Live Coding-friendly)
FRuiNode FShopPanel::Render(FRuiCtx& Ctx) {
	auto Selected = Ctx.UseState<int32>(-1);
	FRuiChildren Kids;
	Kids.Add(V::Text({ .Text = FText::Format(..., Props.Gold), .Style = {...} }));
	for (const FShopItem& Item : Props.Items)
		Kids.Add(V::Button({ .Key = Item.Id, .Text = Item.Name,
			.Enabled = Item.Cost <= Props.Gold,
			.OnClick = [this, Id = Item.Id]{ Logic->OnBuy(Id); } }));  // direct member bind
	if (Selected.Get() >= 0) Kids.Add(V::Text({...}));
	return V::Border({...}, MoveTemp(Kids));
}
```

Attr expressions compile to C++ expressions inside the body; handlers compile to lambdas capturing `this` and calling the logic class directly (type-checked). **UBT integration:** `PreBuildSteps` in the `.uplugin` invokes the compiler (unreal.hx/protoc precedent) with `$(ProjectDir)`/`$(PluginDir)` expansion. UHT constraint respected: UHT plugins (`*.ubtplugin.csproj`, `[UhtExporter]`) are *exporters only* — we don't generate reflected code through UHT; instead we use a UhtExporter to **dump reflection JSON** (available UFUNCTIONs, struct fields) into Intermediate, which the markup compiler + LSP consume to typecheck `@logic` handler names and prop paths offline. The existing TypeScript LSP/TextMate/VS Code+VS2022 stack is reused with a `uetkx` schema; codegen expertise transfers directly.

## 3. Runtime-interpreted path

**Tree spec.** Parser (C++, shared lexer grammar with the compiler) produces:

```cpp
struct FRuiExpr { /* parsed DSL AST: literal | path | binop | fmt | index */ };
struct FRuiNodeSpec {
	FName Element;                       // "Button", "slate:SBorder"
	TMap<FName, FRuiExpr> Attrs;         // incl. style dictionary
	FRuiExpr Key, If;                    // directives
	TOptional<FRuiForSpec> For;          // {ItemName, SourceExpr}
	TArray<FRuiNodeSpec> Children;
	TArray<FRuiHandlerRef> Handlers;     // {EventName, HandlerName, ArgExprs}
};
```

In-editor it lives inside `URuiMarkupAsset : UPrimaryDataAsset` (retains source text; binary-serialized spec for cook — cooked builds *can* still interpret, which is how device hot-tuning works, but codegen is the default ship path). Each render, the interpreter walks the spec evaluating `FRuiExpr` against a context (component props + hook state + the logic object via UE reflection) and emits the same `FRuiNode` vtree the codegen path emits — one reconciler downstream.

**Event handler binding: named registry via UFUNCTION.** The logic class is a normal `UObject`:

```cpp
UCLASS() class UShopLogic : public URuiLogic {
	GENERATED_BODY()
	UFUNCTION() void OnBuy(int32 ItemId);   // interpreted: FindFunction+ProcessEvent
};
```

Interpreted mode resolves `OnBuy` by `FindFunction(FName)` and invokes via `ProcessEvent` with DSL-evaluated args (typed against the reflection dump; mismatches are LSP diagnostics *and* runtime warnings). Non-UObject users get a `RUI_HANDLER(Name, Lambda)` manual-registry macro. Codegen mode binds the same names as direct calls — identical semantics, no reflection cost.

**Hot-reload mechanics.** Editor: `IDirectoryWatcher` (DirectoryWatcher module) on content roots for `*.uetkx`; on change → reparse → validate (parse errors go to Message Log, last-good spec stays live — never crash the session) → swap spec on the asset → `URuiSubsystem` marks mounted roots dirty → next-frame re-render. **State survives** because hook state lives in fibers keyed by position/key (per the family architecture), not in the spec — matching Noesis's state-preserving XAML diff. **PIE:** the watcher runs in the editor process, so the identical path works mid-play; a structural markup edit reconciles against live fibers. Standalone dev builds: the watcher module ships in non-editor dev targets, plus a `rui.reload` console command. Device (Phase 4): editor pushes serialized specs over a socket.

**What interpretation can't express** (documented honestly, mirrors README "Notes & limitations" style): arbitrary statements/loops beyond `@for`, closures, `match`-style dispatch, calls to arbitrary functions (only registered handlers + a curated pure-function whitelist like `fmt`/`clamp`/`lerp`), and new types. The rule of thumb shipped in docs: *structure, styling, simple conditions, and wiring live in markup; anything else is a one-line handler in C++* — which Live Coding patches in seconds since it's a `.cpp` body.

## 4. Debugging / devtools

- **ReactiveUI Inspector** (editor `SDockTab`, Widget Reflector precedent): live fiber tree; per-fiber props, hook slots (index, type, current value), render count, last-render µs; flash-on-rerender highlighting to make bailout failures visible; click-through selects the corresponding `SWidget` in Epic's Widget Reflector (Ctrl+Shift+W muscle memory) so users get Slate-level geometry/invalidation info for free.
- **Runtime overlay:** `rui.stats` (renders/frame, fibers, patch counts — the Doom demo's perf-overlay pattern) and `rui.DumpTree` to log. Optional **ImGui overlay module** with a soft dependency on the IDI-Systems ImGui fork (the community's revealed preference for in-game tooling) for shipping-build inspection.
- **Markup diagnostics:** parse/typecheck errors surface in the Message Log with file:line links that open the external editor; de-duplicated (the Godot plugin's append-only-Errors-dock lesson).

## 5. Testing story

Three layers, mirroring the Godot suite structure:
1. **Core unit tests — no Slate.** The host seam gets a `FRuiMockHost`; reconciler/hooks/keyed-diff/bailout specs run as Automation Specs (`BEGIN_DEFINE_SPEC`) — fast, deterministic, no RHI. This is the port of `core_test.gd`/`update_test.gd`.
2. **Slate integration.** Real `SWidget` trees under `FSlateApplication` with `-nullrhi` (Slate lays out headlessly without a GPU); assert widget types/order/props after mounts and patches.
3. **Markup + parity.** Golden-file tests: `.uetkx` → spec snapshot (interpreter) and `.uetkx` → generated C++ (codegen, compiled in CI); a **parity suite renders every example both ways and asserts identical vtrees** — the Unreal analogue of `demos_test.gd` and the guarantee behind "interpret in dev, compile to ship".

CI invocation: `UnrealEditor-Cmd.exe <Project> -ExecCmds="Automation RunTests ReactiveUI; Quit" -nullrhi -unattended -nosplash -log`, wrapped in UAT for cook-and-test on multiple engine versions.

## 6. First five minutes

Distribution is **Fab, pre-built binary, vanilla engine** (this is the decisive argument against AngelScript-as-foundation). Sequence:
1. Install from Fab → enable plugin → restart (once, ever).
2. Content Browser → right-click → *ReactiveUI → New Component* → creates `Hello.uetkx` from a template and offers "Mount in current level".
3. Mounting is one node or one line: drop the provided `BP_RuiCanvas` actor in the level, or `URuiSubsystem::Get(World)->Mount(TEXT("/Game/UI/Hello.uetkx"))`, or place a `RuiHost` widget inside any existing UMG UserWidget (interop with existing UI, not replacement).
4. Press PIE → UI appears. Edit `Hello.uetkx` in any editor → save → **UI updates in place mid-PIE**. That's the wow moment, and it requires **zero C++ compiles** because the README hello-world is markup-only, using built-in local state actions:

```text
component Hello {
	state count: int32 = 0
	return (
		<VBox style={ {"padding": 16} }>
			<Text text={ fmt("Count: {0}", count) } style={ {"font_size": 28} } />
			<Button text="+1" onClick={ set(count, count + 1) } />
		</VBox>
	)
}
```

README step 2 then introduces the `@logic` class and one `UFUNCTION` handler ("your logic hot-patches via Live Coding; your structure hot-reloads on save"), and step 3 points at the VS Code extension for completion/diagnostics. The pitch line writes itself: *Blueprint-speed iteration, text files you can diff* — directly answering the two validated pains (binary `.uasset` diffing and CreateWidget/ListView ceremony).

## OPEN QUESTIONS
- File extension and naming: is `.uetkx` the right family name (vs `.suitkx`/`.ruitkx`), and should the TextMate grammar/LSP schema be extracted into a shared package consumed by all three siblings (guitkx/uitkx/uetkx)?
- Expression DSL scope: exactly how large may the interpreted expression language grow before it becomes an accidental scripting language — and in codegen mode, is the DSL transpiled to C++ (exact parity, recommended) or replaced by verbatim C++ blocks (more power, breaks interpretation)?
- Generated components as plain C++ vs UCLASS: plain C++ maximizes Live Coding safety and avoids reinstancing, but forfeits Blueprint interop and GC-managed props — do we need a thin optional UCLASS wrapper layer for Blueprint-facing components?
- Lifetime/GC model at the seam: Slate is TSharedRef-based, logic classes are UObjects — what is the ownership contract (strong root set on the mount surface + TWeakObjectPtr in handlers?) to survive Object Reinstancing and level transitions without dangling pointers?
- Fab packaging reality: pre-built binaries must be produced per engine version (5.4–5.8?) — which versions to support at launch, and does the standalone markup compiler ship as C++ inside the plugin (needed for PreBuildSteps on all platforms) or reuse the existing Node toolchain as a dev-only dependency?
- UMG interop depth: is hosting Slate inside a `RuiHost` UMG widget enough, or do users need generated UWidget wrappers so ReactiveUI subtrees preview inside the UMG Designer at edit time (PreConstruct parity)?
- Should cooked/shipping builds retain the interpreter as an opt-in (live-ops UI tuning, the Gameface use case) or is codegen-only the shipped default with interpretation stripped?
- Priority and transport for on-device remote reload (Phase 4): editor-push over TCP vs re-cook-and-deploy of the DataAsset — is console-platform TCP policy a blocker?
- Does the UhtExporter reflection-dump land in Phase 2 (needed for interpreted-mode handler typechecking in the LSP) or can Phase 2 ship with runtime-only validation and defer the exporter to Phase 3?
- Hooks parity decisions: which of the 23 Godot hooks map 1:1, and do we keep the family's documented divergences (synchronous useTransition/useDeferredValue, structural error boundaries) given C++ also lacks catchable exceptions by UE convention?