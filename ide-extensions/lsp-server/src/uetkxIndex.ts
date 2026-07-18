// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// The workspace reference index (TD-033, LSP_COMPLETION_PLAN N0): for one file, every place a
// .uetkx-level symbol is REFERENCED — declaration name tokens, markup tags (open + close),
// verbatim-code identifiers (scope-tracked so locals never count), namespace-qual members,
// import bindings (target + local-alias tokens), export-list entries and the export-default
// marker. Find-all-references and rename are pure queries over this; document/workspace symbols
// use the decl half. Offsets are CODE POINTS (D-18) — callers convert at the LSP edge.
//
// The scope tracker here is the TS half of the family's pragmatic local-declaration heuristic
// (LSP_COMPLETION_PLAN N-07; the C++ half lives in UetkxCodegen/UetkxResolve): brace-scoped,
// recognizing `Type Name =`, `auto Name`, structured bindings `auto [A, B]`, parameter lists
// and for-init declarations. Over-detection suppresses a reference (a missed rename edit fails
// LOUDLY at compile); under-detection is the silent failure mode — patterns err toward
// precision, with every recognized shape test-pinned.

import { findMatching, keywordAt, skipNoncode, skipNoncodeMarkup } from "./cppScanner";
import { fromCodePoints, toCodePoints } from "./codePoints";
import { findMarkupRanges } from "./jsxScan";
import { parseMarkup, UetkxNode } from "./uetkxMarkup";
import {
  parseParams,
  splitMarkupReturn,
  UetkxComponentDecl,
  UetkxFileScanResult,
} from "./uetkxFileScan";

export type UetkxRefKind =
  | "decl-name" // a declaration's own name token
  | "tag" // <Name or </Name in markup position
  | "code" // a bare identifier in verbatim C++ (call, value ref, qual base)
  | "qual-member" // the Member of `Base::Member` where Base is a namespace-import alias
  | "import-target" // the TARGET-name token of a named import binding
  | "import-alias" // a LOCAL alias token (`B` of `A as B`, a `* as X` alias, a default alias)
  | "export-list" // an `export { … };` entry token
  | "export-default"; // the name token of `export default Name;`

export interface UetkxFileRef {
  kind: UetkxRefKind;
  name: string;
  start: number; // code points into the file
  len: number;
  importIndex?: number; // import-target / import-alias / qual-member: which scan.imports entry
  qualBase?: string; // qual-member only: the alias base spelling
  closeTag?: boolean; // tag only: a `</Name` occurrence
}

