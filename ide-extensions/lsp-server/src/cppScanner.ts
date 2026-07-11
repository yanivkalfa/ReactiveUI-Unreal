// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TypeScript port of the .uetkx C++-lexis scanner (FUetkxLexer, UetkxLexer.cpp) — byte-for-
// byte semantics, pinned by the SHARED corpus test-fixtures/uetkx-scanner-cases.json that the
// C++ automation suite also replays (D-22). All indices are CODE POINTS over number[].
//
// C++ lexis: // and /* */ comments; preprocessor LINES (first-non-ws `#`, backslash
// continuations); "..."/'...' with escapes; R"delim(...)delim" raw strings; L/u8/u/U prefixes
// (token-start rule); a C++14 digit-separator apostrophe (1'000'000) is not a char literal.

const C_NL = 10,
  C_CR = 13,
  C_SPACE = 32,
  C_TAB = 9,
  C_QUOTE = 34,
  C_APOS = 39,
  C_HASH = 35,
  C_SLASH = 47,
  C_STAR = 42,
  C_BSLASH = 92,
  C_LT = 60,
  C_BANG = 33,
  C_DASH = 45,
  C_AT = 64,
  C_LPAREN = 40,
  C_RPAREN = 41,
  C_LBRACE = 123,
  C_RBRACE = 125,
  C_LBRACKET = 91,
  C_RBRACKET = 93;

