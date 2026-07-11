"use strict";
// Code-point helpers (D-18): every offset in the .uetkx toolchain — compiler diagnostics,
// sidecars, the shared corpora — is a UNICODE CODE POINT index, never a UTF-16 unit. The
// scanner/parser/formatter therefore run over number[] code points, exactly like the C++
// side's TArray<int32>.
Object.defineProperty(exports, "__esModule", { value: true });
exports.toCodePoints = toCodePoints;
exports.fromCodePoints = fromCodePoints;
exports.utf16ToCodePoint = utf16ToCodePoint;
exports.codePointToUtf16 = codePointToUtf16;
function toCodePoints(src) {
    const out = [];
    for (const ch of src)
        out.push(ch.codePointAt(0));
    return out;
}
function fromCodePoints(src, start = 0, count = src.length) {
    const end = Math.min(start + count, src.length);
    let out = "";
    for (let i = Math.max(start, 0); i < end; i++)
        out += String.fromCodePoint(src[i]);
    return out;
}
/** UTF-16 offset -> code-point offset for a JS string (LSP positions arrive in UTF-16). */
function utf16ToCodePoint(src, utf16Offset) {
    let cp = 0;
    for (let i = 0; i < utf16Offset && i < src.length; i++, cp++) {
        const c = src.charCodeAt(i);
        if (c >= 0xd800 && c <= 0xdbff && i + 1 < src.length)
            i++;
    }
    return cp;
}
/** Code-point offset -> UTF-16 offset. */
function codePointToUtf16(src, cpOffset) {
    let i = 0;
    for (let cp = 0; cp < cpOffset && i < src.length; cp++, i++) {
        const c = src.charCodeAt(i);
        if (c >= 0xd800 && c <= 0xdbff && i + 1 < src.length)
            i++;
    }
    return i;
}
//# sourceMappingURL=codePoints.js.map