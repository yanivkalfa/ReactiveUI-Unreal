// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TypeScript port of FUetkxFileScan (UetkxFileScan.cpp): the .uetkx FILE grammar — preamble
// includes, `component Name(params) {}` declarations, the shared last-top-level-markup-return
// splitter, hook-call scanning and the FNV hook signature. All offsets are code points.

import { findMatching, findMatchingMarkup, keywordAt, skipNoncode, skipNoncodeMarkup } from "./cppScanner";
import { fromCodePoints } from "./codePoints";
import { parseMarkup, UetkxNode } from "./uetkxMarkup";

const C_NL = 10,
  C_CR = 13,
  C_SPACE = 32,
  C_TAB = 9,
  C_HASH = 35,
  C_LT = 60,
  C_AT = 64,
  C_LPAREN = 40,
  C_LBRACE = 123,
  C_QUOTE = 34,
  C_APOS = 39,
  C_COMMA = 44;

export interface UetkxDiag {
  code: string;
  severity: number; // 0 err / 1 warn / 2 hint
  message: string;
  offset: number;
  length: number;
}

export interface UetkxParam {
  name: string;
  type: string;
  default: string;
}

/** The three declaration kinds a .uetkx file may hold, in any number/order (mixed-decl v1). */
export type UetkxDeclKind = "component" | "hook" | "module";

/** `import { A, B } from "specifier"` — a preamble-only static import (named bindings only).
 *
 *  INCLUDE_RETIREMENT_PLAN.md §B: `import "@Header.h"` is a second, braces/`from`-less shape —
 *  a nameless HOST INCLUDE (family shape, ported from Unity's `import "@Namespace"`; our
 *  payload is a header path). hostInclude=true means names/nameAts are EMPTY BY DESIGN;
 *  specifier holds the payload WITHOUT the leading `@`. C++-identical (FUetkxImportDecl). */
export interface UetkxImportDecl {
  names: string[];
  nameAts: number[];
  specifier: string;
  specifierAt: number; // offset of the opening quote
  at: number; // offset of the `import` keyword
  hostInclude: boolean; // `import "@Header.h"` — see the interface comment
}

/** One markup `return ( ... )` span inside a component body (wave G early returns).
 *  Offsets are into the BODY code points; C++-identical (FUetkxReturnSpan). */
export interface UetkxReturnSpan {
  returnAt: number;
  mStart: number;
  mEnd: number;
  afterParen: number;
  topLevel: boolean; // at brace+paren depth 0 (a statement of the body itself)
  root: UetkxNode | null; // the span's single render root (filled by the component scan)
}

export interface UetkxComponentDecl {
  name: string;
  exported: boolean; // `export component`
  at: number; // the `component` keyword offset
  exportAt: number; // the `export` keyword offset when exported, else -1 (the decl's true start)
  nameAt: number;
  params: UetkxParam[];
  setup: string;
  setupAt: number;
  body: string;
  bodyAt: number;
  windowNodes: UetkxNode[];
  /** ALL markup returns in source order (wave G): the last one is the top-level anchor;
   *  earlier ones are early returns the C++ emitter splices in place. */
  returns: UetkxReturnSpan[];
  hookCalls: string[];
  next: number;
}

export interface UetkxHookDecl {
  name: string;
  exported: boolean; // `export hook`
  at: number;
  exportAt: number; // the `export` keyword offset when exported, else -1 (the decl's true start)
  nameAt: number;
  params: string; // verbatim C++ parameter list
  ret: string; // verbatim return type; "" = void
  body: string;
  bodyAt: number;
  next: number;
}

export interface UetkxModuleDecl {
  name: string;
  exported: boolean; // `export module`
  at: number;
  exportAt: number; // the `export` keyword offset when exported, else -1 (the decl's true start)
  nameAt: number;
  body: string; // verbatim C++
  bodyAt: number;
  next: number;
}

export interface UetkxFileScanResult {
  preambleIncludes: string[];
  preambleIncludeAts: number[]; // 1:1 with preambleIncludes — each line's start offset
  imports: UetkxImportDecl[];
  components: UetkxComponentDecl[];
  hooks: UetkxHookDecl[];
  modules: UetkxModuleDecl[];
  order: { kind: UetkxDeclKind; index: number }[]; // source order across all three arrays
  diags: UetkxDiag[];
}

