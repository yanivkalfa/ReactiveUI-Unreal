## Competition for declarative/React-style UI in Unreal Engine

### 1. React-UMG (ncsoft) — post-mortem
**Architecture:** `ncsoft/React-UMG` is a custom React renderer (react-reconciler host config in JavaScript) that emits UMG widgets through **Unreal.js**, NCSoft's V8-embedding plugin (3.8k stars, BSD-3). Web-ish tags map to UMG: `div`→`UVerticalBox`, `span`→`UHorizontalBox`, `text`→`UTextBlock`, `img`→`UImage`, `input`→`EditableText`; arbitrary UMG classes via a `u` prefix (`<uEditableTextBox/>`). Distributed as npm `react-umg` (v0.2.6); Babel for JSX. It's a fork of drywolf/react-umg (66 stars, 2 commits, self-described "very early proof-of-concept").
**Why it stalled:** (a) the whole stack hangs off Unreal.js, whose upstream effectively froze — last GitHub release 2016, marketplace support ends at UE 5.1; (b) React-UMG's last release was **v0.26, Dec 20 2022** (297 stars, 38 forks), issues from 2019 ("How to debug this stuff?", "Will typescript's version be released?") sit unanswered; (c) it inherited UMG's retained-widget semantics with no styling layer, so it solved component composition but not styling/animation. **getnamo's fork** (`getnamo/UnrealJs`, 75 stars) revived the runtime — **v2.0.0 (May 29, 2026) targets UE 5.7 and upgrades V8 from 7.4.288 to 14.6.202** (Win64 validated; Linux/Android WIP) — but it's positioned for "live gameplay coding," not UI; no React-UMG revival.

### 2. Coherent Gameface — the AAA incumbent
**Model:** proprietary middleware = **Cohtml** (from-scratch HTML5/CSS engine, task-based multithreaded layout) + **Renoir** (command-buffer GPU renderer; procedural rendering, geometry caching). JS: **V8 on JIT-allowed platforms (Windows/Android), JavaScriptCore on iOS** (per Coherent docs). Standard web frameworks run on it (React, Preact, jQuery); Coherent ships its own **SolidJS-based component kit (Gameface-UI / GameUIComponents)**. UE integration is deep: `UCohtmlWidget`, `UCohtmlComponent`, `UCohtmlHUD`, `ACohtmlGameHUD`, `SCohtmlWidget` (Slate), UMG hosting, and a data-binding layer (`UCohtmlJSEvent`, `cohtml::UEFunctionEventHandler`, TArray/TMap binding). Docs cover texture atlasing and an editor statistics window showing per-atlas memory footprint — evidence that draw-call/atlas/memory management is a real operational concern users must tune.
**Pricing reality:** no public prices — project-based licensing, "contact sales," 30-day full trial, first-year support included. Coherent killed its old indie subscription model (the "no longer provide subscriptions for Coherent UI" post) and says it responds to "indie requests for special prices" — i.e., practically inaccessible to hobbyists.
**Shipped games (from coherent-labs.com):** Borderlands 4, Marvel's Spider-Man 2, Minecraft, Civilization 7, Alan Wake 2, Control, PUBG, Sea of Thieves, Microsoft Flight Simulator, World of Tanks, World of Warships, Ratchet & Clank: Rift Apart, F1 Manager 2024, Black Desert Online, Planet Coaster 2, Tiny Tina's Wonderlands, Star Atlas. Claims "under 1 ms/frame on a standard PS4."
**Criticisms:** community threads (Epic forums "UMG vs Web Browser Widget", GameDev.net) flag that a full HTML/JS engine is heavyweight — memory overhead, hidden rendering costs, font/localization pain, and needing web engineers rather than UMG artists. Coherent's own perf docs concede caches trade "higher memory usage" for speed.

