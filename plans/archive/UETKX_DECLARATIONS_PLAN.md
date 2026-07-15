# .uetkx declarations v2 — `hook`/`module`, one-component-per-file, folders, recolor

> **Status:** IN PROGRESS 2026-07-11. Owner directive (extension field test, 2026-07-11): match
> the Unity sibling's authoring conventions — per-component folders with `components/` nesting,
> companion files, `hook`/`module` declarations, and grammar-quality coloring. Resolves TD-017
> and part of the Phase-5 polish backlog. Branch: `feat/uetkx-compiler` (rides PR #14).

## Evidence base (researched 2026-07-11, file:line in the two repos)

- **Unity** (`UnityComponents/Assets/ReactiveUIToolKit`): `DirectiveParser.cs:202-211` dispatches
  hook/module FILES by first keyword (never reaches the markup parser); `ParseSingleHook`
  `:586-707`; `ParseSingleModule` `:713-788` (**no param list** — `module Name { ... }`);
  multiple hook/module decls per file loop `:523-540`; ONE component per file — trailing
  content = UITKX2105 `:458-479`; suffixes are **cosmetic** (`HookEmitter.DeriveContainerClassName`
  strips any dot-suffix — "file type is determined by content, not filename"); real suffixes in
  the repo: `.hooks.uitkx`, `.style.uitkx` only; util code = plain-named module files
  (`GameLogic.uitkx`); `components/` folders are **pure convention** (zero tooling awareness —
  discovery is asmdef-scoped); UITKX0103 fires only when a `component` decl exists.
- **Godot** (`ReactiveUI-Gadot/addons/reactive_ui/guitkx/guitkx.gd:210-262, 835-990`): same decl
  triad, `use_` prefix warning (2203), module member/junk/nesting/duplicate errors
  (2105/2504/2505), one top-level decl with `_error_on_trailing`.
- **Grammar** (`ide-extensions~/grammar/uitkx.tmLanguage.json`): color quality comes from a
  hand-rolled `#expression-content` mini-tokenizer (7 scopes: keyword.control, entity.name.type,
  constant.numeric, keyword.operator, entity.name.function, variable.other, string.quoted.double)
  + markup↔code mutual recursion (`code-block-body`) + anti-runaway guards (`(?<==)\{` attr-expr
  lookbehind, dedicated string rule, `(?<![=!<])>` tag-end). **No `source.cs` embed.** Element vs
  component tag split is done by LSP semantic tokens there — we do it statically instead (our
  host-tag set is closed: 15 names, pinned by the schema test).

## Grammar spec (.uetkx v2 file grammar)

A `.uetkx` file is ONE of:
1. **Component file** — preamble + exactly one `component Name(params) { setup; return ( markup ); }`.
   Content after the component body → **UETKX2105** error (family: second components, stray code).
2. **Hook/module file** — a SEQUENCE of `hook` and/or `module` declarations (≥1):
   - `hook UseName(params) [-> RetType] { C++ body }` — params `()` REQUIRED (**UETKX2201**),
     name missing → **UETKX2200**, body missing → **UETKX2202**, name not `Use`-prefixed →
     **UETKX2203** warning. Body is plain C++ (no markup); built-in hook calls get the existing
     `Ctx.` auto-prefix. Omitted ret ⇒ `void`.
   - `module Name { verbatim C++ declarations }` — no `()` (**UETKX2205** if body missing,
     **UETKX2204** if name missing). Body emits verbatim (style dicts, types, statics).
   - Junk between decls → **UETKX2105**.

File naming: suffixes (`X.hooks.uetkx`, `X.style.uetkx`) are cosmetic; kind = content. UETKX0103
(name vs basename) applies to component files only — basename compared after stripping dot-suffixes
so `Foo.style.uetkx` never pressures a rename. UETKX2106 (duplicate binding) extends across
component + module names (C++ symbol space).

## Emit spec (C++)

- `hook UseX(A a, B b) -> R { body }` →
  `inline R UseX(FRuiContext& Ctx, A a, B b)\n{\n<body, Ctx.-prefixed builtins>\n}`
  (Ctx threading is the documented divergence from Unity's ambient statics — same altitude as
  the existing PrefixHooks design). No HMR trampolines (our HMR is interp-based; see below).
- `module X { body }` → `namespace X {\n<verbatim body>\n}`.
- **User-hook call sites** in any embedded C++: a call whose callee's last identifier segment
  matches `Use[A-Z]\w*` and is NOT a built-in gets `Ctx` injected as first argument
  (`UseX(a)` → `UseX(Ctx, a)`, `NS::UseX(a)` → `NS::UseX(Ctx, a)`). Corpus-pinned.
- **Aggregator ordering** (NEW, load-bearing): component tags lower to DIRECT calls, so a
  cross-file `<Sub/>` needs Sub's `.inl` earlier in the aggregator TU. Order: hook/module files
  first (alphabetical), then components **topo-sorted by the compile refs** (file A before B when
  B references A's component); a cross-file reference cycle → **UETKX2107** error (C++ divergence:
  C# is declaration-order-free; merge the cycle into one file or break it).

## Folders + demo restructure

`Source/RuiDemo/Screens/<Name>/<Name>.uetkx` (+ companions beside it), subcomponents under
`<Name>/components/<Sub>/<Sub>.uetkx`, recursively — pure convention, FindAll already recurses.
ContextDemo splits: `DemoContextPanel` → `ContextDemo/components/DemoContextPanel/`. Two real
companions exercise the pipeline in-gallery: `ContextDemo/ContextDemo.style.uetkx` (module with
the panel colors) and `SimpleCounter/SimpleCounter.hooks.uetkx` (`hook UseCounter`). Gallery
registration (RuiDemoScreens.cpp names) is unchanged — registry is name-keyed, not path-keyed.

## Coloring (TextMate, VS Code + VS2022 same file)

Port the Unity grammar architecture verbatim, C++-flavored tokenizer (`auto/const/TEXT/nullptr/
int32/...`, `::`, `->`), add: `hook` → keyword.other.hook + entity.name.function name, `module` →
keyword.other.module + entity.name.type name, and a STATIC element-vs-component tag split
(15-host-tag alternation → entity.name.tag; other PascalCase → support.class.component). Drop the
`source.cpp` embed + braceBalance. `configurationDefaults` `[uetkx]`: default formatter +
`insertSpaces:false` (TAB is the family default). Semantic-token overlay = TD (new entry).

## LSP

- `uetkxFileScan.ts` mirrors the new file grammar (corpus-locked — same cases as the C++ tests).
- Diagnostic dedup: sidecar entries identical to a live diag (code+off+len) are skipped
  (owner-reported double UETKX0103 hover).
- Decl-keyword completion (`component`/`hook`/`module`) at file top level.

## HMR stance

Hook/module bodies are arbitrary C++ — no interp swap. Watcher/HMR: a changed non-component file
compiles + sidecars normally, and ApplyFile reports "hook/module change — Live Coding/rebuild
required" instead of an interp error. A component calling a user hook falls back per the existing
unsupported-construct Note path. Delegate-swap HMR for user hooks = TD (Unity's __hmr_ fields).

## Tests

Scanner corpus (hook/module/one-per-file/junk/2200-2205/suffix-0103), codegen tests (hook emit +
Ctx threading + call-site injection, module verbatim, topo order, 2107 cycle), formatter cases
(hook/module headers canonical, bodies verbatim), driver test (companion aggregation order),
contract fixture (hooks+style companion pair), demos suite (restructured gallery renders),
TS corpus replay + smoke, full battery + acceptance + engine-free gates.
