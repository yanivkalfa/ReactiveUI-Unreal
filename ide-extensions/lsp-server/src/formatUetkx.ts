// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TypeScript port of FUetkxFormatter (UetkxFormatter.cpp) — byte-identical output, pinned by
// the SHARED golden corpus test-fixtures/uetkx-formatter-cases.json the C++ automation suite
// also replays. AST-driven re-emit; VERBATIM on any parse error (never corrupt) with fellBack;
// embedded C++ keeps internal structure (depth-based base-indent re-anchor, G-02 string-line
// mask, G-03 interior blank lines); idempotent.

import { skipNoncode } from "./cppScanner";
import { fromCodePoints, toCodePoints } from "./codePoints";
import { parseMarkup, UetkxAttr, UetkxNode } from "./uetkxMarkup";
import { scanFile, hasError, splitMarkupReturn, UetkxComponentDecl, UetkxHookDecl, UetkxModuleDecl, UetkxParam } from "./uetkxFileScan";

export interface UetkxFormatOptions {
  printWidth: number;
  indentStyle: "tab" | "space";
  indentSize: number;
  singleAttributePerLine: boolean;
  insertSpaceBeforeSelfClose: boolean;
}

export const DEFAULT_FORMAT_OPTIONS: UetkxFormatOptions = {
  printWidth: 100,
  indentStyle: "tab",
  indentSize: 2,
  singleAttributePerLine: false,
  insertSpaceBeforeSelfClose: true,
};

export interface UetkxFormatResult {
  ok: boolean;
  text: string;
  changed: boolean;
  fellBack: boolean;
}

interface FmtState {
  o: UetkxFormatOptions;
  unsafeStrAttr: boolean; // G-05 escape hatch
}

export function formatUetkx(source: string, opts?: Partial<UetkxFormatOptions>): UetkxFormatResult {
  const o: UetkxFormatOptions = { ...DEFAULT_FORMAT_OPTIONS, ...(opts ?? {}) };
  const verbatim = (fellBack: boolean): UetkxFormatResult => ({ ok: true, text: source, changed: false, fellBack });

  const scan = scanFile(source, "");
  if (scan.order.length === 0) return verbatim(hasError(scan));
  if (hasError(scan)) return verbatim(true);

  const state: FmtState = { o, unsafeStrAttr: false };
  const srcCp = toCodePoints(source);
  let out = "";

  // The source-order start/end + canonical re-emit of any top-level declaration kind (M12
  // FmtHook/FmtModule): the mixed-decl file is a SEQUENCE (scan.order) of components + hooks +
  // modules, each canonicalized by its own formatter. trueStart honors a preceding `export`.
  type Ord = (typeof scan.order)[number];
  const trueStart = (ord: Ord): number => {
    if (ord.kind === "component") {
      const d = scan.components[ord.index];
      return d.exported && d.exportAt >= 0 ? d.exportAt : d.at;
    }
    if (ord.kind === "hook") {
      const d = scan.hooks[ord.index];
      return d.exported && d.exportAt >= 0 ? d.exportAt : d.at;
    }
    const d = scan.modules[ord.index];
    return d.exported && d.exportAt >= 0 ? d.exportAt : d.at;
  };
  const declNext = (ord: Ord): number =>
    ord.kind === "component" ? scan.components[ord.index].next : ord.kind === "hook" ? scan.hooks[ord.index].next : scan.modules[ord.index].next;
  const fmtDecl = (ord: Ord): string =>
    ord.kind === "component"
      ? fmtComponent(scan.components[ord.index], state)
      : ord.kind === "hook"
        ? fmtHook(scan.hooks[ord.index], state)
        : fmtModule(scan.modules[ord.index], state);

  // preamble (T1.3 + M11): canonical ONLY when whitespace + #include + `import` lines.
  // Canonical order: #include block, blank, import block, blank.
  const pre = fromCodePoints(srcCp, 0, trueStart(scan.order[0]));
  let preCanonical = true;
  for (const line of pre.split("\n")) {
    const t = line.trim();
    if (t && !t.startsWith("#include") && !t.startsWith("import ")) {
      preCanonical = false;
      break;
    }
    // A trailing/standalone comment on a preamble line is NOT reconstructed structurally — emit the
    // preamble verbatim so the comment survives (bughunt LSP-3). Specifiers/include paths use single
    // slashes, so `//` or `/*` only ever marks a comment here.
    if (t && (t.includes("//") || t.includes("/*"))) {
      preCanonical = false;
      break;
    }
  }
  if (!preCanonical) {
    out += pre;
  } else {
    let block = "";
    if (scan.preambleIncludes.length > 0) block += scan.preambleIncludes.join("\n") + "\n";
    if (scan.imports.length > 0) {
      if (block) block += "\n";
      // `import "@Header.h"` (INCLUDE_RETIREMENT_PLAN.md §B) — a nameless host include, spelled
      // with its own shape rather than the named `{ ... } from` block.
      for (const imp of scan.imports) {
        block += imp.hostInclude
          ? `import "@${imp.specifier}"\n`
          : `import { ${imp.names.join(", ")} } from "${imp.specifier}"\n`;
      }
    }
    if (block) out += block + "\n";
  }

  let cursor = -1;
  for (let k = 0; k < scan.order.length; k++) {
    const ord = scan.order[k];
    if (k > 0) {
      if (fromCodePoints(srcCp, cursor, trueStart(ord) - cursor).trim()) return verbatim(true); // never delete user text
      out += "\n";
    }
    out += fmtDecl(ord);
    cursor = declNext(ord);
  }
  if (state.unsafeStrAttr) return verbatim(true); // G-05

  if (cursor >= 0 && cursor < srcCp.length) {
    const trailing = fromCodePoints(srcCp, cursor, srcCp.length - cursor);
    if (trailing.trim()) out = rstripWsNl(out) + "\n\n" + lstripWsNl(trailing);
  }
  const text = rstripWsNl(out) + "\n";
  return { ok: true, text, changed: text !== source, fellBack: false };
}

