"use strict";
// TypeScript port of FUetkxMarkup (UetkxMarkup.cpp) — the byte-compatible markup parser.
// Line-for-line with the C++ (which is itself line-for-line with guitkx_markup.gd): change
// ALL dialects or none. All offsets are CODE POINTS into the array given to parse().
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseMarkup = parseMarkup;
const cppScanner_1 = require("./cppScanner");
const codePoints_1 = require("./codePoints");
const C_NL = 10, C_CR = 13, C_SPACE = 32, C_TAB = 9, C_QUOTE = 34, C_APOS = 39, C_SLASH = 47, C_STAR = 42, C_LT = 60, C_GT = 62, C_BANG = 33, C_DASH = 45, C_DOT = 46, C_EQ = 61, C_AT = 64, C_LPAREN = 40, C_LBRACE = 123;
function parseMarkup(src, start, end) {
    return new Parser(src).run(start, end);
}
class Parser {
    src;
    err = { code: "", msg: "", at: -1 };
    lineStarts = [0];
    constructor(src) {
        this.src = src;
        for (let i = 0; i < src.length; i++)
            if (src[i] === C_NL)
                this.lineStarts.push(i + 1);
    }
    run(start, end) {
        const r = this.parseNodes(start, end);
        return { nodes: r.nodes, errorCode: this.err.code, errorMsg: this.err.msg, errorAt: this.err.at };
    }
    fail(code, msg, at) {
        if (!this.err.code)
            this.err = { code, msg, at };
    }
    parseNodes(start, end) {
        const s = this.src;
        const nodes = [];
        let i = start;
        while (i < end && !this.err.code) {
            i = this.skipWs(i, end);
            if (i >= end)
                break;
            const c = s[i];
            if (c === C_LT) {
                if (i + 3 < end && s[i + 1] === C_BANG && s[i + 2] === C_DASH && s[i + 3] === C_DASH) {
                    const hce = this.findSeq("-->", i + 4);
                    if (hce === -1 || hce + 3 > end) {
                        this.fail("UETKX0304", "unclosed `<!--` comment", i);
                        break;
                    }
                    nodes.push({ type: "comment", at: i, value: this.slice(i, hce + 3) });
                    i = hce + 3;
                    continue;
                }
                if (i + 1 < end && s[i + 1] === C_SLASH)
                    break; // closing tag belongs to the caller
                const r = this.parseElement(i, end);
                if (this.err.code)
                    break;
                nodes.push(r.node);
                i = r.next;
            }
            else if (c === C_SLASH && i + 1 < end && (s[i + 1] === C_SLASH || s[i + 1] === C_STAR)) {
                if (s[i + 1] === C_SLASH) {
                    let le = -1;
                    for (let k = i; k < end; k++)
                        if (s[k] === C_NL) {
                            le = k;
                            break;
                        }
                    const stop = le === -1 ? end : le;
                    nodes.push({ type: "comment", at: i, value: this.slice(i, stop) });
                    i = stop;
                }
                else {
                    const bce = this.findSeq("*/", i + 2);
                    if (bce === -1 || bce + 2 > end) {
                        this.fail("UETKX0304", "unclosed `/*` comment", i);
                        break;
                    }
                    nodes.push({ type: "comment", at: i, value: this.slice(i, bce + 2) });
                    i = bce + 2;
                }
            }
            else if (c === C_AT) {
                const r = this.parseDirective(i, end);
                if (this.err.code)
                    break;
                nodes.push(r.node);
                i = r.next;
            }
            else if (c === C_LBRACE) {
                const close = (0, cppScanner_1.findMatching)(s, i);
                if (close === -1 || close >= end) {
                    this.fail("UETKX0304", "unclosed `{` expression", i);
                    break;
                }
                nodes.push({
                    type: "expr",
                    at: i,
                    vat: this.skipWs(i + 1, close),
                    code: this.sliceTrimmed(i + 1, close),
                });
                i = close + 1;
            }
            else {
                const r = this.parseText(i, end);
                if (r.node)
                    nodes.push(r.node);
                i = r.next;
            }
        }
        return { nodes, next: i };
    }
    parseElement(openIndex, end) {
        const s = this.src;
        let i = openIndex + 1;
        const line = this.lineOf(openIndex);
        const nameStart = i;
        while (i < end && isTagChar(s[i]))
            i++;
        const tag = this.slice(nameStart, i);
        if (!tag && (i >= end || s[i] !== C_GT)) {
            this.fail("UETKX0300", "invalid tag name -- `<` must be followed by a tag name, or `<>` for a fragment", openIndex);
            return { node: null, next: end };
        }
        if (tag && tag[0] >= "0" && tag[0] <= "9") {
            this.fail("UETKX0300", `tag name cannot start with a digit (<${tag}>)`, openIndex);
            return { node: null, next: end };
        }
        const attrs = [];
        while (i < end) {
            i = this.skipWs(i, end);
            if (i >= end) {
                this.fail("UETKX0303", `unexpected EOF in <${tag}>`, openIndex);
                return { node: null, next: end };
            }
            const c = s[i];
            if (c === C_SLASH && i + 1 < end && s[i + 1] === C_GT) {
                return { node: this.makeEl(tag, attrs, [], line, openIndex), next: i + 2 };
            }
            if (c === C_GT) {
                i++;
                break;
            }
            const ar = this.parseAttribute(i, end);
            if (this.err.code)
                return { node: null, next: end };
            attrs.push(ar.attr);
            i = ar.next;
        }
        const cr = this.parseNodes(i, end);
        if (this.err.code)
            return { node: null, next: end };
        const j = cr.next;
        if (j >= end || s[j] !== C_LT || j + 1 >= end || s[j + 1] !== C_SLASH) {
            this.fail("UETKX0301", `unclosed tag <${tag}>`, openIndex);
            return { node: null, next: end };
        }
        let ce = -1;
        for (let k = j; k < end; k++)
            if (s[k] === C_GT) {
                ce = k;
                break;
            }
        if (ce === -1) {
            this.fail("UETKX0303", `malformed closing tag for <${tag}>`, j);
            return { node: null, next: end };
        }
        const closeName = this.sliceTrimmed(j + 2, ce);
        if (closeName !== tag) {
            this.fail("UETKX0302", `mismatched tag </${closeName}> (expected </${tag}>)`, j);
            return { node: null, next: end };
        }
        return { node: this.makeEl(tag, attrs, cr.nodes, line, openIndex), next: ce + 1 };
    }
    makeEl(tag, attrs, children, line, at) {
        if (!tag)
            return { type: "frag", at, children };
        if (tag.toLowerCase() === "fragment")
            return { type: "frag", at, children, named: tag, attrs };
        return { type: "el", at, tag, attrs, children, line };
    }
    parseAttribute(start, end) {
        const s = this.src;
        let i = start;
        if (s[i] === C_LBRACE) {
            const probe = this.skipWs(i + 1, end);
            if (probe + 1 < end && s[probe] === C_SLASH && s[probe + 1] === C_STAR) {
                const ce2 = this.findSeq("*/", probe + 2);
                if (ce2 === -1 || ce2 + 2 > end) {
                    this.fail("UETKX0304", "unclosed comment in attribute list", i);
                    return { attr: null, next: end };
                }
                const after = this.skipWs(ce2 + 2, end);
                if (after >= end || s[after] !== 125 /*}*/) {
                    this.fail("UETKX0303", "attribute comment must close with `*/}`", i);
                    return { attr: null, next: end };
                }
                return {
                    attr: { name: "", kind: "comment", value: this.slice(i, after + 1), at: i, vat: -1, end: after + 1 },
                    next: after + 1,
                };
            }
            const sclose = (0, cppScanner_1.findMatching)(s, i);
            if (sclose === -1 || sclose >= end) {
                this.fail("UETKX0304", "unclosed `{` in spread attribute", i);
                return { attr: null, next: end };
            }
            const inner = this.sliceTrimmed(i + 1, sclose);
            if (!inner.startsWith("...")) {
                this.fail("UETKX0300", "expected `...spread` or an attribute name", i);
                return { attr: null, next: end };
            }
            const svat = this.skipWs(this.skipWs(i + 1, sclose) + 3, sclose);
            return {
                attr: { name: "", kind: "spread", value: inner.slice(3).trim(), at: i, vat: svat, end: sclose + 1 },
                next: sclose + 1,
            };
        }
        const ns = i;
        while (i < end && isAttrNameChar(s[i]))
            i++;
        const name = this.slice(ns, i);
        const nameEnd = i;
        if (!name) {
            this.fail("UETKX0300", "unexpected token in attributes", i);
            return { attr: null, next: end };
        }
        if (name.startsWith(".") || name.startsWith("-")) {
            this.fail("UETKX0300", `unexpected \`${name[0]}\` in attributes -- dotted/namespaced tags are not supported`, ns);
            return { attr: null, next: end };
        }
        i = this.skipWs(i, end);
        if (i >= end || s[i] !== C_EQ) {
            return { attr: { name, kind: "bool", value: "true", at: ns, vat: -1, end: nameEnd }, next: i };
        }
        i++;
        i = this.skipWs(i, end);
        if (i >= end) {
            this.fail("UETKX0303", `missing attribute value for '${name}'`, ns);
            return { attr: null, next: end };
        }
        const c = s[i];
        if (c === C_QUOTE || c === C_APOS) {
            const se = (0, cppScanner_1.skipNoncodeMarkup)(s, i);
            if (se <= i + 1 || se > end || s[se - 1] !== c) {
                this.fail("UETKX0300", `unterminated string in attribute '${name}'`, i);
                return { attr: null, next: end };
            }
            return { attr: { name, kind: "str", value: this.slice(i + 1, se - 1), at: ns, vat: i + 1, end: se }, next: se };
        }
        if (c === C_LBRACE) {
            const close = (0, cppScanner_1.findMatching)(s, i);
            if (close === -1 || close >= end) {
                this.fail("UETKX0304", `unclosed \`{\` in attribute '${name}'`, i);
                return { attr: null, next: end };
            }
            return {
                attr: {
                    name,
                    kind: "expr",
                    value: this.sliceTrimmed(i + 1, close),
                    at: ns,
                    vat: this.skipWs(i + 1, close),
                    end: close + 1,
                },
                next: close + 1,
            };
        }
        this.fail("UETKX0300", `attribute '${name}' value must be a string or {expr}`, i);
        return { attr: null, next: end };
    }
    parseText(start, end) {
        const s = this.src;
        let i = start;
        while (i < end) {
            const tc = s[i];
            if (tc === C_LT || tc === C_AT)
                break;
            i++;
        }
        const trimmed = this.sliceTrimmed(start, i);
        if (!trimmed)
            return { node: null, next: i };
        return { node: { type: "text", at: start, value: trimmed }, next: i };
    }
    parseDirective(at, end) {
        const s = this.src;
        if ((0, cppScanner_1.keywordAt)(s, at + 1, "if"))
            return this.parseIf(at, end);
        if ((0, cppScanner_1.keywordAt)(s, at + 1, "for"))
            return this.parseLoop(at, end, "for", 4);
        if ((0, cppScanner_1.keywordAt)(s, at + 1, "while"))
            return this.parseLoop(at, end, "while", 6);
        if ((0, cppScanner_1.keywordAt)(s, at + 1, "match"))
            return this.parseMatch(at, end);
        this.fail("UETKX0305", "unknown @directive", at);
        return { node: null, next: end };
    }
    readParen(index, end) {
        const s = this.src;
        const i = this.skipWs(index, end);
        if (i >= end || s[i] !== C_LPAREN) {
            this.fail("UETKX2506", "directive expects `(...)`", i);
            return { text: "", next: end };
        }
        const close = (0, cppScanner_1.findMatching)(s, i);
        if (close === -1 || close >= end) {
            this.fail("UETKX0304", "unclosed `(` in directive", i);
            return { text: "", next: end };
        }
        return { text: this.sliceTrimmed(i + 1, close), next: close + 1 };
    }
    readBraceBody(index, end) {
        const s = this.src;
        const i = this.skipWs(index, end);
        if (i >= end || s[i] !== C_LBRACE) {
            this.fail("UETKX0303", "directive expects `{ ... }` body", i);
            return { text: "", next: end, at: -1 };
        }
        const close = (0, cppScanner_1.findMatchingMarkup)(s, i);
        if (close === -1 || close >= end) {
            this.fail("UETKX0304", "unclosed `{` directive body", i);
            return { text: "", next: end, at: -1 };
        }
        return { text: this.slice(i + 1, close), next: close + 1, at: i + 1 };
    }
    parseIf(at, end) {
        const s = this.src;
        const node = { type: "if", at, branches: [] };
        const p = this.readParen(at + 3, end);
        if (this.err.code)
            return { node: null, next: end };
        const b = this.readBraceBody(p.next, end);
        if (this.err.code)
            return { node: null, next: end };
        node.branches.push({ cond: p.text, bodyMarkup: b.text, bodyAt: b.at });
        let i = b.next;
        for (;;) {
            const k = this.skipWs(i, end);
            if (k >= end || s[k] !== C_AT)
                break;
            if (k + 5 <= end && (0, cppScanner_1.keywordAt)(s, k + 1, "elif")) {
                const pe = this.readParen(k + 5, end);
                if (this.err.code)
                    return { node: null, next: end };
                const be = this.readBraceBody(pe.next, end);
                if (this.err.code)
                    return { node: null, next: end };
                node.branches.push({ cond: pe.text, bodyMarkup: be.text, bodyAt: be.at });
                i = be.next;
            }
            else if (k + 5 <= end && (0, cppScanner_1.keywordAt)(s, k + 1, "else")) {
                const bb = this.readBraceBody(k + 5, end);
                if (this.err.code)
                    return { node: null, next: end };
                node.elseBody = bb.text;
                node.elseBodyAt = bb.at;
                i = bb.next;
                break;
            }
            else
                break;
        }
        return { node, next: i };
    }
    parseLoop(at, end, kind, kwLen) {
        const p = this.readParen(at + kwLen, end);
        if (this.err.code)
            return { node: null, next: end };
        const b = this.readBraceBody(p.next, end);
        if (this.err.code)
            return { node: null, next: end };
        return { node: { type: kind, at, header: p.text, bodyMarkup: b.text, bodyAt: b.at }, next: b.next };
    }
    parseMatch(at, end) {
        const s = this.src;
        const node = { type: "match", at, cases: [] };
        const p = this.readParen(at + 6, end);
        if (this.err.code)
            return { node: null, next: end };
        node.subject = p.text;
        const bi = this.skipWs(p.next, end);
        if (bi >= end || s[bi] !== C_LBRACE) {
            this.fail("UETKX0303", "@match expects `{ ... }` with @case/@default arms", bi);
            return { node: null, next: end };
        }
        const bclose = (0, cppScanner_1.findMatchingMarkup)(s, bi);
        if (bclose === -1 || bclose >= end) {
            this.fail("UETKX0304", "unclosed @match body", bi);
            return { node: null, next: end };
        }
        let j = bi + 1;
        while (j < bclose) {
            j = this.skipWs(j, bclose);
            if (j >= bclose)
                break;
            if (s[j] === C_AT && (0, cppScanner_1.keywordAt)(s, j + 1, "case")) {
                const cp = this.readParen(j + 5, bclose);
                if (this.err.code)
                    return { node: null, next: end };
                const cb = this.readBraceBody(cp.next, bclose);
                if (this.err.code)
                    return { node: null, next: end };
                node.cases.push({ value: cp.text, bodyMarkup: cb.text, bodyAt: cb.at });
                j = cb.next;
            }
            else if (s[j] === C_AT && (0, cppScanner_1.keywordAt)(s, j + 1, "default")) {
                const db = this.readBraceBody(j + 8, bclose);
                if (this.err.code)
                    return { node: null, next: end };
                node.defaultBody = db.text;
                node.defaultBodyAt = db.at;
                j = db.next;
            }
            else {
                this.fail("UETKX2506", "@match body expects @case (...) { } or @default { }", j);
                return { node: null, next: end };
            }
        }
        return { node, next: bclose + 1 };
    }
    skipWs(index, end) {
        const s = this.src;
        while (index < end) {
            const c = s[index];
            if (c !== C_SPACE && c !== C_TAB && c !== C_NL && c !== C_CR)
                break;
            index++;
        }
        return index;
    }
    lineOf(index) {
        let lo = 0;
        let hi = this.lineStarts.length - 1;
        while (lo < hi) {
            const mid = (lo + hi + 1) >> 1;
            if (this.lineStarts[mid] <= index)
                lo = mid;
            else
                hi = mid - 1;
        }
        return lo + 1;
    }
    slice(start, end) {
        return (0, codePoints_1.fromCodePoints)(this.src, start, end - start);
    }
    sliceTrimmed(start, end) {
        return this.slice(start, end).trim();
    }
    findSeq(seq, from) {
        const s = this.src;
        const codes = [];
        for (const ch of seq)
            codes.push(ch.codePointAt(0));
        for (let i = Math.max(from, 0); i + codes.length <= s.length; i++) {
            let match = true;
            for (let k = 0; k < codes.length; k++)
                if (s[i + k] !== codes[k]) {
                    match = false;
                    break;
                }
            if (match)
                return i;
        }
        return -1;
    }
}
function isTagChar(c) {
    return c === 95 || (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || (c >= 48 && c <= 57);
}
function isAttrNameChar(c) {
    return isTagChar(c) || c === C_DASH || c === C_DOT;
}
//# sourceMappingURL=uetkxMarkup.js.map