---
name: grammar-contract
description: How the .uetkx grammar stays byte-compatible with the family (.uitkx Unity — the origin; .guitkx Godot) and how the two in-repo implementations (C++ compiler, TS language server) stay in lockstep — corpus cases, goldens, the .pending divergence protocol, and cross-repo sync. Use for ANY lexer/parser/formatter change, and at release time for the sibling drift check.
---

# Grammar contract

Two invariants, one mechanism.

**Invariant 1 — in-repo lockstep:** the **C++ compiler is the grammar of record**; the TS
language server reproduces it, never the reverse. Both run the SAME corpus.

**Invariant 2 — family byte-compat (D-22e):** markup structure, declaration kinds
(`component`/`hook`/`module`), directives, reserved props parse identically across Unity, Godot,
and this repo. Only the embedded-language LEXIS differs per host (C# / GDScript / C++) — corpus
cases pin BEHAVIOR, not code.

## The rule for every grammar-touching change

A lexer/parser/formatter change **lands WITH its corpus case(s) in the same commit** — no
exceptions, including "behavior-preserving" perf work (a contract diff during such a change is
by definition a bug in the change).

- Scanner cases: `ide-extensions/lsp-server/test-fixtures/uetkx-scanner-cases.json` —
  `{ "input": …, "at": …, "expect": … }` per section (skipNoncode / findMatching / markup
  variants). **Offsets are Unicode CODE POINTS** (D-18) — the TS side converts from UTF-16
  (`codePoints.ts`), the C++ side from UTF-8; include non-BMP (surrogate-pair) cases whenever
  touching offset logic.
- Formatter goldens: `uetkx-formatter-cases.json` — pins the formatter byte-identical on both
  sides; invariants: verbatim-on-parse-error, idempotent, `fellBack` distinguishable.
- Full-file fixtures + goldens (Phase 3+): fixtures under
  `Source/RuiHostTests/ContractFixtures/` (EXCLUDED from the RUICompile sweep and UBT);
  goldens dumped by `-run=RUIContractDump`; `--check` is the CI drift mode; the TS side replays
  every golden.
- Both sides run in CI: the C++ side via `ReactiveUI.Contract`, the TS side via
  `node --test` in lsp-server.

## The `.pending` protocol (known divergences)

A divergence you can't fix now gets pinned, not ignored: rename the fixture
`<name>.pending.uetkx`; the test asserts the divergence STILL EXISTS (so fixing it forces the
rename+regen and the books stay honest). A `.pending` never ships past a release without an
owner decision.

## Cross-repo sync (all three siblings)

- **Outbound** (this repo changed the shared grammar): land the corpus case here; open the
  mirrored corpus PR against the Godot repo (and flag the Unity repo) — track in
  `plans/TECH_DEBT.md` until both merge. A grammar change is not "done" while a sibling's corpus
  lacks its case.
- **Inbound** (a sibling released a grammar change — they demonstrably still do): import their
  new cases, run both in-repo implementations, reconcile or `.pending`-pin.
- **Release-time drift check** (`release-process` §3): hash-compare the shared corpus sections
  against `ReactiveUI-Gadot/ide-extensions/lsp-server/test-fixtures/scanner-cases.json` (and the
  Unity repo's corpus). Host-lexis-specific sections are exempt; structural sections must match.
- The **vendored lsp-server's family-shared files** (`markup.ts`, `sourceMap.ts`, formatter,
  corpora) sync by the same procedure (D-30): diff against the sibling's copy, port deliberately,
  never let them drift silently.

## Scar tissue (why these steps exist)

- **The corpus is why two implementations in two languages stayed byte-identical in production**
  in the Godot repo (GDScript compiler ↔ TS server) — three implementations without it would
  already have diverged.
- **Code points, explicitly**: the sibling repo retrofitted `codePoints.ts` after UTF-16 vs
  code-point offsets silently mismatched on astral characters — here the unit is pinned in the
  spec and the corpus carries non-BMP cases from day one.
- **`.pending` exists** because "we'll fix that divergence later" with no tripwire became
  permanent in early family history; a pinned divergence that BLOCKS fixing silently is one
  you'll actually fix.
