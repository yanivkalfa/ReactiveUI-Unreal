// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-020 — the C++ virtual document. A .uetkx file mixes markup with embedded C++ (component
// `setup` blocks, `hook`/`module` bodies). clangd only understands C++, so we synthesize a C++
// translation unit that concatenates those embedded regions into real function/namespace bodies,
// each copied VERBATIM so a byte in the virtual doc maps 1:1 back to the .uetkx via the source
// map. Hover/completion/definition inside an embedded region then forward to clangd against the
// virtual doc and translate the answer ranges back. Regions outside embedded C++ contribute
// nothing (a request there returns null — markup intelligence is the schema-driven path).

import { collectMarkupReturns, scanFile, UetkxComponentDecl, UetkxHookDecl, UetkxModuleDecl } from "./uetkxFileScan";
import { parseMarkup, UetkxNode } from "./uetkxMarkup";
import { findMarkupRanges } from "./jsxScan";
import { skipNoncode } from "./cppScanner";
import { SourceMap, SourceSpan } from "./sourceMap";
import { codePointToUtf16, fromCodePoints, toCodePoints } from "./codePoints";

export interface VirtualDoc {
  /** The synthesized C++ translation unit. */
  text: string;
  /** Maps offsets between the .uetkx source and `text`. */
  map: SourceMap;
  /** How many embedded regions were lifted (0 = a markup-only file, nothing to forward). */
  regionCount: number;
}

/** The scaffolding prelude: engine context comes from clangd's compile_commands flags; these are
 *  just the surface the embedded snippets reference so a standalone parse still shapes up.
 *  Mirrors the aggregator's auto-included header list (INCLUDE_RETIREMENT_PLAN.md §A,
 *  UetkxDriver.cpp) UNGUARDED — clangd simply fails to resolve a header missing from a
 *  non-interop workspace's compile_commands, degrading exactly as it did before this plan. A
 *  header added to the aggregator prelude should be added here too. */
const guarded = (h: string): string => `#if __has_include("${h}")\n#include "${h}"\n#endif`;
const PRELUDE = [
  "// GENERATED virtual C++ for .uetkx embedded-code intelligence (TD-020). Do not edit.",
  '#include "CoreMinimal.h"',
  // HEADERLESS fallback (N6): without a compile database CoreMinimal.h does not resolve and
  // UE's numeric typedefs are unknown types — clang's recovery then DROPS whole statements
  // from the AST, so clangd's rename returns partial edit sets (bughunt: `int32 X` renamed
  // only its declaration). Native typedefs keep those statements parsing; deliberately NO
  // class stubs (a fake FString would poison overload resolution the moment real headers
  // appear) — statements over unresolved class types stay broken and the rename guard
  // refuses them instead.
  '#if !__has_include("CoreMinimal.h")',
  "typedef signed char int8; typedef short int16; typedef int int32; typedef long long int64;",
  "typedef unsigned char uint8; typedef unsigned short uint16; typedef unsigned int uint32; typedef unsigned long long uint64;",
  "typedef wchar_t TCHAR;",
  "#endif",
  '#include "RuiContext.h"',
  '#include "RuiCoreElements.h"',
  '#include "RuiSignal.h"',
  '#include "RuiSlateElements.h"',
  '#include "RuiStyle.h"',
  '#include "RuiRouter.h"',
  // Optional-module headers are __has_include-guarded EXACTLY like the aggregator prelude
  // (UetkxDriver.cpp): a workspace whose project doesn't link an interop module must not see
  // a file-not-found error in its virtual TU.
  guarded("RuiAssetBrush.h"),
  guarded("RuiFieldHooks.h"),
  guarded("RuiUmgElement.h"),
  guarded("RuiSignalViewModel.h"),
  guarded("RuiHostWidget.h"),
  guarded("RuiWorldSubsystem.h"),
  guarded("RuiActivation.h"),
  guarded("RuiActivatableScreen.h"),
  guarded("RuiMvvmViewModel.h"),
  '#include "UObject/StrongObjectPtr.h"',
  guarded("Engine/Texture2D.h"),
  '#include "Engine/World.h"',
  "using namespace RUI;",
  "",
].join("\n");

