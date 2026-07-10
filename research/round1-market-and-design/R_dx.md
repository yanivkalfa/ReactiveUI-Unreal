## 1. Live Coding (Live++) in UE 5.x

UE's Live Coding is built on **Live++** (Molecular Matters) binary patching, integrated since UE 4.22 and **enabled by default in UE5**; trigger with **Ctrl+Alt+F11** (works from editor or standalone). Windows-focused.

**Can patch reliably:** function-body changes in `.cpp` files, non-reflected code, small logic/value edits; documented as "safe to use during gameplay during Play In Editor."

**Cannot / risky:** Epic's docs say large-scale changes — "new functions, new sets of variables, or dramatic re-factors — will behave unpredictably… without Object Reinstancing." Community guidance (unrealcommunity.wiki, pome.cc): adding/removing `UPROPERTY`/`UFUNCTION`, changing signatures, editing headers scanned by UHT, and constructor/CDO changes require an editor restart in practice. Notable wart: default values set in a `.cpp` constructor do **not** update existing instances after a patch, but the same change in the `.h` does. **Object Reinstancing** (UE 5.x) extends coverage to structural changes on `UCLASS/UFUNCTION/USTRUCT/UENUM/UDELEGATE` types, but you must clean up raw pointers via `ReloadReinstancingCompleteDelegate`/`ReloadCompleteDelegate` or you get shutdown crashes.

**Reliability in practice:** persistent forum reports of crashes and Blueprint corruption (especially with Rider/VS-open workflows); a UE 5.8 bug report of an infinite recompile loop after any `.h` edit (CppDependencyCache partition error). Round-trip for a small `.cpp` change is a few seconds ("takes effect instantly upon recompilation" for a log-string change) versus minutes for close-editor → rebuild → reopen; patches are session-scoped memory patches, so the base binaries still need normal rebuilds.

**Live++ standalone pricing** (if licensing directly, not via UE): Individual €11.90/mo or €119/yr; Business €19.90/mo or €199/yr per seat (excl. VAT); console (Xbox/PS) pricing on request; MSVC/Clang/LLD; 30-day trial.

## 2. Hot Reload (legacy) and Blueprint-only iteration

**Hot Reload** (pre-4.22 path, still the fallback when Live Coding is off) recompiles module DLLs and swaps them in a running editor. It is explicitly labeled a **legacy feature** and is notorious for **Blueprint corruption**: `HOTRELOADED_` ghost classes, property values reset to C++ defaults, corrupted pins, invalid pointers — corruption is baked in if you save assets after a bad reload. It never updates CDOs retroactively. Community consensus: don't use it.

**Blueprint-only iteration** is the DX bar inside UE: in-editor compile in ~1–3 s, no process restart, and UMG Designer gives WYSIWYG preview (with `PreConstruct` running at edit time). Any C++ plugin promising "React-like DX" is competing with this, not with raw C++.

## 3. UnrealEngine-Angelscript (Hazelight fork)

"A set of engine modifications and a plugin" for UE5 — **not installable into stock UE**: you must build the engine from Hazelight's fork (GitHub access requires an Epic-linked account; the repo 404s otherwise), and the fork "is not compatible with pre-built binary plugins" — every plugin must be source-built. That is the adoption barrier.

**Production pedigree:** used daily by 30+ devs at Hazelight since early 2018; *It Takes Two* and *Split Fiction* shipped with the majority of gameplay in Angelscript — Split Fiction contains **1.7M+ lines of Angelscript across 16,000+ files**. **Embark Studios** (*THE FINALS* 2023, *ARC Raiders* 2025, TGA Best Multiplayer) also builds gameplay in C++, Blueprints, and Angelscript.

**Hot reload story (best-in-class):** script changes reflect **immediately on save**, no editor restart; during PIE, **non-structural changes reload without exiting the play session**. VS Code LSP extension with completion, rename, semantic highlighting, and a full debug adapter (breakpoints, inspection).

