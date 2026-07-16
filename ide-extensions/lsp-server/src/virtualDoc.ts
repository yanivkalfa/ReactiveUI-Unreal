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
  '#include "RuiContext.h"',
  '#include "RuiCoreElements.h"',
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

/** Build the virtual C++ document + source map for a .uetkx source. */
export function buildVirtualCpp(source: string, basename = "doc"): VirtualDoc {
  const scan = scanFile(source, basename);
  const parts: string[] = [PRELUDE];
  const spans: SourceSpan[] = [];
  let vir = PRELUDE.length;

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
    const params = hook.params.trim().length > 0 ? `, ${hook.params.trim()}` : "";
    emit(hook.body, hook.bodyAt, `\n${ret} ${hook.name}(FRuiContext& Ctx${params}) {\n`, "\n}\n");
  }
  for (const mod of scan.modules as UetkxModuleDecl[]) {
    emit(mod.body, mod.bodyAt, `\nnamespace ${mod.name} {\n`, "\n}\n");
  }

  return { text: parts.join(""), map: new SourceMap(spans), regionCount: spans.length };
}