// ── indent helpers ─────────────────────────────────────────────────────────────────────────

function pad(indent: number, o: UetkxFormatOptions): string {
  return o.indentStyle === "space" ? " ".repeat(indent * o.indentSize) : "\t".repeat(indent);
}

function leadingWs(s: string): string {
  let i = 0;
  while (i < s.length && (s[i] === " " || s[i] === "\t")) i++;
  return s.slice(0, i);
}

function stripLeadingWs(s: string): string {
  return s.slice(leadingWs(s).length);
}

function collapseSpaces(s: string): string {
  const cp = toCodePoints(s);
  const n = cp.length;
  let out = "";
  let i = 0;
  while (i < n) {
    const j = skipNoncode(cp, i);
    if (j !== i) {
      out += fromCodePoints(cp, i, j - i);
      i = j;
      continue;
    }
    if (cp[i] === 32 && i + 1 < n && cp[i + 1] === 32) {
      out += " ";
      while (i < n && cp[i] === 32) i++;
      continue;
    }
    out += String.fromCodePoint(cp[i]);
    i++;
  }
  return out;
}

function indentUnit(lines: readonly string[]): number {
  const widths = new Set<number>();
  for (const line of lines) {
    let spaces = 0;
    for (const ch of line) {
      if (ch === " ") spaces++;
      else if (ch === "\t") continue;
      else break;
    }
    if (spaces > 0) widths.add(spaces);
  }
  if (widths.size === 0) return 1;
  const sorted = [...widths].sort((a, b) => a - b);
  let unit = sorted[0];
  for (let i = 1; i < sorted.length; i++) {
    const d = sorted[i] - sorted[i - 1];
    if (d > 0 && d < unit) unit = d;
  }
  return Math.max(unit, 1);
}

function indentDepth(s: string, unit: number): number {
  let cols = 0;
  for (const ch of s) {
    if (ch === "\t") cols += unit;
    else if (ch === " ") cols += 1;
    else break;
  }
  return Math.round(cols / unit);
}

function stringLineMask(lines: readonly string[]): boolean[] {
  const cp = toCodePoints(lines.join("\n"));
  const n = cp.length;
  const lineStarts = [0];
  for (let i = 0; i < n; i++) if (cp[i] === 10) lineStarts.push(i + 1);
  const mask = new Array<boolean>(lineStarts.length).fill(false);
  let lineIdx = 0;
  let i = 0;
  while (i < n) {
    while (lineIdx + 1 < lineStarts.length && lineStarts[lineIdx + 1] <= i) lineIdx++;
    const j = skipNoncode(cp, i);
    if (j !== i) {
      let endLine = lineIdx;
      while (endLine + 1 < lineStarts.length && lineStarts[endLine + 1] <= j) endLine++;
      for (let m = lineIdx + 1; m <= endLine && m < mask.length; m++) mask[m] = true;
      i = j;
      continue;
    }
    i++;
  }
  return mask;
}

function splitTrimBlankEdges(code: string): string[] {
  const lines = code.split("\n");
  while (lines.length && !lines[0].trim()) lines.shift();
  while (lines.length && !lines[lines.length - 1].trim()) lines.pop();
  return lines;
}

