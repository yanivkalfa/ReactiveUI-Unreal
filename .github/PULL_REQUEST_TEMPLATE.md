<!-- Campaigns are 1 branch / 1 PR into dev; the PR title becomes the squash title. -->

## What & why

## Verification

<!-- Paste the gates you ran (per CLAUDE.md / the test-run skill). Perf changes must include
     before/after numbers against the committed bench harness (plans/BENCH_BASELINES.md). -->

- [ ] Engine-free gates green (`changelog.mjs verify`, `verify-mirror`, `check-headers`, `lint-skills`)
- [ ] Affected `ReactiveUI.*` suites green (incl. `ReactiveUI.Boot` — unit suites don't run `StartupModule`)
- [ ] Changelog entry in every touched artifact + version bump staged (release-process skill)
- [ ] Docs updated if behavior/UX changed (`docs-sync` skill)
