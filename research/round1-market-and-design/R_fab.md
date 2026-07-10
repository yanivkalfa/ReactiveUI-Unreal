## 1. Fab marketplace: submission, review, pricing, revenue share

Fab launched Oct 22, 2024 as the successor to the UE Marketplace (also absorbing Sketchfab, ArtStation Marketplace, Quixel). Revenue share is **88/12 in the seller's favor** — carried over from the 2018 Marketplace change (Epic even applied it retroactively to 2014). Payouts issue at a **$100 minimum threshold**, monthly, via wire/PayPal. A seller retrospective (StraySpark, 2026 — unverified blog, but its headline figure matches Epic's official post) notes real take-home on a $49.99 sale lands ~$40.50–42.80 after payment fees/FX, and refund rates run 3–5%.

**Code-plugin submission**: "Code Plugins must contain at least one code module." You submit **source, not binaries** ("Unreal Engine-generated Binaries and Intermediate files are not required") via a hosted zip link; **Epic's toolchain compiles the distributed binaries** (Win64-focused). Sellers package with `Engine\Build\BatchFiles\RunUAT.bat BuildPlugin` to pre-validate. Review: Fab docs claim "most updates are processed within 24 hours," and simple approvals land in 2 hours–3 days, but forum reports for code plugins range up to weeks/months with occasional opaque rejections ("changes needed but no changes needed"). Fab's content guidelines differ from the old Marketplace — previously-approved items can fail Fab review.

**Pricing norms for dev-tools/code plugins**: one-time purchase only — **no subscriptions** (explicitly declined by the Fab team on the forums), and seat-based licensing exists but is poorly surfaced at checkout (a live seller complaint). Typical code-plugin anchors: $14.99 (Electronic Nodes), $29.99 (Blueprint Assist), $64.99 (Dialogue Plugin), with UI/system frameworks up to ~$100–250. Fab launched **without ratings/reviews/Q&A** (major seller backlash; old Marketplace reviews were preserved unofficially at orbital-market.com); star ratings and verified-purchase written reviews were added during Fab's first year (Fab docs: 1–5 stars, only from users who acquired the product on Fab). Scale check from Epic's official "Fab 2025 year in review" (Jan 28, 2026): creators earned **>$24M in 2025**, listings tripled to **420,000+**, publishers doubled to **20,000+** — i.e., mean revenue per publisher is ~$1.2K/yr; forum sellers debate whether even €400–500/month is realistic without constant new releases. Known platform gaps sellers cite: no "Runtime" category for code plugins, no CI/CD upload API, no dependency manager.

## 2. Engine-version matrix and per-version burden

Fab rule (same as late Marketplace): **new products must support the latest UE version; published products must maintain support for at least one of the three latest engine versions**. Epic ships ~2 minor versions/year (5.4 Apr 2024, 5.5 Nov 2024, 5.6 Jun 2025, 5.7 late 2025, 5.8 documented by mid-2026), so a plugin drifts out of the compliance window in ~18 months untouched. Fab did *remove* the old three-version cap on what can stay listed/downloadable (versions back to 4.27 are supported for existing uploads). Per-version burden is real: Fab docs state code plugins "always require an updated project to be submitted, even if the plugin works as expected in the latest engine release" — one packaged project per engine version. This is painful enough that community CI exists: **MuddyTerrain/unreal-ci-cd-for-fab** (MIT) automates "develop low, upgrade high" — a master project on the oldest engine, auto-upgraded and `BuildPlugin`-packaged per version (example config: 5.1/5.2/5.3/5.4).