function cppParamList(comp: UetkxComponentDecl): string {
  const params = comp.params.map((p) => `${p.type} ${p.name}`).join(", ");
  return params.length > 0 ? `, ${params}` : "";
}

/** The FRuiContext hook members that markup calls BARE (codegen prefixes `Ctx.` — the virtual
 *  doc mirrors that with object-like #defines, so clangd resolves the REAL members and real
 *  types flow through auto). MUST be object-like (function-like would skip `UseState<int32>(…)`
 *  — the `<` blocks expansion) and MUST come after every #include (an include containing one of
 *  these identifiers would be mangled otherwise). Mirrors RuiContext.h's public hook surface. */
const CTX_MEMBER_HOOKS = [
  "ProvideContext", "UseAnimate", "UseCallback", "UseContext", "UseDeferredValue", "UseEffect",
  "UseImperativeHandle", "UseLayoutEffect", "UseMemo", "UseReducer", "UseRef", "UseSafeArea",
  "UseSfx", "UseSignal", "UseSignalKey", "UseStableAction", "UseStableCallback", "UseStableFunc",
  "UseState", "UseTransition", "UseTween", "UseTweenValue",
];

/** Resolves an import specifier to the exporter's emittable surface (hook signatures + module
 *  bodies + ES-modules value/util decls + the default-export marker) — the server wires
 *  uetkxWorkspace.importedSurfaceFor; tests may pass nothing. */
export type ImportSurfaceResolver = (specifier: string) => {
  hooks: Array<{ name: string; ret: string; params: string }>;
  modules: Array<{ name: string; body: string }>;
  components: string[];
  values?: Array<{ name: string; type: string; init: string }>;
  utils?: Array<{ name: string; retType: string; params: string }>;
  defaultExportName?: string;
} | null;

/** Qualified real-header hook calls (`RuiDoom::UseDoomGame(A, B, C)`): codegen inserts `Ctx`
 *  as the first argument, so the REAL declaration has one more parameter than the markup
 *  call. Collect every `Ns::…::UseX` chain (code-aware — comments/strings can't fake one)
 *  and emit a variadic adapter INSIDE the same namespace whose decltype return type derives
 *  from the real function: overload resolution picks the adapter for the Ctx-less call and
 *  the real one for explicit-Ctx calls. */
