// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// Code-point helpers (D-18): every offset in the .uetkx toolchain — compiler diagnostics,
// sidecars, the shared corpora — is a UNICODE CODE POINT index, never a UTF-16 unit. The
// scanner/parser/formatter therefore run over number[] code points, exactly like the C++
// side's TArray<int32>.

export function toCodePoints(src: string): number[] {
  const out: number[] = [];
  for (const ch of src) out.push(ch.codePointAt(0)!);
  return out;
}

export function fromCodePoints(src: readonly number[], start = 0, count = src.length): string {
  const end = Math.min(start + count, src.length);
  let out = "";
  for (let i = Math.max(start, 0); i < end; i++) out += String.fromCodePoint(src[i]);
  return out;
}

/** UTF-16 offset -> code-point offset for a JS string (LSP positions arrive in UTF-16). */
export function utf16ToCodePoint(src: string, utf16Offset: number): number {
  let cp = 0;
  for (let i = 0; i < utf16Offset && i < src.length; i++, cp++) {
    const c = src.charCodeAt(i);
    if (c >= 0xd800 && c <= 0xdbff && i + 1 < src.length) i++;
  }
  return cp;
}

/** Code-point offset -> UTF-16 offset. */
export function codePointToUtf16(src: string, cpOffset: number): number {
  let i = 0;
  for (let cp = 0; cp < cpOffset && i < src.length; cp++, i++) {
    const c = src.charCodeAt(i);
    if (c >= 0xd800 && c <= 0xdbff && i + 1 < src.length) i++;
  }
  return i;
}