### 3. NoesisGUI — the XAML incumbent
**Model:** vector-rendered **WPF/XAML re-implementation in C++** with first-class **MVVM data binding**; author in Blend for Visual Studio / Noesis Studio. UE plugin (`Noesis/UnrealPlugin`, 356 stars, 32 releases; **latest 3.2.13_UE5.7, Apr 27 2026**) makes XAMLs native UE assets that consume UE Textures/Fonts/Sounds, binds XAML to **Blueprint functions and C++** (ViewModels can be Blueprints), renders to `WidgetComponent` for 3D UI; modules: `NoesisRuntime`, `NoesisEditor`, `NoesisBlueprint`. Samples include a modified ShooterGame.
**Pricing (noesisengine.com/licensing.php):** **Indie €195/project** (<€100K annual revenue, <€250K project budget), royalty-free, perpetual, all platforms incl. consoles; **Pro €9,000 first platform + €3,600/additional** (support €3,200/yr mandatory year one); **Premium €18,000 + €7,200** (<€12M budget); custom above.
**Adoption:** Baldur's Gate 3 (Noesis 3.1.6), plus customers cited: Ninja Theory, Microsoft, Riot, Capcom — "hundreds of games," though flagship wins are mostly custom-engine, not UE. Criticisms from forum users: XAML/Blend learning curve is alien to UE artists, class duplication for editor experience, beta-stage crashes in Noesis Studio's UE workflow.

### 4. ImGui plugins — debug-only niche
`segross/UnrealImGui` (759 stars, 260 forks) embeds Dear ImGui (v1.74 at plugin 1.22), targets UE 4.26-era, maintainer explicitly "busy… changes arrive slowly"; per-PIE-world contexts, NetImgui branch experimental. Successors: `VesCodes/ImGui`, `benui-dev` fork, and especially **`arnaud-jamin/Cog` (598 stars, UE 5.5+, 40+ debug windows** — Abilities, Behavior Trees, Net Emulation, Plots — **"disabled by default in shipping builds"**). Immediate-mode is declarative-ish but the ecosystem self-limits to tooling: no styling/animation/gamepad-navigation/localization stack, programmer-art look, and IMGUI redraw cost — so it proves programmers want code-driven UI, while serving zero production-HUD demand.

### 5. Other declarative attempts
- **ReactorUMG** (`Caleb196x/ReactorUMG`, 41 stars, MIT, alpha; official mirror `puerts/ReactUMG`): React+TypeScript→UMG on **Tencent's PuerTS** runtime; hot reload, live preview, editor-UI focus; README admits it "cannot fully meet the presentation requirements of production-level game UIs"; roadmap: CSS subset, Tailwind. The spiritual successor to React-UMG, still pre-production.
- **RNUE (rnue.dev)** — **React Native as an out-of-tree platform rendering to native Slate**, presented by Danny Povolotski (Engineering Director, **Star Atlas**) at React Advanced, Dec 1 2025. Fast Refresh + React DevTools + npm ecosystem, native Slate output (no web engine). **Closed early-access; GitHub release "in the next few months"** — the most credible new entrant, validating "React DX, native rendering" (exactly the ReactiveUI thesis, in UE).
- **FlutterUnreal** (`BlzFans/FlutterUnreal`, 109 stars, v1.1.2 Jan 2 2025): embeds the Flutter engine (D3D11/12, Vulkan; UE 4.26–5.4, Windows only; Dart/Lua/TypeScript). Niche curiosity.
- **CEF/web-view family:** Tracer Interactive **WebUI** (Fab; CEF 128 / Chromium 128, accelerated paint "4K 60FPS", JSON Blueprint interop), BLUI (`getnamo/BLUI-Unreal`), HTML Menus Browser Plugin, UCefView — full-browser overhead, no engine-native widgets.
- **Lightweight HTML:** RmlUi has no maintained UE integration (only an abandoned Chinese RmlUE4); Ultralight has only community `JailbreakPapa/UltralightUE` (UE 5.3+, Windows-only, docs WIP).
- **Slate itself** is declarative C++ (`SNew`, `SLATE_BEGIN_ARGS`) but retained, verbose, and editor-oriented; no popular React-style wrapper over Slate exists on GitHub.
- **Epic's own direction:** UMG **ViewModel (MVVM) plugin** — introduced UE 5.1, still Beta through 5.8 but widely used; plus community `DoubleDeez/MDViewModel`. Epic is betting on MVVM binding inside the Designer, not code-first declarative trees.
- **Fab UI offerings** are widget packs (Enhanced UI) or browsers — no declarative framework category exists on Fab.

