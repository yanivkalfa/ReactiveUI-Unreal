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
