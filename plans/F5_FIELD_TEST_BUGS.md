# F5 field-test bug ledger — round 2 (2026-07-18, owner gallery sweep)

> Branch: `fix/lsp-field-test-false-positives` (all work lands here — owner directive).
> Round 1 (RouterHome/DoomFace/DoomGame false positives) is fixed and committed (`33aae95`).
> This ledger tracks the SECOND sweep's findings. Process: research → develop → test →
> bughunt → fix → commit per bug; each fix gets a regression pin.

## B1 — the basename-mismatch warning (0103) is obsolete under ES modules

- **Report:** `LabCard.uetkx` declaring `LasCard` (typo) squiggles "component `LasCard`
  differs from file name `LabCard`" — owner: "this should no longer be a thing after our
  import/export changes. at all."
- **Why obsolete:** ES modules made a file's exports EXPLICIT; importers bind by exported
  name and private identity is file-qualified — nothing requires the component name to match
  the basename anymore.
- **Fix:** `UETKX0103` removed from BOTH scanner twins (`UetkxFileScan.cpp` +
  `uetkxFileScan.ts`). No family-corpus case pinned it (the corpus doc-string "0103 never
  fires" stays trivially true), so the corpus hash is unmoved.
- **Status:** FIXED — pins: smoke ("no 0103 basename nudge OK"), full battery green.

## B2 — a heavily-mangled file publishes (almost) ZERO diagnostics

- **Report:** ContextDemo.uetkx mangled everywhere (`Bosrder`, `UseSstate`, unknown attrs) →
  no useful squiggles.
- **Root cause (two-layer masking):** live unknown-tag/attr validation NEVER existed in the
  LSP — it only ever arrived via the hash-gated COMPILER SIDECAR, which is suppressed the
  moment the buffer diverges. And the scanner's first markup parse error (a) gates off the
  resolver pass and (b) DROPS the component record entirely, so the only thing published was
  one `0302 mismatched tag` pointing at the CLOSE tag — every actual typo invisible.
- **Fix:** a live, PARSE-ERROR-RESILIENT schema sweep in `validate()`
  (`sweepMarkupElements` in uetkxIndex.ts — textual, markup-lexis + hole-aware, needs no
  tree): unknown tags → `UETKX2307` (deduped against the resolver's on clean parses; skipped
  when the workspace exports the name), unknown attrs on schema tags → `UETKX0105` (validated
  against the SAME vocabulary attr-completion offers: element attrs + style keys + slot keys
  + key/classes/Ref). When the parse is broken and component records are gone, the sweep
  falls back to the whole file's markup ranges.
- **Deliberate limits:** attrs on an UNKNOWN tag are not validated (its 2307 is the signal;
  a component tag's attrs are params, not schema keys). C++-level typos (`UseSstate`,
  `FVecstor2D`) are clangd's job through the embedded layer and still surface there.
- **Status:** FIXED — repro went from 1 diagnostic to 2307+0105×2+0302 on the exact mangled
  file; smoke pin "parse-error-resilient markup validation OK".

## B3 — the server is pathologically slow (hovers/completions arrive after long waits)

- **Report:** "the whole lsp server barely moves … sooo slow" with ~15 gallery files open.
- **Root cause:** clangd was spawned WITHOUT `--background-index=false`. With the repo's UE
  compile database present (created for TB-10), clangd's default background indexer starts
  compiling the ENTIRE project's translation units — thousands of engine-header parses —
  pegging every core for hours and starving the per-file virtual-doc requests we actually
  make. (Single-file baseline was always fast: completion 8 ms, hover 3 ms in isolation.)
- **Fix:** `--background-index=false` added to the spawn args (we only ever query our own
  virtual docs; a cross-TU engine index buys nothing). Flag verified against the pinned
  clangd 22.1.6 binary.
- **Status:** FIXED in code — needs the owner's confirmation under the real 15-file F5 load.

## B4 — markup completion + diagnostics regressed

- **Report:** tag/attr completion and markup diagnostics "none working".
- **Root cause:** no independent defect found — the scripted baseline (tag completion 64
  items in 8 ms, hover 3 ms) is healthy. The reported symptoms are B3 (requests starved by
  the background indexer → everything *feels* dead) + B2 (the diagnostics that DID compute
  were masked to a single 0302).
- **Status:** RESOLVED-BY-B2/B3 — verify in the same F5 session that confirms B3.

## Verify (owner, next F5 session)

1. Reload the dev host (fresh server pickup). 2. Open the gallery files — CPU should stay
   quiet after the first per-file TU parses. 3. Mangle ContextDemo again — expect immediate
   2307/0105/0302 squiggles at the typos. 4. LabCard's `LasCard` — no basename warn.

---

# Round 3 (2026-07-19, owner typing session): latency arc + vanishing features

Report: "diagnostics/hovers/autocomplete come 2-5 seconds after; they disappear if I play
long enough; some typos (`PsssrovideContsext`, `SetPrssssimary`) NEVER get red; markup
diagnostics are near-instant."

## B5 — unread clangd stderr wedged the pipe (the "disappear over time" arc) — FIXED

The proxy never consumed clangd's stderr. clangd logs `IncludeCleaner: Failed to get an
entry …` lines on EVERY parse of our virtual TUs; the OS pipe buffer is 64KB, and once it
filled, clangd BLOCKED mid-write — latency climbs, then every embedded feature dies.
Stderr is now drained continuously (a 2KB tail is kept for post-mortems).

## B6 — a dead clangd masqueraded as alive — FIXED

No `exit` handler: a crashed/killed clangd left `available=true` and every request just
timed out forever. Exit now fails in-flight requests, surfaces the stderr tail to the log,
and resets the spawn latch so the next request lazily respawns a fresh clangd.

## B7 — per-keystroke TU rebuilds + a preamble-cache killer — FIXED (the 2-5s itself)

Two stacked causes:
- `syncEmbeddedDoc` ran on every keystroke (no debounce) — every character queued a full
  clangd rebuild. Now debounced 300ms trailing per document; position requests are
  unaffected (they sync the current text themselves, hash-deduped).
- **The N6 headerless typedef block sat BETWEEN the prelude #includes.** clang's preamble
  ends at the first non-directive token, so the typedefs cut the cached preamble off right
  after CoreMinimal.h — every rebuild re-parsed the ENTIRE engine include set (~5-10s).
  Moved below the last #include; the preamble now covers all heavy includes.

**Measured (typing-load simulation, real ContextDemo, 40 keystrokes/round × 4 rounds):**
hover after typing stops: 5018ms (timeout) → **~290ms**; fresh embedded diagnostics:
**~290ms** after typing stops; stable across rounds, no degradation, no server errors.

## B8 — `PsssrovideContsext` / `SetPrssssimary` never red — UPSTREAM CLANG, DOCUMENTED

Probe-verified: when a call's ARGUMENTS contain errors (`RuiDsemo::…`, `Themse`,
`bPsssrimary` — all flagged), clang's recovery SUPPRESSES the undeclared-CALLEE error
(unqualified lookup defers to ADL, which gives up on error-typed args). The line still
shows red via the argument errors; this is standard clangd behavior in any C++ editor and
not something the server layer can fix without degrading recovery elsewhere.

## Verify (owner): reload the dev host → type freely in a big file — squiggles/hovers should
track within ~⅓s of pausing, and nothing should degrade or die over a long session.

---

# Round 4 (2026-07-19): intermittent everything + wrong coloring

Report: "sometimes diagnostics work, sometimes not; when they work it's much faster;
structured-binding coloring wrong."

## B9 — the hook ADAPTERS recursed on typo'd hook names (THE intermittency) — FIXED

The round-2 bare-hook adapter (and the older qualified one) declared
`auto UseX(...) -> decltype(UseX(Ctx, ...))`. For a hook that EXISTS the decltype resolves
the real function — but for a TYPO'D name (`UseSsstate`) the unqualified lookup found the
ADAPTER ITSELF: infinite template instantiation → `fatal error: recursive template
instantiation exceeded maximum depth of 1024` → a FATAL kills every other diagnostic in
the TU and burns seconds per rebuild. That is exactly the observed pattern: typos matching
`Use[A-Z]` nuked the file's diagnostics AND slowed it; typos like `UsssseState` (lowercase
4th char → no adapter) left everything working and fast. Semantic-token coloring rides the
same TU, so a fatally-dead TU also uncolors the file.

**Fix:** both adapter kinds gain a recursion guard — a `__rui_ctx_first<TArgs...>`
detector (clang's `__is_same` builtin + hand-rolled strip/enable_if; no std includes, so
headerless mode still parses) SFINAEs the adapter out the moment the decltype re-enters
with Ctx as the first argument. A missing hook is now a plain, finite "No matching
function for call to 'UseSsstate'".

**Field-verified** (the exact screenshot-2 mangles through the real server + clangd):
5 clean diagnostics — UseSsstate, FLinearsColor (+fix), Theme/Thesme, ProvideContssssssext
— no fatal, TU alive, coloring restored. Pinned: the guard rides every adapter; the
unguarded shape is asserted ABSENT.

---

# Round 5 (2026-07-19): coloring by type, component props, hole completion

## R5-1 — `SetPriamsary` silent when the binding's INITIALIZER is also broken — B8 CASCADE

Same upstream clang recovery as B8: `auto [bPrimary, SetPrimary] = UseaState<…>` (typo'd
initializer) gives the bindings error-types, and a call through an error-typed arg
(`SetPriamsary(!bPrimary)`) suppresses the callee error. The owner's own screenshots prove
the control: with a VALID decl the same typo IS flagged. No server-layer fix.

## R5-2 — embedded C++ now has REAL semantic coloring — SHIPPED

Coloring inside setup/hook bodies was pure TextMate (semantic tokens covered import lines
only). clangd's semantic tokens are now forwarded for the virtual doc, translated through
the source map, merged with the import-binding tokens: types color as classes, hooks as
functions (built-ins reach clangd as macros — mapped to function deliberately), variables
as variables. **The owner's color-by-type rule for callables:** clangd calls a TFunction
variable a "variable" (and hovers structured bindings over TTuple as NULL TYPE), so two
overlays re-type them as functions: a hover-based check for plain variables
(TFunction/FRuiCallback/TDelegate in the type), and the FAMILY RULE for bindings — the
second name of `auto [X, SetX] = Use…()` is the setter callable. Probe-verified:
`SetPrimary` → function, `UseState` → function, `FLinearColor` → class, `Theme` → variable.

## R5-3 — attr-expr hole completion offers the attr's TYPE first — SHIPPED

`Size={ | }` + ctrl-space listed generic hooks. The schema speaks in kinds ("vector2"), so
a kind→constructor map now puts `FVector2D(…)` (snippet) first for Size holes — likewise
color→FLinearColor, margin→FMargin, text→FText::FromString, name→FName. clangd items follow
when the cursor is inside the lifted span. Probe: first item `FVector2D(…)`.

## R5-4 — unknown props on USER COMPONENT tags now diagnose — SHIPPED

`<DemoContextPanel Labsssssssssel="…" />` was silent (the sweep skipped component tags).
Component tags now validate attrs against the component's PARAMS — same-file or resolved
through the import (default imports included) — plus the pass-through set (style keys, slot
keys, key/classes/Ref). Unresolvable targets skip (never guess). Verified on the owner's
actual saved file: `unknown prop 'Labsssssssssel' on <DemoContextPanel>`; smoke pin added.

---

# Round 6 (2026-07-19): the owner called out an assumption — course correction

Report: (a) `SetPrimsary(!bPrimary)` still silent when the hook is ALSO broken — "and it
should" flag; (b) the setter colored yellow even with a TYPO'D hook name — "there's some
kind of assumption going on… instead of properly fixing stuff you patch them?"

The criticism was right on (b). Resolution, properly this time:

## R6-1 — the family-rule coloring REMOVED (it was an assumption)

The `auto [X, SetX] = Use\w+` regex colored SetX as a function by PATTERN, firing even when
the hook was broken/nonexistent. Removed. Coloring is now strictly type-driven: the hover
overlay re-types a variable only when clang REPORTS a callable type. Probe evidence of the
hard limit: clangd types TTuple structured bindings as "NULL TYPE" at the decl and returns
NO hover at the use — so `SetPrimary` colors as a variable (blue), which is what the
compiler actually knows. Honest > pretty.

## R6-2 — the local-typo lint (LSP-only UETKX2310) — the PROPER fix for (a)

clang suppresses an undeclared-CALLEE error once an argument is error-typed (the broken-
initializer cascade) — upstream, probe-proven in plain C++. But OUR scope tracker knows the
locals. New lint: a code reference that resolves to NOTHING (not a local/import/decl/
builtin/workspace export) within edit distance 1 (2 from 9 chars) of an IN-SCOPE local,
same first char, ≥5 chars → ERROR (red — owner call, consistent with clangd whether or not the cascade suppresses it): `unknown name 'SetPrimsary' — did you mean the local
'SetPrimary'?`. Deterministic, cascade-proof, markup-fast (no clang round-trip).

**Verified:** the exact reported scenario (UseSstate broken + SetPrimsary use) flags 2310;
gallery-wide sweep over all 36 demo files: exactly ONE hit — the real typo saved in
ContextDemo — zero false positives. Smoke pin added.

---

# Round 7 (2026-07-19): coloring from COMPILER TRUTH — the dig-deeper answer

The owner rejected "clangd can't type it" — correctly. The AST types those bindings fine
(the identical generated code compiles); only clangd's HOVER can't present structured-
binding types. The surface that CAN: **inlay hints** — clangd emits the exact deduced type
(`: TRuiSetter<bool>` for SetPrimary) at every auto/binding decl, and parameter-name hints
(`NewValue:`) inside every call the compiler successfully RESOLVED.

The overlay now runs on those two exact signals instead of hover:
- a TYPE hint whose label is in the framework's callable vocabulary (TRuiSetter, TFunction,
  FRuiCallback, TDelegate, TUniqueFunction, lambdas) marks the declared name callable —
  note the REAL type is `TRuiSetter<bool>`, which the old hover regex would have missed;
- a PARAMETER hint proves the compiler resolved `Name(...)` as a call — callable by proof,
  whatever the type spells.

Custom .uetkx hooks flow through the same pipe: their shims carry the DECLARED return type
from the scan, clang deduces the binding, the hint reports it.

**Verified both ways:** clean file → `SetPrimary` colors as function (compiler-derived);
hook name broken → the binding doesn't type → honest variable/blue (and the 2310 typo lint
covers the diagnostics side). No pattern-matching anywhere in the chain.

---

# Round 8 (2026-07-20): "no way to speed it up?" — background TU pre-warm

The ~10s first-open cost is clangd building a file's ENGINE PREAMBLE — per file, per
session, paid at first interaction. The user does not have to be the one paying it:

- Once clangd is live (first real doc), the server warms the workspace's .uetkx virtual TUs
  in the BACKGROUND — strictly one at a time (nothing like the core-pegging background
  indexer), newest-modified first, capped at 48, stopping instantly if clangd dies.
- A warmed file's later real open hash-dedupes into the already-built TU. The dedupe used to
  swallow diagnostics entirely (measured: silence — clangd never re-publishes for a no-op
  didOpen and the warm-time publishes were dropped while the doc was closed); the proxy now
  stores each TU's last publish and REPLAYS it on a deduped open.

**Measured:** open A (seeds warm), wait, open B → B's diagnostics in **0.11s** vs ~10s
cold — a ~90× faster first open for every file the warm has reached. Files opened before
their turn still pay the classic cost once; the queue drains the rest while you work.

---

# Round 9 (2026-07-20): markup inside DIRECTIVE BODIES was invisible to the live sweep

Owner field find (torture-tested DoomHUD): mangle every tag/attr inside `@for (…) { … }`
bodies → zero diagnostics; top-level markup validates fine. Correct diagnosis by the owner:
"only markup inside return directives."

**Root cause:** the span walkers (the live schema sweep AND the tag-reference collector)
skip `{…}` blocks as expression holes — by design, so a C++ `a < b` can never fake a tag.
But a DIRECTIVE body's brace was skipped by the same rule, hiding its entire return window:
no 2307/0105 there, and tag references inside directives were also missing from the index
(a component renamed while used only inside an `@if` body would have missed an occurrence).

**Fix:** a brace discriminator — a `{` following `@ident (…)`, `@else`, or a `case:`/
`default:` marker is a DIRECTIVE BODY, not a hole. Both walkers now descend: lead C++
statements stay unswept (never tag material), the `return ( … )` window is walked
recursively (nested directives included); the no-return `@if { <T/> }` shape sweeps whole.

**Verified:** the DoomHUD @for-body mangles now flag (`Bssox`, `TextsBlocsk` → 2307);
pinned in index tests (open+close tags, component tags, and NESTED @if-in-@for markup all
indexed + swept). 89/89 + SMOKE.