### 6. UnrealSharp
`UnrealSharp/UnrealSharp` (~1.6k stars, MIT, .NET 10, hot reload, Discord ~858 members) auto-generates C# bindings from UE reflection — so you *can* subclass `UUserWidget` and drive UMG from C#, but **there is no declarative UI layer**: no XAML, no MVVM toolkit, no JSX-alike. Its UI story is "imperative UMG in C#." A C# declarative layer over UMG (the ReactiveUIToolKit model) is an open slot in that ecosystem.

### 7. What it proves / open niche
Demand signals: React-UMG got ~300 stars on a PoC; Tencent built PuerTS ReactUMG; Star Atlas is building RNUE in 2025-26; Coherent's entire business is "web devs make game UI" with a AAA client list. Persistent gaps every entrant leaves open: (a) **script-runtime tax** — every JS/Flutter option drags a VM (V8, Hermes, Dart) into the build with packaging/console pain; (b) **middleware pricing/opacity** (Gameface) or license friction (Noesis Pro €9k+); (c) **styling story** — native-widget renderers (React-UMG, ReactorUMG) die on the lack of CSS-like styling over UMG/Slate; (d) **abandonment risk** — everything OSS here is 1-maintainer. The proven-but-unserved niche is exactly what a first-party-language, zero-VM, native-widget declarative framework fills: for UE that would be "React semantics in C++/Verse over Slate with a real style system"; nobody occupies it — RNUE is closest but carries the React Native runtime. This mirrors the position ReactiveUI-Godot holds in Godot (plain GDScript, no VM, native Controls).

## KEY FACTS
- ncsoft/React-UMG (297 stars) is a React renderer over Unreal.js/V8 mapping JSX tags to UMG widgets (div->UVerticalBox, u-prefix for raw widgets); last release v0.26 on Dec 20 2022, npm react-umg 0.2.6, open issues from 2019 unanswered — effectively dead.
- Upstream ncsoft/Unreal.js (3.8k stars) froze at UE 5.1 marketplace support; getnamo/UnrealJs fork v2.0.0 (May 29 2026) revived it for UE 5.7 with V8 14.6.202, but for gameplay scripting, not UI.
- Coherent Gameface = proprietary Cohtml HTML engine + Renoir GPU renderer; V8 on JIT-allowed platforms, JavaScriptCore on iOS; UE API surface includes UCohtmlWidget, UCohtmlHUD, SCohtmlWidget and TArray/TMap data binding.
- Gameface has no public pricing (project-based, contact-sales, 30-day trial); it killed its old indie subscription; client list includes Borderlands 4, Spider-Man 2, Civilization 7, Alan Wake 2, PUBG, Sea of Thieves, Microsoft Flight Simulator, Minecraft.
- NoesisGUI licensing: Indie EUR 195/project (<EUR 100K revenue), Pro EUR 9,000 first platform + EUR 3,600/additional with mandatory EUR 3,200 first-year support, Premium EUR 18,000; royalty-free perpetual, consoles included.
- Noesis UnrealPlugin (356 stars) latest release 3.2.13_UE5.7 (Apr 27 2026): XAML as native UE assets, MVVM binding to Blueprints/C++, NoesisRuntime/NoesisEditor/NoesisBlueprint modules; flagship shipped title Baldur's Gate 3 (Noesis 3.1.6, custom engine not UE).
- ImGui ecosystem: segross/UnrealImGui 759 stars (stalled, ImGui 1.74, UE 4.26 era); arnaud-jamin/Cog 598 stars, UE 5.5+, 40+ debug windows, explicitly disabled in shipping builds — debug-only niche, never production UI.
- ReactorUMG (Caleb196x, 41 stars, alpha, MIT) is the modern React+TypeScript->UMG attempt built on Tencent's PuerTS; README admits it cannot yet meet production game-UI requirements and recommends editor-tool use.
- RNUE (rnue.dev): React Native as an out-of-tree platform rendering directly to native Slate, built by Star Atlas's Engineering Director Danny Povolotski, presented at React Advanced Dec 1 2025; closed early access, GitHub release promised within months.
- FlutterUnreal (BlzFans, 109 stars, v1.1.2 Jan 2025) embeds the Flutter engine in UE 4.26-5.4 (Windows only, D3D11/12/Vulkan) for Dart/Lua/TypeScript UI.
- CEF-based options (Tracer Interactive WebUI on Fab with CEF/Chromium 128, BLUI, HTML Menus) provide HTML UI via a full browser; RmlUi and Ultralight have no maintained first-party UE integrations.
- Epic's own answer is the UMG ViewModel MVVM plugin: introduced UE 5.1, still Beta through UE 5.8, binding-in-Designer rather than code-first declarative trees.
- UnrealSharp (~1.6k stars, .NET 10, hot reload, MIT) auto-generates C# bindings from UE reflection, so UMG is drivable imperatively from C#, but it ships no declarative UI layer (no XAML/MVVM/JSX equivalent).
- No React-style wrapper over Slate with meaningful adoption exists on GitHub; Fab has no declarative-UI-framework category — only widget packs and embedded browsers.
- Common gaps across all entrants: embedded VM/runtime tax (V8/Dart/CEF), opaque or expensive licensing (Gameface, Noesis Pro), missing CSS-like styling over native widgets, and single-maintainer abandonment risk.

