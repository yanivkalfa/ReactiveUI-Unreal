// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// §4 markup-everywhere: byte-discipline port of the plugin's UetkxJsxScan.cpp (itself the
// family port of guitkx_jsx_scan.gd / Godot's jsxScan.ts). Same function names, same boundary
// set, same { end: -1 } unbalanced contract. Change BOTH files or neither.
//
// The hard problem is telling a markup `<` from a less-than operator. Like the siblings we do
// NOT attempt general disambiguation — a POSITION-GATED whitelist: a `<` begins markup ONLY
// when it follows (whitespace-skipped) a boundary token that can only be followed by an
// expression, AND the char after `<` is a tag-name start (letter/`_`) or `>` (fragment).
// C++ boundary set: start-of-expression, `(`, `[`, `,`, simple `=`, `&&`, `||`, `?`, ternary
// `:` (never `::`), `return`, `else`.

import { skipNoncode, skipNoncodeMarkup, findMatching, keywordAt, isIdentCode } from "./cppScanner";

const LT = "<".codePointAt(0)!;
const GT = ">".codePointAt(0)!;
const SLASH = "/".codePointAt(0)!;
const LBRACE = "{".codePointAt(0)!;
const LPAREN = "(".codePointAt(0)!;
const LBRACKET = "[".codePointAt(0)!;
const COMMA = ",".codePointAt(0)!;
const QUESTION = "?".codePointAt(0)!;
const COLON = ":".codePointAt(0)!;
const EQ = "=".codePointAt(0)!;
const AMP = "&".codePointAt(0)!;
const PIPE = "|".codePointAt(0)!;
const UNDERSCORE = "_".codePointAt(0)!;

export interface MarkupRange {
  start: number;
  end: number; // -1 = unbalanced: markup opens but never closes (owns the rest of the region)
  op: string; // "" | "&&" | "||"
  opPos: number;
}

