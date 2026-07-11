# Pending changelog ledger

One bullet per merged user-relevant change, staged by the `plan-progress` skill at
phase/milestone completion — while the knowledge is fresh. **Drained by `release-process` §0**
into the real lanes (A: root CHANGELOG.md · B: `changelog.mjs add` · C: Discord) and then
EMPTIED. Bullets that never reach a lane are how release notes go missing — drain, don't append
around.

**Format:** `- [lane A|B|C] [artifact] one-line summary (PR #n / sha)`

---

- [lane A] [plugin] Phase 0: repository ecosystem + 8-module plugin skeleton (compiles on UE 5.6; CI gates, skills, templates, docs shell) (PR #4 / 657f279)
- [lane A] [plugin] chore: action pins v7/v6 across workflows, docs deps minor-bumped, dependabot major-ignore policy for the docs shell (chore/post-merge-tidy)
- [lane A] [plugin] Phase 1: the core library — fiber reconciler (subtree-skip, O(1) context, keyed diff, error-boundary latch, restart reclaim), 23 hooks, signals, Suspense; 23 mock-host automation tests + Bench.Core baselines (feat/core-library)
- [lane A] [plugin] Phase 2: Slate host — adapter registry, bind-once-swap-inner event proxy, SRuiRoot mount surfaces, 15 core widgets, style v1 (classes + removal-reset + never-reconstruct), GO-05 widget pool, focus fences, STATGROUP_ReactiveUI, demo gallery + PIE game mode; 38/38 suites (feat/core-library)
- [lane A] [plugin] BREAKING (pre-release): element/props/style vocabulary renamed 1:1 to Unreal names (D-33) — VerticalBox/HorizontalBox/TextBlock/EditableTextBox/RuiCanvas; WidthOverride/BorderBackgroundColor/ColorAndOpacity/DesiredSizeOverride/OnCheckStateChanged; style keys RenderOpacity/RenderTranslation/RenderScale/RenderTransformAngle/RenderTransformPivot/Font.Size/Justification/AutoWrapText/FillColorAndOpacity; + SelfHitTestInvisible visibility value (refactor/unreal-1to1-naming)
- [lane A] [plugin] Phase 3: the .uetkx compiler — C++ lexer/markup parser/jsx scan/file scan (ReactiveUIInterp), codegen to committed .uetkx.inl + stable per-module aggregators, schema-v2 diagnostics sidecars, staleness machinery (error verdicts, mtime-tie hash break, env hold, orphan sweep, UETKX2106 duplicate binding, compiler fingerprint), RUICompile [-full|-check] / RUIExportSchema / RUIContractDump [-check] commandlets, canonical formatter + uetkx.config.json walk-up, 57-case scanner + 16-case formatter shared corpora, 4-fixture contract harness; ALL 11 gallery screens converted to compiled .uetkx (feat/uetkx-compiler)
- [lane A] [plugin] Phase 4: live hot reload — D-20 expression VM (whitelist registry), markup interpreter (interpreted clicks drive interpreted state; setter-call events with the Value payload), FRuiHmr swap/link/reset + family status line, reconciler override seam + HmrRefreshAll, editor watcher (3 triggers, 30s deadman, MessageLog "ReactiveUI" dedup + resolved line), rui.Hmr.AutoLiveCoding cvar (feat/uetkx-compiler)
- [lane B] [both] Phase 5: .uetkx IDE tooling — uetkx-language-server (TS front-end corpus-locked to the C++ compiler; schema-driven completions/hover, hash-gated sidecar diagnostics, golden-corpus formatting), VS Code extension (TextMate grammar with embedded source.cpp, self-contained JS server bundle — one vsix all platforms), VS2022 MEF ILanguageClient host (feat/uetkx-compiler)
- [lane A] [plugin] Phase 6: Epic interop core — URuiHostWidget (Rui inside UMG, design-time placeholder, ReleaseSlateResources unmount), RUI::Umg::UserWidget (UMG inside Rui via SObjectWidget), URuiWorldSubsystem (PIE-end/level-travel teardown contract), RUI::Umg::UseField over FieldNotify (plugin-independent; stale-VM defaults) (feat/uetkx-compiler)
- [lane A] [plugin] Phase 7: animation/media hooks are real — UseTween/UseAnimate/UseTweenValue (host-clock, retarget-from-current, cubic easing, self-re-arming frame chain) + UseSfx sink; every hook now stub-free; exit-animation Presence protocol designed (plans/EXIT_ANIMATION_DESIGN.md) (feat/uetkx-compiler)
- [lane C] [community] The Unreal sibling's headline works end to end: JSX-like .uetkx that compiles to native C++ for shipping AND hot-reloads live in the editor (save → screen < 1s, state preserved); 11-screen gallery fully on the compiled path; VS Code/VS2022 tooling on the shared family server; 53-suite headless battery (feat/uetkx-compiler)
