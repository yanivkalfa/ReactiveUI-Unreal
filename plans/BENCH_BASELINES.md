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

_(no benches exist yet — the harness lands in Phase 1 step 5; the reorder-strategy spike in
Phase 2 step 1 writes the first rows)_
