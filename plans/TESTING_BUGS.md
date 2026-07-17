# Testing bugs — owner IDE-testing session (2026-07-17)

Bugs found by the owner manually testing the VS Code extension (colors → formatting →
IntelliSense passes) against the demo tree, with root causes from tokenizer-harness evidence
(`scratchpad/tmtest` — the real vscode-textmate engine over the shipped grammar). Fixes land on
the extension side (grammar + language config + formatter); every grammar edit syncs the VS2022
copy byte-identically, every formatter edit keeps C++ ↔ LSP parity via
`ide-extensions/lsp-server/test-fixtures/uetkx-formatter-cases.json` (shared by the C++
`ReactiveUI.Uetkx.Formatter` suite and the LSP replay test — NOT family-core-hashed, so cases
may be updated freely).

## TB-1 — Angle brackets: red "unmatched" brackets + generics parsed as markup tags

**Repro:** `AcceptanceLab.uetkx` (brackets "off" everywhere), `CustomDraw.uetkx`
(`UseMemo<TSharedPtr<FRuiDrawFn>>` — `<>` wrong), `DoomGameScreen.uetkx` /
`DoomGame.uetkx` / `DoomHUD` usage (cascading red `>` / `/>`).

**Two independent root causes:**
- (a) `language-configuration.json` declares `["<", ">"]` a **bracket pair** — VS Code's
  bracket matcher then tries to pair every `<`/`>` including comparisons, `->`, and `>>`
  generic closers, painting "unmatched" ones red. JSX/TSX deliberately omit angle brackets
  from `brackets` for exactly this reason.
- (b) The grammar's `tag-open` rule fires on ANY `<Identifier` inside code —
  scope-dump proof: in `TSharedPtr<FRuiDrawFn>&`, `<FRuiDrawFn>` tokenizes as
  `meta.tag.open` with `FRuiDrawFn` as `support.class.component`; in
  `UseMemo<TSharedPtr<`, `TSharedPtr` becomes a tag and `FRuiDrawFn` an
  `entity.other.attribute-name`.

**Fix:** (a) drop `<`,`>` from `brackets` + `surroundingPairs`; (b) guard the tag begin:
negative lookbehind (no identifier char / `)` / `]` before `<` — kills generics), mandatory
tag name, and a post-name lookahead requiring tag-ish continuation (`>`, `/>`, attr, or EOL —
kills `A < B` comparisons). **Status: FIXED (both grammar copies + language config).**

## TB-2 — `key=` attribute colored as keyword, not attribute

**Repro:** `DoomGame.uetkx` — `key="menu"` renders blue while sibling attributes are
attribute-colored.

**Root cause:** `key`/`classes` carry `keyword.other.attribute-name.structural` — the
`keyword.other` prefix wins in every theme.

**Fix:** rescope to `entity.other.attribute-name.structural.uetkx` (structural distinction
kept in the sub-scope, renders like an attribute). **Status: FIXED.**

## TB-3 — Raw text children colored as code

**Repro:** `RouterUser.uetkx` (`Back home` blue, `+1` green-numeric), `SignalCounter` /
`ClickCounter` button labels, `CustomDraw.uetkx` (`Shuffle(bump RedrawKey)` label renders
like a function call — the owner read it as code; it is a Button LABEL, line 73).

**Root cause:** the grammar has no children region at all — a tag's open region ends at `>`,
so raw text children fall through to `code-block-body` and tokenize as C++ (identifiers →
variables, `1` → numeric, `Shuffle(` → function call).

**Fix:** restructure `tag-open`/`tag-close` into a `tag-element` region spanning
`<Name …>` … `</Name>` (the TSX jsx-element approach): an anchored attributes sub-region up
to the open tag's `>`, then children patterns (nested tags recurse; `{ }` expressions,
`@`-directives and comments still tokenize) with a raw-text fallback that carries NO scope —
default foreground. **Status: FIXED (both grammar copies).**

## TB-4 — Formatter explodes short text children to 3 lines

