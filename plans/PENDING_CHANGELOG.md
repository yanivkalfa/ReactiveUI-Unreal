# Pending changelog ledger

One bullet per merged user-relevant change, staged by the `plan-progress` skill at
phase/milestone completion — while the knowledge is fresh. **Drained by `release-process` §0**
into the real lanes (A: root CHANGELOG.md · B: `changelog.mjs add` · C: Discord) and then
EMPTIED. Bullets that never reach a lane are how release notes go missing — drain, don't append
around.

**Format:** `- [lane A|B|C] [artifact] one-line summary (PR #n / sha)`

<!-- Drained 2026-07-16 for the widget-completion releases (plugin 0.5.0→0.9.0 + extensions
     0.3.0): every bullet verified present in Lane A ([0.5.0]/[0.6.0]/[0.7.0]/[0.8.0]/[0.9.0]
     sections), Lane B (changelog.json 0.3.0 — schema catch-up + sinceUE + wave G), or
     Lane C (DISCORD_CHANGELOG [0.5.0]→[0.9.0]). -->
<!-- Drained 2026-07-16 for the extension-listing release (extensions 0.3.1, no plugin
     bump): both bullets went straight into Lane B (changelog.json 0.3.1 — listing overhaul
     shared bullet + vscode README bullet) via plans/archive/EXTENSION_LISTING_PLAN.md's execution;
     no Lane A/C entries apply (listing-only change, not a Discord-worthy release). -->
<!-- Drained 2026-07-16 for the include-retirement release (plugin 0.10.0, extensions 0.4.0):
     the single bullet went straight into Lane A ([0.10.0]), Lane B (changelog.json 0.4.0 —
     shared entry), and Lane C (DISCORD_CHANGELOG [0.10.0]) as it was authored — nothing was
     staged here mid-campaign, so there was nothing to drain from THIS file, only to verify. -->
- [lane A+C] [plugin] Markup everywhere (§4): markup is first-class at every expression
  boundary — `auto X = (<VerticalBox>…);`, call arguments, ternaries, short-circuit — lowering
  in place via the family jsx scan; UETKX0114 narrowed to the paren-less markup return; NEW
  rules-of-hooks diagnostics UETKX0013-0016 (hooks are top-level-body-only); hook signatures
  now exclude markup text (HMR protection — components with statement-level early returns get
  a ONE-TIME signature change: one HMR state reset at upgrade); AcceptanceLab §10 demos it
  (feat/click-counter-demo)
- [lane B] [both extensions] LSP markup-everywhere mirror: jsxScan.ts port, scanner parity
  (0114 narrowed + 0013-0016), jsx-aware virtual doc (value markup neutralizes to a typed
  placeholder, its expressions get full clangd intel), formatter family-parity pin; family
  corpus reconciled across all three repos incl. the wave-G drift (hash 917dd8cd…)
  (feat/click-counter-demo)
- [lane A] [plugin] Editor: ReactiveUetkx menu gains a "Message Log" entry (one-click access
  to the ReactiveUI compile-error page with jump-to-file rows) (feat/click-counter-demo)
- [lane B] [both extensions] Crash-proof server (§1): process-level traps, clangd pump guards,
  URI re-serialization fallback; embedded C++ intelligence in EVERY markup expression (§2):
  expression lifting, compile-DB sanitizer (STL/SDK includes, SharedPCH drop, -fmsc-version),
  clangd discovery chain (feat/click-counter-demo)