const isIdentCp = (c: number): boolean =>
  c === 95 || (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || (c >= 48 && c <= 57);
const isIdentStartCp = (c: number): boolean => isIdentCp(c) && !(c >= 48 && c <= 57);
const isWs = (c: number): boolean => c === 32 || c === 9 || c === 10 || c === 13;

// C++ keywords/common tokens that can head a statement — never referenceable .uetkx symbols.
// Purely a noise filter (queries match by name anyway); NOT load-bearing for correctness.
const CODE_NOISE = new Set([
  "if", "else", "for", "while", "do", "switch", "case", "default", "return", "break", "continue",
  "const", "constexpr", "static", "auto", "void", "bool", "true", "false", "nullptr", "new",
  "delete", "sizeof", "this", "class", "struct", "enum", "using", "namespace", "template",
  "typename", "int32", "int64", "uint32", "uint8", "float", "double", "int",
]);

// Keywords that can never HEAD a type — the ONLY identifiers that turn the type-ish lookbehind
// off. MUST mirror FUetkxScopedLocals::Walk's list exactly (N-07 behavioral identity): anything
// else (`const`, `struct`, `using`, a user type…) keeps a following `Name =/;/{` a declaration.
const NON_TYPEISH = new Set([
  "return", "if", "else", "for", "while", "switch", "case", "break", "continue",
  "new", "delete", "sizeof",
]);

// ── the scope-aware code-reference collector (N-07 TS half) ─────────────────────────────────

interface ScopeState {
  stack: Array<Set<string>>;
}

function declareLocal(scopes: ScopeState, name: string): void {
  scopes.stack[scopes.stack.length - 1].add(name);
}

function isLocal(scopes: ScopeState, name: string): boolean {
  for (let i = scopes.stack.length - 1; i >= 0; i--) {
    if (scopes.stack[i].has(name)) return true;
  }
  return false;
}

/** Parse a parameter-list TEXT (verbatim C++ or the new-form `Type Name = Default` grammar) and
 *  return the parameter NAMES — the scope seed for a hook/util/component body. Reuses the
 *  new-form comma/depth walker (last top-level identifier before `=`/end per piece). */
export function paramNamesOf(paramText: string): string[] {
  if (!paramText.trim()) return [];
  const names: string[] = [];
  // parseNewParams-shaped extraction: reuse parseParams for the legacy colon grammar is wrong
  // here — hook/util params are verbatim C++ (`int32 Start`), so take the LAST identifier of
  // each top-level comma piece before that piece's `=` (mirrors ParseNewParams).
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
    const eq = (() => {
      let d = 0;
      for (let i = 0; i < piece.length; i++) {
        const c = piece[i];
        if (c === "(" || c === "[" || c === "{" || c === "<") d++;
        else if (c === ")" || c === "]" || c === "}" || c === ">") d--;
        else if (c === "=" && d === 0 && piece[i + 1] !== "=") return i;
      }
      return -1;
    })();
    const head = eq === -1 ? piece : piece.slice(0, eq);
    const m = /([A-Za-z_][A-Za-z0-9_]*)\s*$/.exec(head.trim());
    if (m) names.push(m[1]);
  }
  return names;
}

/** Collect code references from a verbatim-C++ region. Scope-tracked: identifiers with a local
 *  declaration in scope are NOT emitted (TD-034 #1/#2 semantics — locals are never symbol
 *  references). `namespaceAliases` maps a `* as X` alias to its scan.imports index so
 *  `X::Member` emits a qual-member ref. BaseAt = the region's absolute code-point offset. */