**Repro:** `ClickCounter.uetkx` — `<Button OnClicked={ … } …>+</Button>` (fits the print
width) reformats to open-tag line / `+` line / `</Button>` line.

**Root cause:** the formatter always puts element children on their own lines; there is no
single-short-text-child inline form.

**Fix:** when an element's only child is raw text and the whole
`<Tag attrs>text</Tag>` fits `printWidth`, keep it inline. Implemented byte-identically in
`UetkxFormatter.cpp` and `formatUetkx.ts`; corpus case added to
`uetkx-formatter-cases.json` (both suites replay it). **Status: FIXED.**

## TB-5 — No dead-code dimming after an early `return null;`

**Repro:** `MvvmDemo.uetkx` — owner added `return null;` at the top of the setup body; the
statements below it do not gray out.

**Fix (shipped):** the family diagnostic — **UETKX0107 "Unreachable code after 'return'."**
(Unity ships the same rule as UITKX0107). Both scanners detect the FIRST top-level `return` of
a component body — a markup `return ( … )` *or* a plain C++ `return x;` (the collector never
sees those) — and hint-diagnose everything after it; the server attaches the LSP `Unnecessary`
tag so the editor FADES the range natively. Corpus-pinned (3 fileScanLeg cases: early plain
return, code after the final return, conditional return = no diagnostic). **Status: FIXED.**

## TB-6 — "Where does `Shuffle(bump RedrawKey)` come from?" (question, not a bug)

It is the Button's raw-text label at `CustomDraw.uetkx:73`. It LOOKED like code because of
TB-3 (labels tokenized as C++), which made `Shuffle(` render as a yellow function call.
Fixed by TB-3's children-text region; the label now renders plain. **Status: ANSWERED.**

## TB-7 — Parens inside raw-text children colored blue by the bracket-pair colorizer

**Repro:** `CustomDraw.uetkx` — the Button label `Shuffle(bump RedrawKey)`: text renders plain
(TB-3 ✓) but the `(` `)` render blue.