export function hasError(result: UetkxFileScanResult): boolean {
  return result.diags.some((d) => d.severity === 0);
}

export const HOOK_NAMES = [
  "UseState",
  "UseReducer",
  "UseRef",
  "UseMemo",
  "UseCallback",
  "UseEffect",
  "UseLayoutEffect",
  "UseContext",
  "ProvideContext",
  "UseDeferredValue",
  "UseTransition",
  "UseStableCallback",
  "UseStableFunc",
  "UseStableAction",
  "UseImperativeHandle",
  "UseSafeArea",
  "UseSignal",
  "UseSignalKey",
  "UseTween",
  "UseAnimate",
  "UseTweenValue",
  "UseSfx",
];

/** Headers the aggregator's generated prelude ALREADY provides (INCLUDE_RETIREMENT_PLAN.md
 *  §A) — an explicit `#include` or `import "@X.h"` naming one of these is redundant (UETKX2317
 *  hint). MUST match UetkxDriver.cpp's aggregator prelude and virtualDoc.ts's PRELUDE exactly.
 *  C++-identical (FUetkxFileScan::AutoIncludedHeaders). */
export const AUTO_INCLUDED_HEADERS = [
  "CoreMinimal.h",
  "RuiContext.h",
  "RuiCoreElements.h",
  "RuiSignal.h",
  "RuiSlateElements.h",
  "RuiStyle.h",
  "RuiRouter.h",
  "RuiAssetBrush.h",
  "RuiFieldHooks.h",
  "RuiUmgElement.h",
  "RuiSignalViewModel.h",
  "RuiHostWidget.h",
  "RuiWorldSubsystem.h",
  "RuiActivation.h",
  "RuiActivatableScreen.h",
  "RuiMvvmViewModel.h",
  "UObject/StrongObjectPtr.h",
  "Engine/World.h",
];

/** The quoted or angle-bracket header path inside a `#include "X.h"` / `#include <X.h>` line,
 *  or "" if malformed (UETKX2317 hint). C++-identical (ExtractIncludeHeader). */
function extractIncludeHeader(line: string): string {
  let open = line.indexOf('"');
  let closeChar = '"';
  if (open === -1) {
    open = line.indexOf("<");
    closeChar = ">";
    if (open === -1) return "";
  }
  const close = line.indexOf(closeChar, open + 1);
  if (close === -1) return "";
  return line.slice(open + 1, close);
}

