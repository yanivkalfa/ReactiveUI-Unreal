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
