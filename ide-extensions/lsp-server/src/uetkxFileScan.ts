// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TypeScript port of FUetkxFileScan (UetkxFileScan.cpp): the .uetkx FILE grammar — preamble
// includes, `component Name(params) {}` declarations, the shared last-top-level-markup-return
// splitter, hook-call scanning and the FNV hook signature. All offsets are code points.

import { findMatching, findMatchingMarkup, isIdentCode, keywordAt, skipNoncode, skipNoncodeMarkup } from "./cppScanner";
import { fromCodePoints, toCodePoints } from "./codePoints";
import { parseMarkup, UetkxNode } from "./uetkxMarkup";
import { findMarkupRanges, MarkupRange } from "./jsxScan";

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

/** §4 markup-everywhere: if code point `i` lands inside a jsx markup range, the index to jump
 *  to (the range end); -1 otherwise. Walkers use this to keep CODE lexis off markup text.
 *  C++-identical (JsxSkipTo). */
function jsxSkipTo(ranges: readonly MarkupRange[], i: number): number {
  for (const r of ranges) {
    if (i >= r.start && i < r.end) return r.end;
  }
  return -1;
}

/** The body's jsx-range list of record (element-form markup at every family boundary position),
 *  with unbalanced ranges clamped to the body end. */
function bodyJsxRanges(body: readonly number[]): MarkupRange[] {
  const ranges = findMarkupRanges(body, 0, body.length);
  for (const r of ranges) {
    if (r.end === -1) r.end = body.length;
  }
  return ranges;
}

/** TB-12 / UETKX0114, NARROWED by §4 markup-everywhere: markup-as-value is now first-class
 *  (`auto X = (<VerticalBox>…);` lowers in place), so the old detector is gone. What stays
 *  illegal is a PAREN-LESS markup return at statement level (`return <Tag/>;`) — the family
 *  return spelling is `return ( <markup> );`. Lambda-nested returns (parenDepth > 0, deduced
 *  return type) lower fine and stay legal. C++-identical (DiagnoseBareMarkupReturn). */
function diagnoseBareMarkupReturn(
  body: readonly number[],
  ranges: readonly MarkupRange[],
  bodyAt: number,
  out: UetkxFileScanResult,
): void {
  const n = body.length;
  const isWs = (c: number) => c === C_SPACE || c === C_TAB || c === C_NL || c === C_CR;
  let parenDepth = 0;
  let i = 0;
  while (i < n) {
    const skip = jsxSkipTo(ranges, i);
    if (skip !== -1) {
      i = skip;
      continue;
    }
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
    if (parenDepth === 0 && c === 114 /*r*/ && keywordAt(body, i, "return")) {
      let p = i + 6;
      while (p < n && isWs(body[p])) p++;
      if (ranges.some((r) => r.start === p)) {
        pushDiag(out, "UETKX0114", 0, "a markup return must be parenthesized — write `return ( <...> );`", bodyAt + p);
        return;
      }
      i += 6;
      continue;
    }
    i++;
  }
}

// ── §4 rules of hooks — the family port (Unity UITKX0013-0016, Godot T2.5), C++-shaped ──────
// Hooks must run unconditionally at the top level of the component body: 0013 conditional
// (if/else — and any hook inside markup, which renders conditionally by construction), 0014
// loop (for/while/@for/@while), 0015 match branch (switch/@match), 0016 callback/lambda (incl.
// event attributes). Every violating call is reported. C++-identical (ValidateHookPlacement).

type HookBlock = "none" | "conditional" | "loop" | "match" | "callback" | "markup";

function hookBlockCode(kind: HookBlock): string {
  switch (kind) {
    case "loop":
      return "UETKX0014";
    case "match":
      return "UETKX0015";
    case "callback":
      return "UETKX0016";
    default:
      return "UETKX0013";
  }
}

function hookBlockMessage(kind: HookBlock): string {
  let what = "conditionally (inside an if/else)";
  if (kind === "loop") what = "inside a loop";
  else if (kind === "match") what = "inside a switch/match branch";
  else if (kind === "callback") what = "inside a callback/lambda";
  else if (kind === "markup") what = "inside markup";
  return `hook called ${what} — hooks must run unconditionally at the top level of the component body`;
}

/** Hook-call match at `i` (same rule as scanHookCalls): a known hook name followed by `(` or a
 *  `<` template-arg list. Returns the name length, or 0. C++-identical (HookCallLenAt). */
function hookCallLenAt(src: readonly number[], i: number): number {
  const n = src.length;
  if (i >= n || (src[i] !== 85 /*U*/ && src[i] !== 80) /*P*/) return 0;
  for (const hook of HOOK_NAMES) {
    if (keywordAt(src, i, hook)) {
      const k = skipWsOnly(src, i + hook.length);
      if (k < n && (src[k] === C_LPAREN || src[k] === C_LT)) return hook.length;
    }
  }
  return 0;
}

/** Code embedded in markup (attr/child expressions, directive headers/bodies): flag hook calls
 *  with `kind`, then recurse into any markup ranges nested in the code (jsx scan). `base` is
 *  the absolute offset of code[0], or -1 when unknown (diags fall back to fallbackAt).
 *  C++-identical (ScanMarkupCodeForHooks). */