/** FNV-1a over the ordered hook-kind sequence with ';' separators (C++-identical). */
export function hookSignature(hookCalls: readonly string[]): number {
  let h = 2166136261 >>> 0;
  for (const call of hookCalls) {
    for (let i = 0; i < call.length; i++) {
      h = (h ^ call.charCodeAt(i)) >>> 0;
      h = Math.imul(h, 16777619) >>> 0;
    }
    h = (h ^ 59) >>> 0; // ';'
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h >>> 0;
}

export interface SplitReturn {
  ok: boolean;
  returnAt: number;
  mStart: number;
  mEnd: number;
  afterParen: number;
}

function skipWsOnly(s: readonly number[], i: number): number {
  while (i < s.length && (s[i] === C_SPACE || s[i] === C_TAB || s[i] === C_NL || s[i] === C_CR)) i++;
  return i;
}

/** The LAST top-level markup `return ( ... )` — the ONE splitter shared with the C++ file
 *  scan / emitter / formatter (they must never disagree on the window). */
export function splitMarkupReturn(body: readonly number[], requireMarkupPeek: boolean): SplitReturn {
  const out: SplitReturn = { ok: false, returnAt: -1, mStart: -1, mEnd: -1, afterParen: -1 };
  const n = body.length;
  let depth = 0;
  let i = 0;
  while (i < n) {
    const j = skipNoncode(body, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = body[i];
    if (c === C_LBRACE || c === C_LPAREN || c === 91 /*[*/) {
      depth++;
      i++;
      continue;
    }
    if (c === 125 /*}*/ || c === 41 /*)*/ || c === 93 /*]*/) {
      depth--;
      i++;
      continue;
    }
    if (depth === 0 && c === 114 /*r*/ && keywordAt(body, i, "return")) {
      const p = skipWsOnly(body, i + 6);
      if (p < n && body[p] === C_LPAREN) {
        let markup = true;
        if (requireMarkupPeek) {
          let first = skipWsOnly(body, p + 1);
          for (;;) {
            const skipped = skipNoncodeMarkup(body, first);
            if (skipped === first) break;
            first = skipWsOnly(body, skipped);
          }
          markup = first < n && (body[first] === C_LT || body[first] === C_AT || body[first] === C_LBRACE);
        }
        const close = markup ? findMatchingMarkup(body, p) : findMatching(body, p);
        if (close === -1) {
          i++;
          continue;
        }
        if (markup) {
          out.ok = true; // keep scanning: the LAST one wins
          out.returnAt = i;
          out.mStart = p + 1;
          out.mEnd = close;
          out.afterParen = close + 1;
        }
        i = close + 1;
        continue;
      }
      i += 6;
      continue;
    }
    i++;
  }
  return out;
}

/** ALL markup returns in a component body, source order (wave G early returns). Paren/
 *  bracket nesting hides a `return` (call arguments); brace nesting does NOT (if/else
 *  blocks — the early-return shape). The window must peek like markup: `<`/`@` anywhere,
 *  `{` (expression window) only at top level. C++-identical (CollectMarkupReturns). */
export function collectMarkupReturns(body: readonly number[]): UetkxReturnSpan[] {
  const out: UetkxReturnSpan[] = [];
  const n = body.length;
  let parenDepth = 0; // parens + brackets hide returns (call args); braces do NOT (if/else)
  let braceDepth = 0;
  let i = 0;
  while (i < n) {
    const j = skipNoncode(body, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = body[i];
    if (c === C_LPAREN || c === 91 /*[*/) {
      parenDepth++;
      i++;
      continue;
    }
    if (c === 41 /*)*/ || c === 93 /*]*/) {
      parenDepth--;
      i++;
      continue;
    }
    if (c === C_LBRACE) {
      braceDepth++;
      i++;
      continue;
    }
    if (c === 125 /*}*/) {
      braceDepth--;
      i++;
      continue;
    }
    if (parenDepth === 0 && c === 114 /*r*/ && keywordAt(body, i, "return")) {
      const p = skipWsOnly(body, i + 6);
      if (p < n && body[p] === C_LPAREN) {
        let first = skipWsOnly(body, p + 1);
        for (;;) {
          const skipped = skipNoncodeMarkup(body, first);
          if (skipped === first) break;
          first = skipWsOnly(body, skipped);
        }
        const topLevel = braceDepth === 0;
        const markup = first < n && (body[first] === C_LT || body[first] === C_AT || (body[first] === C_LBRACE && topLevel));
        const close = markup ? findMatchingMarkup(body, p) : findMatching(body, p);
        if (close === -1) {
          i++;
          continue;
        }
        if (markup) {
          out.push({ returnAt: i, mStart: p + 1, mEnd: close, afterParen: close + 1, topLevel, root: null });
        }
        i = close + 1;
        continue;
      }
      i += 6;
      continue;
    }
    i++;
  }
  return out;
}

/** Family param grammar `Name: Type = Default, ...` with depth-aware splitting. */
export function parseParams(paramText: string): UetkxParam[] {
  const out: UetkxParam[] = [];
  const pieces: string[] = [];
  let depth = 0;
  let cur = "";
  for (const ch of paramText) {
    if (ch === "(" || ch === "[" || ch === "{" || ch === "<") depth++;
    else if (ch === ")" || ch === "]" || ch === "}" || ch === ">") depth--;
    if (ch === "," && depth === 0) {
      pieces.push(cur);
      cur = "";
      continue;
    }
    cur += ch;
  }
  if (cur.trim()) pieces.push(cur);
  for (const piece of pieces) {
    let rest = piece;
    let def = "";
    let d2 = 0;
    let eqIdx = -1;
    for (let i = 0; i < rest.length; i++) {
      const c = rest[i];
      if (c === "(" || c === "[" || c === "{" || c === "<") d2++;
      else if (c === ")" || c === "]" || c === "}" || c === ">") d2--;
      else if (c === "=" && d2 === 0 && rest[i + 1] !== "=") {
        eqIdx = i;
        break;
      }
    }
    if (eqIdx !== -1) {
      def = rest.slice(eqIdx + 1).trim();
      rest = rest.slice(0, eqIdx);
    }
    const colon = rest.indexOf(":");
    const name = (colon === -1 ? rest : rest.slice(0, colon)).trim();
    const type = (colon === -1 ? "" : rest.slice(colon + 1)).trim();
    if (name) out.push({ name, type, default: def });
  }
  return out;
}

function scanHookCalls(setup: readonly number[]): string[] {
  const out: string[] = [];
  const n = setup.length;
  let i = 0;
  while (i < n) {
    const j = skipNoncode(setup, i);
    if (j !== i) {
      i = j;
      continue;
    }
    if (setup[i] === 85 /*U*/ || setup[i] === 80 /*P*/) {
      for (const hook of HOOK_NAMES) {
        if (keywordAt(setup, i, hook)) {
          const k = skipWsOnly(setup, i + hook.length);
          if (k < n && (setup[k] === C_LPAREN || setup[k] === C_LT)) {
            out.push(hook);
            i += hook.length;
            break;
          }
        }
      }
    }
    i++;
  }
  return out;
}

function pushDiag(
  out: UetkxFileScanResult,
  code: string,
  severity: number,
  message: string,
  offset: number,
  length = 1,
): void {
  out.diags.push({ code, severity, message, offset, length });
}

/** Advance past the rest of the current line (error resync for a malformed preamble import). */
function advancePastLine(src: number[], i: number): number {
  const n = src.length;
  while (i < n && src[i] !== C_NL) i++;
  return i < n ? i + 1 : i;
}

/** Parse a preamble import at `start` — either the NAMED form `import { A, B } from "spec"`
 *  (named exports only — no `*`, no default, A1) or the HOST INCLUDE form `import "@Header.h"`
 *  (INCLUDE_RETIREMENT_PLAN.md §B — no braces, no `from`, no names). Records the decl +
 *  duplicate-import diagnostics (UETKX2303); importedFrom tracks name -> first specifier for
 *  named imports and payload -> payload for host includes (keyspaces cannot collide). Returns
 *  the index past the import (or resync point). C++-identical (ParseImport). */
function parseImport(src: number[], start: number, out: UetkxFileScanResult, importedFrom: Map<string, string>): number {
  const n = src.length;
  let k = skipWsOnly(src, start + 6); // past "import"

  // INCLUDE_RETIREMENT_PLAN.md §B: `import "@Header.h"` — a nameless HOST INCLUDE. The named
  // form always starts `{`; a leading quote routes here instead (no `from` clause).
  if (k < n && (src[k] === C_QUOTE || src[k] === C_APOS)) {
    const quote = src[k];
    const quoteAt = k;
    let q = k + 1;
    while (q < n && src[q] !== quote && src[q] !== C_NL) q++;
    if (q >= n || src[q] !== quote) {
      pushDiag(out, "UETKX0300", 0, "unterminated import specifier string", quoteAt);
      return advancePastLine(src, quoteAt);
    }
    const payload = fromCodePoints(src, quoteAt + 1, q - (quoteAt + 1));
    const end = q + 1;
    if (!payload.startsWith("@")) {
      pushDiag(out, "UETKX0303", 0, 'import expects `{ Name, ... } from "..."` or a `"@Header.h"` host include', quoteAt);
      return advancePastLine(src, end);
    }
    const hostImp: UetkxImportDecl = {
      names: [],
      nameAts: [],
      specifier: payload.slice(1), // strip the leading '@'
      specifierAt: quoteAt,
      at: start,
      hostInclude: true,
    };
    // Duplicate-payload check (2303 rule, applied to host includes): importedFrom is keyed by
    // NAME for named imports and by PAYLOAD here — the keyspaces never collide (names are bare
    // identifiers; header payloads always contain `/` or `.`).
    if (importedFrom.has(hostImp.specifier)) {
      pushDiag(out, "UETKX2303", 0, `duplicate host include \`${hostImp.specifier}\``, quoteAt, payload.length);
    } else {
      importedFrom.set(hostImp.specifier, hostImp.specifier);
    }
    out.imports.push(hostImp);
    return end;
  }

  const imp: UetkxImportDecl = { names: [], nameAts: [], specifier: "", specifierAt: -1, at: start, hostInclude: false };
  if (k >= n || src[k] !== C_LBRACE) {
    pushDiag(out, "UETKX0303", 0, 'import expects `{ Name, ... }` or a `"@Header.h"` host include after `import`', Math.min(k, n - 1));
    return advancePastLine(src, k);
  }
  const bclose = findMatching(src, k);
  if (bclose === -1) {
    pushDiag(out, "UETKX0304", 0, "unclosed `{` in import list", k);
    return n;
  }
  for (let p = k + 1; p < bclose; ) {
    p = skipWsOnly(src, p);
    if (p >= bclose) break;
    if (src[p] === C_COMMA) {
      p++;
      continue;
    }
    const s = p;
    while (p < bclose && isIdentCp(src[p])) p++;
    if (p === s) {
      pushDiag(out, "UETKX0300", 0, "import list expects bare names — named exports only (no `*`, no default)", s);
      return advancePastLine(src, bclose);
    }
    imp.names.push(fromCodePoints(src, s, p - s));
    imp.nameAts.push(s);
  }
  if (imp.names.length === 0) {
    pushDiag(out, "UETKX0303", 0, 'import must name at least one binding: `import { Name } from "..."`', k);
  }
  let f = skipWsOnly(src, bclose + 1);
  if (!keywordAt(src, f, "from")) {
    pushDiag(out, "UETKX0303", 0, 'import expects `from "specifier"`', Math.min(f, n - 1));
    return advancePastLine(src, f);
  }
  f = skipWsOnly(src, f + 4);
  if (f >= n || (src[f] !== C_QUOTE && src[f] !== C_APOS)) {
    pushDiag(out, "UETKX0303", 0, 'import specifier must be a quoted path, e.g. `"./Foo"`', Math.min(f, n - 1));
    return advancePastLine(src, f);
  }
  const quote = src[f];
  imp.specifierAt = f;
  let q = f + 1;
  while (q < n && src[q] !== quote && src[q] !== C_NL) q++;
  if (q >= n || src[q] !== quote) {
    pushDiag(out, "UETKX0300", 0, "unterminated import specifier string", f);
    return advancePastLine(src, f);
  }
  imp.specifier = fromCodePoints(src, f + 1, q - (f + 1));
  const end = q + 1;

  const thisImport = new Set<string>();
  for (let idx = 0; idx < imp.names.length; idx++) {
    const name = imp.names[idx];
    const nameAt = imp.nameAts[idx];
    const prev = importedFrom.get(name);
    if (prev !== undefined) {
      pushDiag(out, "UETKX2303", 0, `duplicate import of \`${name}\` (already imported from ${prev})`, nameAt, name.length);
    } else if (thisImport.has(name)) {
      pushDiag(out, "UETKX2303", 0, `duplicate import of \`${name}\` (already imported from ${imp.specifier})`, nameAt, name.length);
    } else {
      thisImport.add(name);
    }
  }
  for (const name of thisImport) importedFrom.set(name, imp.specifier);
  out.imports.push(imp);
  return end;
}

/** Parse a `component` decl at `ci`; a preceding `export` is passed as `exported`. Pushes to
 *  out.components + out.order and returns Next, or -1 on a fatal structural error. */
function parseComponent(src: number[], ci: number, exported: boolean, out: UetkxFileScanResult): number {
  const n = src.length;
  let k = skipWsOnly(src, ci + 9);
  const ns = k;
  while (k < n && isIdentCp(src[k])) k++;
  const name = fromCodePoints(src, ns, k - ns);
  if (!name) {
    pushDiag(out, "UETKX0300", 0, "missing component name", k);
    return -1;
  }
  if (!(name[0] >= "A" && name[0] <= "Z")) {
    pushDiag(out, "UETKX2100", 0, `component name \`${name}\` must be PascalCase`, ns, name.length);
  }
  k = skipWsOnly(src, k);
  let params: UetkxParam[] = [];
  if (k < n && src[k] === C_LPAREN) {
    const pc = findMatching(src, k);
    if (pc === -1) {
      pushDiag(out, "UETKX0304", 0, "unclosed `(` in component params", k);
      return -1;
    }
    params = parseParams(fromCodePoints(src, k + 1, pc - k - 1));
    k = pc + 1;
  }
  k = skipWsOnly(src, k);
  if (k >= n || src[k] !== C_LBRACE) {
    pushDiag(out, "UETKX0303", 0, "component body `{ ... }` expected", Math.min(k, n - 1));
    return -1;
  }
  const bclose = findMatchingMarkup(src, k);
  if (bclose === -1) {
    pushDiag(out, "UETKX0304", 0, "unclosed component body", k);
    return -1;
  }
  const bodyAt = k + 1;
  const body = src.slice(bodyAt, bclose);
  // Wave G: collect ALL markup returns (early returns); the FINAL span must be top-level —
  // it anchors the window/setup split (T1.4 last-wins). C++-identical (ParseComponent).
  const returns = collectMarkupReturns(body);
  if (returns.length === 0) {
    pushDiag(out, "UETKX2101", 0, "component has no `return ( ... )` markup return", ci, 9);
    return -1;
  }
  const final = returns[returns.length - 1];
  if (!final.topLevel) {
    pushDiag(out, "UETKX3007", 0, "the component's final markup `return ( ... )` must be at the top level of the body", bodyAt + final.returnAt, 6);
    return -1;
  }
  const setup = fromCodePoints(body, 0, final.returnAt);
  let windowNodes: UetkxNode[] = [];
  for (const span of returns) {
    const parsed = parseMarkup(body, span.mStart, span.mEnd);
    if (parsed.errorCode) {
      pushDiag(out, parsed.errorCode, 0, parsed.errorMsg, bodyAt + Math.max(0, parsed.errorAt));
      return -1;
    }
    const renderRoots = parsed.nodes.filter((node) => node.type !== "comment");
    if (renderRoots.length !== 1) {
      pushDiag(out, "UETKX0108", 0, `a component must return exactly one root element (got ${renderRoots.length})`, bodyAt + span.mStart);
      return -1;
    }
    span.root = renderRoots[0];
    if (span === final) windowNodes = parsed.nodes; // the final window keeps the formatter contract
  }
  const idx = out.components.length;
  out.components.push({
    name,
    exported,
    at: ci,
    exportAt: -1, // set by scanFile once the `export` offset is known (the decl's true start)
    nameAt: ns,
    params,
    setup,
    setupAt: bodyAt,
    body: fromCodePoints(body, 0, body.length),
    bodyAt,
    windowNodes,
    returns,
    hookCalls: scanHookCalls(body.slice(0, final.returnAt)),
    next: bclose + 1,
  });
  out.order.push({ kind: "component", index: idx });
  return bclose + 1;
}

/** Parse a `hook UseName(params) [-> Ret] { body }` decl at `hi`. Returns Next, or -1 on error. */
function parseHook(src: number[], hi: number, exported: boolean, out: UetkxFileScanResult): number {
  const n = src.length;
  let k = skipWsOnly(src, hi + 4);
  const nameAt = k;
  while (k < n && isIdentCp(src[k])) k++;
  const name = fromCodePoints(src, nameAt, k - nameAt);
  if (!name) {
    pushDiag(out, "UETKX2200", 0, "missing hook name", k);
    return -1;
  }
  if (!(name.startsWith("Use") && name.length >= 4 && name[3] >= "A" && name[3] <= "Z")) {
    pushDiag(out, "UETKX2203", 1, `hook name \`${name}\` should start with \`Use\``, nameAt, name.length);
  }
  k = skipWsOnly(src, k);
  if (k < n && src[k] === 0x3c /* < */) {
    pushDiag(out, "UETKX3006", 0, "template `hook` declarations are not supported — write a C++ template in a `module`", k);
    return -1;
  }
  if (k >= n || src[k] !== C_LPAREN) {
    pushDiag(out, "UETKX2201", 0, `hook \`${name}\` is missing its parameter list \`( ... )\``, k);
    return -1;
  }
  const pc = findMatching(src, k);
  if (pc === -1) {
    pushDiag(out, "UETKX0304", 0, "unclosed `(` in hook params", k);
    return -1;
  }
  const params = fromCodePoints(src, k + 1, pc - k - 1).trim();
  k = skipWsOnly(src, pc + 1);
  let ret = "";
  if (k + 1 < n && src[k] === 0x2d /* - */ && src[k + 1] === 0x3e /* > */) {
    const rh = k + 2;
    while (k < n && src[k] !== C_LBRACE) k++;
    ret = fromCodePoints(src, rh, k - rh).trim();
  }
  k = skipWsOnly(src, k);
  if (k >= n || src[k] !== C_LBRACE) {
    pushDiag(out, "UETKX2202", 0, `hook \`${name}\` body \`{ ... }\` expected`, Math.min(k, n - 1));
    return -1;
  }
  const bclose = findMatching(src, k);
  if (bclose === -1) {
    pushDiag(out, "UETKX0304", 0, "unclosed hook body", k);
    return -1;
  }
  const idx = out.hooks.length;
  out.hooks.push({ name, exported, at: hi, exportAt: -1, nameAt, params, ret, body: fromCodePoints(src, k + 1, bclose - k - 1), bodyAt: k + 1, next: bclose + 1 });
  out.order.push({ kind: "hook", index: idx });
  return bclose + 1;
}

/** Parse a `module Name { verbatim C++ }` decl at `mi`. Returns Next, or -1 on error. */
function parseModule(src: number[], mi: number, exported: boolean, out: UetkxFileScanResult): number {
  const n = src.length;
  let k = skipWsOnly(src, mi + 6);
  const nameAt = k;
  while (k < n && isIdentCp(src[k])) k++;
  const name = fromCodePoints(src, nameAt, k - nameAt);
  if (!name) {
    pushDiag(out, "UETKX2204", 0, "missing module name", k);
    return -1;
  }
  k = skipWsOnly(src, k);
  if (k >= n || src[k] !== C_LBRACE) {
    pushDiag(out, "UETKX2205", 0, `module \`${name}\` is missing a body — expected \`{\` after the name (modules take no \`( )\`)`, Math.min(k, n - 1));
    return -1;
  }
  const bclose = findMatching(src, k);
  if (bclose === -1) {
    pushDiag(out, "UETKX0304", 0, "unclosed module body", k);
    return -1;
  }
  const idx = out.modules.length;
  out.modules.push({ name, exported, at: mi, exportAt: -1, nameAt, body: fromCodePoints(src, k + 1, bclose - k - 1), bodyAt: k + 1, next: bclose + 1 });
  out.order.push({ kind: "module", index: idx });
  return bclose + 1;
}

/** Resync to the next top-level declaration keyword at a line start AFTER `from` (best-effort recovery
 *  for a body parse error). Returns its offset, or -1 if none — used only by the signature-list mode. */
function resyncToNextDecl(src: number[], from: number): number {
  // Advance past the rest of the failed declaration's current line first.
  let i = from;
  while (i < src.length && src[i] !== C_NL) i++;
  for (; i < src.length; i++) {
    if (src[i] !== C_NL) continue;
    const s = skipWsOnly(src, i + 1);
    if (keywordAt(src, s, "export") || keywordAt(src, s, "component") || keywordAt(src, s, "hook") || keywordAt(src, s, "module")) {
      return s;
    }
  }
  return -1;
}

/** Scan a .uetkx file. `resyncOnBodyError` (signature-list mode, e.g. getDecls) keeps collecting later
 *  declarations after a malformed body instead of aborting — mirroring the C++ ScanPreamble, which only
 *  reads decl signatures and so never truncates the exported-decl list on a body error (bughunt LSP-1). */
export function scanFile(source: string, basename: string, resyncOnBodyError = false): UetkxFileScanResult {
  const out: UetkxFileScanResult = {
    preambleIncludes: [],
    preambleIncludeAts: [],
    imports: [],
    components: [],
    hooks: [],
    modules: [],
    order: [],
    diags: [],
  };
  const src: number[] = [];
  for (const ch of source) src.push(ch.codePointAt(0)!);
  const n = src.length;
  const importedFrom = new Map<string, string>(); // name -> first specifier (UETKX2303)

  // preamble: verbatim #include lines + `import { … } from "…"`, until the first declaration
  let i = 0;
  while (i < n) {
    const j = skipNoncode(src, i);
    if (j !== i) {
      if (src[i] === C_HASH) {
        const line = fromCodePoints(src, i, j - i).trim();
        if (line.startsWith("#include")) {
          out.preambleIncludes.push(line);
          out.preambleIncludeAts.push(i);
        }
      }
      i = j;
      continue;
    }
    if (keywordAt(src, i, "import")) {
      i = parseImport(src, i, out, importedFrom);
      continue;
    }
    if (keywordAt(src, i, "export") || keywordAt(src, i, "component") || keywordAt(src, i, "hook") || keywordAt(src, i, "module")) break;
    i++;
  }

  // UETKX2317 (hint): a #include or `import "@X.h"` naming a header the generated prelude
  // already provides (INCLUDE_RETIREMENT_PLAN.md §B — the family's redundant-using hint,
  // ported per-leg). Severity 2, so it never contributes to hasError(). C++-identical.
  for (let idx = 0; idx < out.preambleIncludes.length; idx++) {
    const header = extractIncludeHeader(out.preambleIncludes[idx]);
    if (header && AUTO_INCLUDED_HEADERS.includes(header)) {
      pushDiag(
        out,
        "UETKX2317",
        2,
        `\`${header}\` is auto-included by the generated prelude — this line is redundant`,
        out.preambleIncludeAts[idx],
        out.preambleIncludes[idx].length,
      );
    }
  }
  for (const imp of out.imports) {
    if (imp.hostInclude && AUTO_INCLUDED_HEADERS.includes(imp.specifier)) {
      pushDiag(
        out,
        "UETKX2317",
        2,
        `\`${imp.specifier}\` is auto-included by the generated prelude — this line is redundant`,
        imp.specifierAt,
        imp.specifier.length + 3, // +3: '@' + the two quotes
      );
    }
  }

  // declarations: a SEQUENCE of components/hooks/modules in any order (FULL MIXED-DECL v1)
  while (i < n) {
    const j = skipNoncode(src, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = src[i];
    if (c === C_SPACE || c === C_TAB || c === C_NL || c === C_CR) {
      i++;
      continue;
    }
    if (keywordAt(src, i, "import")) {
      pushDiag(out, "UETKX2309", 0, "import must appear in the preamble, before the first declaration", i);
      i = parseImport(src, i, out, importedFrom); // consume it so the scan can resync
      continue;
    }
    let exported = false;
    const declStart = i;
    if (keywordAt(src, i, "export")) {
      exported = true;
      i = skipWsOnly(src, i + 6);
    }
    let next = -1;
    if (keywordAt(src, i, "component")) {
      next = parseComponent(src, i, exported, out);
      if (next >= 0 && exported) out.components[out.components.length - 1].exportAt = declStart; // the decl's true start
    } else if (keywordAt(src, i, "hook")) {
      next = parseHook(src, i, exported, out);
      if (next >= 0 && exported) out.hooks[out.hooks.length - 1].exportAt = declStart; // the decl's true start
    } else if (keywordAt(src, i, "module")) {
      next = parseModule(src, i, exported, out);
      if (next >= 0 && exported) out.modules[out.modules.length - 1].exportAt = declStart; // the decl's true start
    }
    else {
      pushDiag(
        out,
        "UETKX2101",
        0,
        exported
          ? "`export` must be followed by `component`, `hook`, or `module`"
          : "expected a declaration (`component`, `hook`, or `module`)",
        declStart,
      );
      return out;
    }
    if (next < 0) {
      // Signature-list mode recovers to the next declaration so a body error does not truncate the
      // exported-decl list (bughunt LSP-1); the compiler path still aborts on the first fatal error.
      if (resyncOnBodyError) {
        const r = resyncToNextDecl(src, declStart);
        if (r < 0) return out;
        i = r;
        continue;
      }
      return out; // fatal decl parse error (diag already recorded)
    }
    i = next;
  }

  if (out.components.length === 0 && out.hooks.length === 0 && out.modules.length === 0) {
    pushDiag(out, "UETKX2101", 0, "no `component`, `hook`, or `module` declaration found", -1);
    return out;
  }

  // component/file-name nudge (0103) — kept for the one-component ergonomic; multi-component
  // files are flagged by the convention warn (2105) instead.
  if (out.components.length === 1) {
    const baseCmp = basename.includes(".") ? basename.slice(0, basename.indexOf(".")) : basename;
    const only = out.components[0];
    if (only.name !== baseCmp && baseCmp) {
      pushDiag(out, "UETKX0103", 1, `component \`${only.name}\` differs from file name \`${baseCmp}\``, only.nameAt, only.name.length);
    }
  }

  // ONE component per file is now a CONVENTION (A3): >1 component is a LINT warn, not an error.
  if (out.components.length > 1) {
    pushDiag(
      out,
      "UETKX2105",
      1,
      `${out.components.length} components in one file — one component per file (convention); move siblings into components/<Name>/<Name>.uetkx`,
      out.components[1].at,
    );
  }
  return out;
}

function isIdentCp(c: number): boolean {
  return c === 95 || (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || (c >= 48 && c <= 57);
}