function scanQualifiedHookCalls(source: string): Array<{ namespaces: string[]; name: string }> {
  const cp = toCodePoints(source);
  const n = cp.length;
  const isIdent = (c: number) => (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c === 95;
  const out = new Map<string, { namespaces: string[]; name: string }>();
  let i = 0;
  while (i < n) {
    const j = skipNoncode(cp, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const wordStart = (i === 0 || !isIdent(cp[i - 1])) && isIdent(cp[i]) && !(cp[i] >= 48 && cp[i] <= 57);
    if (!wordStart) {
      i++;
      continue;
    }
    // Read an ident::ident::…::ident chain.
    const chain: string[] = [];
    let k = i;
    for (;;) {
      const s = k;
      while (k < n && isIdent(cp[k])) k++;
      chain.push(fromCodePoints(cp, s, k - s));
      if (k + 1 < n && cp[k] === 58 /*:*/ && cp[k + 1] === 58) {
        k += 2;
        continue;
      }
      break;
    }
    if (chain.length >= 2 && /^Use[A-Z]/.test(chain[chain.length - 1]) && k < n && (cp[k] === 40 /*(*/ || cp[k] === 60 /*<*/)) {
      const name = chain[chain.length - 1];
      const namespaces = chain.slice(0, -1);
      out.set(chain.join("::"), { namespaces, name });
    }
    i = k > i ? k : i + 1;
  }
  return [...out.values()];
}

/** Every BARE `UseX(`/`UseX<` call name in the source (member/scope-prefixed excluded) — the
 *  hand-written-header hook shape (`UseNavigate()` against RuiRouter.h's
 *  `UseNavigate(FRuiContext&)`). Codegen injects Ctx into these at emit; the virtual doc
 *  mirrors it with the SAME variadic decltype adapter the qualified calls get (F5 field
 *  test: RouterHome's bare router hooks read as "no matching function" without one). The
 *  CALLER excludes built-ins, same-file hooks, and imported-hook shims. */
function scanBareHookCalls(source: string): string[] {
  const cp = toCodePoints(source);
  const n = cp.length;
  const isIdent = (c: number) => (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c === 95;
  const out = new Set<string>();
  let i = 0;
  while (i < n) {
    const j = skipNoncode(cp, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const wordStart = (i === 0 || !isIdent(cp[i - 1])) && isIdent(cp[i]) && !(cp[i] >= 48 && cp[i] <= 57);
    if (!wordStart) {
      i++;
      continue;
    }
    const s = i;
    while (i < n && isIdent(cp[i])) i++;
    const name = fromCodePoints(cp, s, i - s);
    if (!/^Use[A-Z]/.test(name) || i >= n || (cp[i] !== 40 /*(*/ && cp[i] !== 60 /*<*/)) continue;
    // a member/scope prefix means it is NOT the bare hand-header shape
    let prefixed = false;
    for (let k = s - 1; k >= 0; k--) {
      const p = cp[k];
      if (p === 32 || p === 9) continue;
      prefixed = p === 46 /* . */ || (p === 62 /* > */ && k > 0 && cp[k - 1] === 45 /* - */) || (p === 58 /* : */ && k > 0 && cp[k - 1] === 58);
      break;
    }
    if (!prefixed) out.add(name);
  }
  return [...out];
}

/** Per-file virtual-doc prefix (TB-10): the fixed PRELUDE, then the file's OWN host includes
 *  (`import "@X.h"` — without them `RuiDemo::…` symbols are undeclared in the virtual TU),
 *  then REAL declarations for the file's cross-file imports (hook signatures + module
 *  namespaces — clangd types them instead of the server suppressing their errors), then the
 *  bare-hook #define shims and a global Ctx declaration (hook bodies reference Ctx; custom
 *  hooks are emitted WITHOUT a Ctx param so bare in-file calls keep their arity). Order
 *  matters: all #includes and verbatim bodies BEFORE the #defines, or the shims would mangle
 *  any occurrence of a builtin hook name inside them. */
function buildPrefix(scan: ReturnType<typeof scanFile>, source: string, resolveImport?: ImportSurfaceResolver): string {
  const lines: string[] = [PRELUDE];
  for (const imp of scan.imports) {
    if (imp.hostInclude) {
      lines.push(`#include "${imp.specifier}"`);
    }
  }
  // Ctx BEFORE the surfaces/adapters — the qualified-hook adapters' decltype references it.
  lines.push("extern FRuiContext Ctx;");
  // §4 markup-everywhere: the typed placeholder every neutralized markup range lowers to in the
  // virtual TU (`auto X = (<VerticalBox>…);` reads `auto X = (__rui_rn);` — X types FRuiNode,
  // exactly what codegen produces). Unmapped glue: it can never squiggle user code.
  lines.push("extern FRuiNode __rui_rn;");
  const declaredHookShims = new Set<string>();
  if (resolveImport) {
    for (const imp of scan.imports) {
      if (imp.hostInclude) continue;
      const surface = resolveImport(imp.specifier);
      if (!surface) continue; // unresolved — the server's suppression fallback covers it
      // No exclusive branching below — an ES COMBINED import (`import Def, { a } from` /
      // `import Def, * as X from`) carries default + named/star parts in one declaration and
      // EVERY part must declare its surface.
      // ES-modules (G-05): `* as X` — everything exported becomes reachable as `X::Member`;
      // emit the surface inside `namespace X` so clangd types the quals.
      if (imp.isNamespace) {
        lines.push(`namespace ${imp.namespaceAlias} {`);
        for (const h of surface.hooks) {
          const ret = h.ret.trim().length > 0 ? h.ret.trim() : "void";
          lines.push(`${ret} ${h.name}(${h.params.trim()});`);
        }
        for (const v of surface.values ?? []) {
          lines.push(v.type.trim() ? `extern const ${v.type.trim()} ${v.name};` : `inline const auto ${v.name} = ${v.init};`);
        }
        for (const u of surface.utils ?? []) {
          lines.push(`${u.retType.trim()} ${u.name}(${u.params.trim()});`);
        }
        for (const c of surface.components) {
          lines.push(`FRuiNode ${c}();`);
        }
        lines.push(`}`);
      }
      // ES-modules (U-08): a default import binds the target's default symbol under the LOCAL
      // alias name — declare the alias with the default symbol's shape.
      if (imp.isDefault) {
        const def = surface.defaultExportName ?? "";
        if (def) {
          const hook = surface.hooks.find((h) => h.name === def);
          const util = (surface.utils ?? []).find((u) => u.name === def);
          const value = (surface.values ?? []).find((v) => v.name === def);
          if (hook) {
            const ret = hook.ret.trim().length > 0 ? hook.ret.trim() : "void";
            lines.push(`${ret} ${imp.defaultAlias}(${hook.params.trim()});`);
            declaredHookShims.add(imp.defaultAlias);
          } else if (util) {
            lines.push(`${util.retType.trim()} ${imp.defaultAlias}(${util.params.trim()});`);
          } else if (value) {
            lines.push(value.type.trim() ? `extern const ${value.type.trim()} ${imp.defaultAlias};` : `extern const FRuiNode ${imp.defaultAlias};`);
          } else {
            lines.push(`FRuiNode ${imp.defaultAlias}();`); // component default (the common case)
          }
        }
      }
      if (imp.names.length === 0) continue;
      // Rename-aware (`{ A as B }`): the surface is looked up by TARGET name, declared under the
      // LOCAL name — exactly what the codegen alias plane does at emit.
      const localOf = new Map<string, string>();
      imp.names.forEach((name, n) => localOf.set(name, imp.localNames[n] ?? name));
      for (const h of surface.hooks) {
        const local = localOf.get(h.name);
        if (local) {
          const ret = h.ret.trim().length > 0 ? h.ret.trim() : "void";
          lines.push(`${ret} ${local}(${h.params.trim()});`);
          declaredHookShims.add(local);
        }
      }
      for (const m of surface.modules) {
        const local = localOf.get(m.name);
        if (local) {
          lines.push(`namespace ${local} {`, m.body, `}`);
        }
      }
      for (const v of surface.values ?? []) {
        const local = localOf.get(v.name);
        if (local) {
          lines.push(v.type.trim() ? `extern const ${v.type.trim()} ${local};` : `inline const auto ${local} = ${v.init};`);
        }
      }
      for (const u of surface.utils ?? []) {
        const local = localOf.get(u.name);
        if (local) {
          lines.push(`${u.retType.trim()} ${local}(${u.params.trim()});`);
        }
      }
      for (const c of surface.components) {
        const local = localOf.get(c);
        if (local) {
          lines.push(`FRuiNode ${local}();`); // the generated wrapper is fully-defaulted
        }
      }
    }
  }
  for (const q of scanQualifiedHookCalls(source)) {
    const open = q.namespaces.map((ns) => `namespace ${ns} { `).join("");
    const close = q.namespaces.map(() => "}").join(" ");
    lines.push(
      `${open}template <typename... TArgs> auto ${q.name}(TArgs&&... Args) -> decltype(${q.name}(Ctx, static_cast<TArgs&&>(Args)...)); ${close}`,
    );
  }
  // Bare hand-header hooks (RuiRouter.h et al: free functions taking Ctx first) — codegen
  // injects Ctx at emit; mirror with the same variadic decltype adapter the qualified calls
  // get. Built-ins (#define'd below), same-file hooks, and imported shims keep source arity.
  const bareExcluded = new Set<string>([...CTX_MEMBER_HOOKS, "UseSignal", "UseSignalKey", ...scan.hooks.map((h) => h.name), ...declaredHookShims]);
  for (const name of scanBareHookCalls(source)) {
    if (bareExcluded.has(name)) continue;
    lines.push(`template <typename... TArgs> auto ${name}(TArgs&&... Args) -> decltype(${name}(Ctx, static_cast<TArgs&&>(Args)...));`);
  }
  for (const hook of CTX_MEMBER_HOOKS) {
    lines.push(`#define ${hook} Ctx.${hook}`);
  }
  lines.push("");
  return lines.join("\n");
}

/** Build the virtual C++ document + source map for a .uetkx source. */
export function buildVirtualCpp(source: string, basename = "doc", resolveImport?: ImportSurfaceResolver): VirtualDoc {
  const scan = scanFile(source, basename);
  const prefix = buildPrefix(scan, source, resolveImport);
  const parts: string[] = [prefix];
  const spans: SourceSpan[] = [];
  let vir = prefix.length;

  const emit = (regionText: string, srcAt: number, prefix: string, suffix: string): void => {
    if (srcAt < 0 || regionText.length === 0) {
      return;
    }
    parts.push(prefix);
    vir += prefix.length;
    // scanFile offsets are CODE POINTS, but the virtual side and every server consumer (doc.offsetAt /
    // doc.positionAt) are UTF-16 — convert srcStart to UTF-16 (bughunt B14: without this, an astral-plane
    // character before a region shifted the map, so hover/definition landed on the wrong token).
    spans.push({ srcStart: codePointToUtf16(source, srcAt), virStart: vir, length: regionText.length });
    parts.push(regionText);
    vir += regionText.length;
    parts.push(suffix);
    vir += suffix.length;
  };

  const raw = (text: string): void => {
    parts.push(text);
    vir += text.length;
  };

  // ── §4 markup-everywhere: EVERY code emission is jsx-aware. Markup ranges nested in code
  // (`auto X = (<VerticalBox>…);`, `cond && <Chip/>`) neutralize to the typed `__rui_rn`
  // placeholder (short-circuit ops desugar `? __rui_rn : __rui_rn`, mirroring codegen's
  // ternary), the code segments around them stay EXACT mapped substrings, and each range's
  // parsed tree is returned for the caller to lift at the nearest STATEMENT context (its attr
  // exprs get their own mapped regions there). `excludeSpans` marks early-return windows in
  // setup: their glue still neutralizes, but their roots are lifted by the caller (comp.returns)
  // — parsing them here would double-lift. Ranges never sever the map's byte-identity contract:
  // only whole segments are emitted, glue is raw.
  type Deferred = { node: UetkxNode; base: number };
  const emitCode = (
    text: string,
    srcAt: number,
    prefix: string,
    suffix: string,
    excludeSpans?: ReadonlyArray<{ mStart: number; mEnd: number; returnAt: number; afterParen: number }>,
  ): Deferred[] => {
    const cp = toCodePoints(text);
    type Neutral = { start: number; end: number; op: string; opPos: number; parse: boolean };
    const neutrals: Neutral[] = findMarkupRanges(cp, 0, cp.length).map((r) => ({
      start: r.start,
      end: r.end === -1 ? cp.length : r.end,
      op: r.op,
      opPos: r.opPos,
      parse: true,
    }));
    if (excludeSpans) {
      for (const nr of neutrals) {
        if (excludeSpans.some((s) => nr.start >= s.returnAt && nr.start < s.afterParen)) nr.parse = false;
      }
      // directive-form early-return windows (`return ( @if … )`) are not jsx ranges — neutralize
      // the whole window so raw `@if` text never reaches clangd (their roots lift via comp.returns).
      for (const s of excludeSpans) {
        if (s.mStart < cp.length && !neutrals.some((nr) => nr.start >= s.returnAt && nr.start < s.afterParen)) {
          neutrals.push({ start: s.mStart, end: Math.min(s.mEnd, cp.length), op: "", opPos: s.mStart, parse: false });
        }
      }
      neutrals.sort((a, b) => a.start - b.start);
    }
    if (neutrals.length === 0) {
      emit(text, srcAt, prefix, suffix);
      return [];
    }
    raw(prefix);
    const deferred: Deferred[] = [];
    let cursor = 0;
    for (const r of neutrals) {
      if (r.start < cursor) continue;
      const upTo = r.op !== "" ? r.opPos : r.start;
      const seg = fromCodePoints(cp, cursor, upTo - cursor);
      if (seg.length > 0) emit(seg, srcAt + cursor, "", "");
      raw(r.op !== "" ? " ? __rui_rn : __rui_rn" : "__rui_rn");
      if (r.parse) {
        const parsed = parseMarkup(cp, r.start, r.end);
        if (!parsed.errorCode) {
          for (const n of parsed.nodes) deferred.push({ node: n, base: srcAt });
        }
      }
      cursor = r.end;
    }
    const tail = fromCodePoints(cp, cursor, cp.length - cursor);
    if (tail.length > 0) emit(tail, srcAt + cursor, "", "");
    raw(suffix);
    return deferred;
  };

  const flushDeferred = (deferred: Deferred[]): void => {
    for (const d of deferred) liftNode(d.node, d.base);
  };

  // ── §2 (MARKUP_EVERYWHERE_PLAN): lift every markup EXPRESSION into the component's
  // function scope so clangd sees ALL embedded C++, not just the setup. A body is a body:
  // directives mirror as REAL C++ control flow (`if`/`for`/`while` with their actual
  // condition/header text) so directive-local declarations scope correctly for the
  // expressions nested under them. Event handlers wrap in codegen's exact lambda shape
  // (`[=](const FRuiValue& Value)`) so `Value.…` types for real. Every emitted region is an
  // EXACT source substring (the parser's trimmed-start offsets guarantee it) — the source
  // map's byte-identity contract is untouched.
  const liftBody = (bodyText: string, absAt: number): void => {
    // A directive body is a mini-body: verbatim statements + markup returns (wave G).
    // Slices go through code points — bodyText.slice() with cp indices would shear on
    // non-BMP content (the B14 lesson).
    const cp = toCodePoints(bodyText);
    const returns = collectMarkupReturns(cp);
    let cursor = 0;
    for (const span of returns) {
      const stmts = fromCodePoints(cp, cursor, span.returnAt - cursor);
      if (stmts.trim().length > 0) {
        flushDeferred(emitCode(stmts, absAt + cursor, "\n", "\n"));
      }
      const parsed = parseMarkup(cp, span.mStart, span.mEnd);
      if (!parsed.errorCode) {
        for (const n of parsed.nodes) liftNode(n, absAt);
      }
      cursor = span.afterParen;
    }
    const tail = fromCodePoints(cp, cursor, cp.length - cursor);
    if (tail.trim().length > 0) {
      flushDeferred(emitCode(tail, absAt + cursor, "\n", "\n"));
    }
  };

  const liftNode = (node: UetkxNode, base: number): void => {
    switch (node.type) {
      case "el":
      case "frag": {
        for (const a of node.attrs ?? []) {
          if (a.kind === "expr" || a.kind === "spread") {
            if (/^On[A-Z]/.test(a.name)) {
              // The event-handler shape codegen emits (verified: SimpleTextField.uetkx.inl).
              flushDeferred(emitCode(a.value, base + a.vat, "\n{ (void)[=](const FRuiValue& Value) { (void)(\n", "\n); }; }\n"));
            } else {
              flushDeferred(emitCode(a.value, base + a.vat, "\n{ (void)(\n", "\n); }\n"));
            }
          }
        }
        for (const c of node.children ?? []) liftNode(c, base);
        break;
      }
      case "expr":
        if (node.code !== undefined && node.vat !== undefined) {
          flushDeferred(emitCode(node.code, base + node.vat, "\n{ (void)(\n", "\n); }\n"));
        }
        break;
      case "if": {
        const branches = node.branches ?? [];
        for (let k = 0; k < branches.length; k++) {
          const br = branches[k];
          if (br.condAt < 0) continue;
          raw(k === 0 ? "\nif (" : "\nelse if (");
          const condDeferred = emitCode(br.cond, base + br.condAt, "", "");
          raw(") {\n");
          flushDeferred(condDeferred); // markup nested in the cond lifts inside the branch scope
          if (br.bodyAt >= 0) liftBody(br.bodyMarkup, base + br.bodyAt);
          raw("}\n");
        }
        if (node.elseBody !== undefined && node.elseBodyAt !== undefined && node.elseBodyAt >= 0) {
          raw("\nelse {\n");
          liftBody(node.elseBody, base + node.elseBodyAt);
          raw("}\n");
        }
        break;
      }
      case "for":
      case "while": {
        if (node.headerAt === undefined || node.headerAt < 0 || node.header === undefined) break;
        raw(node.type === "for" ? "\nfor (" : "\nwhile (");
        const headerDeferred = emitCode(node.header, base + node.headerAt, "", "");
        raw(") {\n");
        flushDeferred(headerDeferred); // header locals stay in scope for the lifted markup
        if (node.bodyMarkup !== undefined && node.bodyAt !== undefined && node.bodyAt >= 0) {
          liftBody(node.bodyMarkup, base + node.bodyAt);
        }
        raw("}\n");
        break;
      }
      case "match": {
        raw("\n{\n");
        if (node.subject !== undefined && node.subjectAt !== undefined && node.subjectAt >= 0) {
          flushDeferred(emitCode(node.subject, base + node.subjectAt, "{ (void)(\n", "\n); }\n"));
        }
        for (const c of node.cases ?? []) {
          raw("{\n");
          if (c.valueAt >= 0) flushDeferred(emitCode(c.value, base + c.valueAt, "{ (void)(\n", "\n); }\n"));
          if (c.bodyAt >= 0) liftBody(c.bodyMarkup, base + c.bodyAt);
          raw("}\n");
        }
        if (node.defaultBody !== undefined && node.defaultBodyAt !== undefined && node.defaultBodyAt >= 0) {
          raw("{\n");
          liftBody(node.defaultBody, base + node.defaultBodyAt);
          raw("}\n");
        }
        raw("}\n");
        break;
      }
      case "text":
      case "comment":
        break;
    }
  };

  for (const comp of scan.components) {
    // The function scaffold is raw'd (not emit-prefixed) so a return-only component with an
    // EMPTY setup still gets a scope for its lifted markup expressions (HelloWorld shape).
    raw(`\nvoid __rui_setup_${comp.name}(FRuiContext& Ctx${cppParamList(comp)}) {\n`);
    // §4: setup is jsx-aware — value markup neutralizes to __rui_rn (its attr exprs lift as
    // deferred statements below), early-return windows neutralize whole (their roots lift via
    // comp.returns — excludeSpans stops the double-lift).
    flushDeferred(emitCode(comp.setup, comp.setupAt, "", "\n", comp.returns ?? []));
    for (const span of comp.returns ?? []) {
      if (span.root) liftNode(span.root, comp.setupAt);
    }
    raw("}\n");
  }
  for (const hook of scan.hooks as UetkxHookDecl[]) {
    const ret = hook.ret.trim().length > 0 ? hook.ret.trim() : "void";
    // NO Ctx param (TB-10): in-file bare calls (`UseCounter(0)`) must keep their arity; the
    // body's Ctx references resolve against the prefix's global declaration instead.
    raw(`\n${ret} ${hook.name}(${hook.params.trim()}) {\n`);
    flushDeferred(emitCode(hook.body, hook.bodyAt, "", "\n"));
    raw("}\n");
  }
  for (const mod of scan.modules as UetkxModuleDecl[]) {
    emit(mod.body, mod.bodyAt, `\nnamespace ${mod.name} {\n`, "\n}\n");
  }
  // ES-modules (M6): value initializers + util bodies are REAL mapped C++ regions — clangd
  // types them (completion/hover/diagnostics) exactly like setup/hook bodies.
  for (const value of scan.values) {
    const type = value.type.trim() || "auto";
    emit(value.init, value.initAt, `\nconst ${type} ${value.name} =\n`, ";\n");
  }
  for (const util of scan.utils) {
    raw(`\n${util.retType.trim()} ${util.name}(${util.params.trim()}) {\n`);
    flushDeferred(emitCode(util.body, util.bodyAt, "", "\n"));
    raw("}\n");
  }

  return { text: parts.join(""), map: new SourceMap(spans), regionCount: spans.length };
}
