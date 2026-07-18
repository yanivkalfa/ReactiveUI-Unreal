// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// Kind-accurate coloring of imported BINDING names (family parity, Unity 0.9.1 field find):
// the static TextMate grammar can only give every imported name one uniform tint — it cannot
// know what the name binds in the TARGET file. This pass resolves each import's target from
// the workspace and emits semantic tokens by the KIND of the export the binding binds:
// components color as types/classes (tag-shaped), hooks and utils as functions, values as
// variables, legacy modules and `* as X` aliases as namespaces (they are spelled `X::Member`
// at use sites), and a default alias as whatever the target's `export default` declares.
// Semantic tokens override the static grammar in every LSP client; an unresolvable target
// degrades silently (no token — the grammar tint stays).

import { scanFile, UetkxDeclKind } from "./uetkxFileScan";
import { defaultExportOf, getDecls, resolveSpecifier } from "./uetkxWorkspace";

/** The server's semantic-token legend (indexes into this array are the wire token types). */
export const SEMANTIC_TOKEN_TYPES = ["class", "function", "variable", "namespace"] as const;

const CLASS = 0;
const FUNCTION = 1;
const VARIABLE = 2;
const NAMESPACE = 3;

export interface ImportBindingToken {
  cpStart: number; // code-point offset of the binding token
  length: number;
  type: number; // index into SEMANTIC_TOKEN_TYPES
}

function typeForKind(kind: UetkxDeclKind): number {
  switch (kind) {
    case "component":
      return CLASS;
    case "hook":
    case "util":
      return FUNCTION;
    case "value":
      return VARIABLE;
    case "module":
      return NAMESPACE;
  }
}

/** Every import-binding token of `scan` with its kind-accurate token type, sorted by offset.
 *  COMBINED declarations (`import Def, { a } from` / `import Def, * as X from`) contribute
 *  every part they carry. Pure half — the server converts cp offsets to LSP positions. */
export function importBindingTokens(scan: ReturnType<typeof scanFile>, fsPath: string): ImportBindingToken[] {
  const out: ImportBindingToken[] = [];
  for (const imp of scan.imports) {
    if (imp.hostInclude) continue;
    const key = resolveSpecifier(fsPath, imp.specifier);
    const decls = key ? getDecls(key) : null;
    if (!decls) continue; // degrade silently — the grammar tint stays
    const kindOf = new Map<string, UetkxDeclKind>(decls.map((d) => [d.name, d.kind]));
    for (let n = 0; n < imp.names.length; n++) {
      const kind = kindOf.get(imp.names[n]);
      if (kind === undefined) continue; // not declared there — 2302's business, not coloring's
      const type = typeForKind(kind);
      out.push({ cpStart: imp.nameAts[n], length: imp.names[n].length, type });
      const local = imp.localNames[n] ?? imp.names[n];
      if (local !== imp.names[n]) {
        out.push({ cpStart: imp.localNameAts[n], length: local.length, type });
      }
    }
    if (imp.isNamespace) {
      out.push({ cpStart: imp.namespaceAliasAt, length: imp.namespaceAlias.length, type: NAMESPACE });
    }
    if (imp.isDefault) {
      const def = key ? defaultExportOf(key) : "";
      const kind = def ? kindOf.get(def) : undefined;
      out.push({
        cpStart: imp.defaultAliasAt,
        length: imp.defaultAlias.length,
        type: kind !== undefined ? typeForKind(kind) : VARIABLE,
      });
    }
  }
  return out.sort((a, b) => a.cpStart - b.cpStart);
}
