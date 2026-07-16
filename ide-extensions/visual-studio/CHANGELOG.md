# Changelog

## [0.4.1] - 2026-07-16
- The marketplace description now credits the family Discord (discord.gg/Knedqu4Wyv) and the GitHub repo, kept under 280 characters for both listings.

## [0.4.0] - 2026-07-16
- `.uetkx` preambles are imports-only now: an `import "@Header.h"` host-include form (family shape, ported from Unity's `import "@Namespace"`; our payload is a header path) compiles to `#include "Header.h"` — nameless by design, since the C++ compiler resolves the header's symbols. A raw `#include` line still works too, but naming a header the generated file already auto-includes now gets a redundancy hint (`UETKX2317`). Syntax highlighting, live diagnostics, and a discoverable `import "@"` completion snippet ship for the new form.

## [0.3.1] - 2026-07-16
- Marketplace listing overhaul: distinguishable display names — `UETKX (Unreal - VS Code)` / `UETKX (Unreal - VS2022)` — and a structured page body (Title / Description / Features / Requirements / Changelog) on both marketplaces.

## [0.3.0] - 2026-07-16
- Schema catch-up to plugin 0.9.0: the bundled .uetkx vocabulary grows 38 → 63 tags (the widget-completion campaign — ConstraintCanvas, Splitter, MenuAnchor, NotificationList, color pickers, input boxes, and 20+ more), 16 style keys, 16 slot keys.
- First engine-gated tag: SearchableComboBox carries sinceUE: 5.7 in the schema — completions/hover know it does not exist on UE 5.6.
- Grammar wave G: early component-level markup returns are accepted (multi-return collector, byte-identical to the C++ compiler per the grammar contract); the final return must be top-level (new UETKX3007); short-circuit &&/|| markup is no longer a diagnostic (UETKX3002 retired). Scanner corpus +4 cases.

## [0.2.0] - 2026-07-15
- Embedded C++ completion: inside a component setup / hook / module body, completion now forwards to clangd over the virtual document (locals, engine symbols, members) — hover, go-to-definition, and completion are all clangd-backed there. Replace ranges map back to .uetkx coordinates; without clangd on PATH everything degrades to the markup baseline as before.

## [0.1.2] - 2026-07-14
- Refreshed the ReactiveUI brand icon.

## [0.1.1] - 2026-07-13
- Added the ReactiveUI brand icon to the marketplace listing.

## [0.1.0] - 2026-07-13
- First beta — .uetkx language support for VS Code and Visual Studio 2022: schema-driven tag/attribute/event completions and hover, live parse diagnostics, import intelligence (specifier + name completions, go-to-definition, and strict resolution diagnostics), golden-corpus formatting, and embedded-C++ awareness. Visual Studio 2022 adds a Tools > Options Format-on-Save setting.
