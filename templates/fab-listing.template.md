<!-- Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved. -->
<!-- ── release-process TEMPLATE: the Fab listing ──────────────────────────────────────
     Rendered manually at listing/update time (Fab has no API). Placeholders in __CAPS__.
     The four review-enforced compliance items (MASTER_PLAN D-29) are BAKED IN below —
     do not remove them to "clean up" the listing: (1) AI disclosure, (2) example project
     per engine version, (3) copyright lines (CI-linted in-repo), (4) title must not be
     trademark-adjacent (owner picks from drafted variants at Phase 8). -->

# __LISTING_TITLE__  <!-- owner-picked variant; avoid "React"-adjacent titles (rejection risk) -->

**React-style UI for Unreal in pure C++ — function components, hooks, and live-editable
markup that compiles to native code. No script VM in your shipped game.**

Write UI as functions returning a widget tree; a fiber reconciler patches only what changed on
real Slate widgets. State lives in hooks (`UseState`, `UseEffect`, … — 23 total). `.uetkx`
markup hot-reloads mid-PIE in under a second during development and compiles to plain C++ for
shipping.

- Native Slate output — Widget Reflector, styling, and accessibility see ordinary widgets
- UMG both ways: host inside UserWidgets (`URuiHostWidget`), embed UMG widgets (`RUI::Umg`)
- CommonUI-native screens (`URuiActivatableScreen`) — your input routing stays untouched
- MVVM/FieldNotify viewmodels feed components via `UseField`
- __WIDGET_COUNT__ wrapped widgets + a virtualized list; localization-ready (`NSLOCTEXT` gather)
- VS Code + VS2022 extensions: completion, diagnostics, formatting for `.uetkx`
- Blueprint users: markup + built-in state + Blueprint logic classes work without C++;
  hand-written handlers require C++ (honest scope — see docs)

**Supported:** UE __ENGINE_VERSIONS__ · Win64 + Linux (tested), Mac (best-effort), consoles &
mobile compile from source (untested — pure C++/AOT, no JIT requirements).

**Source & license:** PolyForm Shield 1.0.0 on GitHub — __GITHUB_URL__ (free to use and ship
in your games and projects; not to be redistributed or sold as a competing product; Fab
downloads additionally governed by the Fab EULA). Code generated from your `.uetkx` files
belongs to your project.

**Support:** GitHub Issues — __ISSUES_URL__ · Discord — __DISCORD_URL__ (#unreal)

**Docs:** __DOCS_URL__ · Demo video: __VIDEO_URL__

---

*Developed with AI assistance (Anthropic Claude) under human direction.*

<!-- Upload checklist (release-process §6): one zip per engine version, matching
     .uplugin EngineVersion, Binaries/Intermediate stripped; updated example project per
     engine version; gallery/feature images at Fab's required sizes; supported-platforms
     declaration mirrors D-28 exactly; verified-purchase review link in Discord post. -->