## SOURCES
https://github.com/ncsoft/React-UMG
https://github.com/ncsoft/React-UMG/issues
https://github.com/drywolf/react-umg
https://github.com/ncsoft/Unreal.js
https://github.com/getnamo/UnrealJs
https://coherent-labs.com/products/coherent-gameface/
https://coherent-labs.com/frequently-asked-questions/
https://coherent-labs.com/posts/coherent-labs-will-no-longer-provide-subscriptions-for-coherent-ui/
https://docs.coherent-labs.com/cpp-gameface/what_is_gfp/overview/
https://docs.coherent-labs.com/cpp-gameface/integration/rendering/
https://docs.coherent-labs.com/unreal-gameface/overview/
https://coherent-labs.com/Documentation/cpp-hb/df/d01/javascript_virtual_machine.html
https://github.com/CoherentLabs/Gameface-UI/
https://www.noesisengine.com/licensing.php
https://github.com/Noesis/UnrealPlugin
https://www.gamedev.net/news/noesis-technologies-rolls-out-30-ui-tool-used-in-baldurs-gate-iii-r1347/
https://forums.unrealengine.com/t/noesisgui-user-interface/107886
https://github.com/segross/UnrealImGui
https://github.com/arnaud-jamin/Cog
https://github.com/Caleb196x/ReactorUMG
https://github.com/puerts/ReactUMG
https://gitnation.com/contents/react-native-as-a-gaming-ui-system-in-unreal-engine
https://rnue.dev
https://github.com/BlzFans/FlutterUnreal
https://tracerinteractive.com/plugins/webui
https://www.unrealengine.com/marketplace/en-US/product/web-ui
https://github.com/JailbreakPapa/UltralightUE
https://github.com/mikke89/RmlUi/discussions/186
https://forums.unrealengine.com/t/umg-vs-web-browser-widget-html-css-and-js-to-make-a-ui/2668224
https://dev.epicgames.com/documentation/unreal-engine/umg-viewmodel-for-unreal-engine?lang=en-US
https://github.com/UnrealSharp/UnrealSharp
https://www.unrealsharp.com/
https://github.com/DoubleDeez/MDViewModel