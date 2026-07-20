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

import { findMatching, keywordAt, skipNoncode, skipNoncodeMarkup, skipString } from "./cppScanner";
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

// ── the scope-aware local-declaration tracker (N-07 TS half) ────────────────────────────────
// MUST mirror FUetkxScopedLocals (UetkxScopedLocals.h) exactly — one pre-walk records brace
// scopes + the locals declared in each (params, `Type Name =/;/{/:`, `auto`, structured
// bindings, lambda parameters), skipping markup ranges so tags/attrs never mint junk
// declarations. Consumers ask isLocal(name, offset) / namesInScopeAt(offset).

interface ScopeRec {
  start: number;
  end: number;
  parent: number;
}
interface DeclRec {
  name: string;
  scope: number;
  at: number;
}

export class UetkxScopedLocals {
  private readonly scopes: ScopeRec[] = [];
  private readonly decls: DeclRec[] = [];

  constructor(
    cp: readonly number[],
    paramSeed: readonly string[],
    skipRanges?: ReadonlyArray<{ start: number; end: number }>,
  ) {
    this.scopes.push({ start: 0, end: cp.length, parent: -1 });
    for (const p of paramSeed) this.decls.push({ name: p, scope: 0, at: 0 });
    this.walk(cp, skipRanges);
  }

  isLocal(name: string, offset: number): boolean {
    for (const d of this.decls) {
      if (d.name !== name || d.at > offset) continue;
      const s = this.scopes[d.scope];
      if (offset >= s.start && offset < s.end) return true;
    }
    return false;
  }

  /** The names with a local declaration IN SCOPE at `offset` — the precise cross-region seed
   *  for markup islands (a phantom ref here would be EDITED by rename — corruption). */
  namesInScopeAt(offset: number): string[] {
    const names = new Set<string>();
    for (const d of this.decls) {
      if (d.at > offset) continue;
      const s = this.scopes[d.scope];
      if (offset >= s.start && offset < s.end) names.add(d.name);
    }
    return [...names];
  }

  /** Scope/offset-agnostic union — the conservative fallback when an offset is unknown. */
  allDeclNames(): string[] {
    return [...new Set(this.decls.map((d) => d.name))];
  }