export function isIdentCode(c: number): boolean {
  return c === 95 || (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || (c >= 48 && c <= 57);
}

function prefixedQuoteAt(src: readonly number[], index: number): { quoteAt: number; raw: boolean } | null {
  const n = src.length;
  if (index > 0 && isIdentCode(src[index - 1])) return null; // mid-identifier
  let i = index;
  const c = src[i];
  if (c === 76 /*L*/ || c === 85 /*U*/) i++;
  else if (c === 117 /*u*/) {
    i++;
    if (i < n && src[i] === 56 /*8*/) i++;
  }
  let raw = false;
  if (i < n && src[i] === 82 /*R*/) {
    raw = true;
    i++;
  }
  if (i > index && i < n && src[i] === C_QUOTE) return { quoteAt: i, raw };
  return null;
}

export function isPreprocessorLine(src: readonly number[], index: number): boolean {
  for (let k = index - 1; k >= 0; k--) {
    const c = src[k];
    if (c === C_NL) return true;
    if (c !== C_SPACE && c !== C_TAB && c !== C_CR) return false;
  }
  return true;
}

export function skipString(src: readonly number[], quoteIndex: number): number {
  const n = src.length;
  const q = src[quoteIndex];
  if (quoteIndex + 2 < n && src[quoteIndex + 1] === q && src[quoteIndex + 2] === q) {
    let j = quoteIndex + 3;
    while (j < n) {
      if (src[j] === C_BSLASH) {
        j += 2;
        continue;
      }
      if (src[j] === q && j + 2 < n && src[j + 1] === q && src[j + 2] === q) return j + 3;
      j++;
    }
    return n;
  }
  let k = quoteIndex + 1;
  while (k < n) {
    const ch = src[k];
    if (ch === C_BSLASH) {
      k += 2;
      continue;
    }
    if (ch === q) return k + 1;
    if (ch === C_NL) return k; // single-line literals don't span newlines (family rule)
    k++;
  }
  return n;
}

export function skipRawString(src: readonly number[], rQuoteIndex: number): number {
  const n = src.length;
  let j = rQuoteIndex + 1;
  const delim: number[] = [];
  while (j < n && src[j] !== C_LPAREN && src[j] !== C_NL && j - rQuoteIndex <= 17) {
    delim.push(src[j]);
    j++;
  }
  if (j >= n || src[j] !== C_LPAREN) return skipString(src, rQuoteIndex); // malformed — degrade
  j++;
  while (j < n) {
    if (src[j] === C_RPAREN) {
      let k = j + 1;
      let d = 0;
      while (d < delim.length && k < n && src[k] === delim[d]) {
        k++;
        d++;
      }
      if (d === delim.length && k < n && src[k] === C_QUOTE) return k + 1;
    }
    j++;
  }
  return n;
}

export function skipNoncode(src: readonly number[], index: number): number {
  const n = src.length;
  if (index >= n) return index;
  const c = src[index];
  if (c === C_SLASH && index + 1 < n && src[index + 1] === C_SLASH) {
    let j = index + 2;
    while (j < n && src[j] !== C_NL) j++;
    return j;
  }
  if (c === C_SLASH && index + 1 < n && src[index + 1] === C_STAR) {
    let j = index + 2;
    while (j + 1 < n) {
      if (src[j] === C_STAR && src[j + 1] === C_SLASH) return j + 2;
      j++;
    }
    return n;
  }
  if (c === C_HASH && isPreprocessorLine(src, index)) {
    let j = index + 1;
    while (j < n) {
      if (src[j] === C_NL) {
        let k = j - 1;
        if (k >= 0 && src[k] === C_CR) k--;
        if (k >= index && src[k] === C_BSLASH) {
          j++;
          continue;
        }
        return j;
      }
      j++;
    }
    return n;
  }
  const pref = prefixedQuoteAt(src, index);
  if (pref) return pref.raw ? skipRawString(src, pref.quoteAt) : skipString(src, pref.quoteAt);
  if (c === 82 /*R*/ && index + 1 < n && src[index + 1] === C_QUOTE && (index === 0 || !isIdentCode(src[index - 1]))) {
    return skipRawString(src, index + 1);
  }
  if (c === C_QUOTE || c === C_APOS) {
    if (c === C_APOS && index > 0 && src[index - 1] >= 48 && src[index - 1] <= 57) return index; // digit separator
    return skipString(src, index);
  }
  return index;
}

export function skipNoncodeMarkup(src: readonly number[], index: number): number {
  const n = src.length;
  if (index >= n) return index;
  const c = src[index];
  if (c === C_SLASH && index + 1 < n && src[index + 1] === C_SLASH) {
    let j = index + 2;
    while (j < n && src[j] !== C_NL) j++;
    return j;
  }
  if (c === C_SLASH && index + 1 < n && src[index + 1] === C_STAR) {
    let j = index + 2;
    while (j + 1 < n) {
      if (src[j] === C_STAR && src[j + 1] === C_SLASH) return j + 2;
      j++;
    }
    return n;
  }
  if (c === C_LT && index + 3 < n && src[index + 1] === C_BANG && src[index + 2] === C_DASH && src[index + 3] === C_DASH) {
    let j = index + 4;
    while (j + 2 < n) {
      if (src[j] === C_DASH && src[j + 1] === C_DASH && src[j + 2] === 62 /*>*/) return j + 3;
      j++;
    }
    return n;
  }
  if (c === C_QUOTE || c === C_APOS) return skipString(src, index);
  return index;
}

export function findMatching(src: readonly number[], openIndex: number): number {
  const n = src.length;
  const stack: number[] = [];
  let i = openIndex;
  while (i < n) {
    const j = skipNoncode(src, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = src[i];
    if (c === C_LPAREN || c === C_LBRACE || c === C_LBRACKET) stack.push(c);
    else if (c === C_RPAREN || c === C_RBRACE || c === C_RBRACKET) {
      if (stack.length === 0) return -1;
      const top = stack.pop()!;
      if (
        (c === C_RPAREN && top !== C_LPAREN) ||
        (c === C_RBRACE && top !== C_LBRACE) ||
        (c === C_RBRACKET && top !== C_LBRACKET)
      )
        return -1;
      if (stack.length === 0) return i;
    }
    i++;
  }
  return -1;
}

export function keywordAt(src: readonly number[], index: number, word: string): boolean {
  const n = src.length;
  const w = toCodes(word);
  if (index < 0 || index + w.length > n) return false;
  for (let k = 0; k < w.length; k++) if (src[index + k] !== w[k]) return false;
  if (index > 0 && isIdentCode(src[index - 1])) return false;
  if (index + w.length < n && isIdentCode(src[index + w.length])) return false;
  return true;
}

function toCodes(word: string): number[] {
  const out: number[] = [];
  for (const ch of word) out.push(ch.codePointAt(0)!);
  return out;
}

export function isMarkupLine(src: readonly number[], lineStart: number, lineEnd: number): boolean {
  let k = lineStart;
  while (k < lineEnd) {
    const c = src[k];
    if (c === C_SPACE || c === C_TAB || c === C_CR) {
      k++;
      continue;
    }
    if (c === C_LT || c === C_LBRACE) return true;
    if (c === C_SLASH && k + 1 < lineEnd) {
      const nx = src[k + 1];
      return nx === C_SLASH || nx === C_STAR;
    }
    if (c === C_AT) {
      return (
        keywordAt(src, k + 1, "if") ||
        keywordAt(src, k + 1, "elif") ||
        keywordAt(src, k + 1, "else") ||
        keywordAt(src, k + 1, "for") ||
        keywordAt(src, k + 1, "while") ||
        keywordAt(src, k + 1, "match") ||
        keywordAt(src, k + 1, "case") ||
        keywordAt(src, k + 1, "default")
      );
    }
    return false;
  }
  return false;
}

export function findMatchingMarkup(src: readonly number[], openIndex: number): number {
  const enum Mode {
    Body,
    Markup,
    Code,
  }
  const n = src.length;
  const delims: number[] = [src[openIndex]];
  const modes: Mode[] = [src[openIndex] === C_LBRACE ? Mode.Body : Mode.Markup];
  let expectBody = false;
  let expectMarkupParen = false;

  let lineStart = 0;
  for (let k = openIndex - 1; k >= 0; k--) {
    if (src[k] === C_NL) {
      lineStart = k + 1;
      break;
    }
  }
  let lineEnd = n;
  for (let k = openIndex; k < n; k++) {
    if (src[k] === C_NL) {
      lineEnd = k;
      break;
    }
  }
  let lineMarkup = isMarkupLine(src, lineStart, lineEnd);

  let i = openIndex + 1;
  while (i < n) {
    while (i > lineEnd) {
      lineStart = lineEnd + 1;
      lineEnd = n;
      for (let k = lineStart; k < n; k++) {
        if (src[k] === C_NL) {
          lineEnd = k;
          break;
        }
      }
      lineMarkup = isMarkupLine(src, lineStart, lineEnd);
    }
    const mode = modes[modes.length - 1];
    const inCode = mode === Mode.Code;
    const markupLexis = mode === Mode.Markup || (mode === Mode.Body && lineMarkup);
    const j = markupLexis ? skipNoncodeMarkup(src, i) : skipNoncode(src, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = src[i];
    if (c === C_SPACE || c === C_TAB || c === C_NL || c === C_CR) {
      i++;
      continue;
    }
    if (!inCode) {
      if (c === C_AT && keywordAt(src, i + 1, "else")) {
        expectBody = true;
        expectMarkupParen = false;
        i += 5;
        continue;
      }
      if (c === C_AT && keywordAt(src, i + 1, "default")) {
        expectBody = true;
        expectMarkupParen = false;
        i += 8;
        continue;
      }
      if (c === 114 /*r*/ && keywordAt(src, i, "return")) {
        expectMarkupParen = true;
        expectBody = false;
        i += 6;
        continue;
      }
      if (c === C_LPAREN) {
        delims.push(c);
        modes.push(expectMarkupParen ? Mode.Markup : Mode.Code);
        expectBody = false;
        expectMarkupParen = false;
        i++;
        continue;
      }
      if (c === C_LBRACE) {
        delims.push(c);
        modes.push(expectBody ? Mode.Body : Mode.Code);
        expectBody = false;
        expectMarkupParen = false;
        i++;
        continue;
      }
      if (c === C_LBRACKET) {
        delims.push(c);
        modes.push(mode); // inherit
        expectBody = false;
        expectMarkupParen = false;
        i++;
        continue;
      }
      if (c === C_RPAREN || c === C_RBRACE || c === C_RBRACKET) {
        if (delims.length === 0) return -1;
        const top = delims.pop()!;
        modes.pop();
        if (
          (c === C_RPAREN && top !== C_LPAREN) ||
          (c === C_RBRACE && top !== C_LBRACE) ||
          (c === C_RBRACKET && top !== C_LBRACKET)
        )
          return -1;
        if (delims.length === 0) return i;
        i++;
        continue;
      }
      expectBody = false;
      expectMarkupParen = false;
      i++;
      continue;
    } else {
      if (c === C_LPAREN || c === C_LBRACE || c === C_LBRACKET) {
        delims.push(c);
        modes.push(Mode.Code);
        i++;
        continue;
      }
      if (c === C_RPAREN || c === C_RBRACE || c === C_RBRACKET) {
        if (delims.length === 0) return -1;
        const top = delims.pop()!;
        modes.pop();
        if (
          (c === C_RPAREN && top !== C_LPAREN) ||
          (c === C_RBRACE && top !== C_LBRACE) ||
          (c === C_RBRACKET && top !== C_LBRACKET)
        )
          return -1;
        if (delims.length === 0) return i;
        if (modes[modes.length - 1] !== Mode.Code) expectBody = top === C_LPAREN; // header close re-arms body
        i++;
        continue;
      }
      i++;
      continue;
    }
  }
  return -1;
}

/** FNV-1a (2166136261/16777619) over code points' 4 LE bytes — the sidecar src_hash contract
 *  (SrcHash("ab") === 0x221505E6, pinned by both sides). */
export function srcHash(source: string): number {
  let h = 2166136261 >>> 0;
  for (const ch of source) {
    const cp = ch.codePointAt(0)!;
    for (let s = 0; s < 32; s += 8) {
      h = (h ^ ((cp >>> s) & 0xff)) >>> 0;
      h = Math.imul(h, 16777619) >>> 0;
    }
  }
  return h >>> 0;
}