**Root cause:** VS Code's bracket-pair colorization runs OUTSIDE TextMate — literal brackets in
any non-string/non-comment token get depth colors, and a text child can't be scoped as
string/comment without tinting it. The Unity sibling ships
`"editor.bracketPairColorization.enabled": false` in its `[uitkx]` configurationDefaults for
exactly this reason (its screenshots' plain white brackets).

**Fix:** mirror the sibling — `[uetkx]` configurationDefaults now disables bracket-pair
colorization. **Status: FIXED.**

## TB-8 — Unreal editor: compile errors in the Message Log are not clickable (OPEN)

**Repro (owner question):** "In Unity you can go to the file that has errors from the console —
how does it work for Unreal?" Today: `UetkxWatcher.cpp` logs errors via
`FMessageLog::Error(plain FText)` — the ReactiveUI Message Log page shows the text but clicking
does nothing; the owner must find the file manually. (VS Code-side this already works — the
Problems panel jumps to file:line via the LSP.)

**Fix (shipped):** tokenized messages — each error row carries a `→ File(line,col)` action
token that opens the `.uetkx` at the error position: `code --goto file:line:col` when VS Code
is on PATH (probed once), else the OS default app. The watcher now passes structured diags
through, and line/col derive from the scanner's code-point offset. **Status: FIXED.**

## TB-9 — Host includes: wrong header name gives no squiggle, no go-to-definition

**Repro:** `CommonUiDemo.uetkx` — `import "@RuiDemoSupport.h"` / `import "@DemoInteropWidgets.h"`:
misspell the name → no diagnostic; click → no definition.

**Root cause:** host includes are nameless BY DESIGN (the C++ compiler owns the symbols), so
the resolver skips them entirely — but the LSP has workspace visibility the compiler lacks:
project headers under `Source/`/`Plugins/*/Source/` are findable. `UETKX2316` was reserved for
exactly this (docs said post-v1 because the COMPILER can't see UBT include paths — the LSP
partially can).

**Fix:** workspace header index (basename+suffix match) → go-to-definition on the specifier;
`UETKX2316` (error, LSP-only — owner call 2026-07-17) when a `/`-less specifier resolves to nothing in the workspace
(slashed specifiers stay undiagnosed — they're usually engine paths the LSP can't verify).
Docs updated (2316 no longer "reserved"). **Status: FIXED.**

## TB-10 — Embedded C++ intelligence completely dark (no diagnostics, no completion, weak hover)

**Repro (owner):** mangled `CommonUiDemo.uetkx` setup wholesale (`UseaState`,
`ProvideContesxt`, `Staste.`) — zero squiggles; no completion in C++ regions; hover far below
the Unity sibling.

**Root causes (three, stacked):**
1. **No clangd on this machine** — not on PATH; VS 2022 ships only clang-format/clang-tidy
   (clangd is a separate component). The channel degraded to markup-only exactly as designed —
   but SILENTLY, which is the real bug.
2. **No `compile_commands.json`** — never generated for this project (UBT
   `-mode=GenerateClangDatabase`), so even a present clangd would parse blind.
3. **Diagnostics were never forwarded** — `clangdProxy.ts` wires completion/hover/definition
   but has no `publishDiagnostics` handling, so C++ typos can't squiggle even with 1+2 solved.

**Fix:** (1) clangd autodiscovery (PATH → `uetkx.clangd.path` setting → common install
locations) + a one-time visible warning naming what's missing and how to get it; (2)
`GenerateClangDatabase` runbook + `findCompileCommands` covering the generated location; (3)
forward clangd diagnostics for virtual docs through the source map into the real document
(prelude-range diagnostics dropped). **Status: FIXED (channel verified live end-to-end).**

## TB-11 — Hover too thin (markup and C++) vs the Unity sibling

**Repro (owner):** hover on tags/attributes shows less than ReactiveUIToolkit's.

**Fix:** hover markdown rebuilt from the full schema row — element hover: Slate class, module,
description, slot model, `sinceUE`; attribute hover: type, which setter/property/delegate it
maps to (D-33 loyalty means the Unreal name IS the doc pointer), style-key/slot-key/event
classification, enum values where the schema pins them. C++ hover rides TB-10's clangd
channel. **Status: FIXED.**

## TB-12 — Markup as a VALUE (`auto X = (<VerticalBox>…);`) — silently unsupported

**Repro (owner):** `ClickCounter.uetkx` — assigned a markup tree to a local, embedded it with
`{ SomeComponent }`; nothing diagnosed it, nothing formatted it, and the generated C++ would
hand raw markup to MSVC (a brutal, misattributed compile error at build time).

**Root cause:** the grammar only admits markup in return position (family-wide — the Unity
sibling's samples never use markup-as-value either); everything else in a body is verbatim
C++. The gap is that violating this produced NO friendly diagnostic.

**Fix now:** `UETKX0114` (error, both scanners, corpus-pinned): "markup is only legal in a
`return ( … )` — markup-as-value is not supported; extract a component instead". Full
markup-as-value support recorded as **TD-032** (family RFC — same lane as TD-031's
`<Provider>`). **Status: diagnostic FIXED; feature OPEN as TD-032.**

## Decision — UETKX3007 stays (owner, 2026-07-17)

React-style branched finals (`if/else` in plain C++ each returning markup, no top-level final
return) stay REJECTED. The endorsed spelling is the markup directive inside one top-level
return — `return ( @if (cond) { return (…); } @else { return (…); } )` (DoomGame.uetkx is the
living example). Rationale: the setup body is verbatim, unparsed C++ — proving branch
exhaustiveness there means real C++ flow analysis, while `@if/@else` is grammar the compiler
enforces structurally. Owner reviewed and accepted the `@if` form; no grammar change.

---
*Verification per fix: tokenizer harness scope dumps (before/after) on SimpleCounter,
AcceptanceLab, CustomDraw, DoomGameScreen, RouterUser; formatter corpus replay green in BOTH
implementations; format sweep idempotent over all 56 committed `.uetkx`; battery unaffected
(formatting is `.inl`-insensitive — proven by `RUICompile -check` = 0 drift after the first
save-format wave).*
