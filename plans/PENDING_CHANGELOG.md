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
<!-- Drained 2026-07-17 for the markup-everywhere release (plugin 0.11.0, extensions 0.5.0):
     the §4 grammar bullet went into Lane A ([0.11.0] Added/Changed) + Lane C
     (DISCORD_CHANGELOG [0.11.0]); the Message Log menu bullet into Lane A ([0.11.0] Added);
     the LSP-mirror, crash-proofing/§2-channel, and §5 bundled-clangd bullets into Lane B
     (changelog.json 2026-07-17 shared entries, vscode+vs2022 0.5.0). -->
<!-- Drained 2026-07-18 for the ES-modules release (plugin 0.12.0, extensions 0.6.0): the
     campaign wrote its lanes directly at M8 (nothing was staged here mid-campaign) — Lane A
     ([0.12.0] Added/Changed/Fixed), Lane B (changelog.json shared entries: ES-modules LSP,
     TD-024 sidecar gate, Value.-payload ordering — vscode+vs2022 0.6.0), Lane C
     (DISCORD_CHANGELOG [0.12.0], 1895 chars). -->
(empty — drained 2026-07-18)

- [lane A] [plugin] UETKX0106: the compiler now rejects invalid enum-string attr values (HAlign="cesnter" etc.) — 20 closed vocabularies exported from the runtime's own parse tables; previously typos compiled clean and silently rendered as the fallback (fix/lsp-field-test-false-positives, round 10)
- [lane B] [both extensions] UETKX2311 live attr-VALUE validation (enum vocabularies incl. style/slot keys, float/int/bool formats, margin arity) + enum value completion inside the quotes + expr-only string forms surfaced live (round 10)
- [lane A] [plugin] style-key string literals now PARSE like slot keys (SLOT-1 hardening, style side): RenderOpacity="0.5" no longer renders invisible, Enabled="true" no longer disables, Font.Size/translation/scale/pivot/bool keys all honor the literal form; flag form lowers as true; UETKX0106 rejects malformed typed strings + color string forms at compile (round 11)
- [lane B] [both extensions] live 2311/0105 for malformed style/slot value strings, flag-form misuse, string/flag props on typed component params, and Ref without {expr}; schema now carries attrKinds (round 11)
- [lane A] [plugin] UETKX0109/0110/0111: compile-time duplicate-attribute, duplicate-sibling-key, and ignored-slot-key errors (full per-panel consumption map exported as slotConsumption); Slot.Column/Slot.Row added to the slot-key canon + RUI::Slot() builders (were consumed by GridPanel but unpublished); DoomHUD overlay centering fixed (was silently uncentered via ignored slot keys) (round 12)
- [lane B] [both extensions] live 0109/0110/0111 mirrors (parent-tracked sweep incl. directive bodies), UETKX2312 event-payload misuse (void/mismatched/StringValue reads are silently empty at runtime), UETKX2313 sinceUE-vs-EngineAssociation gate (too-new widgets render null slots) (round 12)
- [lane A] [plugin] UETKX0106 brush-name validation: BorderImage values are checked against the engine's actual resolvable FCoreStyle set (editor module enumerates the style registry through the parent chain and injects it; disarmed in bare unit contexts) (round 13)
- [lane B] [both extensions] brush-name validation live (2311 + did-you-mean) and value completion parity: FCoreStyle brush names inside BorderImage="", true/false inside bool-kind values — completes the R10 enum-value completion story (round 13)
- [lane A] [plugin] UETKX0112 canonical attr casing (slot routing was IgnoreCase — slot.fill silently worked while all validation disarmed; element attrs FName-matched silently); ctor-style local declarations (`const FLinearColor X(...)`) now tracked by the codegen scope oracle (round 14)
- [lane B] [both extensions] instant 2310 for butchered ctor-style decls (the DoomFace blind spot) + looser did-you-mean threshold; 0112 casing mirror; attr-name completion fixed (whole-token textEdit/filterText — no more slot.Clipping hybrids; Slot. prefix narrows to slot keys) (round 14)
- [lane B] [both extensions] completion/diagnostic parity: slot-key suggestions filter by the parent's consumption map (labeled "read by <parent>"), already-present attrs and sinceUE-gated tags are not offered — accepting a suggestion can no longer produce an immediate error (round 15)