**Third-party plugin APIs:** yes, automatically — "If it can be used from Blueprint, it should be usable from Angelscript." `UCLASS` with `BlueprintType`/any `BlueprintCallable` function auto-binds; properties with `BlueprintReadWrite/ReadOnly` or `EditAnywhere` bind; script-only exposure via `ScriptCallable`/`ScriptReadWrite`/`ScriptReadOnly`; opt-out via `NotInAngelscript`; manual "script mixin libraries" for advanced bindings. Caveats: Hazelight guarantees no support ("have an engine programmer available"); tested on Windows/PS5/Xbox Series. A community re-packaging as a vanilla-engine plugin exists (WillGordon9999/UE-Angelscript) but has ~18 stars and stalled (last push Mar 2025). Relevant precedent: Hazelight's **EmmsUI** — immediate-mode UMG UI written in Angelscript.

## 4. UnrealSharp (C#)

Plugin (vanilla engine — no fork) enabling C# with **.NET 10** (needs 10.0.5+); supports **UE 5.6–5.8**; MIT; **1,872 GitHub stars** (created Nov 2023; last push Jul 2026); **no tagged releases** — you build from source; active Discord. **Hot reload:** recompile and reload C# assemblies without restarting the editor (collectible AssemblyLoadContext pattern). **Plugin interop:** the C# API is **auto-generated from all reflected C++** — engine, plugins, and project code — so a third-party plugin's Blueprint-exposed API is automatically usable. Platforms: Windows/Mac work; iOS/Android/Linux "planned." Maturity: pre-1.0; showcased projects are game-jam scale (Mini Jam 174, Epic MegaJam 2025 "Slime Guzzler"); no shipped commercial titles cited.

## 5. Verse in mainline UE (mid-2026 status)

**Not usable outside UEFN today.** Epic's official "Road to UE6" (June 2025) and State of Unreal 2026 (June 2026): UE6 merges UE5 + UEFN; the gameplay programming model moves to **Verse** with a new **Scene Graph** framework (replacing Actors/Components, built on Verse + software transactional memory). **UE6 Early Access is targeted for end of 2027**, full release 12–18 months later; Blueprints/Actors remain in early UE6 and get deprecated "eventually" with promised conversion tools. The Verse language implementation is now visible on GitHub but Epic says it is "**not intended for general adoption at this point**." Nothing shipped in UE 5.6–5.8 lets a plugin host Verse gameplay scripting.

## 6. Data-driven UI in UE

- **Native pattern** (Epic tech blog "Creating a Data-Driven UI with UMG"): DataTables (direct **JSON/CSV import**) drive runtime widget construction — a master widget reads rows (`Get Data Table Row`) and `CreateWidget`s an item-row widget per entry into a ScrollBox; `PreConstruct` gives design-time preview. Designers retune without touching UI.
- **UMG Viewmodel** (`ModelViewViewModel` plugin): Epic's MVVM, **Beta since UE 5.1**, field-notify bindings; designers restructure widgets without code changes. Community alternatives: DoubleDeez/MDViewModel, druhasu/UnrealMvvm.
- **Middleware proving the markup+hot-reload loop is commercially established:** **NoesisGUI** (XAML → GPU-rendered UI; hot reload diffs the XAML and patches only changed properties/controls **preserving UI state**; Indie license **€195/project** under €100K revenue/€250K budget; Pro €9,000 first platform + €3,600/platform + mandatory €3,200/yr support) and **Coherent Gameface** (HTML5/JS UI with Chrome DevTools live DOM editing; custom-quoted pricing; used in PUBG-class titles). Remote-reload UI tuning in shipped games is typically done via these web/XAML layers or in-house JSON UI definitions reloaded at runtime.

## 7. Custom build-step codegen in plugins

