// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-020 — the C++ virtual document. A .uetkx file mixes markup with embedded C++ (component
// `setup` blocks, `hook`/`module` bodies). clangd only understands C++, so we synthesize a C++
// translation unit that concatenates those embedded regions into real function/namespace bodies,
// each copied VERBATIM so a byte in the virtual doc maps 1:1 back to the .uetkx via the source
// map. Hover/completion/definition inside an embedded region then forward to clangd against the
// virtual doc and translate the answer ranges back. Regions outside embedded C++ contribute
// nothing (a request there returns null — markup intelligence is the schema-driven path).

import { scanFile, UetkxComponentDecl, UetkxHookDecl, UetkxModuleDecl } from "./uetkxFileScan";
import { SourceMap, SourceSpan } from "./sourceMap";
import { codePointToUtf16 } from "./codePoints";

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
const PRELUDE = [
  "// GENERATED virtual C++ for .uetkx embedded-code intelligence (TD-020). Do not edit.",
  '#include "CoreMinimal.h"',
  '#include "RuiContext.h"',
  '#include "RuiCoreElements.h"',
  '#include "RuiSignal.h"',
  '#include "RuiSlateElements.h"',
  '#include "RuiStyle.h"',
  '#include "RuiRouter.h"',
  '#include "RuiAssetBrush.h"',
  '#include "RuiFieldHooks.h"',
  '#include "RuiUmgElement.h"',
  '#include "RuiSignalViewModel.h"',
  '#include "RuiHostWidget.h"',
  '#include "RuiWorldSubsystem.h"',
  '#include "RuiActivation.h"',
  '#include "RuiActivatableScreen.h"',
  '#include "RuiMvvmViewModel.h"',
  '#include "UObject/StrongObjectPtr.h"',
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
 *  bodies) — the server wires uetkxWorkspace.importedSurfaceFor; tests may pass nothing. */
export type ImportSurfaceResolver = (specifier: string) => {
  hooks: Array<{ name: string; ret: string; params: string }>;
  modules: Array<{ name: string; body: string }>;
} | null;

/** Per-file virtual-doc prefix (TB-10): the fixed PRELUDE, then the file's OWN host includes
 *  (`import "@X.h"` — without them `RuiDemo::…` symbols are undeclared in the virtual TU),
 *  then REAL declarations for the file's cross-file imports (hook signatures + module
 *  namespaces — clangd types them instead of the server suppressing their errors), then the
 *  bare-hook #define shims and a global Ctx declaration (hook bodies reference Ctx; custom
 *  hooks are emitted WITHOUT a Ctx param so bare in-file calls keep their arity). Order
 *  matters: all #includes and verbatim bodies BEFORE the #defines, or the shims would mangle
 *  any occurrence of a builtin hook name inside them. */
function buildPrefix(scan: ReturnType<typeof scanFile>, resolveImport?: ImportSurfaceResolver): string {
  const lines: string[] = [PRELUDE];
  for (const imp of scan.imports) {
    if (imp.hostInclude) {
      lines.push(`#include "${imp.specifier}"`);
    }
  }
  if (resolveImport) {
    for (const imp of scan.imports) {
      if (imp.hostInclude || imp.names.length === 0) continue;
      const surface = resolveImport(imp.specifier);
      if (!surface) continue; // unresolved — the server's suppression fallback covers it
      const wanted = new Set(imp.names);
      for (const h of surface.hooks) {
        if (wanted.has(h.name)) {
          const ret = h.ret.trim().length > 0 ? h.ret.trim() : "void";
          lines.push(`${ret} ${h.name}(${h.params.trim()});`);
        }
      }
      for (const m of surface.modules) {
        if (wanted.has(m.name)) {
          lines.push(`namespace ${m.name} {`, m.body, `}`);
        }
      }
    }
  }
  lines.push("extern FRuiContext Ctx;");
  for (const hook of CTX_MEMBER_HOOKS) {
    lines.push(`#define ${hook} Ctx.${hook}`);
  }
  lines.push("");
  return lines.join("\n");
}

/** Build the virtual C++ document + source map for a .uetkx source. */
export function buildVirtualCpp(source: string, basename = "doc", resolveImport?: ImportSurfaceResolver): VirtualDoc {
  const scan = scanFile(source, basename);
  const prefix = buildPrefix(scan, resolveImport);
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

  for (const comp of scan.components) {
    emit(comp.setup, comp.setupAt, `\nvoid __rui_setup_${comp.name}(FRuiContext& Ctx${cppParamList(comp)}) {\n`, "\n}\n");
  }
  for (const hook of scan.hooks as UetkxHookDecl[]) {
    const ret = hook.ret.trim().length > 0 ? hook.ret.trim() : "void";
    // NO Ctx param (TB-10): in-file bare calls (`UseCounter(0)`) must keep their arity; the
    // body's Ctx references resolve against the prefix's global declaration instead.
    emit(hook.body, hook.bodyAt, `\n${ret} ${hook.name}(${hook.params.trim()}) {\n`, "\n}\n");
  }
  for (const mod of scan.modules as UetkxModuleDecl[]) {
    emit(mod.body, mod.bodyAt, `\nnamespace ${mod.name} {\n`, "\n}\n");
  }

  return { text: parts.join(""), map: new SourceMap(spans), regionCount: spans.length };
}
