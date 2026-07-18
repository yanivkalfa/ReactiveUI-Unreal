// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TD-033 N3 (LSP_COMPLETION_PLAN): the PURE halves of the code-action fixes — testable without
// an LSP connection. server.ts converts the cp spans these return into TextEdits. The 2320
// wrapper→plain rewrite MUST stay byte-identical to the codemod's per-decl forms
// (RUIMigrateImportsCommandlet pass 3) — pinned by actions.test.ts.

import { scanFile } from "./uetkxFileScan";
/** The first declaration's true start (export prefix included) — the import-insertion point. */
export function firstDeclStartCp(scan: ReturnType<typeof scanFile>): number {
  let min = Number.MAX_SAFE_INTEGER;
  const consider = (at: number, exportAt: number) => {
    const start = exportAt >= 0 ? exportAt : at;
    if (start >= 0 && start < min) min = start;
  };
  for (const d of scan.components) consider(d.at, d.exportAt);
  for (const d of scan.hooks) consider(d.at, d.exportAt);
  for (const d of scan.modules) consider(d.at, d.exportAt);
  for (const d of scan.values) consider(d.at, d.exportAt);
  for (const d of scan.utils) consider(d.at, d.exportAt);
  return min === Number.MAX_SAFE_INTEGER ? 0 : min;
}

/** A declaration's true start (for the 2301 `export `-insertion) by name; -1 when absent. */
export function declStartCpOf(scan: ReturnType<typeof scanFile>, name: string): number {
  const pick = (at: number, exportAt: number) => (exportAt >= 0 ? exportAt : at);
  for (const d of scan.components) if (d.name === name) return pick(d.at, d.exportAt);
  for (const d of scan.hooks) if (d.name === name) return pick(d.at, d.exportAt);
  for (const d of scan.modules) if (d.name === name) return pick(d.at, d.exportAt);
  for (const d of scan.values) if (d.name === name) return pick(d.at, d.exportAt);
  for (const d of scan.utils) if (d.name === name) return pick(d.at, d.exportAt);
  return -1;
}

/** The cp span to delete for an unused-import fix at `diagCp` (the local binding token):
 *  the binding (with its comma) — or the WHOLE import line when it is the only binding /
 *  a namespace / a default import. */
export function unusedImportRemoval(scan: ReturnType<typeof scanFile>, text: string, diagCp: number): { start: number; end: number } | null {
  const lineSpanAt = (cp: number): { start: number; end: number } => {
    const cps = [...text].map((c) => c.codePointAt(0)!);
    let s = cp;
    while (s > 0 && cps[s - 1] !== 10) s--;
    let e = cp;
    while (e < cps.length && cps[e] !== 10) e++;
    if (e < cps.length) e++;
    return { start: s, end: e };
  };
  for (const imp of scan.imports) {
    if (imp.hostInclude) continue;
    if (imp.isNamespace && diagCp >= imp.namespaceAliasAt && diagCp <= imp.namespaceAliasAt + imp.namespaceAlias.length) {
      return lineSpanAt(imp.at);
    }
    if (imp.isDefault && diagCp >= imp.defaultAliasAt && diagCp <= imp.defaultAliasAt + imp.defaultAlias.length) {
      return lineSpanAt(imp.at);
    }
    for (let n = 0; n < imp.names.length; n++) {
      const localAt = imp.localNameAts[n] ?? imp.nameAts[n];
      const local = imp.localNames[n] ?? imp.names[n];
      const onName = diagCp >= imp.nameAts[n] && diagCp <= imp.nameAts[n] + imp.names[n].length;
      const onLocal = diagCp >= localAt && diagCp <= localAt + local.length;
      if (!onName && !onLocal) continue;
      if (imp.names.length === 1) return lineSpanAt(imp.at);
      // remove `Name[ as Local]` plus one adjacent comma
      const startCp = imp.nameAts[n];
      const endCp = localAt + local.length;
      const cps = [...text].map((c) => c.codePointAt(0)!);
      let s = startCp;
      let e = endCp;
      // prefer trailing `, ` (keeps `{ A, B }` shapes tidy), else a leading `, `
      let t = e;
      while (t < cps.length && (cps[t] === 32 || cps[t] === 9)) t++;
      if (t < cps.length && cps[t] === 44) {
        e = t + 1;
        while (e < cps.length && cps[e] === 32) e++;
      } else {
        let h = s - 1;
        while (h >= 0 && (cps[h] === 32 || cps[h] === 9)) h--;
        if (h >= 0 && cps[h] === 44) s = h;
      }
      return { start: s, end: e };
    }
  }
  return null;
}

/** The codemod's per-declaration wrapper→plain rewrite for the decl containing `cp` —
 *  components and hooks only (modules hoist with cross-file ripple; `-run=RUIMigrateEsModules`
 *  owns those). The output shape MUST stay identical to the codemod's (test-pinned). */
export function wrapperRewriteAt(scan: ReturnType<typeof scanFile>, cp: number): { start: number; end: number; text: string } | null {
  for (const d of scan.components) {
    if (!d.legacySyntax) continue;
    const start = d.at;
    if (cp < start || cp > d.bodyAt) continue;
    const parts = d.params.map((p) => {
      let part = p.type ? `${p.type} ${p.name}` : p.name;
      if (p.default) part += ` = ${p.default}`;
      return part;
    });
    return { start, end: d.bodyAt - 1, text: `FRuiNode ${d.name}(${parts.join(", ")}) ` };
  }
  for (const d of scan.hooks) {
    if (!d.legacySyntax) continue;
    const start = d.at;
    if (cp < start || cp > d.bodyAt) continue;
    const ret = d.ret.trim() || "void";
    return { start, end: d.bodyAt - 1, text: `${ret} ${d.name}(${d.params.trim()}) ` };
  }
  return null;
}

