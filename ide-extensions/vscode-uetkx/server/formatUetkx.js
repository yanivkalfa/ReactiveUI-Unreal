"use strict";
// TypeScript port of FUetkxFormatter (UetkxFormatter.cpp) — byte-identical output, pinned by
// the SHARED golden corpus test-fixtures/uetkx-formatter-cases.json the C++ automation suite
// also replays. AST-driven re-emit; VERBATIM on any parse error (never corrupt) with fellBack;
// embedded C++ keeps internal structure (depth-based base-indent re-anchor, G-02 string-line
// mask, G-03 interior blank lines); idempotent.
Object.defineProperty(exports, "__esModule", { value: true });
exports.DEFAULT_FORMAT_OPTIONS = void 0;
exports.formatUetkx = formatUetkx;
const cppScanner_1 = require("./cppScanner");
const codePoints_1 = require("./codePoints");
const uetkxMarkup_1 = require("./uetkxMarkup");
const uetkxFileScan_1 = require("./uetkxFileScan");
exports.DEFAULT_FORMAT_OPTIONS = {
    printWidth: 100,
    indentStyle: "tab",
    indentSize: 2,
    singleAttributePerLine: false,
    insertSpaceBeforeSelfClose: true,
};
function formatUetkx(source, opts) {
    const o = { ...exports.DEFAULT_FORMAT_OPTIONS, ...(opts ?? {}) };
    const verbatim = (fellBack) => ({ ok: true, text: source, changed: false, fellBack });
    const scan = (0, uetkxFileScan_1.scanFile)(source, "");
    if (scan.components.length === 0)
        return verbatim((0, uetkxFileScan_1.hasError)(scan));
    if ((0, uetkxFileScan_1.hasError)(scan))
        return verbatim(true);
    const state = { o, unsafeStrAttr: false };
    const srcCp = (0, codePoints_1.toCodePoints)(source);
    let out = "";
    // preamble (T1.3): canonical ONLY when whitespace + #include lines
    const pre = (0, codePoints_1.fromCodePoints)(srcCp, 0, scan.components[0].at);
    let preCanonical = true;
    for (const line of pre.split("\n")) {
        const t = line.trim();
        if (t && !t.startsWith("#include")) {
            preCanonical = false;
            break;
        }
    }
    if (!preCanonical)
        out += pre;
    else if (scan.preambleIncludes.length > 0)
        out += scan.preambleIncludes.join("\n") + "\n\n";
    let cursor = -1;
    for (let k = 0; k < scan.components.length; k++) {
        const decl = scan.components[k];
        if (k > 0) {
            if ((0, codePoints_1.fromCodePoints)(srcCp, cursor, decl.at - cursor).trim())
                return verbatim(true); // never delete user text
            out += "\n";
        }
        out += fmtComponent(decl, state);
        cursor = decl.next;
    }
    if (state.unsafeStrAttr)
        return verbatim(true); // G-05
    if (cursor >= 0 && cursor < srcCp.length) {
        const trailing = (0, codePoints_1.fromCodePoints)(srcCp, cursor, srcCp.length - cursor);
        if (trailing.trim())
            out = rstripWsNl(out) + "\n\n" + lstripWsNl(trailing);
    }
    const text = rstripWsNl(out) + "\n";
    return { ok: true, text, changed: text !== source, fellBack: false };
}
// ── indent helpers ─────────────────────────────────────────────────────────────────────────
function pad(indent, o) {
    return o.indentStyle === "space" ? " ".repeat(indent * o.indentSize) : "\t".repeat(indent);
}
function leadingWs(s) {
    let i = 0;
    while (i < s.length && (s[i] === " " || s[i] === "\t"))
        i++;
    return s.slice(0, i);
}
function stripLeadingWs(s) {
    return s.slice(leadingWs(s).length);
}
function collapseSpaces(s) {
    const cp = (0, codePoints_1.toCodePoints)(s);
    const n = cp.length;
    let out = "";
    let i = 0;
    while (i < n) {
        const j = (0, cppScanner_1.skipNoncode)(cp, i);
        if (j !== i) {
            out += (0, codePoints_1.fromCodePoints)(cp, i, j - i);
            i = j;
            continue;
        }
        if (cp[i] === 32 && i + 1 < n && cp[i + 1] === 32) {
            out += " ";
            while (i < n && cp[i] === 32)
                i++;
            continue;
        }
        out += String.fromCodePoint(cp[i]);
        i++;
    }
    return out;
}
function indentUnit(lines) {
    const widths = new Set();
    for (const line of lines) {
        let spaces = 0;
        for (const ch of line) {
            if (ch === " ")
                spaces++;
            else if (ch === "\t")
                continue;
            else
                break;
        }
        if (spaces > 0)
            widths.add(spaces);
    }
    if (widths.size === 0)
        return 1;
    const sorted = [...widths].sort((a, b) => a - b);
    let unit = sorted[0];
    for (let i = 1; i < sorted.length; i++) {
        const d = sorted[i] - sorted[i - 1];
        if (d > 0 && d < unit)
            unit = d;
    }
    return Math.max(unit, 1);
}
function indentDepth(s, unit) {
    let cols = 0;
    for (const ch of s) {
        if (ch === "\t")
            cols += unit;
        else if (ch === " ")
            cols += 1;
        else
            break;
    }
    return Math.round(cols / unit);
}
function stringLineMask(lines) {
    const cp = (0, codePoints_1.toCodePoints)(lines.join("\n"));
    const n = cp.length;
    const lineStarts = [0];
    for (let i = 0; i < n; i++)
        if (cp[i] === 10)
            lineStarts.push(i + 1);
    const mask = new Array(lineStarts.length).fill(false);
    let lineIdx = 0;
    let i = 0;
    while (i < n) {
        while (lineIdx + 1 < lineStarts.length && lineStarts[lineIdx + 1] <= i)
            lineIdx++;
        const j = (0, cppScanner_1.skipNoncode)(cp, i);
        if (j !== i) {
            let endLine = lineIdx;
            while (endLine + 1 < lineStarts.length && lineStarts[endLine + 1] <= j)
                endLine++;
            for (let m = lineIdx + 1; m <= endLine && m < mask.length; m++)
                mask[m] = true;
            i = j;
            continue;
        }
        i++;
    }
    return mask;
}
function splitTrimBlankEdges(code) {
    const lines = code.split("\n");
    while (lines.length && !lines[0].trim())
        lines.shift();
    while (lines.length && !lines[lines.length - 1].trim())
        lines.pop();
    return lines;
}
function reanchor(code, indent, o) {
    const lines = splitTrimBlankEdges(code);
    if (lines.length === 0)
        return "";
    const mask = stringLineMask(lines);
    const unitLines = lines.filter((_, i) => !mask[i]);
    const unit = indentUnit(unitLines);
    let anchor = -1;
    let anchorAny = -1;
    const depths = [];
    for (let i = 0; i < lines.length; i++) {
        if (mask[i] || !lines[i].trim()) {
            depths.push(-1);
            continue;
        }
        const d = indentDepth(lines[i], unit);
        depths.push(d);
        if (anchorAny === -1)
            anchorAny = d;
        if (anchor === -1 && !lines[i].trim().startsWith("//"))
            anchor = d;
    }
    if (anchor === -1)
        anchor = anchorAny;
    let out = "";
    for (let i = 0; i < lines.length; i++) {
        if (mask[i])
            out += lines[i] + "\n";
        else if (depths[i] === -1)
            out += "\n";
        else
            out += pad(indent + Math.max(0, depths[i] - anchor), o) + collapseSpaces(stripLeadingWs(lines[i])) + "\n";
    }
    return out;
}
function reanchorRel(code, indent, unit, anchor, o) {
    const lines = splitTrimBlankEdges(code);
    if (lines.length === 0)
        return "";
    const mask = stringLineMask(lines);
    let out = "";
    for (let i = 0; i < lines.length; i++) {
        if (mask[i]) {
            out += lines[i] + "\n";
            continue;
        }
        if (!lines[i].trim()) {
            out += "\n"; // G-03 interior blank line
            continue;
        }
        out += pad(indent + Math.max(0, indentDepth(lines[i], unit) - anchor), o) + collapseSpaces(stripLeadingWs(lines[i])) + "\n";
    }
    return out;
}
function bodyGeometry(body) {
    const lines = splitTrimBlankEdges(body);
    const mask = stringLineMask(lines);
    const unitLines = lines.filter((_, i) => !mask[i]);
    const unit = indentUnit(unitLines);
    let anchor = -1;
    let anchorAny = -1;
    for (let i = 0; i < lines.length; i++) {
        if (mask[i] || !lines[i].trim())
            continue;
        const d = indentDepth(lines[i], unit);
        if (anchorAny === -1)
            anchorAny = d;
        if (anchor === -1 && !lines[i].trim().startsWith("//")) {
            anchor = d;
            break;
        }
    }
    if (anchor === -1)
        anchor = Math.max(anchorAny, 0);
    return { unit, anchor };
}
function hasLeadingBlank(s) {
    let i = 0;
    const n = s.length;
    while (i < n && (s[i] === " " || s[i] === "\t"))
        i++;
    if (i >= n || s[i] !== "\n")
        return false;
    i++;
    while (i < n && (s[i] === " " || s[i] === "\t"))
        i++;
    return i < n && s[i] === "\n";
}
function hasTrailingBlank(s) {
    let i = s.length - 1;
    while (i >= 0 && (s[i] === " " || s[i] === "\t"))
        i--;
    if (i < 0 || s[i] !== "\n")
        return false;
    i--;
    while (i >= 0 && (s[i] === " " || s[i] === "\t"))
        i--;
    return i >= 0 && s[i] === "\n";
}
function rstripWsNl(s) {
    let end = s.length;
    while (end > 0 && (s[end - 1] === " " || s[end - 1] === "\t" || s[end - 1] === "\n"))
        end--;
    return s.slice(0, end);
}
function lstripWsNl(s) {
    let start = 0;
    while (start < s.length && (s[start] === " " || s[start] === "\t" || s[start] === "\n"))
        start++;
    return s.slice(start);
}
// ── markup re-emit ─────────────────────────────────────────────────────────────────────────
function fmtAttr(a, state) {
    switch (a.kind) {
        case "str":
            if (a.value.includes('"'))
                state.unsafeStrAttr = true; // G-05
            return `${a.name}="${a.value}"`;
        case "expr":
            return `${a.name}={ ${a.value.trim()} }`;
        case "spread":
            return `{...${a.value.trim()}}`;
        case "comment":
            return a.value; // T2.1 verbatim
        case "bool":
            return a.name;
    }
}
function fmtChildren(children, indent, state) {
    let out = "";
    for (const child of children ?? [])
        out += fmtNode(child, indent, state);
    return out;
}
function fmtElement(node, indent, state) {
    const p = pad(indent, state.o);
    const attrStrs = (node.attrs ?? []).map((a) => fmtAttr(a, state));
    const children = (node.children ?? []).filter(Boolean);
    const selfClose = children.length === 0;
    let head = `<${node.tag}`;
    if (attrStrs.length)
        head += " " + attrStrs.join(" ");
    const singleClose = selfClose ? (state.o.insertSpaceBeforeSelfClose ? " />" : "/>") : ">";
    const single = head + singleClose;
    let wrap = state.o.singleAttributePerLine && attrStrs.length > 1;
    if (!wrap && p.length + single.length > state.o.printWidth && attrStrs.length > 1)
        wrap = true;
    let out = "";
    if (!wrap) {
        if (selfClose)
            return p + single + "\n";
        out += p + single + "\n";
    }
    else {
        out += p + `<${node.tag}\n`;
        const apad = pad(indent + 1, state.o);
        for (let k = 0; k < attrStrs.length; k++) {
            const last = k === attrStrs.length - 1;
            if (last && selfClose)
                out += apad + attrStrs[k] + (state.o.insertSpaceBeforeSelfClose ? " />" : "/>") + "\n";
            else if (last)
                out += apad + attrStrs[k] + "\n" + p + ">\n";
            else
                out += apad + attrStrs[k] + "\n";
        }
        if (selfClose)
            return out;
    }
    out += fmtChildren(children, indent + 1, state);
    out += p + `</${node.tag}>\n`;
    return out;
}
function fmtBody(bodySrc, indent, state) {
    const cp = (0, codePoints_1.toCodePoints)(bodySrc);
    const split = (0, uetkxFileScan_1.splitMarkupReturn)(cp, false);
    if (!split.ok)
        return reanchor(bodySrc, indent, state.o);
    let tail = split.afterParen;
    if (tail < cp.length && cp[tail] === 59 /*;*/)
        tail++;
    if ((0, codePoints_1.fromCodePoints)(cp, tail, cp.length - tail).trim())
        return reanchor(bodySrc, indent, state.o);
    const parsed = (0, uetkxMarkup_1.parseMarkup)(cp, split.mStart, split.mEnd);
    if (parsed.errorCode)
        return reanchor(bodySrc, indent, state.o);
    let out = "";
    const lead = (0, codePoints_1.fromCodePoints)(cp, 0, split.returnAt);
    if (lead.trim()) {
        const { unit, anchor } = bodyGeometry(bodySrc);
        out += reanchorRel(lead, indent, unit, anchor, state.o);
    }
    const p = pad(indent, state.o);
    out += p + "return (\n";
    for (const node of parsed.nodes)
        out += fmtNode(node, indent + 1, state);
    out += p + ");\n";
    return out;
}
function fmtIf(node, indent, state) {
    const p = pad(indent, state.o);
    let out = "";
    const branches = node.branches ?? [];
    for (let k = 0; k < branches.length; k++) {
        const br = branches[k];
        if (k === 0)
            out += p + `@if (${br.cond.trim()}) {\n`;
        else
            out = rstripWsNl(out) + ` @elif (${br.cond.trim()}) {\n`;
        out += fmtBody(br.bodyMarkup, indent + 1, state);
        out += p + "}\n";
    }
    if (node.elseBody !== undefined) {
        out = rstripWsNl(out) + " @else {\n";
        out += fmtBody(node.elseBody, indent + 1, state);
        out += p + "}\n";
    }
    return out;
}
function fmtLoop(node, indent, state, keyword) {
    const p = pad(indent, state.o);
    let out = p + `@${keyword} (${(node.header ?? "").trim()}) {\n`;
    out += fmtBody(node.bodyMarkup ?? "", indent + 1, state);
    out += p + "}\n";
    return out;
}
function fmtMatch(node, indent, state) {
    const p = pad(indent, state.o);
    let out = p + `@match (${(node.subject ?? "").trim()}) {\n`;
    for (const c of node.cases ?? []) {
        out += pad(indent + 1, state.o) + `@case (${c.value.trim()}) {\n`;
        out += fmtBody(c.bodyMarkup, indent + 2, state);
        out += pad(indent + 1, state.o) + "}\n";
    }
    if (node.defaultBody !== undefined) {
        out += pad(indent + 1, state.o) + "@default {\n";
        out += fmtBody(node.defaultBody, indent + 2, state);
        out += pad(indent + 1, state.o) + "}\n";
    }
    out += p + "}\n";
    return out;
}
function fmtNode(node, indent, state) {
    switch (node.type) {
        case "el":
            return fmtElement(node, indent, state);
        case "frag": {
            const inner = fmtChildren(node.children, indent + 1, state);
            const p = pad(indent, state.o);
            if (node.named) {
                let head = `<${node.named}`;
                for (const a of node.attrs ?? [])
                    head += " " + fmtAttr(a, state);
                return `${p}${head}>\n${inner}${p}</${node.named}>\n`;
            }
            return `${p}<>\n${inner}${p}</>\n`;
        }
        case "comment":
            return pad(indent, state.o) + (node.value ?? "").trim() + "\n";
        case "text":
            return pad(indent, state.o) + (node.value ?? "").trim() + "\n";
        case "expr":
            return pad(indent, state.o) + `{ ${(node.code ?? "").trim()} }\n`;
        case "if":
            return fmtIf(node, indent, state);
        case "for":
            return fmtLoop(node, indent, state, "for");
        case "while":
            return fmtLoop(node, indent, state, "while");
        case "match":
            return fmtMatch(node, indent, state);
    }
}
function fmtParams(params) {
    if (params.length === 0)
        return "";
    const parts = params.map((p) => {
        let part = p.name;
        if (p.type)
            part += ": " + p.type;
        if (p.default)
            part += " = " + p.default;
        return part;
    });
    return `(${parts.join(", ")})`;
}
function fmtComponent(decl, state) {
    let out = `component ${decl.name}${fmtParams(decl.params)} {\n`;
    const setup = reanchor(decl.setup, 1, state.o);
    if (setup) {
        if (hasLeadingBlank(decl.setup))
            out += "\n";
        out += setup;
        if (hasTrailingBlank(decl.setup))
            out += "\n";
    }
    out += pad(1, state.o) + "return (\n";
    out += fmtChildren(decl.windowNodes, 2, state);
    out += pad(1, state.o) + ");\n";
    out += "}\n";
    return out;
}
//# sourceMappingURL=formatUetkx.js.map