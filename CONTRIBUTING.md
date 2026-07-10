# Contributing

Thanks for the interest! Until v1 ships, the surface is moving fast — **open an issue or ask in
[Discord](https://discord.gg/Knedqu4Wyv) (#unreal) before writing a big PR**, so we can point you
at the right phase of [plans/MASTER_PLAN.md](plans/MASTER_PLAN.md) and save you rework.

## Ground rules

- **Branch model:** PRs target `dev` (never `main` — `main` is release-only and fast-forwarded
  from `dev`). One branch, one PR per topic; the PR title becomes the squash title.
- **The loop:** research → develop → test → bughunt → fix → commit. Root cause only — no
  bandaids, no special-casing on top of shared machinery.
- **Never weaken a gate** (lint, test, CI) to get green. If a gate fails, the code is wrong.
- **Verification travels with the PR** — the template's checklist is real; perf changes need
  before/after numbers against `plans/BENCH_BASELINES.md`.
- **Grammar changes** to `.uetkx` must land with contract-corpus cases (the grammar is
  byte-compatible across the Unity/Godot/Unreal siblings — see the `grammar-contract` skill).
- **Copyright header** on every new source file (CI-linted):
  `// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.`
- No Epic-owned content or engine-derived source in the repo, ever (`THIRD_PARTY_NOTICES.md`
  covers everything third-party that IS allowed).

## Setup

UE 5.6+ and VS2022 (C++ workload). Commands live in [CLAUDE.md](CLAUDE.md) — the same file the
resident AI uses; humans and machines run identical gates here.