Two sanctioned extension points:
- **`PreBuildSteps`/`PostBuildSteps`** in `.uplugin`/`.uproject`/`.Target.cs`: shell commands run before compile, with variable expansion (`$(PluginDir)`, `$(EngineDir)`, `$(ProjectDir)`, `$(TargetName)`, `$(TargetPlatform)`, `$(TargetConfiguration)`); UBT writes them as .bat/.sh into `Intermediate/Build/...`. Precedent: **unreal.hx** (Haxe) drives its codegen from `PreBuildSteps` in its `.uplugin`; protobuf plugins invoke `protoc` this way (e.g., JGameEngine/UE-Protobuf-Plugin).
- **UHT plugins** (documented for UE 5.3): a C# project named `*.ubtplugin.csproj` is auto-detected by UBT; class tagged `[UnrealHeaderTool]`, entry point `[UhtExporter(Name=…, ModuleName=…, Options=…)]`, referencing `EpicGames.UHT`; you get the fully parsed reflection model (`factory.Session`) and can emit generated C++/JSON into Intermediate every build (`UhtExporterOptions.Default`). Limitation: only exporters — no script generators via plugin yet.
So DSL→`.h/.cpp` pre-build is well-trodden; the catch is downstream: newly generated headers hit Live Coding's weakest path (topic 1).

## 8. Realistic "edit markup → see UI" loops