export function collectCodeRefs(
  regionCp: readonly number[],
  baseAt: number,
  paramSeed: readonly string[],
  namespaceAliases: ReadonlyMap<string, number>,
  out: UetkxFileRef[],
): void {
  const n = regionCp.length;
  const scopes: ScopeState = { stack: [new Set(paramSeed)] };
  let i = 0;
  // one-token lookbehind for the declaration heuristic
  let prevIdent: string | null = null; // the identifier immediately before the current one
  let prevIdentWasTypeish = false; // prevIdent looked like a type head (ident possibly with ::<>&*)
  const resetDeclState = () => {
    prevIdent = null;
    prevIdentWasTypeish = false;
  };
  while (i < n) {
    const j = skipNoncode(regionCp, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = regionCp[i];
    if (c === 123 /* { */) {
      scopes.stack.push(new Set());
      resetDeclState();
      i++;
      continue;
    }
    if (c === 125 /* } */) {
      if (scopes.stack.length > 1) scopes.stack.pop();
      resetDeclState();
      i++;
      continue;
    }
    if (isIdentStartCp(c) && (i === 0 || !isIdentCp(regionCp[i - 1]))) {
      const s = i;
      while (i < n && isIdentCp(regionCp[i])) i++;
      const ident = fromCodePoints(regionCp, s, i - s);

      // `auto [A, B] = …` — structured bindings declare every bound name.
      if (ident === "auto") {
        let p = i;
        while (p < n && isWs(regionCp[p])) p++;
        // reference decorations: auto& / auto&& / auto*
        while (p < n && (regionCp[p] === 38 /* & */ || regionCp[p] === 42 /* * */)) p++;
        while (p < n && isWs(regionCp[p])) p++;
        if (p < n && regionCp[p] === 91 /* [ */) {
          const close = findMatching(regionCp, p);
          if (close !== -1) {
            let q = p + 1;
            while (q < close) {
              while (q < close && !isIdentStartCp(regionCp[q])) q++;
              const bs = q;
              while (q < close && isIdentCp(regionCp[q])) q++;
              if (q > bs) declareLocal(scopes, fromCodePoints(regionCp, bs, q - bs));
            }
            i = close + 1;
            resetDeclState();
            continue;
          }
        }
        prevIdent = ident;
        prevIdentWasTypeish = true;
        continue;
      }

      // scan-back: member access / scope qual (the IMPORT-3 colon rule)
      let member = false;
      let scoped = false;
      for (let k = s - 1; k >= 0; k--) {
        const p = regionCp[k];
        if (p === 32 || p === 9) continue;
        member = p === 46 /* . */ || (p === 62 /* > */ && k > 0 && regionCp[k - 1] === 45 /* - */);
        scoped = p === 58 /* : */ && k > 0 && regionCp[k - 1] === 58;
        break;
      }
      // lookahead: `::` (qual) or the declaration trigger
      let p2 = i;
      while (p2 < n && (regionCp[p2] === 32 || regionCp[p2] === 9)) p2++;
      const followedByQual = p2 + 1 < n && regionCp[p2] === 58 && regionCp[p2 + 1] === 58;
      const followedByEq =
        p2 < n &&
        regionCp[p2] === 61 /* = */ &&
        regionCp[p2 + 1] !== 61 &&
        (s === 0 || (regionCp[s - 1] !== 61 && regionCp[s - 1] !== 33 && regionCp[s - 1] !== 60 && regionCp[s - 1] !== 62));
      const followedBySemi = p2 < n && regionCp[p2] === 59; /* ; */
      const followedByBrace = p2 < n && regionCp[p2] === 123; /* { */

      // Declaration heuristic: `<type-ish> Name =` / `<type-ish> Name;` / `<type-ish> Name{…}`
      // declares Name (covers `const FLinearColor X = …`, `FString S;`, `TArray<int32> A{…}`,
      // for-init decls — conservative: declared into the CURRENT scope).
      if (!member && !scoped && prevIdentWasTypeish && (followedByEq || followedBySemi || followedByBrace)) {
        declareLocal(scopes, ident);
        resetDeclState();
        continue; // a declaration is not a reference
      }

      if (!member && !scoped) {
        if (isLocal(scopes, ident)) {
          // a local — never a symbol reference (TD-034)
        } else if (followedByQual && namespaceAliases.has(ident)) {
          // Base::Member through a namespace alias: emit the base as code + the member as
          // qual-member (rename of the TARGET decl edits the member token; rename of the
          // ALIAS edits base tokens).
          out.push({ kind: "code", name: ident, start: baseAt + s, len: ident.length, importIndex: namespaceAliases.get(ident) });
          let m2 = p2 + 2;
          while (m2 < n && (regionCp[m2] === 32 || regionCp[m2] === 9)) m2++;
          const ms = m2;
          while (m2 < n && isIdentCp(regionCp[m2])) m2++;
          if (m2 > ms) {
            out.push({
              kind: "qual-member",
              name: fromCodePoints(regionCp, ms, m2 - ms),
              start: baseAt + ms,
              len: m2 - ms,
              importIndex: namespaceAliases.get(ident),
              qualBase: ident,
            });
          }
          i = m2;
          resetDeclState();
          continue;
        } else if (!CODE_NOISE.has(ident)) {
          out.push({ kind: "code", name: ident, start: baseAt + s, len: ident.length });
        }
      }

      // update decl-lookbehind: an identifier (possibly to-be-decorated) may be a type head.
      prevIdent = ident;
      prevIdentWasTypeish = !NON_TYPEISH.has(ident);
      continue;
    }
    // whitespace and type decorations keep the type-ish state alive; other punctuation resets.
    // A colon keeps it ONLY as part of a `::` qual — a LONE ternary/label colon resets (the
    // IMPORT-3 rule; `U = a ? B : B;` otherwise reads the second arm as a declaration).
    if (
      isWs(c) || c === 60 /* < */ || c === 62 /* > */ || c === 38 /* & */ || c === 42 /* * */ ||
      (c === 58 /* : */ && (regionCp[i + 1] === 58 || (i > 0 && regionCp[i - 1] === 58)))
    ) {
      i++;
      continue;
    }
    if (c === 44 /* , */) {
      // `Type A = 1, B = 2` — keep type-ish so B declares too (rare but real)
      i++;
      continue;
    }
    resetDeclState();
    i++;
  }
}

// ── markup-span walking: tags (open + close) + embedded expression code ─────────────────────

/** Scan ONE markup span for `<Tag` / `</Tag` tokens. Markup-lexis-aware (comments/strings via
 *  skipNoncodeMarkup) and hole-aware: a braced expression `{ … }` and a parenthesized directive
 *  header `( … )` are CODE islands — skipped whole via findMatching so a `<` comparison inside
 *  them can never read as a tag (the `@if (a <b)` shape). */
function collectTagRefs(bodyCp: readonly number[], spanStart: number, spanEnd: number, baseAt: number, out: UetkxFileRef[]): void {
  let i = spanStart;
  while (i < spanEnd) {
    const j = skipNoncodeMarkup(bodyCp, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = bodyCp[i];
    if (c === 123 /* { */ || c === 40 /* ( */) {
      const close = findMatching(bodyCp, i);
      if (close === -1 || close >= spanEnd) {
        i++;
        continue;
      }
      i = close + 1;
      continue;
    }
    if (c === 60 /* < */) {
      let p = i + 1;
      let close = false;
      if (p < spanEnd && bodyCp[p] === 47 /* / */) {
        close = true;
        p++;
      }
      if (p < spanEnd && isIdentStartCp(bodyCp[p])) {
        const s = p;
        while (p < spanEnd && isIdentCp(bodyCp[p])) p++;
        const name = fromCodePoints(bodyCp, s, p - s);
        out.push({ kind: "tag", name, start: baseAt + s, len: name.length, closeTag: close });
        i = p;
        continue;
      }
    }
    i++;
  }
}

/** Walk a parsed markup node tree, collecting code refs from every embedded expression: attr
 *  exprs (events included), expr children, directive headers/conditions/subjects/case values,
 *  and directive BODIES (leading statements + the nested markup, recursively — the
 *  ValidateDirectiveBodyHooks split). Tag refs come from collectTagRefs (positional, includes
 *  close tags), NOT from here. */
function walkMarkupNodeCode(
  node: UetkxNode,
  bodyBase: number, // absolute offset of the BODY the node offsets are relative to
  paramSeed: readonly string[],
  namespaceAliases: ReadonlyMap<string, number>,
  out: UetkxFileRef[],
): void {
  const code = (text: string | undefined, at: number | undefined) => {
    if (!text || at === undefined || at < 0) return;
    collectCodeRefs(toCodePoints(text), bodyBase + at, paramSeed, namespaceAliases, out);
  };
  const markupBody = (text: string | undefined, at: number | undefined) => {
    if (!text || at === undefined || at < 0) return;
    const cp = toCodePoints(text);
    // A directive body = leading C++ statements + (optionally) a markup return window.
    const split = splitMarkupReturn(cp, false);
    if (split.ok) {
      code(fromCodePoints(cp, 0, split.returnAt), at);
      const parsed = parseMarkup(cp, split.mStart, split.mEnd);
      if (!parsed.errorCode) {
        for (const child of parsed.nodes) walkMarkupNodeCode(child, bodyBase + at, paramSeed, namespaceAliases, out);
      }
    } else {
      // no return window: the body is markup nodes directly (the @if { <T/> } shape)
      const parsed = parseMarkup(cp, 0, cp.length);
      if (!parsed.errorCode) {
        for (const child of parsed.nodes) walkMarkupNodeCode(child, bodyBase + at, paramSeed, namespaceAliases, out);
      } else {
        code(text, at); // unparseable as markup — treat as code (best-effort)
      }
    }
  };

  switch (node.type) {
    case "el":
    case "frag":
      for (const attr of node.attrs ?? []) {
        if ((attr.kind === "expr" || attr.kind === "spread") && attr.vat >= 0) {
          code(attr.value, attr.vat);
        }
      }
      for (const child of node.children ?? []) {
        walkMarkupNodeCode(child, bodyBase, paramSeed, namespaceAliases, out);
      }
      break;
    case "expr":
      if (node.vat !== undefined && node.vat >= 0) code(node.code, node.vat);
      break;
    case "if":
      for (const branch of node.branches ?? []) {
        code(branch.cond, branch.condAt);
        markupBody(branch.bodyMarkup, branch.bodyAt);
      }
      markupBody(node.elseBody, node.elseBodyAt);
      break;
    case "for":
    case "while":
      code(node.header, node.headerAt);
      markupBody(node.bodyMarkup, node.bodyAt);
      break;
    case "match":
      code(node.subject, node.subjectAt);
      for (const c of node.cases ?? []) {
        code(c.value, c.valueAt);
        markupBody(c.bodyMarkup, c.bodyAt);
      }
      markupBody(node.defaultBody, node.defaultBodyAt);
      break;
    case "text":
    case "comment":
      break; // prose can never reference (N-08 — the no-false-positive rule)
  }
}

// ── the per-file collector ──────────────────────────────────────────────────────────────────

/** Every reference in one file (N-03). `srcText` must be the exact text `scan` was produced
 *  from. Sorted by start offset. */
export function collectFileReferences(scan: UetkxFileScanResult, srcText: string): UetkxFileRef[] {
  const out: UetkxFileRef[] = [];
  const namespaceAliases = new Map<string, number>();
  scan.imports.forEach((imp, idx) => {
    if (imp.isNamespace) namespaceAliases.set(imp.namespaceAlias, idx);
  });

  // imports: target tokens + local-alias tokens
  scan.imports.forEach((imp, idx) => {
    if (imp.hostInclude) return;
    if (imp.isNamespace) {
      out.push({ kind: "import-alias", name: imp.namespaceAlias, start: imp.namespaceAliasAt, len: imp.namespaceAlias.length, importIndex: idx });
      return;
    }
    if (imp.isDefault) {
      out.push({ kind: "import-alias", name: imp.defaultAlias, start: imp.defaultAliasAt, len: imp.defaultAlias.length, importIndex: idx });
      return;
    }
    for (let n = 0; n < imp.names.length; n++) {
      out.push({ kind: "import-target", name: imp.names[n], start: imp.nameAts[n], len: imp.names[n].length, importIndex: idx });
      const local = imp.localNames[n] ?? imp.names[n];
      if (local !== imp.names[n]) {
        out.push({ kind: "import-alias", name: local, start: imp.localNameAts[n], len: local.length, importIndex: idx });
      }
    }
  });

  // declaration name tokens
  for (const d of scan.components) out.push({ kind: "decl-name", name: d.name, start: d.nameAt, len: d.name.length });
  for (const d of scan.hooks) out.push({ kind: "decl-name", name: d.name, start: d.nameAt, len: d.name.length });
  for (const d of scan.modules) out.push({ kind: "decl-name", name: d.name, start: d.nameAt, len: d.name.length });
  for (const d of scan.values) out.push({ kind: "decl-name", name: d.name, start: d.nameAt, len: d.name.length });
  for (const d of scan.utils) out.push({ kind: "decl-name", name: d.name, start: d.nameAt, len: d.name.length });

  // export list + export default
  for (const e of scan.exportListEntries) out.push({ kind: "export-list", name: e.name, start: e.at, len: e.name.length });
  if (scan.defaultExportName && scan.defaultExportAt >= 0) {
    out.push({ kind: "export-default", name: scan.defaultExportName, start: scan.defaultExportAt, len: scan.defaultExportName.length });
  }

  // component bodies: markup spans (return windows + value-markup ranges) get tag refs +
  // tree-walked expression code; everything OUTSIDE the spans is setup/tail code.
  for (const comp of scan.components as UetkxComponentDecl[]) {
    const bodyCp = toCodePoints(comp.body);
    const paramSeed = comp.params.map((p) => p.name);
    // markup spans, body-relative: the collected return windows + jsx value-markup ranges
    const spans: Array<[number, number]> = [];
    for (const span of comp.returns ?? []) {
      if (span.mStart >= 0 && span.mEnd >= 0) spans.push([span.mStart, span.mEnd]);
    }
    for (const r of findMarkupRanges(bodyCp, 0, bodyCp.length)) {
      spans.push([r.start, r.end === -1 ? bodyCp.length : r.end]);
    }
    // merge overlapping spans
    spans.sort((a, b) => a[0] - b[0]);
    const merged: Array<[number, number]> = [];
    for (const s of spans) {
      const last = merged[merged.length - 1];
      if (last && s[0] <= last[1]) last[1] = Math.max(last[1], s[1]);
      else merged.push([s[0], s[1]]);
    }
    // tag refs inside each span (offsets are body-relative; comp.bodyAt makes them absolute)
    for (const [s, e] of merged) collectTagRefs(bodyCp, s, e, comp.bodyAt, out);
    // expression code inside the parsed trees: return windows come pre-parsed on the scan…
    const windowSpans: Array<[number, number]> = (comp.returns ?? [])
      .filter((sp) => sp.mStart >= 0 && sp.mEnd >= 0)
      .map((sp) => [sp.mStart, sp.mEnd] as [number, number]);
    for (const span of comp.returns ?? []) {
      if (span.root) walkMarkupNodeCode(span.root, comp.bodyAt, paramSeed, namespaceAliases, out);
    }
    // …value-markup ranges (setup AND tails between early returns) are parsed on demand — the
    // scan stores trees only for return windows.
    for (const r of findMarkupRanges(bodyCp, 0, bodyCp.length)) {
      const end = r.end === -1 ? bodyCp.length : r.end;
      if (windowSpans.some(([ws, we]) => r.start >= ws && r.start < we)) continue; // a return window, already walked
      const parsed = parseMarkup(bodyCp, r.start, end);
      if (!parsed.errorCode) {
        for (const nd of parsed.nodes) walkMarkupNodeCode(nd, comp.bodyAt, paramSeed, namespaceAliases, out);
      }
    }
    // code OUTSIDE the merged spans
    let cursor = 0;
    for (const [s, e] of merged) {
      if (cursor < s) collectCodeRefs(bodyCp.slice(cursor, s), comp.bodyAt + cursor, paramSeed, namespaceAliases, out);
      cursor = e;
    }
    if (cursor < bodyCp.length) {
      collectCodeRefs(bodyCp.slice(cursor), comp.bodyAt + cursor, paramSeed, namespaceAliases, out);
    }
  }

  // hooks / utils / modules / values: plain code regions
  for (const h of scan.hooks) {
    collectCodeRefs(toCodePoints(h.body), h.bodyAt, paramNamesOf(h.params), namespaceAliases, out);
  }
  for (const u of scan.utils) {
    collectCodeRefs(toCodePoints(u.body), u.bodyAt, paramNamesOf(u.params), namespaceAliases, out);
  }
  for (const m of scan.modules) {
    collectCodeRefs(toCodePoints(m.body), m.bodyAt, [], namespaceAliases, out);
  }
  for (const v of scan.values) {
    collectCodeRefs(toCodePoints(v.init), v.initAt, [], namespaceAliases, out);
  }

  void srcText;
  out.sort((a, b) => a.start - b.start);
  return out;
}