function scanMarkupCodeForHooks(
  code: string,
  base: number,
  fallbackAt: number,
  kind: HookBlock,
  out: UetkxFileScanResult,
): void {
  const src = toCodePoints(code);
  const ranges = findMarkupRanges(src, 0, src.length);
  for (const r of ranges) {
    if (r.end === -1) r.end = src.length;
  }
  const n = src.length;
  let i = 0;
  while (i < n) {
    const jump = jsxSkipTo(ranges, i);
    if (jump !== -1) {
      i = jump;
      continue;
    }
    const j = skipNoncode(src, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const hookLen = hookCallLenAt(src, i);
    if (hookLen > 0) {
      pushDiag(out, hookBlockCode(kind), 0, hookBlockMessage(kind), base < 0 ? fallbackAt : base + i, hookLen);
      i += hookLen;
      continue;
    }
    if (isIdentCode(src[i])) {
      while (i < n && isIdentCode(src[i])) i++;
      continue;
    }
    i++;
  }
  for (const r of ranges) {
    const parsed = parseMarkup(src, r.start, r.end);
    if (parsed.errorCode) continue; // the compiler owns markup parse errors
    for (const nd of parsed.nodes) {
      validateMarkupNodeHooks(nd, base, base < 0 ? fallbackAt : base + r.start, kind, out);
    }
  }
}

/** A directive body — statements + `return ( <markup> )` (family 0.7): validate the lead code
 *  (jsx-aware), then every root of the return window. Mirrors the emitter's split exactly.
 *  C++-identical (ValidateDirectiveBodyHooks). */
function validateDirectiveBodyHooks(
  bodyMarkup: string,
  base: number,
  fallbackAt: number,
  enclosing: HookBlock,
  out: UetkxFileScanResult,
): void {
  const body = toCodePoints(bodyMarkup);
  const split = splitMarkupReturn(body, false);
  if (!split.ok) {
    scanMarkupCodeForHooks(bodyMarkup, base, fallbackAt, enclosing, out);
    return;
  }
  scanMarkupCodeForHooks(fromCodePoints(body, 0, split.returnAt), base, fallbackAt, enclosing, out);
  const parsed = parseMarkup(body, split.mStart, split.mEnd);
  if (!parsed.errorCode) {
    for (const nd of parsed.nodes) {
      validateMarkupNodeHooks(nd, base, base < 0 ? fallbackAt : base + split.mStart, enclosing, out);
    }
  }
}

/** Walk one parsed markup node: attr/child expressions, directive headers and bodies. `base` is
 *  the absolute anchor of this tree's offset frame (node.at/vat/bodyAt compose on top of it),
 *  or -1 when detached (diags fall back to fallbackAt). C++-identical (ValidateMarkupNodeHooks)
 *  — with the TS parser's trim-exact header offsets (condAt/headerAt/subjectAt/valueAt) used
 *  where the C++ falls back to the directive's `@`. */
function validateMarkupNodeHooks(
  node: UetkxNode,
  base: number,
  fallbackAt: number,
  enclosing: HookBlock,
  out: UetkxFileScanResult,
): void {
  const nodeAt = base < 0 ? fallbackAt : base + node.at;
  const sub = (at: number | undefined) => (base < 0 || at === undefined || at < 0 ? -1 : base + at);
  switch (node.type) {
    case "el":
    case "frag": {
      for (const attr of node.attrs ?? []) {
        if (attr.kind !== "expr" && attr.kind !== "spread") continue;
        let kind: HookBlock = enclosing;
        if (attr.name.length > 2 && attr.name.startsWith("On")) kind = "callback"; // event attrs run as callbacks
        else if (kind === "none") kind = "markup";
        scanMarkupCodeForHooks(attr.value, sub(attr.vat), nodeAt, kind, out);
      }
      for (const child of node.children ?? []) {
        validateMarkupNodeHooks(child, base, nodeAt, enclosing, out);
      }
      break;
    }
    case "expr":
      scanMarkupCodeForHooks(node.code ?? "", sub(node.vat), nodeAt, enclosing === "none" ? "markup" : enclosing, out);
      break;
    case "if": {
      for (const branch of node.branches ?? []) {
        scanMarkupCodeForHooks(branch.cond, sub(branch.condAt), nodeAt, "conditional", out);
        validateDirectiveBodyHooks(branch.bodyMarkup, sub(branch.bodyAt), nodeAt, "conditional", out);
      }
      if (node.elseBody !== undefined) {
        validateDirectiveBodyHooks(node.elseBody, sub(node.elseBodyAt), nodeAt, "conditional", out);
      }
      break;
    }
    case "for":
    case "while":
      scanMarkupCodeForHooks(node.header ?? "", sub(node.headerAt), nodeAt, "loop", out);
      validateDirectiveBodyHooks(node.bodyMarkup ?? "", sub(node.bodyAt), nodeAt, "loop", out);
      break;
    case "match": {
      scanMarkupCodeForHooks(node.subject ?? "", sub(node.subjectAt), nodeAt, "match", out);
      for (const c of node.cases ?? []) {
        scanMarkupCodeForHooks(c.value, sub(c.valueAt), nodeAt, "match", out);
        validateDirectiveBodyHooks(c.bodyMarkup, sub(c.bodyAt), nodeAt, "match", out);
      }
      if (node.defaultBody !== undefined) {
        validateDirectiveBodyHooks(node.defaultBody, sub(node.defaultBodyAt), nodeAt, "match", out);
      }
      break;
    }
    default:
      break;
  }
}

/** The C++ brace-stack walk over the body's CODE (markup regions skipped): flag hook calls
 *  under if/else/for/while/do/switch blocks (incl. brace-less bodies + headers) and inside
 *  lambdas. C++-identical (CodeWalkHooks). */
function codeWalkHooks(
  body: readonly number[],
  skip: readonly MarkupRange[],
  bodyAt: number,
  out: UetkxFileScanResult,
): void {
  const stack: HookBlock[] = []; // one entry per '{'
  const lambdaBraces = new Set<number>(); // '{' offsets recognized as lambda bodies
  let pending: HookBlock = "none";
  let parenDepth = 0;
  const n = body.length;
  let i = 0;
  while (i < n) {
    const jump = jsxSkipTo(skip, i);
    if (jump !== -1) {
      i = jump;
      continue;
    }
    const j = skipNoncode(body, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = body[i];
    if (c === C_LPAREN) {
      parenDepth++;
      i++;
      continue;
    }
    if (c === 41 /*)*/) {
      parenDepth--;
      i++;
      continue;
    }
    if (c === 91 /*[*/) {
      // lambda intro? peek non-destructively: [captures] (params)? specifiers? '{'
      const closeB = findMatching(body, i);
      if (closeB !== -1) {
        let k = skipWsOnly(body, closeB + 1);
        if (k < n && body[k] === C_LPAREN) {
          const closeP = findMatching(body, k);
          k = closeP === -1 ? n : skipWsOnly(body, closeP + 1);
        }
        let guard = 0;
        const tolerated = (cc: number) =>
          isIdentCode(cc) ||
          cc === C_SPACE ||
          cc === C_TAB ||
          cc === C_NL ||
          cc === C_CR ||
          cc === 45 /*-*/ ||
          cc === 62 /*>*/ ||
          cc === 58 /*:*/ ||
          cc === 38 /*&*/ ||
          cc === 42 /***/ ||
          cc === C_LT ||
          cc === C_COMMA;
        while (k < n && body[k] !== C_LBRACE && guard++ < 64 && tolerated(body[k])) k++;
        if (k < n && body[k] === C_LBRACE) lambdaBraces.add(k);
      }
      i++; // walk the capture list normally — init-captures run unconditionally
      continue;
    }
    if (c === C_LBRACE) {
      let kind: HookBlock = "none";
      if (lambdaBraces.has(i)) kind = "callback";
      else if (pending !== "none") kind = pending;
      stack.push(kind);
      pending = "none";
      i++;
      continue;
    }
    if (c === 125 /*}*/) {
      if (stack.length > 0) stack.pop();
      i++;
      continue;
    }
    if (c === 59 /*;*/ && parenDepth === 0) {
      pending = "none"; // a brace-less `if (x) stmt;` body ended
      i++;
      continue;
    }
    if (isIdentCode(c) && (i === 0 || !isIdentCode(body[i - 1]))) {
      const hookLen = hookCallLenAt(body, i);
      if (hookLen > 0) {
        let ctx: HookBlock = pending;
        for (let s = stack.length - 1; ctx === "none" && s >= 0; s--) ctx = stack[s];
        if (ctx !== "none") {
          pushDiag(out, hookBlockCode(ctx), 0, hookBlockMessage(ctx), bodyAt + i, hookLen);
        }
        i += hookLen;
        continue;
      }
      if (keywordAt(body, i, "if") || keywordAt(body, i, "else")) pending = "conditional";
      else if (keywordAt(body, i, "for") || keywordAt(body, i, "while") || keywordAt(body, i, "do")) pending = "loop";
      else if (keywordAt(body, i, "switch")) pending = "match";
      while (i < n && isIdentCode(body[i])) i++;
      continue;
    }
    i++;
  }
}

/** §4 rules-of-hooks driver: the code walk (outside markup), then every markup region's parsed
 *  tree — return windows use their already-parsed roots; value-markup ranges parse here
 *  (tolerantly — the compiler owns parse errors). C++-identical (ValidateHookPlacement). */
function validateHookPlacement(
  body: readonly number[],
  jsxRanges: readonly MarkupRange[],
  returns: ReadonlyArray<UetkxReturnSpan>,
  bodyAt: number,
  out: UetkxFileScanResult,
): void {
  const skip: MarkupRange[] = [...jsxRanges];
  for (const span of returns) {
    // directive-form windows (`return ( @if … )`) are not jsx ranges
    skip.push({ start: span.mStart, end: span.mEnd, op: "", opPos: span.mStart });
  }
  codeWalkHooks(body, skip, bodyAt, out);
  for (const span of returns) {
    if (span.root) validateMarkupNodeHooks(span.root, bodyAt, bodyAt + span.mStart, "none", out);
  }
  for (const r of jsxRanges) {
    if (returns.some((span) => r.start >= span.mStart && r.start < span.mEnd)) continue;
    const parsed = parseMarkup(body, r.start, r.end);
    if (parsed.errorCode) continue;
    for (const nd of parsed.nodes) {
      validateMarkupNodeHooks(nd, bodyAt, bodyAt + r.start, "none", out);
    }
  }
}

/** TB-5 / UETKX0107 — Unity's UnreachableAfterReturn (UITKX0107), family-numbered: dead code
 *  after the FIRST top-level `return` statement of a component body. Handles BOTH shapes — a
 *  top-level markup `return ( … )` before the body's end, and a plain C++ early `return x;`
 *  in setup (the collector never sees those — no markup). Severity 2 (hint); the server
 *  attaches the Unnecessary tag so editors FADE the range. Comment-only tails don't count.
 *  C++-identical (DiagnoseUnreachableAfterReturn). */
function diagnoseUnreachableAfterReturn(
  body: readonly number[],
  returns: ReadonlyArray<{ returnAt: number; afterParen: number }>,
  ranges: readonly MarkupRange[],
  bodyAt: number,
  out: UetkxFileScanResult,
): void {
  const n = body.length;
  let deadStart = -1;
  let depth = 0;
  let i = 0;
  while (i < n) {
    const skip = jsxSkipTo(ranges, i);
    if (skip !== -1) {
      i = skip;
      continue;
    }
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
      const span = returns.find((s) => s.returnAt === i);
      let end: number;
      if (span) {
        end = span.afterParen;
        while (end < n && (body[end] === C_SPACE || body[end] === C_TAB || body[end] === C_NL || body[end] === C_CR)) end++;
        if (end < n && body[end] === 59 /*;*/) end++;
      } else {
        // A plain C++ `return expr;` — scan to its statement-ending top-level `;`.
        // Markup ranges jump whole (a `;` inside markup text is not a terminator).
        let d2 = 0;
        let k = i + 6;
        while (k < n) {
          const skip2 = jsxSkipTo(ranges, k);
          if (skip2 !== -1) {
            k = skip2;
            continue;
          }
          const j2 = skipNoncode(body, k);
          if (j2 !== k) {
            k = j2;
            continue;
          }
          const c2 = body[k];
          if (c2 === C_LBRACE || c2 === C_LPAREN || c2 === 91) d2++;
          else if (c2 === 125 || c2 === 41 || c2 === 93) d2--;
          else if (c2 === 59 /*;*/ && d2 === 0) {
            k++;
            break;
          }
          k++;
        }
        end = k;
      }
      deadStart = end;
      break;
    }
    i++;
  }
  if (deadStart < 0) return;
  let first = -1;
  i = deadStart;
  while (i < n) {
    const j = skipNoncode(body, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = body[i];
    if (c === C_SPACE || c === C_TAB || c === C_NL || c === C_CR) {
      i++;
      continue;
    }
    first = i;
    break;
  }
  if (first < 0) return;
  let last = n;
  while (last > first && (body[last - 1] === C_SPACE || body[last - 1] === C_TAB || body[last - 1] === C_NL || body[last - 1] === C_CR)) last--;
  pushDiag(out, "UETKX0107", 2, "Unreachable code after 'return'.", bodyAt + first, last - first);
}

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

/** The five declaration kinds a .uetkx file may hold, in any number/order (mixed-decl v1).
 *  ES-modules (plans/ES_MODULES_EXECUTION_PLAN.md G-03/U-01, "S5"): `value`/`util` are the new
 *  plain-declaration kinds — C++-identical (EUetkxDeclKind). */
export type UetkxDeclKind = "component" | "hook" | "module" | "value" | "util";

/** `import { A, B } from "specifier"` — a preamble-only static import (named bindings only).
 *
 *  INCLUDE_RETIREMENT_PLAN.md §B: `import "@Header.h"` is a second, braces/`from`-less shape —
 *  a nameless HOST INCLUDE (family shape, ported from Unity's `import "@Namespace"`; our
 *  payload is a header path). hostInclude=true means names/nameAts are EMPTY BY DESIGN;
 *  specifier holds the payload WITHOUT the leading `@`. C++-identical (FUetkxImportDecl). */
export interface UetkxImportDecl {
  names: string[];
  nameAts: number[];
  // ES-modules (G-05): the LOCAL binding for each names[n] — identical to names[n] unless the
  // import renamed it (`{ A as B }`, localNames[n] === "B"). Always 1:1 with names.
  localNames: string[];
  // The alias TOKEN's offset for a renamed binding (the `B` of `A as B`); == nameAts[n] when no
  // rename (the LSP rename/references index needs the exact token — TD-033).
  localNameAts: number[];
  specifier: string;
  specifierAt: number; // offset of the opening quote
  at: number; // offset of the `import` keyword
  hostInclude: boolean; // `import "@Header.h"` — see the interface comment
  // ES-modules (G-05): `import * as X from "./x"` — names/localNames stay EMPTY; namespaceAlias
  // is the local binding "X". `import Y from "./x"` (default) — defaultAlias is the local
  // binding "Y". The ES COMBINED forms (`import Def, { A, B } from` / `import Def, * as X
  // from`) are ONE declaration carrying the default binding PLUS the named/star surface —
  // consumers must treat the parts as independent, never mutually exclusive.
  isNamespace: boolean;
  namespaceAlias: string;
  namespaceAliasAt: number;
  isDefault: boolean;
  defaultAlias: string;
  defaultAliasAt: number;
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
  // ES-modules (G-10): true for the deprecated `component Name(...) { }` wrapper (fires 2320);
  // false for the new plain form `[export] FRuiNode Name(...) { }`.
  legacySyntax: boolean;
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
  // ES-modules (G-10): see UetkxComponentDecl.legacySyntax.
  legacySyntax: boolean;
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

/** `[export] <Type> Name = <Init>;` — a module-level VALUE export (G-03/U-01). Immutable by
 *  construction; `type` empty means inference sugar, valid ONLY when `init` begins
 *  `Ident(`/`Ident{`/`Ident<` (UETKX2322 otherwise). C++-identical (FUetkxValueDecl). */
export interface UetkxValueDecl {
  name: string;
  nameAt: number;
  type: string; // "" = inferred
  typeAt: number;
  init: string;
  initAt: number;
  exported: boolean;
  exportAt: number;
  at: number;
  next: number;
}

/** `[export] <Type> Name(params) { body }` — a module-level UTIL function (G-03/U-01): not
 *  `Use`-prefixed, not FRuiNode-returning. C++-identical (FUetkxUtilDecl). */
export interface UetkxUtilDecl {
  name: string;
  nameAt: number;
  retType: string;
  retTypeAt: number;
  params: string;
  body: string;
  bodyAt: number;
  exported: boolean;
  exportAt: number;
  at: number;
  next: number;
}

export interface UetkxFileScanResult {
  preambleIncludes: string[];
  preambleIncludeAts: number[]; // 1:1 with preambleIncludes — each line's start offset
  imports: UetkxImportDecl[];
  components: UetkxComponentDecl[];
  hooks: UetkxHookDecl[];
  modules: UetkxModuleDecl[];
  values: UetkxValueDecl[]; // ES-modules (G-03/U-01)
  utils: UetkxUtilDecl[]; // ES-modules (G-03/U-01)
  order: { kind: UetkxDeclKind; index: number }[]; // source order across all five arrays
  diags: UetkxDiag[];
  // ES-modules (U-08): `export default <Name>;` target, if any (at most one per file — a
  // second is UETKX2327). Does NOT imply `exported` on the named decl (ES parity).
  defaultExportName: string;
  defaultExportAt: number;
  // ES-modules (U-09): every `export { … };` list entry with its token offset — the LSP
  // rename/references index (TD-033); the two-pass export resolution runs off the same records.
  exportListEntries: Array<{ name: string; at: number }>;
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

/** Split a param-list text on top-level commas (depth-aware — `()[]{}<>` all nest). Shared by the
 *  legacy `Name: Type = Default` splitter (parseParams) and the ES-modules new-form
 *  `Type Name = Default` splitter (parseNewParams). */
function splitTopLevelCommaPieces(paramText: string): string[] {
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
  return pieces;
}

/** Family param grammar `Name: Type = Default, ...` with depth-aware splitting. */
export function parseParams(paramText: string): UetkxParam[] {
  const out: UetkxParam[] = [];
  const pieces = splitTopLevelCommaPieces(paramText);
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

/** ES-modules (U-01, "S5"): split NEW-form params `Type Name = Default, ...` (type-first — the
 *  mirror image of legacy parseParams' `Name: Type = Default`). Per piece, NAME is the LAST
 *  top-level identifier before that piece's own top-level `=` (or before its end when there is
 *  no default) — the same rule scanDeclHead uses for the outer declaration head. */
export function parseNewParams(paramText: string): UetkxParam[] {
  const out: UetkxParam[] = [];
  const pieces = splitTopLevelCommaPieces(paramText);
  const isIdentChar = (c: string) => /[A-Za-z0-9_]/.test(c);
  for (const piece of pieces) {
    let depth = 0;
    let identStart = -1;
    let identEnd = -1;
    let eqIdx = -1;
    for (let i = 0; i < piece.length; i++) {
      const c = piece[i];
      if (c === "(" || c === "[" || c === "{" || c === "<") {
        depth++;
        continue;
      }
      if (c === ")" || c === "]" || c === "}" || c === ">") {
        depth--;
        continue;
      }
      if (depth === 0 && c === "=" && piece[i + 1] !== "=") {
        eqIdx = i;
        break;
      }
      if (depth === 0 && /[A-Za-z_]/.test(c)) {
        const s = i;
        while (i < piece.length && isIdentChar(piece[i])) i++;
        identStart = s;
        identEnd = i;
        i--; // the for-loop's i++ resumes right after this identifier
      }
    }
    const head = eqIdx === -1 ? piece : piece.slice(0, eqIdx);
    let name = "";
    let type = "";
    if (identStart !== -1 && identEnd <= head.length) {
      name = head.slice(identStart, identEnd).trim();
      type = head.slice(0, identStart).trim();
    } else {
      type = head.trim();
    }
    const def = eqIdx === -1 ? "" : piece.slice(eqIdx + 1).trim();
    if (name) out.push({ name, type, default: def });
  }
  return out;
}

/** ES-modules (U-02): one type-first declaration head, scanned directly over source code points
 *  (mirrors ScanDeclHead — UetkxFileScan.cpp). */
interface UetkxDeclHead {
  typeAt: number;
  typeLen: number;
  nameAt: number;
  nameLen: number;
  triggerAt: number;
  trigger: "(" | "=" | "{" | ""; // "" = EOF/unterminated
}

/** Scan a type-first declaration head starting at `start` (just past an optional `export`),
 *  stopping at the first top-level `(` (component/hook/util param list), `=` (value export), or
 *  an unexpected top-level `{` (no `(`/`=` seen — not a recognized head). NAME is the LAST
 *  top-level identifier before the trigger; TYPE is everything before NAME (empty ⇒ value
 *  inference sugar). `<`/`{`/`[` always nest here — pure TYPE position. C++-identical
 *  (ScanDeclHead). */
function scanDeclHead(src: number[], start: number): UetkxDeclHead {
  const n = src.length;
  const out: UetkxDeclHead = { typeAt: -1, typeLen: 0, nameAt: -1, nameLen: 0, triggerAt: -1, trigger: "" };
  let depth = 0;
  let identStart = -1;
  let identEnd = -1;
  let p = start;
  while (p < n) {
    const j = skipNoncode(src, p);
    if (j !== p) {
      p = j;
      continue;
    }
    const c = src[p];
    if (c === C_SPACE || c === C_TAB || c === C_NL || c === C_CR) {
      p++;
      continue;
    }
    if (depth === 0 && isIdentCp(c) && !(c >= 48 && c <= 57)) {
      const s = p;
      while (p < n && isIdentCp(src[p])) p++;
      identStart = s;
      identEnd = p;
      continue;
    }
    if (c === C_LPAREN) {
      if (depth === 0) {
        out.trigger = "(";
        out.triggerAt = p;
        break;
      }
      depth++;
      p++;
      continue;
    }
    if (c === C_LBRACE) {
      if (depth === 0) {
        out.trigger = "{";
        out.triggerAt = p;
        break;
      }
      depth++;
      p++;
      continue;
    }
    if (c === 91 /* [ */ || c === C_LT) {
      depth++;
      p++;
      continue;
    }
    if (c === 41 /* ) */ || c === 125 /* } */ || c === 93 /* ] */ || c === 0x3e /* > */) {
      depth--;
      p++;
      continue;
    }
    if (depth === 0 && c === 0x3d /* = */ && src[p + 1] !== 0x3d && (p === start || (src[p - 1] !== 0x3d && src[p - 1] !== 0x21 /* ! */ && src[p - 1] !== C_LT && src[p - 1] !== 0x3e))) {
      out.trigger = "=";
      out.triggerAt = p;
      break;
    }
    p++;
  }
  if (identStart !== -1) {
    out.nameAt = identStart;
    out.nameLen = identEnd - identStart;
    out.typeAt = start;
    out.typeLen = identStart - start;
  }
  return out;
}

/** True when `name` looks like a hook name (U-02 rule 2: `Use[A-Z]\w*`) — the exact predicate
 *  legacy parseHook's UETKX2203 warn already uses. */
function looksLikeHookName(name: string): boolean {
  return name.startsWith("Use") && name.length >= 4 && name[3] >= "A" && name[3] <= "Z";
}

/** Find the top-level `;` terminating a value-export initializer starting at `start` (just past
 *  '='). Only `()[]{}` nest (EXPRESSION position — `<`/`>` are real operators here, unlike a decl
 *  head). Returns -1 when unterminated. C++-identical (FindValueEnd). */
function findValueEnd(src: number[], start: number): number {
  const n = src.length;
  let depth = 0;
  let p = start;
  while (p < n) {
    const j = skipNoncode(src, p);
    if (j !== p) {
      p = j;
      continue;
    }
    const c = src[p];
    if (c === C_LPAREN || c === 91 /* [ */ || c === C_LBRACE) {
      depth++;
      p++;
      continue;
    }
    if (c === 41 /* ) */ || c === 93 /* ] */ || c === 125 /* } */) {
      depth--;
      p++;
      continue;
    }
    if (depth === 0 && c === 0x3b /* ; */) return p;
    p++;
  }
  return -1;
}

/** §4.3 HMR protection: markup ranges are EXCLUDED — hooks are illegal inside markup
 *  (UETKX0013), so markup text must never perturb the hook signature: editing a value-markup
 *  child would otherwise flip the signature and spuriously reset live state on a HMR swap.
 *  C++-identical (ScanHookCalls). */
function scanHookCalls(setup: readonly number[], skipRanges?: readonly MarkupRange[]): string[] {
  const out: string[] = [];
  const n = setup.length;
  let i = 0;
  while (i < n) {
    if (skipRanges) {
      const skip = jsxSkipTo(skipRanges, i);
      if (skip !== -1) {
        i = skip;
        continue;
      }
    }
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
/** ES-modules (G-05): parse the common `from "specifier"` tail shared by every import form. */
function parseFromSpecifier(
  src: number[],
  f: number,
  out: UetkxFileScanResult,
): { ok: true; specifier: string; specifierAt: number; end: number } | { ok: false; end: number } {
  const n = src.length;
  f = skipWsOnly(src, f);
  if (!keywordAt(src, f, "from")) {
    pushDiag(out, "UETKX0303", 0, 'import expects `from "specifier"`', Math.min(f, n - 1));
    return { ok: false, end: advancePastLine(src, f) };
  }
  f = skipWsOnly(src, f + 4);
  if (f >= n || (src[f] !== C_QUOTE && src[f] !== C_APOS)) {
    pushDiag(out, "UETKX0303", 0, 'import specifier must be a quoted path, e.g. `"./Foo"`', Math.min(f, n - 1));
    return { ok: false, end: advancePastLine(src, f) };
  }
  const quote = src[f];
  const specifierAt = f;
  let q = f + 1;
  while (q < n && src[q] !== quote && src[q] !== C_NL) q++;
  if (q >= n || src[q] !== quote) {
    pushDiag(out, "UETKX0300", 0, "unterminated import specifier string", f);
    return { ok: false, end: advancePastLine(src, f) };
  }
  return { ok: true, specifier: fromCodePoints(src, f + 1, q - (f + 1)), specifierAt, end: q + 1 };
}

/** ES-modules: parse a braced import name list at `braceAt` (the `{`) into imp.names/nameAts/
 *  localNames/localNameAts (rename-aware; comments between names tolerated — bughunt SCAN-2).
 *  Returns the index past the closing `}` on success; on failure emits the diag itself and
 *  returns the bitwise-NOT of the resync index (callers `return ~r` — 0 stays valid). Shared by
 *  the plain named form and the ES COMBINED form (`import Def, { A, B } from`). C++-identical
 *  (ParseImportNameList). */
function parseImportNameList(src: number[], braceAt: number, imp: UetkxImportDecl, out: UetkxFileScanResult): number {
  const bclose = findMatching(src, braceAt);
  if (bclose === -1) {
    pushDiag(out, "UETKX0304", 0, "unclosed `{` in import list", braceAt);
    return ~src.length;
  }
  // Skip whitespace AND comments between names — a `//`/`/* */` inside the brace list must not
  // be misread as a bad name that drops the whole import (bughunt SCAN-2).
  const skipWsAndComments = (p: number): number => {
    for (;;) {
      p = skipWsOnly(src, p);
      const nc = skipNoncode(src, p);
      if (nc === p) return p;
      p = nc;
    }
  };
  for (let p = braceAt + 1; p < bclose; ) {
    p = skipWsAndComments(p);
    if (p >= bclose) break;
    if (src[p] === C_COMMA) {
      p++;
      continue;
    }
    const s = p;
    while (p < bclose && isIdentCp(src[p])) p++;
    if (p === s) {
      pushDiag(out, "UETKX0300", 0, "import list expects bare names, optionally renamed (`Name as Alias`)", s);
      return ~advancePastLine(src, bclose);
    }
    const importedName = fromCodePoints(src, s, p - s);
    imp.names.push(importedName);
    imp.nameAts.push(s);
    // ES-modules (G-05): `{ A as B }` — rename-on-import. localNames stays 1:1 with names; no
    // `as` means the local binding is the imported name itself (alias token == name token —
    // localNameAts mirrors that for the LSP index, TD-033).
    let localName = importedName;
    let localNameAt = s;
    const afterName = skipWsAndComments(p);
    if (afterName < bclose && keywordAt(src, afterName, "as")) {
      const aliasStart = skipWsAndComments(afterName + 2);
      let ap = aliasStart;
      while (ap < bclose && isIdentCp(src[ap])) ap++;
      if (ap === aliasStart) {
        pushDiag(out, "UETKX0300", 0, "missing local alias after `as`", aliasStart);
        return ~advancePastLine(src, bclose);
      }
      localName = fromCodePoints(src, aliasStart, ap - aliasStart);
      localNameAt = aliasStart;
      p = ap;
    }
    imp.localNames.push(localName);
    imp.localNameAts.push(localNameAt);
  }
  if (imp.names.length === 0) {
    pushDiag(out, "UETKX0303", 0, 'import must name at least one binding: `import { Name } from "..."`', braceAt);
  }
  return bclose + 1;
}

/** Duplicate-import diagnostics (2303) for a parsed name list: a name already imported earlier
 *  in this file, or repeated within this same import's braces; records the names into
 *  importedFrom. Shared by the plain named form and the ES COMBINED form. C++-identical
 *  (RecordNamedImportDups). */
function recordNamedImportDups(imp: UetkxImportDecl, importedFrom: Map<string, string>, out: UetkxFileScanResult): void {
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
}

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
      localNames: [],
      localNameAts: [],
      specifier: payload.slice(1), // strip the leading '@'
      specifierAt: quoteAt,
      at: start,
      hostInclude: true,
      isNamespace: false,
      namespaceAlias: "",
      namespaceAliasAt: -1,
      isDefault: false,
      defaultAlias: "",
      defaultAliasAt: -1,
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

  // ES-modules (G-05): `import * as X from "spec"` — namespace import.
  if (k < n && src[k] === 0x2a /* * */) {
    let p = skipWsOnly(src, k + 1);
    if (!keywordAt(src, p, "as")) {
      pushDiag(out, "UETKX0303", 0, 'namespace import expects `* as Name from "..."`', Math.min(p, n - 1));
      return advancePastLine(src, p);
    }
    p = skipWsOnly(src, p + 2);
    const s = p;
    while (p < n && isIdentCp(src[p])) p++;
    if (p === s) {
      pushDiag(out, "UETKX0300", 0, "missing namespace import alias after `as`", p);
      return advancePastLine(src, p);
    }
    const nsImp: UetkxImportDecl = {
      names: [],
      nameAts: [],
      localNames: [],
      localNameAts: [],
      specifier: "",
      specifierAt: -1,
      at: start,
      hostInclude: false,
      isNamespace: true,
      namespaceAlias: fromCodePoints(src, s, p - s),
      namespaceAliasAt: s,
      isDefault: false,
      defaultAlias: "",
      defaultAliasAt: -1,
    };
    const spec = parseFromSpecifier(src, p, out);
    if (!spec.ok) return spec.end;
    nsImp.specifier = spec.specifier;
    nsImp.specifierAt = spec.specifierAt;
    // Local-alias collisions (against other imports and against declarations) are ALL resolved
    // in one end-of-scan pass (UETKX2325) — see scanFile's two-pass block.
    out.imports.push(nsImp);
    return spec.end;
  }

  // ES-modules (G-05): `import Name from "spec"` — default import (a bare identifier where the
  // named form always starts `{`).
  if (k < n && isIdentCp(src[k]) && !(src[k] >= 48 && src[k] <= 57)) {
    const s = k;
    let p = k;
    while (p < n && isIdentCp(src[p])) p++;
    const defImp: UetkxImportDecl = {
      names: [],
      nameAts: [],
      localNames: [],
      localNameAts: [],
      specifier: "",
      specifierAt: -1,
      at: start,
      hostInclude: false,
      isNamespace: false,
      namespaceAlias: "",
      namespaceAliasAt: -1,
      isDefault: true,
      defaultAlias: fromCodePoints(src, s, p - s),
      defaultAliasAt: s,
    };
    // ES COMBINED forms (family parity, Unity 0.9.1 field find): `import Def, { A, B } from`
    // and `import Def, * as X from` — ONE declaration carrying the default binding PLUS the
    // named/star surface (the record models the parts independently). C++-identical.
    let c = skipWsOnly(src, p);
    if (c < n && src[c] === C_COMMA) {
      c = skipWsOnly(src, c + 1);
      if (c < n && src[c] === C_LBRACE) {
        const afterList = parseImportNameList(src, c, defImp, out);
        if (afterList < 0) return ~afterList;
        p = afterList;
      } else if (c < n && src[c] === 0x2a /* * */) {
        let q = skipWsOnly(src, c + 1);
        if (!keywordAt(src, q, "as")) {
          pushDiag(out, "UETKX0303", 0, 'namespace import expects `* as Name from "..."`', Math.min(q, n - 1));
          return advancePastLine(src, q);
        }
        q = skipWsOnly(src, q + 2);
        const s2 = q;
        while (q < n && isIdentCp(src[q])) q++;
        if (q === s2) {
          pushDiag(out, "UETKX0300", 0, "missing namespace import alias after `as`", q);
          return advancePastLine(src, q);
        }
        defImp.isNamespace = true;
        defImp.namespaceAlias = fromCodePoints(src, s2, q - s2);
        defImp.namespaceAliasAt = s2;
        p = q;
      } else {
        pushDiag(out, "UETKX0303", 0, "combined import expects `{ Name, ... }` or `* as Name` after the default binding", Math.min(c, n - 1));
        return advancePastLine(src, c);
      }
    }
    const spec = parseFromSpecifier(src, p, out);
    if (!spec.ok) return spec.end;
    defImp.specifier = spec.specifier;
    defImp.specifierAt = spec.specifierAt;
    // Named-part duplicate diagnostics (2303) apply to a combined name list too; the
    // default/star aliases resolve collisions in the end-of-scan pass (UETKX2325).
    recordNamedImportDups(defImp, importedFrom, out);
    out.imports.push(defImp);
    return spec.end;
  }

  const imp: UetkxImportDecl = {
    names: [],
    nameAts: [],
    localNames: [],
      localNameAts: [],
    specifier: "",
    specifierAt: -1,
    at: start,
    hostInclude: false,
    isNamespace: false,
    namespaceAlias: "",
    namespaceAliasAt: -1,
    isDefault: false,
    defaultAlias: "",
    defaultAliasAt: -1,
  };
  if (k >= n || src[k] !== C_LBRACE) {
    pushDiag(out, "UETKX0303", 0, 'import expects `{ Name, ... }` or a `"@Header.h"` host include after `import`', Math.min(k, n - 1));
    return advancePastLine(src, k);
  }
  const afterList = parseImportNameList(src, k, imp, out);
  if (afterList < 0) return ~afterList;
  const spec = parseFromSpecifier(src, afterList, out);
  if (!spec.ok) return spec.end;
  imp.specifier = spec.specifier;
  imp.specifierAt = spec.specifierAt;
  recordNamedImportDups(imp, importedFrom, out);
  out.imports.push(imp);
  return spec.end;
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
  // §4 markup-everywhere: the jsx-range list of record for this body. Computed before the
  // no-return early-out so a paren-less `return <Tag/>` gets its precise 0114 instead of a
  // bare 2101. C++-identical (ScanComponentBody).
  const jsxRanges = bodyJsxRanges(body);
  diagnoseBareMarkupReturn(body, jsxRanges, bodyAt, out);
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
  diagnoseUnreachableAfterReturn(body, returns, jsxRanges, bodyAt, out);
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
  // §4 rules of hooks (family 0013-0016) — needs the parsed window roots, so it runs last.
  validateHookPlacement(body, jsxRanges, returns, bodyAt, out);
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
    hookCalls: scanHookCalls(body.slice(0, final.returnAt), jsxRanges),
    next: bclose + 1,
    legacySyntax: true,
  });
  out.order.push({ kind: "component", index: idx });
  return bclose + 1;
}

/** ES-modules (U-02): `[export] FRuiNode Name(Type Param = Default, ...) { body }` — the new
 *  plain-declaration component form. The BODY parsing (markup returns, jsx-range scan, hook-call
 *  scan, rules-of-hooks) is IDENTICAL to the legacy wrapper's tail — kept as a separate
 *  near-duplicate rather than refactored into a shared helper (the legacy path is load-bearing
 *  and untouched by this leg). C++-identical (ParseNewComponent). */
function parseNewComponent(
  src: number[],
  declStart: number,
  type: string,
  head: UetkxDeclHead,
  paramText: string,
  parenClose: number,
  exported: boolean,
  out: UetkxFileScanResult,
): number {
  const n = src.length;
  const name = fromCodePoints(src, head.nameAt, head.nameLen);
  if (!name || !(name[0] >= "A" && name[0] <= "Z")) {
    pushDiag(out, "UETKX2100", 0, `component name \`${name}\` must be PascalCase`, head.nameAt, name.length);
  }
  const params = parseNewParams(paramText);
  const k = skipWsOnly(src, parenClose + 1);
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
  const returns = collectMarkupReturns(body);
  const jsxRanges = bodyJsxRanges(body);
  diagnoseBareMarkupReturn(body, jsxRanges, bodyAt, out);
  if (returns.length === 0) {
    pushDiag(out, "UETKX2101", 0, "component has no `return ( ... )` markup return", declStart, 1);
    return -1;
  }
  const final = returns[returns.length - 1];
  if (!final.topLevel) {
    pushDiag(out, "UETKX3007", 0, "the component's final markup `return ( ... )` must be at the top level of the body", bodyAt + final.returnAt, 6);
    return -1;
  }
  const setup = fromCodePoints(body, 0, final.returnAt);
  diagnoseUnreachableAfterReturn(body, returns, jsxRanges, bodyAt, out);
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
    if (span === final) windowNodes = parsed.nodes;
  }
  validateHookPlacement(body, jsxRanges, returns, bodyAt, out);
  const idx = out.components.length;
  out.components.push({
    name,
    exported,
    at: head.typeAt,
    exportAt: exported ? declStart : -1,
    nameAt: head.nameAt,
    params,
    setup,
    setupAt: bodyAt,
    body: fromCodePoints(body, 0, body.length),
    bodyAt,
    windowNodes,
    returns,
    hookCalls: scanHookCalls(body.slice(0, final.returnAt), jsxRanges),
    next: bclose + 1,
    legacySyntax: false,
  });
  out.order.push({ kind: "component", index: idx });
  return bclose + 1;
}

/** ES-modules (U-02): `[export] <Ret> UseName(params) { body }` — the new plain-declaration hook
 *  form (return type LEADS; no `->`; `void` must be written explicitly). C++-identical
 *  (ParseNewHook). */
function parseNewHook(
  src: number[],
  declStart: number,
  retType: string,
  head: UetkxDeclHead,
  paramText: string,
  parenClose: number,
  exported: boolean,
  out: UetkxFileScanResult,
): number {
  const n = src.length;
  const name = fromCodePoints(src, head.nameAt, head.nameLen);
  const params = paramText.trim();
  const k = skipWsOnly(src, parenClose + 1);
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
  out.hooks.push({
    name,
    exported,
    at: head.typeAt,
    exportAt: exported ? declStart : -1,
    nameAt: head.nameAt,
    params,
    ret: retType,
    body: fromCodePoints(src, k + 1, bclose - k - 1),
    bodyAt: k + 1,
    next: bclose + 1,
    legacySyntax: false,
  });
  out.order.push({ kind: "hook", index: idx });
  return bclose + 1;
}

/** ES-modules (U-02): `[export] <Ret> Name(params) { body }` — a module-level UTIL function (not
 *  `Use`-prefixed, not FRuiNode-returning). C++-identical (ParseNewUtil). */
function parseNewUtil(
  src: number[],
  declStart: number,
  retType: string,
  head: UetkxDeclHead,
  paramText: string,
  parenClose: number,
  exported: boolean,
  out: UetkxFileScanResult,
): number {
  const n = src.length;
  const name = fromCodePoints(src, head.nameAt, head.nameLen);
  const params = paramText.trim();
  const k = skipWsOnly(src, parenClose + 1);
  if (k >= n || src[k] !== C_LBRACE) {
    pushDiag(out, "UETKX0303", 0, `function \`${name}\` body \`{ ... }\` expected`, Math.min(k, n - 1));
    return -1;
  }
  const bclose = findMatching(src, k);
  if (bclose === -1) {
    pushDiag(out, "UETKX0304", 0, "unclosed function body", k);
    return -1;
  }
  const idx = out.utils.length;
  out.utils.push({
    name,
    nameAt: head.nameAt,
    retType,
    retTypeAt: head.typeAt,
    params,
    body: fromCodePoints(src, k + 1, bclose - k - 1),
    bodyAt: k + 1,
    exported,
    exportAt: exported ? declStart : -1,
    at: head.typeAt,
    next: bclose + 1,
  });
  out.order.push({ kind: "util", index: idx });
  return bclose + 1;
}

/** ES-modules (U-01): `[export] <Type> Name = <Init>;` — a module-level VALUE export. `type` may
 *  be empty (inference sugar — UETKX2322 when the initializer doesn't name its own type).
 *  C++-identical (ParseNewValue). */
function parseNewValue(src: number[], declStart: number, type: string, head: UetkxDeclHead, exported: boolean, out: UetkxFileScanResult): number {
  const n = src.length;
  const name = fromCodePoints(src, head.nameAt, head.nameLen);
  const initStart = skipWsOnly(src, head.triggerAt + 1);
  if (!type) {
    let p = initStart;
    const s = p;
    while (p < n && isIdentCp(src[p])) p++;
    const isIdent = p > s;
    const q = skipWsOnly(src, p);
    const namesType = isIdent && q < n && (src[q] === C_LPAREN || src[q] === C_LBRACE || src[q] === C_LT);
    if (!namesType) {
      pushDiag(
        out,
        "UETKX2322",
        0,
        `cannot infer the type of \`${name}\` — the initializer must name the type (\`T(...)\` / \`T{...}\`), or declare it: \`export <Type> ${name} = ...\``,
        initStart,
      );
    }
  }
  const semiAt = findValueEnd(src, initStart);
  if (semiAt === -1) {
    pushDiag(out, "UETKX0304", 0, "unterminated value declaration — expected `;`", initStart);
    return -1;
  }
  const idx = out.values.length;
  out.values.push({
    name,
    nameAt: head.nameAt,
    type,
    typeAt: type ? head.typeAt : -1,
    init: fromCodePoints(src, initStart, semiAt - initStart).trim(),
    initAt: initStart,
    exported,
    exportAt: exported ? declStart : -1,
    at: head.typeAt,
    next: semiAt + 1,
  });
  out.order.push({ kind: "value", index: idx });
  return semiAt + 1;
}

/** ES-modules (U-02): try to parse a NEW plain declaration at `start` (just past the optional
 *  `export`) — signature-only classification. Returns the index past the declaration on success,
 *  -1 on a fatal parse error (diag already recorded), or -2 when the head doesn't classify as any
 *  declaration kind at all (the caller falls back to the legacy 2101). C++-identical
 *  (ParseNewFormDecl). */
function parseNewFormDecl(src: number[], start: number, exported: boolean, declStart: number, out: UetkxFileScanResult): number {
  const head = scanDeclHead(src, start);
  if (head.trigger === "" || head.trigger === "{" || head.nameAt === -1) {
    return -2;
  }
  const name = fromCodePoints(src, head.nameAt, head.nameLen);
  const type = fromCodePoints(src, head.typeAt, Math.max(0, head.typeLen)).trim();
  const looksLikeHook = looksLikeHookName(name);

  if (head.trigger === "(") {
    const pc = findMatching(src, head.triggerAt);
    if (pc === -1) {
      pushDiag(out, "UETKX0304", 0, "unclosed `(` in parameter list", head.triggerAt);
      return -1;
    }
    const paramText = fromCodePoints(src, head.triggerAt + 1, pc - head.triggerAt - 1);

    if (type === "FRuiNode") {
      if (looksLikeHook) {
        pushDiag(
          out,
          "UETKX2321",
          0,
          `\`${name}\` is \`Use\`-prefixed but returns FRuiNode — did you mean a component? (hooks must not return markup nodes)`,
          head.nameAt,
          name.length,
        );
      }
      return parseNewComponent(src, declStart, type, head, paramText, pc, exported, out);
    }
    if (looksLikeHook) {
      return parseNewHook(src, declStart, type, head, paramText, pc, exported, out);
    }
    return parseNewUtil(src, declStart, type, head, paramText, pc, exported, out);
  }

  // head.trigger === "="
  return parseNewValue(src, declStart, type, head, exported, out);
}

/** ES-modules (U-09): one name requested by a deferred `export { ... };` list, awaiting
 *  end-of-scan resolution. */
interface PendingExportName {
  name: string;
  at: number;
}

/** ES-modules (U-09): `export { a, b };` — a deferred export-list STATEMENT, not a declaration.
 *  `braceAt` is the `{`; names are recorded into pendingList for end-of-scan resolution
 *  (UETKX2323/2324). Returns the index past an optional trailing `;`. C++-identical
 *  (ParseExportList). */
function parseExportList(src: number[], braceAt: number, out: UetkxFileScanResult, pendingList: PendingExportName[]): number {
  const n = src.length;
  const bclose = findMatching(src, braceAt);
  if (bclose === -1) {
    pushDiag(out, "UETKX0304", 0, "unclosed `{` in export list", braceAt);
    return n;
  }
  for (let p = braceAt + 1; p < bclose; ) {
    const j = skipNoncode(src, p);
    if (j !== p) {
      p = j;
      continue;
    }
    const c = src[p];
    if (c === C_SPACE || c === C_TAB || c === C_NL || c === C_CR) {
      p++;
      continue;
    }
    if (c === C_COMMA) {
      p++;
      continue;
    }
    const s = p;
    while (p < bclose && isIdentCp(src[p])) p++;
    if (p === s) {
      pushDiag(out, "UETKX0300", 0, "export list expects bare names: `export { A, B };`", s);
      return advancePastLine(src, bclose);
    }
    pendingList.push({ name: fromCodePoints(src, s, p - s), at: s });
  }
  let end = bclose + 1;
  const sc = skipWsOnly(src, end);
  if (sc < n && src[sc] === 0x3b /* ; */) end = sc + 1;
  return end;
}

/** ES-modules (U-08): `export default Name;` — a STATEMENT, one per file (a second is UETKX2327,
 *  checked immediately). `defaultAt` is the `default` keyword; `exportKeywordAt` anchors the 2327
 *  diag at the offending statement's `export`. Whether the named decl actually EXISTS is resolved
 *  at end-of-scan (UETKX2323), like export lists. C++-identical (ParseExportDefault). */
function parseExportDefault(src: number[], defaultAt: number, exportKeywordAt: number, out: UetkxFileScanResult): number {
  const n = src.length;
  let k = skipWsOnly(src, defaultAt + 7); // past "default"
  const s = k;
  while (k < n && isIdentCp(src[k])) k++;
  if (k === s) {
    pushDiag(out, "UETKX0300", 0, "`export default` expects a declared name", k);
    return advancePastLine(src, k);
  }
  const name = fromCodePoints(src, s, k - s);
  if (out.defaultExportName) {
    pushDiag(out, "UETKX2327", 0, "duplicate `export default` (a file has at most one default export)", exportKeywordAt, 6);
  } else {
    out.defaultExportName = name;
    out.defaultExportAt = s;
  }
  let end = k;
  const sc = skipWsOnly(src, end);
  if (sc < n && src[sc] === 0x3b /* ; */) end = sc + 1;
  return end;
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
  out.hooks.push({ name, exported, at: hi, exportAt: -1, nameAt, params, ret, body: fromCodePoints(src, k + 1, bclose - k - 1), bodyAt: k + 1, next: bclose + 1, legacySyntax: true });
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
    if (s >= src.length) continue;
    // ES-modules (U-01): a new-form declaration can start with ANY type identifier, so this can
    // no longer key off a fixed keyword list — any non-blank line start is a resync candidate
    // (same reasoning as scanFile's preamble-end fix).
    if (skipNoncode(src, s) === s && src[s] !== C_SPACE && src[s] !== C_TAB && src[s] !== C_CR) {
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
    values: [],
    utils: [],
    order: [],
    diags: [],
    defaultExportName: "",
    defaultExportAt: -1,
    exportListEntries: [],
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
    if (src[i] === C_SPACE || src[i] === C_TAB || src[i] === C_NL || src[i] === C_CR) {
      i++;
      continue;
    }
    // ES-modules (U-01): nothing else is legal in the preamble — the first declaration may be a
    // legacy wrapper keyword OR a new-form typed declaration (which can start with ANY type
    // identifier, so it can no longer be recognized by an explicit keyword list here). Anything
    // that isn't whitespace/a comment/an import IS the first declaration; the declaration loop
    // below classifies it. C++-identical (FUetkxFileScan::Scan preamble loop).
    break;
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

  // ES-modules (U-09): `export { a, b };` requests, resolved once every decl kind is scanned.
  // Kept on the result (exportListEntries) for the LSP rename/references index (TD-033).
  const pendingExportList: PendingExportName[] = out.exportListEntries;

  const wrapperDeprecation =
    "wrapper syntax is deprecated — write a plain typed declaration (`export FRuiNode Name(...)` / `export <Type> UseName(...)` / `export <Type> Name = ...`); run `-run=RUIMigrateEsModules`. Removed in the next minor.";

  // declarations: a SEQUENCE of components/hooks/modules/values/utils in any order (mixed-decl v1)
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

    // ES-modules (U-09): `export { a, b };` — a deferred export list, not a declaration.
    if (exported && i < n && src[i] === C_LBRACE) {
      i = parseExportList(src, i, out, pendingExportList);
      continue;
    }
    // ES-modules (U-08): `export default Name;` — a statement, not a declaration.
    if (exported && keywordAt(src, i, "default")) {
      i = parseExportDefault(src, i, declStart, out);
      continue;
    }

    let next = -1;
    if (keywordAt(src, i, "component")) {
      pushDiag(out, "UETKX2320", 1, `\`component\` ${wrapperDeprecation}`, i, 9);
      next = parseComponent(src, i, exported, out);
      if (next >= 0 && exported) out.components[out.components.length - 1].exportAt = declStart; // the decl's true start
    } else if (keywordAt(src, i, "hook")) {
      pushDiag(out, "UETKX2320", 1, `\`hook\` ${wrapperDeprecation}`, i, 4);
      next = parseHook(src, i, exported, out);
      if (next >= 0 && exported) out.hooks[out.hooks.length - 1].exportAt = declStart; // the decl's true start
    } else if (keywordAt(src, i, "module")) {
      pushDiag(out, "UETKX2320", 1, `\`module\` ${wrapperDeprecation}`, i, 6);
      next = parseModule(src, i, exported, out);
      if (next >= 0 && exported) out.modules[out.modules.length - 1].exportAt = declStart; // the decl's true start
    }
    else {
      // ES-modules (U-02): not a wrapper keyword — try the new plain-declaration grammar
      // (signature-only classification). Falls through to the legacy catch-all 2101 when the
      // head doesn't resolve to any declaration kind at all (e.g. EOF, a bare `{`).
      next = parseNewFormDecl(src, i, exported, declStart, out);
      if (next === -2) {
        pushDiag(
          out,
          "UETKX2101",
          0,
          exported
            ? "`export` must be followed by `component`, `hook`, `module`, or a typed declaration"
            : "expected a declaration (`component`, `hook`, `module`, or a typed declaration)",
          declStart,
        );
        return out;
      }
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

  if (
    out.components.length === 0 &&
    out.hooks.length === 0 &&
    out.modules.length === 0 &&
    out.values.length === 0 &&
    out.utils.length === 0
  ) {
    pushDiag(out, "UETKX2101", 0, "no `component`, `hook`, or `module` declaration found", -1);
    return out;
  }

  // ── ES-modules (U-08/U-09) two-pass resolution: `export { … }` / `export default` may name a
  // declaration anywhere in the file (forward or backward) — resolve now that every kind's array
  // is fully populated. Import-alias-vs-declaration collisions (2325) resolve here too, since
  // declarations aren't known yet while imports are still being scanned (preamble-only, always
  // preceding every declaration). C++-identical (FUetkxFileScan::Scan's two-pass block).
  {
    type AnyDecl = { name: string; exported: boolean; exportAt: number };
    const allDecls = (): AnyDecl[] => [...out.components, ...out.hooks, ...out.modules, ...out.values, ...out.utils];
    const findDecl = (name: string): AnyDecl | undefined => allDecls().find((d) => d.name === name);

    // exportAt stays -1 for a LIST-exported decl (U-09) — `exported && exportAt < 0` is how the
    // formatter's form-preservation distinguishes "exported via `export { … };`" from an inline
    // `export` prefix (C++-identical — MarkExported).
    for (const e of pendingExportList) {
      const decl = findDecl(e.name);
      if (!decl) {
        pushDiag(out, "UETKX2323", 0, `\`export\` names \`${e.name}\`, which is not declared in this file`, e.at, e.name.length);
        continue;
      }
      if (decl.exported) {
        pushDiag(
          out,
          "UETKX2324",
          0,
          `duplicate export of \`${e.name}\` (already exported inline or in a previous export list)`,
          e.at,
          e.name.length,
        );
        continue;
      }
      decl.exported = true;
    }

    if (out.defaultExportName) {
      const decl = findDecl(out.defaultExportName);
      if (!decl) {
        pushDiag(
          out,
          "UETKX2323",
          0,
          `\`export\` names \`${out.defaultExportName}\`, which is not declared in this file`,
          out.defaultExportAt,
          out.defaultExportName.length,
        );
      }
    }

    // 2325: import local-alias collisions — against every declared name (any visibility), and
    // against EACH OTHER. The "against each other" axis deliberately EXCLUDES a plain-vs-plain
    // collision (a named import with no `as`) — that shape is UETKX2303's job (dedup by imported
    // name) and already fires for it; a plain alias colliding with a RENAMED/namespace/default
    // alias (a shape 2303 can't see, since it keys on the imported name) still gets 2325.
    const localAliasSeen = new Map<string, boolean>(); // local name -> was its FIRST occurrence "plain"?
    const checkAlias = (local: string, at: number, plain: boolean) => {
      if (!local) return;
      const collidesWithDecl = findDecl(local) !== undefined;
      const prevPlain = localAliasSeen.get(local);
      const collidesWithImport = prevPlain !== undefined && !(plain && prevPlain);
      if (collidesWithDecl || collidesWithImport) {
        pushDiag(out, "UETKX2325", 0, `import alias \`${local}\` collides with a declaration or another import in this file`, at, local.length);
      }
      if (prevPlain === undefined) localAliasSeen.set(local, plain);
    };
    // No exclusive branching — an ES COMBINED import (`import Def, { A } from` / `import Def,
    // * as X from`) carries default + named/star parts in one declaration, and EVERY part's
    // local binding joins the collision space (`import a, { b as a }` must 2325).
    for (const imp of out.imports) {
      if (imp.hostInclude) continue;
      if (imp.isDefault) {
        checkAlias(imp.defaultAlias, imp.defaultAliasAt, false);
      }
      if (imp.isNamespace) {
        checkAlias(imp.namespaceAlias, imp.namespaceAliasAt, false);
      }
      for (let idx2 = 0; idx2 < imp.localNames.length; idx2++) {
        const plain = imp.localNames[idx2] === imp.names[idx2];
        checkAlias(imp.localNames[idx2], imp.nameAts[idx2] ?? imp.at, plain);
      }
    }
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
