// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// Cursor classification for completions — the markup-intelligence baseline. Deliberately
// light: find the nearest unclosed `<` on the cursor's side to decide tag vs attribute
// position; `@` at markup node-start offers directives; everything else offers hooks (setup).

import { toCodePoints } from "./codePoints";
import { skipNoncodeMarkup } from "./cppScanner";

export type CursorContext =
  | { kind: "tag"; partial: string }
  | { kind: "attr"; tag: string; partial: string }
  | { kind: "directive"; partial: string }
  | { kind: "code" };

export function classifyCursor(source: string, cpOffset: number): CursorContext {
  const src = toCodePoints(source);
  const at = Math.min(cpOffset, src.length);

  // scan backward for the nearest markup-significant delimiter
  let openLt = -1;
  for (let i = at - 1; i >= 0; i--) {
    const c = src[i];
    if (c === 62 /*>*/) break; // a completed tag before the cursor
    if (c === 60 /*<*/) {
      openLt = i;
      break;
    }
    if (c === 10 && openLt === -1 && at - i > 400) break; // don't scan forever
  }
  if (openLt !== -1) {
    // inside a tag: `<Name attrs...`
    let i = openLt + 1;
    if (i < at && src[i] === 47 /*/*/) i++; // closing tag `</Na|`
    const nameStart = i;
    while (i < at && isIdent(src[i])) i++;
    const tag = text(src, nameStart, i);
    if (i >= at) return { kind: "tag", partial: tag };
    // past the name: attribute position — partial = the ident being typed
    let ps = at;
    while (ps > i && (isIdent(src[ps - 1]) || src[ps - 1] === 46 /*.*/)) ps--;
    // inside an attr VALUE (after = or inside quotes/braces)? treat as code
    for (let k = at - 1; k > openLt; k--) {
      const c = src[k];
      if (c === 61 /*=*/) return { kind: "code" };
      if (c === 32 || c === 9 || c === 10) break;
    }
    return { kind: "attr", tag, partial: text(src, ps, at) };
  }
  // `@` directive at node start
  for (let i = at - 1; i >= 0; i--) {
    const c = src[i];
    if (c === 64 /*@*/) {
      let onlyIdent = true;
      for (let k = i + 1; k < at; k++)
        if (!isIdent(src[k])) {
          onlyIdent = false;
          break;
        }
      if (onlyIdent) return { kind: "directive", partial: text(src, i + 1, at) };
      break;
    }
    if (!isIdent(c)) break;
  }
  return { kind: "code" };
}

function isIdent(c: number): boolean {
  return c === 95 || (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || (c >= 48 && c <= 57);
}

function text(src: readonly number[], start: number, end: number): string {
  let out = "";
  for (let i = start; i < end; i++) out += String.fromCodePoint(src[i]);
  return out;
}

export const DIRECTIVES = ["if", "elif", "else", "for", "while", "match", "case", "default"];

/** Suppress markup noise: is the offset inside a markup comment/string? */
export function insideNoncodeMarkup(source: string, cpOffset: number): boolean {
  const src = toCodePoints(source);
  let i = 0;
  while (i < src.length && i < cpOffset) {
    const j = skipNoncodeMarkup(src, i);
    if (j !== i) {
      if (cpOffset > i && cpOffset < j) return true;
      i = j;
      continue;
    }
    i++;
  }
  return false;
}
