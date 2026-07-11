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
  C_LBRACE = 123;

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

export interface UetkxComponentDecl {
  name: string;
  at: number; // the `component` keyword offset
  nameAt: number;
  params: UetkxParam[];
  setup: string;
  setupAt: number;
  body: string;
  bodyAt: number;
  windowNodes: UetkxNode[];
  hookCalls: string[];
  next: number;
}

export interface UetkxFileScanResult {
  preambleIncludes: string[];
  components: UetkxComponentDecl[];
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
  "UseImperativeHandle",
  "UseSafeArea",
  "UseSignal",
  "UseSignalKey",
  "UseTween",
  "UseAnimate",
  "UseTweenValue",
  "UseSfx",
];

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

export function scanFile(source: string, basename: string): UetkxFileScanResult {
  const out: UetkxFileScanResult = { preambleIncludes: [], components: [], diags: [] };
  const src: number[] = [];
  for (const ch of source) src.push(ch.codePointAt(0)!);
  const n = src.length;
  const addDiag = (code: string, severity: number, message: string, offset: number, length = 1) =>
    out.diags.push({ code, severity, message, offset, length });

  // preamble: verbatim #include lines before the first declaration
  let i = 0;
  while (i < n) {
    const j = skipNoncode(src, i);
    if (j !== i) {
      if (src[i] === C_HASH) {
        const line = fromCodePoints(src, i, j - i).trim();
        if (line.startsWith("#include")) out.preambleIncludes.push(line);
      }
      i = j;
      continue;
    }
    if (keywordAt(src, i, "component")) break;
    i++;
  }

  let any = false;
  while (i < n) {
    const j = skipNoncode(src, i);
    if (j !== i) {
      i = j;
      continue;
    }
    if (!keywordAt(src, i, "component")) {
      i++;
      continue;
    }
    any = true;
    const ci = i;
    let k = skipWsOnly(src, ci + 9);
    const ns = k;
    while (k < n && isIdentCp(src[k])) k++;
    const name = fromCodePoints(src, ns, k - ns);
    if (!name) {
      addDiag("UETKX0300", 0, "missing component name", k);
      return out;
    }
    if (!(name[0] >= "A" && name[0] <= "Z")) {
      addDiag("UETKX2100", 0, `component name \`${name}\` must be PascalCase`, ns, name.length);
    }
    k = skipWsOnly(src, k);
    let params: UetkxParam[] = [];
    if (k < n && src[k] === C_LPAREN) {
      const pc = findMatching(src, k);
      if (pc === -1) {
        addDiag("UETKX0304", 0, "unclosed `(` in component params", k);
        return out;
      }
      params = parseParams(fromCodePoints(src, k + 1, pc - k - 1));
      k = pc + 1;
    }
    k = skipWsOnly(src, k);
    if (k >= n || src[k] !== C_LBRACE) {
      addDiag("UETKX0303", 0, "component body `{ ... }` expected", Math.min(k, n - 1));
      return out;
    }
    const bclose = findMatchingMarkup(src, k);
    if (bclose === -1) {
      addDiag("UETKX0304", 0, "unclosed component body", k);
      return out;
    }
    const bodyAt = k + 1;
    const body = src.slice(bodyAt, bclose);
    const split = splitMarkupReturn(body, true);
    if (!split.ok) {
      addDiag("UETKX2101", 0, "component has no `return ( ... )` markup return", ci, 9);
      return out;
    }
    const setup = fromCodePoints(body, 0, split.returnAt);
    const parsed = parseMarkup(body, split.mStart, split.mEnd);
    if (parsed.errorCode) {
      addDiag(parsed.errorCode, 0, parsed.errorMsg, bodyAt + Math.max(0, parsed.errorAt));
      return out;
    }
    const renderRoots = parsed.nodes.filter((node) => node.type !== "comment");
    if (renderRoots.length !== 1) {
      addDiag("UETKX0108", 0, `a component must return exactly one root element (got ${renderRoots.length})`,
              bodyAt + split.mStart);
      return out;
    }
    if (name !== basename && basename) {
      addDiag("UETKX0103", 1, `component \`${name}\` differs from file name \`${basename}\``, ns, name.length);
    }
    out.components.push({
      name,
      at: ci,
      nameAt: ns,
      params,
      setup,
      setupAt: bodyAt,
      body: fromCodePoints(body, 0, body.length),
      bodyAt,
      windowNodes: parsed.nodes,
      hookCalls: scanHookCalls(body.slice(0, split.returnAt)),
      next: bclose + 1,
    });
    i = bclose + 1;
  }
  if (!any) addDiag("UETKX2101", 0, "no `component` declaration found", -1);
  return out;
}

function isIdentCp(c: number): boolean {
  return c === 95 || (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || (c >= 48 && c <= 57);
}