  private walk(cp: readonly number[], skipRanges?: ReadonlyArray<{ start: number; end: number }>): void {
    const n = cp.length;
    let current = 0;
    let prevTypeish = false;
    let i = 0;
    while (i < n) {
      if (skipRanges) {
        let jumped = false;
        for (const r of skipRanges) {
          const end = r.end === -1 ? n : r.end;
          if (i >= r.start && i < end) {
            i = end;
            prevTypeish = false;
            jumped = true;
            break;
          }
        }
        if (jumped) continue;
      }
      const j = skipNoncode(cp, i);
      if (j !== i) {
        i = j;
        continue;
      }
      const c = cp[i];
      if (c === 123 /* { */) {
        this.scopes.push({ start: i, end: n, parent: current });
        current = this.scopes.length - 1;
        prevTypeish = false;
        i++;
        continue;
      }
      if (c === 125 /* } */) {
        if (current !== 0) {
          this.scopes[current].end = i;
          current = this.scopes[current].parent;
        }
        prevTypeish = false;
        i++;
        continue;
      }
      if (isIdentStartCp(c) && (i === 0 || !isIdentCp(cp[i - 1]))) {
        const s = i;
        while (i < n && isIdentCp(cp[i])) i++;
        const ident = fromCodePoints(cp, s, i - s);

        // `auto [A, B] = …` — structured bindings declare every bound name.
        if (ident === "auto") {
          let p = i;
          while (p < n && isWs(cp[p])) p++;
          while (p < n && (cp[p] === 38 /* & */ || cp[p] === 42 /* * */)) p++;
          while (p < n && isWs(cp[p])) p++;
          if (p < n && cp[p] === 91 /* [ */) {
            const close = findMatching(cp, p);
            if (close !== -1) {
              let q = p + 1;
              while (q < close) {
                while (q < close && !isIdentStartCp(cp[q])) q++;
                const bs = q;
                while (q < close && isIdentCp(cp[q])) q++;
                if (q > bs) this.decls.push({ name: fromCodePoints(cp, bs, q - bs), scope: current, at: bs });
              }
              i = close + 1;
              prevTypeish = false;
              continue;
            }
          }
          prevTypeish = true;
          continue;
        }

        // member/scope prefixes never start a declaration name (IMPORT-3 lone-colon rule)
        let member = false;
        let scoped = false;
        for (let k = s - 1; k >= 0; k--) {
          const p = cp[k];
          if (p === 32 || p === 9) continue;
          member = p === 46 /* . */ || (p === 62 /* > */ && k > 0 && cp[k - 1] === 45 /* - */);
          scoped = p === 58 /* : */ && k > 0 && cp[k - 1] === 58;
          break;
        }
        let p2 = i;
        while (p2 < n && (cp[p2] === 32 || cp[p2] === 9)) p2++;
        const eq =
          p2 < n &&
          cp[p2] === 61 /* = */ &&
          (p2 + 1 >= n || cp[p2 + 1] !== 61) &&
          (s === 0 || (cp[s - 1] !== 61 && cp[s - 1] !== 33 && cp[s - 1] !== 60 && cp[s - 1] !== 62));
        const semi = p2 < n && cp[p2] === 59; /* ; */
        // range-for + bitfields: a LONE colon after a type-ish juxtaposition declares (`::`
        // never does; ternary arms are safe — the `?` reset kills type-ish first)
        const colon = p2 < n && cp[p2] === 58 && (p2 + 1 >= n || cp[p2 + 1] !== 58);
        const brace = p2 < n && cp[p2] === 123; /* { */
        if (!member && !scoped && prevTypeish && (eq || semi || brace || colon)) {
          this.decls.push({ name: ident, scope: current, at: s });
          prevTypeish = false;
          continue;
        }
        prevTypeish = !NON_TYPEISH.has(ident);
        continue;
      }
      // Lambda parameters: `[caps](int32 A, const F& B) { … }` declares its params — recognized
      // at an EXPRESSION boundary (never after an ident/`)`/`]`/`}` — a subscript — except after
      // `return`); params land in the CURRENT scope at the `[` offset.
      if (c === 91 /* [ */) {
        let lambda = true;
        for (let k = i - 1; k >= 0; k--) {
          const p = cp[k];
          if (p === 32 || p === 9) continue;
          if (p === 41 /* ) */ || p === 93 /* ] */ || p === 125 /* } */) lambda = false;
          else if (isIdentCp(p)) {
            let ks = k;
            while (ks > 0 && isIdentCp(cp[ks - 1])) ks--;
            lambda = fromCodePoints(cp, ks, k - ks + 1) === "return";
          }
          break;
        }
        const closeB = lambda ? findMatching(cp, i) : -1;
        if (closeB !== -1) {
          let p = closeB + 1;
          while (p < n && isWs(cp[p])) p++;
          if (p < n && cp[p] === 40 /* ( */) {
            const closeP = findMatching(cp, p);
            if (closeP !== -1) {
              for (const param of paramNamesOf(fromCodePoints(cp, p + 1, closeP - p - 1))) {
                this.decls.push({ name: param, scope: current, at: i });
              }
              prevTypeish = false;
              i = closeP + 1;
              continue;
            }
          }
          prevTypeish = false;
          i = closeB + 1;
          continue;
        }
      }
      // whitespace + type decorations keep type-ish alive; everything else resets — a COMMA
      // included (keeping it through `,` mis-declared `B` in `Func(A, B = 1)`); a colon keeps
      // it ONLY as part of a `::` qual (the IMPORT-3 rule).
      if (
        isWs(c) || c === 60 /* < */ || c === 62 /* > */ || c === 38 /* & */ || c === 42 /* * */ ||
        (c === 58 /* : */ && (cp[i + 1] === 58 || (i > 0 && cp[i - 1] === 58)))
      ) {
        i++;
        continue;
      }
      prevTypeish = false;
      i++;
    }
  }
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

/** Collect code references from a verbatim-C++ region. Scope-tracked via the SHARED
 *  UetkxScopedLocals pre-walk (the N-07 twin — one heuristic, two consumers): identifiers with
 *  a local declaration in scope are NOT emitted (TD-034 — locals are never symbol references).
 *  `paramSeed` = the names live at the region's start (params + any cross-region in-scope
 *  seed). `namespaceAliases` maps a `* as X` alias to its scan.imports index so `X::Member`
 *  emits a qual-member ref. BaseAt = the region's absolute code-point offset. */
export function collectCodeRefs(
  regionCp: readonly number[],
  baseAt: number,
  paramSeed: readonly string[],
  namespaceAliases: ReadonlyMap<string, number>,
  out: UetkxFileRef[],
): void {
  const n = regionCp.length;
  const locals = new UetkxScopedLocals(regionCp, paramSeed);
  let i = 0;
  while (i < n) {
    const j = skipNoncode(regionCp, i);
    if (j !== i) {
      i = j;
      continue;
    }
    const c = regionCp[i];
    if (isIdentStartCp(c) && (i === 0 || !isIdentCp(regionCp[i - 1]))) {
      const s = i;
      while (i < n && isIdentCp(regionCp[i])) i++;
      const ident = fromCodePoints(regionCp, s, i - s);

      // scan-back: member access / scope qual (the IMPORT-3 lone-colon rule)
      let member = false;
      let scoped = false;
      for (let k = s - 1; k >= 0; k--) {
        const p = regionCp[k];
        if (p === 32 || p === 9) continue;
        member = p === 46 /* . */ || (p === 62 /* > */ && k > 0 && regionCp[k - 1] === 45 /* - */);
        scoped = p === 58 /* : */ && k > 0 && regionCp[k - 1] === 58;
        break;
      }
      if (member || scoped) continue;
      // a local (param, declaration, structured binding, range-for var, lambda param) is never
      // a symbol reference — its DECL token included (at <= offset at the decl site itself)
      if (locals.isLocal(ident, s)) continue;

      let p2 = i;
      while (p2 < n && (regionCp[p2] === 32 || regionCp[p2] === 9)) p2++;
      const followedByQual = p2 + 1 < n && regionCp[p2] === 58 && regionCp[p2 + 1] === 58;
      if (followedByQual && namespaceAliases.has(ident)) {
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
        continue;
      }
      if (!CODE_NOISE.has(ident)) {
        out.push({ kind: "code", name: ident, start: baseAt + s, len: ident.length });
      }
      continue;
    }
    i++;
  }
}

// ── markup-span walking: tags (open + close) + embedded expression code ─────────────────────

/** Scan ONE markup span for `<Tag` / `</Tag` tokens. Markup-lexis-aware (comments/strings via
 *  skipNoncodeMarkup) and hole-aware: a braced expression `{ … }` and a parenthesized directive
 *  header `( … )` are CODE islands — skipped whole via findMatching so a `<` comparison inside
 *  them can never read as a tag (the `@if (a <b)` shape). */

/** Does the `{` at `bracePos` open a DIRECTIVE BODY (`@for (…) {`, `@if (…) {`, `@else {`,
 *  a `case v:` / `default:` body) rather than an expression hole? Expression holes hang off
 *  `=` / expr-child positions; directive bodies follow a `)` whose opener is preceded by an
 *  `@ident` header, the `else` keyword, or a case/default `:`. (R9 field find: treating
 *  directive bodies as holes hid EVERY tag/attr inside them from the live sweep AND the tag
 *  index — "markup inside return directives gets no diagnostics".) */
function isDirectiveBodyBrace(bodyCp: readonly number[], bracePos: number, spanStart: number): boolean {
  let k = bracePos - 1;
  while (k >= spanStart && (bodyCp[k] === 32 || bodyCp[k] === 9 || bodyCp[k] === 10 || bodyCp[k] === 13)) k--;
  if (k < spanStart) return false;
  const c = bodyCp[k];
  if (c === 58 /* : */) return true; // case v: { … } / default: { … }
  if (c === 101 /* e */) {
    // the else keyword (} @else { / } else {)
    let s = k;
    while (s > spanStart && isIdentCp(bodyCp[s - 1])) s--;
    return fromCodePoints(bodyCp, s, k - s + 1) === "else";
  }
  if (c !== 41 /* ) */) return false;
  // walk back to the matching open paren of this header
  let depth = 0;
  let p = k;
  for (; p >= spanStart; p--) {
    if (bodyCp[p] === 41) depth++;
    else if (bodyCp[p] === 40) {
      depth--;
      if (depth === 0) break;
    }
  }
  if (p < spanStart || depth !== 0) return false;
  let q = p - 1;
  while (q >= spanStart && (bodyCp[q] === 32 || bodyCp[q] === 9)) q--;
  // the header ident (for/if/while/match) with its @ sigil
  let s2 = q;
  while (s2 > spanStart && isIdentCp(bodyCp[s2])) s2--;
  return bodyCp[s2] === 64 /* @ */;
}

/** Descend into a directive body's MARKUP: its lead statements are C++ (never tag-swept), the
 *  markup lives in the `return ( … )` window (or, with no return, the body IS markup — the
 *  `@if { <T/> }` shape). Returns the sub-ranges (body-relative) a span walker should treat
 *  as markup. */
function directiveMarkupRanges(bodyCp: readonly number[], braceOpen: number, braceClose: number): Array<[number, number]> {
  const inner = bodyCp.slice(braceOpen + 1, braceClose);
  const split = splitMarkupReturn(inner, false);
  if (split.ok && split.mStart >= 0 && split.mEnd >= 0) {
    return [[braceOpen + 1 + split.mStart, braceOpen + 1 + split.mEnd]];
  }
  return [[braceOpen + 1, braceClose]]; // direct-markup body
}

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
      if (c === 123 && isDirectiveBodyBrace(bodyCp, i, spanStart)) {
        for (const [rs, re] of directiveMarkupRanges(bodyCp, i, close)) {
          collectTagRefs(bodyCp, rs, re, baseAt, out);
        }
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

/** Walk one parsed markup node's EMBEDDED CODE (N-08): attr/spread exprs (events included),
 *  expr children, directive headers/conditions/subjects/case values, and directive BODIES
 *  (leading statements + the nested markup, recursively — the ValidateDirectiveBodyHooks
 *  split). Tag refs come from collectTagRefs (positional, includes close tags), NOT from here.
 *  SEEDS (audit): each island is seeded with the locals IN SCOPE at its body offset
 *  (`seedAt`) plus the accumulated directive-frame locals (`extraSeed`: @for headers +
 *  directive-lead declarations) — a phantom ref for a live local would be EDITED by rename. */
function walkMarkupNodeCode(
  node: UetkxNode,
  bodyBase: number, // absolute offset of the frame buffer the node offsets are relative to
  bodyAt: number, // absolute offset of the COMPONENT BODY (seedAt's coordinate origin)
  seedAt: (bodyRel: number) => readonly string[],
  extraSeed: readonly string[],
  namespaceAliases: ReadonlyMap<string, number>,
  out: UetkxFileRef[],
): void {
  const code = (text: string | undefined, at: number | undefined, extra: readonly string[] = extraSeed) => {
    if (!text || at === undefined || at < 0) return;
    const abs = bodyBase + at;
    const seed = [...seedAt(abs - bodyAt), ...extra];
    collectCodeRefs(toCodePoints(text), abs, seed, namespaceAliases, out);
  };
  const markupBody = (text: string | undefined, at: number | undefined, extra: readonly string[] = extraSeed) => {
    if (!text || at === undefined || at < 0) return;
    const cp = toCodePoints(text);
    // A directive body = leading C++ statements + (optionally) a markup return window.
    const split = splitMarkupReturn(cp, false);
    if (split.ok) {
      const lead = fromCodePoints(cp, 0, split.returnAt);
      code(lead, at, extra);
      // the lead's own declarations are live inside the nested window (audit)
      const leadLocals = new UetkxScopedLocals(toCodePoints(lead), []);
      const nested = [...extra, ...leadLocals.allDeclNames()];
      const parsed = parseMarkup(cp, split.mStart, split.mEnd);
      if (!parsed.errorCode) {
        for (const child of parsed.nodes) {
          walkMarkupNodeCode(child, bodyBase + at, bodyAt, seedAt, nested, namespaceAliases, out);
        }
      }
    } else {
      // no return window: the body is markup nodes directly (the @if { <T/> } shape)
      const parsed = parseMarkup(cp, 0, cp.length);
      if (!parsed.errorCode) {
        for (const child of parsed.nodes) {
          walkMarkupNodeCode(child, bodyBase + at, bodyAt, seedAt, extra, namespaceAliases, out);
        }
      } else {
        code(text, at, extra); // unparseable as markup — treat as code (best-effort)
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
        walkMarkupNodeCode(child, bodyBase, bodyAt, seedAt, extraSeed, namespaceAliases, out);
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
    case "while": {
      code(node.header, node.headerAt);
      // the header's declarations (loop vars — index AND range form) are live in the body
      const headerLocals = new UetkxScopedLocals(toCodePoints(node.header ?? ""), []);
      markupBody(node.bodyMarkup, node.bodyAt, [...extraSeed, ...headerLocals.allDeclNames()]);
      break;
    }
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

  // imports: target tokens + local-alias tokens. No exclusive branching — an ES COMBINED
  // import (`import Def, { A } from` / `import Def, * as X from`) carries default + named/star
  // parts in one declaration and EVERY part's tokens join the reference index.
  scan.imports.forEach((imp, idx) => {
    if (imp.hostInclude) return;
    if (imp.isDefault) {
      out.push({ kind: "import-alias", name: imp.defaultAlias, start: imp.defaultAliasAt, len: imp.defaultAlias.length, importIndex: idx });
    }
    if (imp.isNamespace) {
      out.push({ kind: "import-alias", name: imp.namespaceAlias, start: imp.namespaceAliasAt, len: imp.namespaceAlias.length, importIndex: idx });
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
    // the body-wide in-scope oracle (audit): markup ranges are SKIPPED (no junk decls from
    // tags/attrs); islands and post-range fragments are seeded from it so setup locals used
    // inside markup are never phantom refs (rename would edit them).
    const bodyLocals = new UetkxScopedLocals(bodyCp, paramSeed, merged.map(([s, e]) => ({ start: s, end: e })));
    const seedAt = (bodyRel: number): readonly string[] =>
      bodyRel >= 0 && bodyRel <= bodyCp.length ? bodyLocals.namesInScopeAt(bodyRel) : bodyLocals.allDeclNames();
    // expression code inside the parsed trees: return windows come pre-parsed on the scan…
    const windowSpans: Array<[number, number]> = (comp.returns ?? [])
      .filter((sp) => sp.mStart >= 0 && sp.mEnd >= 0)
      .map((sp) => [sp.mStart, sp.mEnd] as [number, number]);
    for (const span of comp.returns ?? []) {
      if (span.root) walkMarkupNodeCode(span.root, comp.bodyAt, comp.bodyAt, seedAt, [], namespaceAliases, out);
    }
    // …value-markup ranges (setup AND tails between early returns) are parsed on demand — the
    // scan stores trees only for return windows.
    for (const r of findMarkupRanges(bodyCp, 0, bodyCp.length)) {
      const end = r.end === -1 ? bodyCp.length : r.end;
      if (windowSpans.some(([ws, we]) => r.start >= ws && r.start < we)) continue; // a return window, already walked
      const parsed = parseMarkup(bodyCp, r.start, end);
      if (!parsed.errorCode) {
        for (const nd of parsed.nodes) walkMarkupNodeCode(nd, comp.bodyAt, comp.bodyAt, seedAt, [], namespaceAliases, out);
      }
    }
    // code OUTSIDE the merged spans
    let cursor = 0;
    for (const [s, e] of merged) {
      if (cursor < s) collectCodeRefs(bodyCp.slice(cursor, s), comp.bodyAt + cursor, seedAt(cursor), namespaceAliases, out);
      cursor = e;
    }
    if (cursor < bodyCp.length) {
      collectCodeRefs(bodyCp.slice(cursor), comp.bodyAt + cursor, seedAt(cursor), namespaceAliases, out);
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

// ── live schema sweep (F5 field-test B2): tags + attr names, PARSE-ERROR RESILIENT ──────────

export interface UetkxSweptElement {
  tag: string;
  at: number; // code points, span-buffer-relative
  /** Index (into the SAME returned array) of the enclosing element, or undefined for a
   *  span-root element (R12: parent-aware lints — sibling key dupes, slot-key consumption).
   *  Directive bodies inherit the element enclosing the directive. */
  parent?: number;
  /** `value`/`valueAt` are present only for STRING-literal values (`Name="…"`) — the R10
   *  value-validation surface. Expression holes and flag attrs carry no value fields; `form`
   *  (R11) says which shape the attr took: `str` | `expr` | `flag`. For expr attrs,
   *  `exprAt`/`exprEnd` (R12) bound the hole CONTENT (between the braces) so consumers can
   *  lint inside it (event-payload field misuse) without re-lexing. */
  attrs: Array<{
    name: string;
    at: number;
    value?: string;
    valueAt?: number;
    form?: "str" | "expr" | "flag";
    exprAt?: number;
    exprEnd?: number;
  }>;
}

/** Sweep ONE markup span for OPEN tags and their attribute-name tokens — markup-lexis- and
 *  hole-aware like collectTagRefs, but purely textual: it works on spans whose TREE parse
 *  failed (the B2 masking bug — a single mismatched close tag silenced every unknown-tag/attr
 *  diagnostic). Close tags are skipped (0302 owns mismatches); attr VALUES are skipped whole
 *  (strings by markup lexis, `{ … }` holes by matching). */
export function sweepMarkupElements(
  bodyCp: readonly number[],
  spanStart: number,
  spanEnd: number,
  out: UetkxSweptElement[] = [],
  seedParent?: number,
): UetkxSweptElement[] {
  const isAttrChar = (c: number) => isIdentCp(c) || c === 46; /* . */
  // R12: open-element stack — indexes into `out` — so every element records its enclosing
  // element (parent-aware lints). Recursive directive-body calls append into the SAME array
  // (indices stay valid) seeded with the element enclosing the directive.
  const stack: number[] = [];
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
      if (c === 123 && isDirectiveBodyBrace(bodyCp, i, spanStart)) {
        for (const [rs, re] of directiveMarkupRanges(bodyCp, i, close)) {
          sweepMarkupElements(bodyCp, rs, re, out, stack.length ? stack[stack.length - 1] : seedParent);
        }
      }
      i = close + 1;
      continue;
    }
    if (c !== 60 /* < */) {
      i++;
      continue;
    }
    if (i + 1 < spanEnd && bodyCp[i + 1] === 47 /* / */) {
      // close tag — pop the enclosing element, skip to `>`
      if (stack.length) stack.pop();
      let k = i + 2;
      while (k < spanEnd && bodyCp[k] !== 62 /* > */) k++;
      i = k + 1;
      continue;
    }
    if (i + 1 >= spanEnd || !isIdentStartCp(bodyCp[i + 1])) {
      i++;
      continue;
    }
    const ts = i + 1;
    let k = ts;
    while (k < spanEnd && isIdentCp(bodyCp[k])) k++;
    const el: UetkxSweptElement = {
      tag: fromCodePoints(bodyCp, ts, k - ts),
      at: ts,
      parent: stack.length ? stack[stack.length - 1] : seedParent,
      attrs: [],
    };
    // attr scan until the tag bracket closes
    let selfClosed = false;
    while (k < spanEnd) {
      const nk = skipNoncodeMarkup(bodyCp, k);
      if (nk !== k) {
        k = nk;
        continue;
      }
      const ck = bodyCp[k];
      if (ck === 123 /* { */) {
        const close = findMatching(bodyCp, k);
        if (close === -1 || close >= spanEnd) break;
        k = close + 1;
        continue;
      }
      if (ck === 62 /* > */ || (ck === 47 /* / */ && k + 1 < spanEnd && bodyCp[k + 1] === 62)) {
        selfClosed = ck === 47;
        k += ck === 47 ? 2 : 1;
        break;
      }
      if (isIdentStartCp(ck)) {
        const as = k;
        while (k < spanEnd && isAttrChar(bodyCp[k])) k++;
        const attr: UetkxSweptElement["attrs"][number] = { name: fromCodePoints(bodyCp, as, k - as), at: as, form: "flag" };
        // R10/R11: capture STRING-literal values and the attr FORM so the schema sweep can
        // type-check values (enum vocabularies + numeric/margin formats) and reject string/
        // flag forms where an {expr} is required. Holes themselves stay opaque here.
        let p = k;
        while (p < spanEnd && (bodyCp[p] === 32 || bodyCp[p] === 9 || bodyCp[p] === 10 || bodyCp[p] === 13)) p++;
        if (p < spanEnd && bodyCp[p] === 61 /* = */) {
          p++;
          while (p < spanEnd && (bodyCp[p] === 32 || bodyCp[p] === 9 || bodyCp[p] === 10 || bodyCp[p] === 13)) p++;
          if (p < spanEnd && (bodyCp[p] === 34 /* " */ || bodyCp[p] === 39 /* ' */)) {
            attr.form = "str";
            const vEnd = skipString(bodyCp, p);
            if (vEnd <= spanEnd && vEnd > p + 1 && bodyCp[vEnd - 1] === bodyCp[p]) {
              attr.value = fromCodePoints(bodyCp, p + 1, vEnd - p - 2);
              attr.valueAt = p + 1;
              k = vEnd;
            }
          } else if (p < spanEnd && bodyCp[p] === 123 /* { */) {
            attr.form = "expr"; // the main loop's hole-skip consumes it
            const holeClose = findMatching(bodyCp, p);
            if (holeClose !== -1 && holeClose < spanEnd) {
              attr.exprAt = p + 1;
              attr.exprEnd = holeClose;
            }
          }
        }
        el.attrs.push(attr);
        continue;
      }
      k++;
    }
    out.push(el);
    if (!selfClosed) stack.push(out.length - 1); // open element — children record it as parent
    i = k;
  }
  return out;
}
