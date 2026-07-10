# Second-pass audit (2026-07-10) — findings ledger

After the first three-critic round, the owner asked for another pass "more in depth and in
bigger picture". Two fresh reviewers ran (technical-correctness/executor-blind-spots;
big-picture vs the actual Godot repo + research corpus) plus the author's own re-read.
**Every finding below was applied to ROADMAP.md / MASTER_PLAN.md the same day.** This file is
the provenance record; the plans are the source of truth.

## Blocker

1. "Runtime (compiled out of Shipping)" is not a module Type — real mechanism is the
   descriptor's `TargetConfigurationDenyList: ["Shipping"]` + a `WITH_RUI_INTERP` linkage rule
   (Interp self-registers; no static refs from always-compiled modules). → D-20, D-27.

## Major (technical)

2. Byte-drift gates (RUICompile -check, changelog mirror) false-fail on CRLF — `.gitattributes`
   pins `eol=lf` on generated/corpus/changelog files; codegen always emits LF. → D-19, Phase 0.
3. `V::Umg` contradicted D-01/D-02 (7 occurrences) → `RUI::Umg` everywhere.
4. Target-platform support never decided → D-28: no PlatformAllowList restriction; claimed =
   Win64 + Linux (CI), Mac best-effort; consoles compile-from-source, listed untested.
5. No dedicated-server policy → D-27: no TargetDenyList (breaks user Build.cs); subsystem
   ShouldCreateSubsystem false on DS; mounts checkf non-server; DS compile leg in armed CI.
6. Event handled-vs-unhandled API was decided in Phase 6, two phases after 15 widgets ship on
   it → decided in Phase 2 step 1 (void callbacks default→Handled(); optional FReply overload;
   typedefs in ReactiveUISlate).
7. `set()`/`@logic` were cited in §6 but never designed → D-20 designs the real surface:
   reserved plain attribute `logic={UClass}` for handler-name resolution; hook setters callable
   from event expressions in the VM (the one sanctioned VM side effect; participates in the
   hook-signature hash).
8. clangd binary sourcing + compile_commands.json location (UBT writes to engine root —
   read-only for Launcher installs) unspecified → D-23 discovery order + project-root recipe.
9. `.uetkx` placement/onboarding contract missing → D-19: module Source dirs (Private/
   convention), discovery roots, first-aggregator one-time re-gather is OK, aggregator never
   deleted, no Build.cs edits, watcher warns outside Source.
10. Changelog byte-mirror had no engine-free enforcement here → `scripts/verify-mirror.mjs` in
    the always-on CI job.
11. Phase 0 bootstrap committed directly to `dev`, violating §8 → feat/phase-0-bootstrap flow.

## Major (big picture)

12. Grammar surface incomplete: `hook`/`module` companion declarations, `@while`,
    `@uss`/`@theme` (recognized → post-v1 diagnostic), and the family rule that
    Portal/Suspense/ErrorBoundary/Memo are escape-hatch calls, not tags → D-03 full inventory;
    Phase 3 codegen shapes; docs + new-component skill.
13. Cross-repo sync named only Godot; Unity (`.uitkx`, the origin) is a third corpus party and
    the grammar still evolves → D-22e all-three-siblings + release-time sibling drift check.
14. No legal block → D-32: context-aware generated-code banner (user output belongs to the
    user), MIT-on-GitHub/Fab-EULA dual statement, THIRD_PARTY_NOTICES.md, never commit
    Epic-owned content (dev-process law).
15. No local publish/package rail despite unarmed-CI being the default → package-plugin.ps1 +
    extension publish scripts + gitignored publisher-secrets.json (Godot pattern).
16. Demo video absent (family convention, de-facto Fab requirement, owner-only) → Phase 8
    step 5 + owner checklist.
17. No marquee demo strategy (Godot's Doom clone = perf driver + marketing engine) → Phase 8
    step 1 ship-or-defer decision; Doom `.guitkx` port is the leading candidate.
18. Phase 8 docs list far thinner than the family site inventory → full checklist (Diagnostics
    catalog generated from the UETKX source, Config, Styling, Events, Signals, Context,
    CustomRendering, Portals, Suspense, Assets, Debugging, CompanionFiles, KnownIssues…).
19. D-22 lacked the stronger half of the family harness → grammar-of-record declaration,
    full-file fixtures + goldens (`-run=RUIContractDump --check`), `.pending` convention,
    fixtures path excluded from sweeps.

## Minor (all applied)

20. Gate said "four late-wired hooks"; five (UseSafeArea) → fixed.
21. 25-widget arithmetic (15+10+adapter=26) → 25 (15+10) **plus** the adapter.
22. `useTransition`/`useDeferredValue` casing in §5 → PascalCase.
23. CVar/log/MessageLog naming conventions absent → D-14 (`rui.Stats`, `LogRui*`, page
    "ReactiveUI").
24. UE-only diagnostic codes → allocated from UETKX3000+ (D-18).
25. `@while` missing from directive list + interp classification → D-03/D-20 (10k iteration cap).
26. `FLiveCodingModule` → `ILiveCodingModule`; D-27 dep cell "ReactiveUICore".
27. ROADMAP's OUT-of-v1 list missed shortcuts/remote-reload/UnrealSharp-AngelScript → appended.
28. Staged CI jobs ("once Phase N starts") had no mechanism vs required-checks → self-guarding
    jobs that exit 0 with "not armed yet".
29. Bench numbers had no committed home → `plans/BENCH_BASELINES.md` + machine/config context
    rule + packaged-Test rule for public numbers.
30. Localization overshot (custom gatherer) → NSLOCTEXT in codegen + GatherTextFromSource masks.
31. Header/skills lint scripts unnamed → `scripts/check-headers.mjs`, `scripts/lint-skills.mjs`.
32. Editor-tab mount surface straddled the module boundary in Phase 2 → SWindow in Phase 2,
    tab with Phase 8 Inspector; EUW support verified once in Phase 6.
33. No deprecation policy → D-30 (UE_DEPRECATED ≥1 minor + schema `deprecated` field).
34. plans/ artifact formats name-dropped but unspecified (fresh repo has nothing to imitate) →
    Phase 0 step 4 pins TD-##/archive-table/Discord-entry/PENDING/BENCH formats in-file.
35. PENDING_CHANGELOG was written but never consumed → release-process drains it first.
36. Docs version selector (versionManifest.ts) unassigned → Phase 0 step 8.
37. Repo metadata (About/topics/social preview) unowned → Phase 9 step 6.
38. Support channel = the EXISTING family Discord (+#unreal channel), never a new server.
39. Fab listing text needed a committed home → `templates/fab-listing.template.md`.
40. Asset actions / New Component flow / mount-by-path unassigned → Phase 4 step 6.
41. Offset unit pinned (Unicode code points + non-BMP corpus cases) — round-1 fix, re-verified.

Facts checked and confirmed fine (not changed): FText/FName are Core-module types; `.inl` under
Source/ is not compiled standalone by UBT; `FSlateApplication::OnPreTick` semantics; the §4
automation CLI recipe; prop-map schema produced (Phase 0) before consumed (Phase 2).
