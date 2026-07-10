# Benchmark baselines

The single committed home for all `ReactiveUI.Bench` numbers (benches are NOT pass/fail).
Perf work cites before/after rows from here in its PR; Phase 8's public/marketing numbers come
ONLY from packaged **Test**-configuration builds recorded here.

**Rules (MASTER_PLAN §4):**
- Every row carries its context — numbers without context are noise:
  `machine (CPU / RAM / OS) · engine version · build config (DebugGame/Development/Test) ·
  Slate.EnableGlobalInvalidation state · date · commit sha`.
- **Cross-machine comparisons are invalid.** Compare only rows sharing a machine + config.
- A superseded baseline is struck (`~~…~~`), never deleted — history is the point.

**Table format** (one table per bench, newest row first):

```
## <BenchName> (what it measures, workload size)
| date | sha | machine | engine | config | globalInv | metric1 | metric2 | notes |
```

---

**Shared context for the rows below** — `M1`: i9-12900K / 128GB / Win11 Home 10.0.26200 ·
UE 5.6.1 · Development editor (`-nullrhi`, headless Automation) · globalInv n/a (mock host —
these are reconciler-only benches, no Slate). Run: `Automation RunTests ReactiveUI.Bench`.
Values are min/med of n=5 in-process reps (µs).

## Bench.Core / mount_1000_leaves (cold mount: render + fibers + host nodes; 1 container + 1000 unkeyed leaf boxes)
| date | sha | machine | config | min µs | med µs | notes |
|---|---|---|---|---|---|---|
| 2026-07-10 | 286fecb | M1 | Dev editor, mock host | 188 | 199 | labels precomputed; teardown untimed |

## Bench.Core / noop_rerender_1000 (root RequestUpdate, zero prop changes)
| date | sha | machine | config | min µs | med µs | notes |
|---|---|---|---|---|---|---|
| 2026-07-10 | 286fecb | M1 | Dev editor, mock host | 0 | 0 | SUBTREE-SKIP adopts the whole tree — measures the skip path, not a bailout walk |

## Bench.Core / update_1_of_1000 (setState → 1 changed label among 1000)
| date | sha | machine | config | min µs | med µs | notes |
|---|---|---|---|---|---|---|
| 2026-07-10 | 286fecb | M1 | Dev editor, mock host | 134 | 152 | full component re-render + fast-leaf diff + 1 CommitUpdate; 1 FString::Printf |

## Bench.Core / keyed_reverse_500 (500 keyed boxes, order fully reversed)
| date | sha | machine | config | min µs | med µs | notes |
|---|---|---|---|---|---|---|
| 2026-07-10 | 286fecb | M1 | Dev editor, mock host | 165 | 173 | keyed mark-sweep + ReorderChildren enforce |

## Bench.Core / mount_unmount_churn_200 (20 rows × 10 cells, mount + full teardown)
| date | sha | machine | config | min µs | med µs | notes |
|---|---|---|---|---|---|---|
| 2026-07-10 | 286fecb | M1 | Dev editor, mock host | 54 | 55 | includes cleanups + slab release + mock-node teardown |

## Bench.SlateReorder — the Phase 2 step 1 reorder-strategy spike (raw SVerticalBox, 200 STextBlocks, no reconciler)
**Decision (recorded in TECH_DEBT.md): minimal-move ships.** It wins the common case
(1 moved child: **3µs vs 60µs**, 20×) and loses only the pathological full reverse
(91µs vs 61µs, 1.5×) — real UIs perturb, they don't reverse. The invalidation-boundary
wrap variant was not measured: the two strategies don't disagree badly enough to justify it
(the critique already downgraded that claim).

| date | sha | machine | config | workload | strategy | min µs | med µs |
|---|---|---|---|---|---|---|---|
| 2026-07-10 | M7 | M1 | Dev editor, real Slate widgets | 1 moved end→front | minimal-move | 3 | 3 |
| 2026-07-10 | M7 | M1 | Dev editor, real Slate widgets | 1 moved end→front | clear+rebuild | 59 | 60 |
| 2026-07-10 | M7 | M1 | Dev editor, real Slate widgets | full reverse | minimal-move | 89 | 91 |
| 2026-07-10 | M7 | M1 | Dev editor, real Slate widgets | full reverse | clear+rebuild | 60 | 61 |