**Slate/UMG churn 5.0→5.8** (why per-version isn't just a checkbox): UE 5.0's "Starship" editor restyle changed editor styling wholesale; **5.1 deprecated `FEditorStyle` → `FAppStyle`** (`FEditorStyle::GetBrush()`→`FAppStyle::GetBrush()`, `GetStyleSetName()`→`GetAppStyleSetName()`), a mechanical but universal editor-plugin migration; 5.1.1/5.2 shipped runtime widget regressions documented in the long forum thread "EPIC, please stop breaking Slate and UMG in new releases" (`UEditableTextBox::SetText` reverting, slider `SetValue` broken, `UInputKeySelector::SetTextBlockVisibility`, `FAnalogCursor::HandleKeyDownEvent` assuming mouse input, `SetPositionInViewport`, widget-animation 1-frame playback) — Epic acknowledged triage but no public tracking; 5.2 tightened IWYU includes; UE now **requires C++20** (5.3+ default) and 5.6 requires newer MSVC (14.44+) with reported UBA/toolchain breakage; **5.6 also introduced SlateIM**, Epic's own experimental immediate-mode wrapper over Slate (ImGui-like, editor+runtime) — both a validation that Slate's retained API is considered painful and potential first-party competition for any alternative UI-framework plugin.

## 3. Successful UI/framework/dev-tool code plugins (price / rating proxies)

- **Electronic Nodes** (hugoattal) — blueprint-wire rendering: **$14.99, 4.9★, 458 ratings**, released Aug 2019, supports 4.20–5.7 (~14 engine versions maintained).
- **Blueprint Assist** (fpwong) — graph auto-formatting: **$29.99, 4.8★, 178 ratings**.
- **Dialogue Plugin** (CodeSpartan) — node-editor dialogue system: **$64.99, 4.6★, 177 ratings**, listed 4.11–5.3; its support forum thread has **859+ posts since 2016**, mostly author-answered.
- **Async Loading Screen** (truong-bui) — free OSS code plugin, 1,093 GitHub stars.
- **Noesis GUI** (XAML UI framework — closest analog to a React-style UI framework for UE): plugin source free on GitHub (356 stars); monetized **outside Fab** via per-project, per-platform licenses; indie tier free/cheap under €100K revenue.
- **Common UI** — Epic's own free built-in cross-platform UI plugin (Fortnite/Lyra); with UMG+MVVM (ViewModel plugin) and now SlateIM, Epic occupies the free tier of UI frameworks — a paid UI framework must beat "free, first-party, console-certified."

## 4. Open-source + paid-support vs paid-plugin precedents

- **Voxel Plugin**: free version (full source, commercial use) + **Pro** (Voxel Graphs, Discord support, team sharing; not available to projects budgeted >$200K) + **enterprise at $200K** (support contract, Slack). Core module (`VoxelPlugin/VoxelCore`) is MIT OSS (509 stars). The free tier built one of the largest UE plugin communities.
- **FlowGraph** (MothCocoon): fully free OSS (1,860 stars, 360 forks), design-agnostic node system; author monetizes reputation/consulting, maintains per-engine-version branches.
- **VRExpansionPlugin** (mordentral): MIT OSS (746 stars), never sold on Marketplace; monetized via **Patreon** + prebuilt-binary packages for supporters — the canonical "free code, paid convenience/support" UE model.
- **CommonUI / MVVM / SlateIM**: Epic gives away UI infrastructure, which historically kills paid generic-UI plugins and pushes paid survivors toward *tooling* (editors, formatters) or *vertical systems* (dialogue, inventory).

## 5. GitHub-distributed plugins: consumption and friction

Standard consumption: clone/submodule into `ProjectName/Plugins/<PluginName>/` (VRExpansionPlugin's documented install; ProteusVR templates vendor it in-tree). Friction vs Fab: (a) **source plugins require a C++ project** — Blueprint-only projects fail to package with a source-only plugin ("Cannot build a blueprint project with plugins that have C++ code"); workarounds are converting to C++, building in a throwaway C++ project, or authors shipping prebuilt binaries/`InstallPlugin.bat`; (b) Launcher-installed engines can't take engine-level plugins without a matching prebuilt build; (c) no launcher-managed updates/versioning — teams pin submodule SHAs; (d) upside: no review gate, all engine versions/branches, consoles trivially supported since users compile. Fab conversely gives one-click install into Launcher engines with Epic-built binaries but only for the compliant version window.

## 6. Console/platform constraints for UI plugins

Plain C++/Slate/UMG plugin code has **no console-specific restriction**: `TSharedPtr`/SlateCore are engine-standard on every platform. The `.uplugin` `WhitelistPlatforms`/`PlatformAllowList` controls what UAT compiles; Fab/Marketplace distribution covers public platforms only (Win64/Mac/Linux/Android/iOS) — **console binaries can't be distributed** (NDA), but because buyers receive full source they compile for PS5/Xbox/Switch themselves under their own platform agreements (the FMOD-style "platform specifics" pattern). The real constraint is **no JIT on consoles or iOS**: iOS apps "are not allowed to generate code at runtime" (LuaJIT falls back to its interpreter; entitlement-based JIT isn't available to App Store games), and consoles similarly prohibit W^X code generation. So script-hosted UI approaches must use interpreters or AOT (LuaMachine ships interpreter-only iOS/Android; puerts/sluaunreal use interpreter/quickjs modes) — a GDScript-like hosted-language layer would face the same rule, while a pure-C++ reactive framework (or one whose "markup" compiles to C++/asset data ahead of time) is unaffected.

## 7. Reported support burden

Sellers on the Epic forums describe plugin selling as "a full time job — updates, support, documentation, marketing"; one successful seller explicitly advises art assets over code because they need "way less maintenance and user support." Concrete proxies: CodeSpartan personally answered a 859-post forum thread over ~8 years; Electronic Nodes has tracked 14 engine versions since 2019; FlowGraph maintains per-version branches and migration notes each engine release; the FEditorStyle→FAppStyle migration alone shows up as issues across dozens of OSS plugin repos (ElgEditorScripting, BlueprintRetarget, VRM4U). The three-version window plus ~2 engine releases/year forces at minimum two recompile/retest/resubmit cycles per year per product, and Slate regressions (Section 2) mean "recompile" often becomes "debug Epic's changes." Discord servers (Voxel Plugin, FlowGraph) and Patreon tiers (mordentral) are the dominant support channels; Fab itself provides sellers no ticketing — only reviews and off-platform support links.

## KEY FACTS
- Fab (launched Oct 22, 2024, successor to UE Marketplace) pays sellers an 88/12 revenue share with a $100 payout threshold; no subscription pricing is offered for plugins.
- Fab rule: new products must support the latest UE version, and published products must maintain support for at least one of the three latest engine versions (~18-month window at Epic's ~2 releases/year cadence).
- Code plugins are submitted as source (no binaries); Epic's toolchain compiles distributed binaries, and a separate packaged project must be submitted per supported engine version (community CI tool: MuddyTerrain/unreal-ci-cd-for-fab).
- Epic's official 'Fab 2025 year in review' (Jan 28, 2026): creators earned >$24M in 2025 across 420,000+ listings and 20,000+ publishers (~$1.2K mean per publisher).
- Successful dev-tool code plugin price/volume anchors: Electronic Nodes $14.99 (4.9 stars, 458 ratings, supports UE 4.20-5.7), Blueprint Assist $29.99 (4.8 stars, 178 ratings), Dialogue Plugin $64.99 (4.6 stars, 177 ratings).
- Slate churn examples: UE 5.1 deprecated FEditorStyle in favor of FAppStyle (GetStyleSetName -> GetAppStyleSetName); 5.1.1/5.2 broke UEditableTextBox::SetText, slider SetValue, UInputKeySelector::SetTextBlockVisibility; UE requires C++20 (5.3+) and MSVC 14.44+ for 5.6.
- UE 5.6 introduced SlateIM, Epic's own experimental immediate-mode (ImGui-like) wrapper over Slate for editor and runtime, adding first-party competition in the UI-framework space.
- Epic gives away UI infrastructure (Common UI from Fortnite, UMG ViewModel/MVVM, SlateIM), which pushes paid plugins toward tooling or vertical systems rather than generic UI frameworks.
- OSS+paid precedents: Voxel Plugin free/Pro/enterprise ($200K budget cap on Pro, $200K enterprise tier); FlowGraph fully free OSS (1,860 GitHub stars); VRExpansionPlugin MIT + Patreon for support/prebuilt binaries (746 stars); Noesis GUI free plugin source (356 stars) with per-project per-platform licensing, free indie tier under EUR 100K.
- GitHub consumption is clone/submodule into ProjectName/Plugins/, but Blueprint-only projects cannot build source plugins (need a C++ project or author-shipped prebuilt binaries) - the main friction vs Fab's one-click launcher install.
- Fab/Marketplace cannot distribute console binaries (NDA platforms); buyers compile plugin source for PS5/Xbox/Switch under their own platform agreements via .uplugin WhitelistPlatforms/PlatformAllowList; TSharedPtr/Slate C++ is unrestricted on all platforms.
- No-JIT rules on consoles and iOS mean script-hosted approaches must use interpreters or AOT (LuaJIT falls back to interpreter on iOS; LuaMachine/puerts ship interpreter modes); pure C++ or compile-ahead markup is unaffected.
- Fab launched without ratings/reviews/Q&A (seller backlash); verified-purchase star ratings and written reviews were added during its first year.
- Support burden reports: sellers call plugin selling 'a full time job'; CodeSpartan answered an 859-post support thread over ~8 years; one seller recommends art assets over code because they need 'way less maintenance and user support'; forum consensus doubts even EUR 400-500/month for most code sellers.
- Fab review latency: docs claim most updates process within 24 hours; simple listings approve in 2 hours-3 days, but code-plugin reviews are reported to take weeks and occasionally months.

## SOURCES
https://dev.epicgames.com/documentation/fab/publishing-assets-for-sale-or-free-download-in-fab
https://support.fab.com/s/article/FAB-TECHNICAL-REQUIREMENTS?language=en_US
https://dev.epicgames.com/documentation/fab/asset-file-format-and-structure-requirements-in-fab
https://forums.unrealengine.com/t/unreal-engine-code-plugin-distribution-on-fab/2025967
https://www.unrealengine.com/blog/epic-announces-unreal-engine-marketplace-88-12-revenue-share
https://www.unrealengine.com/news/fab-2025-year-in-review
https://www.strayspark.studio/blog/fab-marketplace-12-month-retrospective-seller-2026
https://www.fab.com/listings/14d7ba87-a587-406d-9369-ed75fa0a55ed
https://www.fab.com/listings/d6148766-27b1-47db-a730-832c53b7a895
https://forums.unrealengine.com/t/dialogue-plugin/59871
https://voxelplugin.com/
https://github.com/VoxelPlugin/VoxelCore
https://github.com/MothCocoon/FlowGraph
https://github.com/mordentral/VRExpansionPlugin
https://www.patreon.com/mordentral
https://github.com/Noesis/UnrealPlugin
https://www.noesisengine.com/licensing.php
https://forums.unrealengine.com/t/epic-please-stop-breaking-slate-and-umg-in-new-releases/1179127
https://github.com/MuddyTerrain/unreal-ci-cd-for-fab
https://forums.unrealengine.com/t/selling-on-ue-marketplace-fab/2038322
https://github.com/rdeioris/LuaMachine
https://github.com/LuaJIT/LuaJIT/issues/1072
https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine
https://dev.epicgames.com/documentation/unreal-engine/API/PluginIndex/SlateIM
https://forums.unrealengine.com/t/cannot-build-a-blueprint-project-with-plugins-that-have-c-code/236828
https://forums.unrealengine.com/t/is-it-possible-to-make-a-plugin-in-c-that-will-work-in-a-blueprint-only-project/434933
https://dev.epicgames.com/documentation/fab/reviews-in-fab
https://forums.unrealengine.com/t/question-about-fab-submission-process/2657762
https://forums.unrealengine.com/t/fab-review-result-unclear-changes-needed-but-no-changes-needed/2036677
https://github.com/truong-bui/AsyncLoadingScreen
https://gamefromscratch.com/massive-fab-updates-unreal-asset-giveaway-sept-2025/
https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/1165-c-20-default-version
https://forums.unrealengine.com/t/whitelist-blacklist-allow-for-uplugin/1392743
https://fmod.com/docs/2.01/unreal/platform-specifics.html