- **C++ + Live Coding:** edit DSL → codegen `.h/.cpp` → Ctrl+Alt+F11 → ~3–15 s patch — but only if codegen keeps changes inside `.cpp` bodies. Any new component type/prop that emits a new class/`UPROPERTY` triggers the header/reinstancing path: unpredictable patches or editor restart. Worst case for a markup compiler that regenerates headers constantly.
- **Data-driven (interpret markup at runtime):** file-watch the markup, reparse, diff, patch the live UMG/Slate tree — **sub-second, state-preserving, zero compile**, works in PIE and on device (Noesis XAML and Gameface HTML both ship exactly this loop). Strongest fit for a React-style plugin: keep a stable C++ core (reconciler + host bindings, the equivalent of this repo's `RUIHost` seam) and treat markup as hot-reloadable data, with optional UHT/PreBuildSteps codegen only for shipping-time typed bindings.
- **Script-language host:** Angelscript gives save→instant reload even mid-PIE with auto-bound plugin APIs — the best iteration in the UE ecosystem — but forces every user onto a custom engine build. UnrealSharp gives seconds-scale C# hot reload on vanilla UE with auto-generated plugin bindings, but is pre-release (no tagged releases, Win/Mac only). Verse is not an option before UE6 EA (end of 2027).

## KEY FACTS
- UE Live Coding (Live++-based, default-on in UE5, Ctrl+Alt+F11) reliably patches .cpp function bodies; Epic docs say new functions/variables/refactors 'behave unpredictably' without Object Reinstancing, and header/UPROPERTY/UFUNCTION/signature changes require an editor restart in practice.
- Object Reinstancing covers UCLASS/UFUNCTION/USTRUCT/UENUM/UDELEGATE types but requires manual pointer cleanup via ReloadReinstancingCompleteDelegate/ReloadCompleteDelegate or the editor crashes at shutdown; constructor-set defaults in .cpp never propagate to existing instances after a patch.
- Legacy Hot Reload is a deprecated fallback known for Blueprint corruption (HOTRELOADED_ classes, reset property values); community advice is to avoid it entirely; Blueprint/UMG-Designer iteration (~1-3 s compile, WYSIWYG preview) is the in-engine DX baseline.
- Live++ standalone licensing: Individual EUR 119/yr, Business EUR 199/yr per seat (excl. VAT), console pricing on request; MSVC/Clang/LLD on Windows/Xbox/PS.
- UnrealEngine-Angelscript requires building Epic-source-access custom engine from Hazelight's fork and is incompatible with pre-built binary plugins; it hot-reloads scripts on save, including non-structural changes during PIE without exiting the session.
- Angelscript shipped It Takes Two and Split Fiction (1.7M+ lines of Angelscript in 16,000+ files) and is used by Embark Studios in THE FINALS (2023) and ARC Raiders (2025).
- Angelscript auto-binds any Blueprint-exposed C++ (BlueprintType/BlueprintCallable/BlueprintReadWrite), with ScriptCallable/ScriptReadWrite for script-only exposure and NotInAngelscript to opt out — so third-party plugin APIs are automatically available if the plugin is source-built against the fork.
- UnrealSharp: MIT, 1,872 GitHub stars (July 2026), .NET 10, UE 5.6-5.8, C# hot reload without editor restart, C# API auto-generated from all reflected C++ including plugins; no tagged releases, Windows/Mac only, jam-scale showcase projects.
- Verse remains UEFN-only as of mid-2026; UE6 (Verse + Scene Graph replacing Actors/Blueprints) targets Early Access end of 2027 with full release 12-18 months later; Epic published the Verse implementation on GitHub but says it is 'not intended for general adoption at this point'.
- NoesisGUI ships XAML-with-hot-reload UI for UE (diffs XAML and patches only changed properties, preserving UI state); Indie license EUR 195/project (<EUR 100K revenue), Pro EUR 9,000 first platform + mandatory EUR 3,200/yr support.
- Coherent Gameface provides HTML5/JS game UI for UE with live iteration via embedded Chrome DevTools; pricing is custom-quoted; used in titles like PUBG.
- Epic's UMG Viewmodel (ModelViewViewModel) MVVM plugin has been Beta since UE 5.1; DataTables import JSON/CSV directly and drive runtime CreateWidget-based data-driven UI (Epic tech blog pattern).
- .uplugin/.uproject/.Target.cs support PreBuildSteps shell commands with $(PluginDir)/$(EngineDir)/$(TargetConfiguration) expansion (precedent: unreal.hx Haxe codegen, protobuf plugins invoking protoc).
- UHT is extensible via C# projects named *.ubtplugin.csproj with [UnrealHeaderTool]/[UhtExporter] attributes (documented UE 5.3): full access to parsed reflection data, generates files into Intermediate each build; only exporters supported, not script generators.
- UE 5.8 has reported Live Coding regressions including an infinite recompile loop after any header edit (CppDependencyCache partition error), reinforcing that generated-header churn is the worst case for a codegen-based markup workflow.

## SOURCES
https://dev.epicgames.com/documentation/en-us/unreal-engine/using-live-coding-to-recompile-unreal-engine-applications-at-runtime
https://unrealcommunity.wiki/live-compiling-in-unreal-projects-tp14jcgs
https://www.pome.cc/en/tips/livecoding/
https://forums.unrealengine.com/t/live-coding-enters-infinite-recompile-loop-after-header-h-modification-due-to-cppdependencycache-partition-error-in-ue-5-8/2734201
https://liveplusplus.tech/
https://liveplusplus.tech/pricing.html
https://angelscript.hazelight.se/
https://angelscript.hazelight.se/project/development-status/
https://angelscript.hazelight.se/getting-started/installation/
https://angelscript.hazelight.se/cpp-bindings/automatic-bindings/
https://angelscript.hazelight.se/project/resources/
https://github.com/Hazelight/EmmsUI
https://github.com/WillGordon9999/UE-Angelscript
https://careers.embark-studios.com/pages/gameplay-at-embark
https://www.unrealengine.com/developer-interviews/embark-studios-build-the-award-winning-arc-raiders-with-unreal-engine
https://github.com/UnrealSharp/UnrealSharp
https://www.unrealsharp.com/
https://www.unrealengine.com/news/the-road-to-ue-6
https://www.unrealengine.com/news/state-of-unreal-2026-top-news-from-the-show
https://80.lv/articles/state-of-unreal-ue6-reactions-hype-skepticism-and-what-it-means-for-game-devs
https://www.gamedeveloper.com/programming/unreal-engine-6-will-merge-ue5-and-uefn-into-a-single-unified-engine-
https://www.noesisengine.com/licensing.php
https://www.noesisengine.com/docs/Gui.Core.HotReloadTutorial.html
https://coherent-labs.com/products/coherent-gameface/
https://www.unrealengine.com/en-US/tech-blog/creating-a-data-driven-ui-with-umg
https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-viewmodel-for-unreal-engine
https://github.com/DoubleDeez/MDViewModel
https://unrealist.org/uht-plugins/
https://github.com/proletariatgames/unreal.hx/blob/development/UnrealHx.uplugin
https://ikrima.dev/ue4guide/build-guide/plugins-modules/uprojectuplugin-descriptor/
https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-build-tool-target-reference?lang=en-US
https://github.com/JGameEngine/UE-Protobuf-Plugin