function reanchor(code: string, indent: number, o: UetkxFormatOptions): string {
  const lines = splitTrimBlankEdges(code);
  if (lines.length === 0) return "";
  const mask = stringLineMask(lines);
  const unitLines = lines.filter((_, i) => !mask[i]);
  const unit = indentUnit(unitLines);
  let anchor = -1;
  let anchorAny = -1;
  const depths: number[] = [];
  for (let i = 0; i < lines.length; i++) {
    if (mask[i] || !lines[i].trim()) {
      depths.push(-1);
      continue;
    }
    const d = indentDepth(lines[i], unit);
    depths.push(d);
    if (anchorAny === -1) anchorAny = d;
    if (anchor === -1 && !lines[i].trim().startsWith("//")) anchor = d;
  }
  if (anchor === -1) anchor = anchorAny;
  let out = "";
  for (let i = 0; i < lines.length; i++) {
    if (mask[i]) out += lines[i] + "\n";
    else if (depths[i] === -1) out += "\n";
    else out += pad(indent + Math.max(0, depths[i] - anchor), o) + collapseSpaces(stripLeadingWs(lines[i])) + "\n";
  }
  return out;
}

function reanchorRel(code: string, indent: number, unit: number, anchor: number, o: UetkxFormatOptions): string {
  const lines = splitTrimBlankEdges(code);
  if (lines.length === 0) return "";
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

function bodyGeometry(body: string): { unit: number; anchor: number } {
  const lines = splitTrimBlankEdges(body);
  const mask = stringLineMask(lines);
  const unitLines = lines.filter((_, i) => !mask[i]);
  const unit = indentUnit(unitLines);
  let anchor = -1;
  let anchorAny = -1;
  for (let i = 0; i < lines.length; i++) {
    if (mask[i] || !lines[i].trim()) continue;
    const d = indentDepth(lines[i], unit);
    if (anchorAny === -1) anchorAny = d;
    if (anchor === -1 && !lines[i].trim().startsWith("//")) {
      anchor = d;
      break;
    }
  }
  if (anchor === -1) anchor = Math.max(anchorAny, 0);
  return { unit, anchor };
}

function hasLeadingBlank(s: string): boolean {
  let i = 0;
  const n = s.length;
  while (i < n && (s[i] === " " || s[i] === "\t")) i++;
  if (i >= n || s[i] !== "\n") return false;
  i++;
  while (i < n && (s[i] === " " || s[i] === "\t")) i++;
  return i < n && s[i] === "\n";
}

function hasTrailingBlank(s: string): boolean {
  let i = s.length - 1;
  while (i >= 0 && (s[i] === " " || s[i] === "\t")) i--;
  if (i < 0 || s[i] !== "\n") return false;
  i--;
  while (i >= 0 && (s[i] === " " || s[i] === "\t")) i--;
  return i >= 0 && s[i] === "\n";
}

function rstripWsNl(s: string): string {
  let end = s.length;
  while (end > 0 && (s[end - 1] === " " || s[end - 1] === "\t" || s[end - 1] === "\n")) end--;
  return s.slice(0, end);
}

function lstripWsNl(s: string): string {
  let start = 0;
  while (start < s.length && (s[start] === " " || s[start] === "\t" || s[start] === "\n")) start++;
  return s.slice(start);
}

// ── markup re-emit ─────────────────────────────────────────────────────────────────────────

function fmtAttr(a: UetkxAttr, state: FmtState): string {
  switch (a.kind) {
    case "str":
      if (a.value.includes('"')) state.unsafeStrAttr = true; // G-05
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

function fmtChildren(children: readonly UetkxNode[] | undefined, indent: number, state: FmtState): string {
  let out = "";
  for (const child of children ?? []) out += fmtNode(child, indent, state);
  return out;
}

function fmtElement(node: UetkxNode, indent: number, state: FmtState): string {
  const p = pad(indent, state.o);
  const attrStrs = (node.attrs ?? []).map((a) => fmtAttr(a, state));
  const children = (node.children ?? []).filter(Boolean);
  const selfClose = children.length === 0;
  let head = `<${node.tag}`;
  if (attrStrs.length) head += " " + attrStrs.join(" ");
  const singleClose = selfClose ? (state.o.insertSpaceBeforeSelfClose ? " />" : "/>") : ">";
  const single = head + singleClose;
  let wrap = state.o.singleAttributePerLine && attrStrs.length > 1;
  if (!wrap && p.length + single.length > state.o.printWidth && attrStrs.length > 1) wrap = true;
  let out = "";
  if (!wrap) {
    if (selfClose) return p + single + "\n";
    out += p + single + "\n";
  } else {
    out += p + `<${node.tag}\n`;
    const apad = pad(indent + 1, state.o);
    for (let k = 0; k < attrStrs.length; k++) {
      const last = k === attrStrs.length - 1;
      if (last && selfClose) out += apad + attrStrs[k] + (state.o.insertSpaceBeforeSelfClose ? " />" : "/>") + "\n";
      else if (last) out += apad + attrStrs[k] + "\n" + p + ">\n";
      else out += apad + attrStrs[k] + "\n";
    }
    if (selfClose) return out;
  }
  out += fmtChildren(children, indent + 1, state);
  out += p + `</${node.tag}>\n`;
  return out;
}

function fmtBody(bodySrc: string, indent: number, state: FmtState): string {
  const cp = toCodePoints(bodySrc);
  const split = splitMarkupReturn(cp, false);
  if (!split.ok) return reanchor(bodySrc, indent, state.o);
  let tail = split.afterParen;
  if (tail < cp.length && cp[tail] === 59 /*;*/) tail++;
  if (fromCodePoints(cp, tail, cp.length - tail).trim()) return reanchor(bodySrc, indent, state.o);
  const parsed = parseMarkup(cp, split.mStart, split.mEnd);
  if (parsed.errorCode) return reanchor(bodySrc, indent, state.o);
  let out = "";
  const lead = fromCodePoints(cp, 0, split.returnAt);
  if (lead.trim()) {
    const { unit, anchor } = bodyGeometry(bodySrc);
    out += reanchorRel(lead, indent, unit, anchor, state.o);
  }
  const p = pad(indent, state.o);
  out += p + "return (\n";
  for (const node of parsed.nodes) out += fmtNode(node, indent + 1, state);
  out += p + ");\n";
  return out;
}

function fmtIf(node: UetkxNode, indent: number, state: FmtState): string {
  const p = pad(indent, state.o);
  let out = "";
  const branches = node.branches ?? [];
  for (let k = 0; k < branches.length; k++) {
    const br = branches[k];
    if (k === 0) out += p + `@if (${br.cond.trim()}) {\n`;
    else out = rstripWsNl(out) + ` @elif (${br.cond.trim()}) {\n`;
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

function fmtLoop(node: UetkxNode, indent: number, state: FmtState, keyword: string): string {
  const p = pad(indent, state.o);
  let out = p + `@${keyword} (${(node.header ?? "").trim()}) {\n`;
  out += fmtBody(node.bodyMarkup ?? "", indent + 1, state);
  out += p + "}\n";
  return out;
}

function fmtMatch(node: UetkxNode, indent: number, state: FmtState): string {
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

function fmtNode(node: UetkxNode, indent: number, state: FmtState): string {
  switch (node.type) {
    case "el":
      return fmtElement(node, indent, state);
    case "frag": {
      const inner = fmtChildren(node.children, indent + 1, state);
      const p = pad(indent, state.o);
      if (node.named) {
        let head = `<${node.named}`;
        for (const a of node.attrs ?? []) head += " " + fmtAttr(a, state);
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

function fmtParams(params: readonly UetkxParam[]): string {
  if (params.length === 0) return "";
  const parts = params.map((p) => {
    let part = p.name;
    if (p.type) part += ": " + p.type;
    if (p.default) part += " = " + p.default;
    return part;
  });
  return `(${parts.join(", ")})`;
}

function fmtComponent(decl: UetkxComponentDecl, state: FmtState): string {
  let out = `${decl.exported ? "export " : ""}component ${decl.name}${fmtParams(decl.params)} {\n`;
  const setup = reanchor(decl.setup, 1, state.o);
  if (setup) {
    if (hasLeadingBlank(decl.setup)) out += "\n";
    out += setup;
    if (hasTrailingBlank(decl.setup)) out += "\n";
  }
  out += pad(1, state.o) + "return (\n";
  out += fmtChildren(decl.windowNodes, 2, state);
  out += pad(1, state.o) + ");\n";
  out += "}\n";
  return out;
}

// A verbatim-C++ body (hook / module) canonically re-anchored at depth 1 with the authored
// leading/trailing blank lines kept — the fmtComponent setup treatment, shared so the two
// verbatim-body declaration kinds format identically (mirrors FmtVerbatimBody).
function fmtVerbatimBody(body: string, state: FmtState): string {
  let out = "";
  const reanchored = reanchor(body, 1, state.o);
  if (reanchored) {
    if (hasLeadingBlank(body)) out += "\n";
    out += reanchored;
    if (hasTrailingBlank(body)) out += "\n";
  }
  return out;
}

function fmtHook(decl: UetkxHookDecl, state: FmtState): string {
  // `export? hook Name(<verbatim C++ params>) [-> <verbatim Ret>] {`. Params/Ret are verbatim C++
  // (template commas the family grammar can't split) — trimmed, never re-parsed.
  let header = `${decl.exported ? "export " : ""}hook ${decl.name}(${decl.params.trim()})`;
  const ret = decl.ret.trim();
  if (ret) header += ` -> ${ret}`;
  return header + " {\n" + fmtVerbatimBody(decl.body, state) + "}\n";
}

function fmtModule(decl: UetkxModuleDecl, state: FmtState): string {
  const header = `${decl.exported ? "export " : ""}module ${decl.name}`;
  return header + " {\n" + fmtVerbatimBody(decl.body, state) + "}\n";
}