export function findMarkupRanges(src: readonly number[], start: number, end: number): MarkupRange[] {
  const out: MarkupRange[] = [];
  let i = start;
  // markup at the very start of the expression (e.g. an attr value that IS markup)
  const s0 = skipWs(src, start, end);
  if (markupAt(src, s0, end)) {
    const e0 = findElementEnd(src, s0, end);
    if (e0 === -1) return [{ start: s0, end: -1, op: "", opPos: start }];
    out.push({ start: s0, end: e0, op: "", opPos: start });
    i = e0;
  }
  while (i < end) {
    const j = skipNoncode(src, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = src[i];
    if (c === LPAREN || c === LBRACKET || c === COMMA || c === QUESTION) {
      i = tryAt(src, i + 1, end, "", i, out, i + 1);
      continue;
    }
    if (c === EQ && isSimpleAssign(src, i, end)) {
      i = tryAt(src, i + 1, end, "", i, out, i + 1);
      continue;
    }
    if (c === AMP && i + 1 < end && src[i + 1] === AMP) {
      i = tryAt(src, i + 2, end, "&&", i, out, i + 2);
      continue;
    }
    if (c === PIPE && i + 1 < end && src[i + 1] === PIPE) {
      i = tryAt(src, i + 2, end, "||", i, out, i + 2);
      continue;
    }
    // ternary else-branch `:` (never `::`, never preceded by `:`)
    if (c === COLON && !(i + 1 < end && src[i + 1] === COLON) && !(i > 0 && src[i - 1] === COLON)) {
      i = tryAt(src, i + 1, end, "", i, out, i + 1);
      continue;
    }
    if ((c === 0x72 /* r */ || c === 0x65 /* e */) && isIdentBoundary(src, i)) {
      // keyword boundaries: return / else
      if (keywordAt(src, i, "return")) {
        i = tryAt(src, i + 6, end, "", i, out, i + 6);
        continue;
      }
      if (keywordAt(src, i, "else")) {
        i = tryAt(src, i + 4, end, "", i, out, i + 4);
        continue;
      }
    }
    i += 1;
  }
  return out;
}

// Peek for markup at the next ws-skipped position; if found, record [p, elemEnd) (with the
// boundary op + its position) and return elemEnd so the caller jumps past it. Markup that never
// closes is recorded as { end: -1 } and ends the scan. Otherwise return `fallback`.
function tryAt(
  src: readonly number[],
  after: number,
  end: number,
  op: string,
  opPos: number,
  out: MarkupRange[],
  fallback: number,
): number {
  const p = skipWs(src, after, end);
  if (markupAt(src, p, end)) {
    const e = findElementEnd(src, p, end);
    if (e === -1) {
      out.push({ start: p, end: -1, op, opPos });
      return end;
    }
    out.push({ start: p, end: e, op, opPos });
    return e;
  }
  return fallback;
}

export function markupAt(src: readonly number[], i: number, end: number): boolean {
  if (i >= end || src[i] !== LT) return false;
  if (i + 1 >= end) return false;
  const c = src[i + 1];
  return (
    c === GT ||
    c === UNDERSCORE ||
    (c >= 0x61 && c <= 0x7a) || // a-z
    (c >= 0x41 && c <= 0x5a) // A-Z
  );
}

// From a `<` at `open`, the index just past the outermost element close. Tracks tag depth,
// routing strings/comments + balanced `{…}` attribute/child holes through the lexer. -1 if
// unbalanced.
export function findElementEnd(src: readonly number[], open: number, end: number): number {
  let depth = 0;
  let i = open;
  while (i < end) {
    // markup lexis over tag names/text/attributes; `{…}` holes route through findMatching
    // (C++ lexis) below.
    const j = skipNoncodeMarkup(src, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = src[i];
    if (c === LBRACE) {
      const close = findMatching(src, i); // skip an attr/child {…} hole whole
      if (close === -1 || close >= end) return -1;
      i = close + 1;
      continue;
    }
    if (c === LT) {
      if (i + 1 < end && src[i + 1] === SLASH) {
        // closing tag </...> or fragment </>
        depth -= 1;
        let gt = -1;
        for (let k = i; k < end; k += 1) {
          if (src[k] === GT) {
            gt = k;
            break;
          }
        }
        if (gt === -1) return -1;
        i = gt + 1;
        if (depth === 0) return i;
        continue;
      }
      if (i + 1 < end && src[i + 1] === GT) {
        depth += 1; // fragment open <>
        i += 2;
        continue;
      }
      if (markupAt(src, i, end)) {
        const t = scanOpenTag(src, i, end);
        if (t.gt === -1) return -1;
        i = t.gt + 1;
        if (t.selfClosing) {
          if (depth === 0) return i;
        } else {
          depth += 1;
        }
        continue;
      }
    }
    i += 1;
  }
  return -1;
}

// Scan an opening tag from its `<` to its terminating `>` / `/>`, treating every attribute
// `{…}` hole and quoted string as opaque.
function scanOpenTag(src: readonly number[], lt: number, end: number): { gt: number; selfClosing: boolean } {
  let i = lt + 1;
  while (i < end && isIdentCode(src[i])) i += 1; // tag name
  while (i < end) {
    const j = skipNoncodeMarkup(src, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = src[i];
    if (c === LBRACE) {
      const close = findMatching(src, i);
      if (close === -1 || close >= end) return { gt: -1, selfClosing: false };
      i = close + 1;
      continue;
    }
    if (c === SLASH && i + 1 < end && src[i + 1] === GT) return { gt: i + 1, selfClosing: true };
    if (c === GT) return { gt: i, selfClosing: false };
    i += 1;
  }
  return { gt: -1, selfClosing: false };
}

// --- token helpers ---
function skipWs(src: readonly number[], i: number, end: number): number {
  while (i < end && (src[i] === 0x20 || src[i] === 0x09 || src[i] === 0x0a || src[i] === 0x0d)) i += 1;
  return i;
}

function isIdentBoundary(src: readonly number[], i: number): boolean {
  return i === 0 || !isIdentCode(src[i - 1]);
}

// A `=` is a simple assignment — not ==, <=, >=, !=, +=, -=, *=, /=, %=, &=, |=, ^= (and never
// the 2nd `=` of `==`).
function isSimpleAssign(src: readonly number[], i: number, end: number): boolean {
  if (i + 1 < end && src[i + 1] === EQ) return false;
  if (i === 0) return true;
  const p = src[i - 1];
  const bad = "=!<>+-*/%&|^";
  for (let k = 0; k < bad.length; k += 1) {
    if (p === bad.codePointAt(k)) return false;
  }
  return true;
}
