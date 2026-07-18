# Versioning & Deprecation Policy

> Applies to the ReactiveUI plugin (Unreal), the `.uetkx` language, and the IDE
> extensions. The Unreal sibling of ReactiveUIToolKit's policy — same rules,
> retargeted.

---

## Versioning Strategy (SemVer)

All releases follow [Semantic Versioning 2.0.0](https://semver.org/).

### Major (X.0.0)

A major bump signals breaking changes. Users may need to update code or markup.

Examples of breaking changes:
- Removing or renaming a public C++ API (`RUI::` factories, `FRuiRoot` surfaces,
  hook signatures, the interop classes `URuiHostWidget`/`URuiActivatableScreen`)
- Removing or changing `.uetkx` syntax semantics (removing a directive, changing
  control-flow behavior, changing what a prop/style key means)
- Dropping support for an Unreal Engine version

### Minor (X.Y.0)

A minor bump adds new functionality. Existing code and markup keep working.

Examples of minor changes:
- New hooks, new host elements (tags), new directives
- New diagnostic codes (may produce new warnings, but don't break builds)
- New extension features (completions, hover, formatting improvements)
- New configuration options (`uetkx.config.json` keys, `rui.*` CVars, editor settings)
- Supporting a NEW Unreal Engine version (additive — see the `engine-catchup` skill)

### Patch (X.Y.Z)

A patch bump fixes bugs. No API or syntax changes.

Examples of patch changes:
- Reconciler/adapter/codegen bug fixes; formatter, completion, or
  diagnostic false-positive fixes
- Documentation and packaged-README fixes
- Performance improvements with no API change

Perf/refactor-only changes to shipped bytes still bump + get a changelog line —
shipped code never changes silently (house law, `dev-process` skill).

### IDE Extensions

The VS Code and VS2022 extensions are versioned independently from the plugin;
their versions do not need to match it. The vendored `lsp-server` is
version-locked to the VS Code extension (D-30), and any `lsp-server/src` change
bumps BOTH extensions — republishing is how server fixes ship.

---

## Deprecation Policy

### Timeline

- **One minor version warning**: deprecated C++ APIs are marked
  `UE_DEPRECATED(Version, "Use X instead. Will be removed in vN.Y+1.")` for at
  least one minor release before removal.
- **Removal in the next minor**: if version 1.1 deprecates an API, version 1.2
  may remove it.

### Communication

- Every deprecation is documented in `CHANGELOG.md` with the version that
  introduced it and the planned removal version.
- Every removal is documented in `CHANGELOG.md` when it ships.

### `.uetkx` Syntax

Syntax deprecations follow the same one-minor-version rule: the compiler emits a
warning diagnostic (a `UETKX####` code) for one minor version before the syntax
is removed. Grammar changes are additionally bound by the FAMILY byte-compat
contract (`grammar-contract` skill) — a corpus case lands with the change and
the siblings are notified.

### Exceptions

- **Security fixes** may remove or change APIs immediately, without a
  deprecation period.
- **Pre-1.0 releases** (the current beta) make no stability guarantees, though
  we follow the policy in spirit.

---

## Compatibility

| Dependency | Minimum Version |
|------------|-----------------|
| Unreal Engine | 5.6 (floor) — battery-verified on 5.6, 5.7, and 5.8 |
| VS Code | 1.75+ |
| Visual Studio | 2022 (17.0+) |
| Node (docs site / repo scripts only) | 20+ |

Supporting a new engine version is a process, not a hope — see
`.claude/skills/engine-catchup/SKILL.md` and `scripts/engine-api-diff.ps1`.

---

## Distribution Channels

| Channel | What | Pricing |
|---------|------|---------|
| GitHub (this repo) | ReactiveUI plugin, source | Free (PolyForm Shield 1.0.0) |
| Fab | ReactiveUI plugin, per-engine-version listing | Free (Fab EULA applies) |
| VS Code Marketplace | UETKX VS Code extension | Free |
| Open VSX | UETKX VS Code extension | Free |
| Visual Studio Marketplace | UETKX VS2022 extension | Free |
