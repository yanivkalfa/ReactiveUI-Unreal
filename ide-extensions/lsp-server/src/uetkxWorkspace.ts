// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// Import intelligence for the .uetkx language server — the TS mirror of FUetkxFsResolver +
// FUetkxResolve::Apply (UetkxResolve.cpp). Resolves a specifier to a workspace file (`./` `../`
// relative, `~/` root-anchored, implicit `.uetkx`; engine-native + bare forbidden), reads a
// target's exported decls, indexes the workspace for FindExporter, and produces the same
// resolution diagnostics (2300/2301/2302/2308) the compiler emits — live, off the sidecar. Powers
// specifier/name completions, go-to-definition, and live import diagnostics in server.ts.

import * as fs from "node:fs";
import * as path from "node:path";
import { AUTO_INCLUDED_HEADERS, scanFile, UetkxDeclKind, UetkxFileScanResult } from "./uetkxFileScan";
import { collectFileReferences, UetkxFileRef } from "./uetkxIndex";
import { rootAnchorFor } from "./uetkxSchema";

export interface ExportedDecl {
  name: string;
  kind: UetkxDeclKind;
  exported: boolean;
  nameAt: number; // code-point offset of the name in its file (go-to-def target)
}

export interface ResolveDiag {
  code: string;
  severity: number;
  message: string;
  off: number; // code-point offset in the IMPORTER
  len: number;
}

// ── per-file parse cache (mtime-gated, like FUetkxFsResolver::CachedScan) ────────────────────

interface CacheEntry {
  mtimeMs: number;
  decls: ExportedDecl[];
  defaultExportName: string; // ES-modules (U-08): `export default <Name>` target ("" = none)
  scan: UetkxFileScanResult; // the full scan — imports/refs queries (TD-033 index)
  refs: UetkxFileRef[]; // the reference index (collectFileReferences)
  text: string; // the exact text scan/refs were produced from
}
const scanCache = new Map<string, CacheEntry>();

function normAbs(p: string): string {
  return path.resolve(p).replace(/\\/g, "/");
}

function buildEntry(source: string, key: string, mtimeMs: number): CacheEntry {
  // Signature-list mode: keep listing exported decls even if one body is mid-edit / malformed, so the
  // export list matches the C++ ScanPreamble resolver instead of truncating at the error (bughunt LSP-1).
  const scan = scanFile(source, path.basename(key, ".uetkx"), /*resyncOnBodyError*/ true);
  const decls: ExportedDecl[] = [];
  for (const c of scan.components) decls.push({ name: c.name, kind: "component", exported: c.exported, nameAt: c.nameAt });
  for (const h of scan.hooks) decls.push({ name: h.name, kind: "hook", exported: h.exported, nameAt: h.nameAt });
  for (const m of scan.modules) decls.push({ name: m.name, kind: "module", exported: m.exported, nameAt: m.nameAt });
  // ES-modules (M6): value/util decls join the export index (completions / go-to-def / fix-its).
  for (const v of scan.values) decls.push({ name: v.name, kind: "value", exported: v.exported, nameAt: v.nameAt });
  for (const u of scan.utils) decls.push({ name: u.name, kind: "util", exported: u.exported, nameAt: u.nameAt });
  return { mtimeMs, decls, defaultExportName: scan.defaultExportName, scan, refs: collectFileReferences(scan, source), text: source };
}

/** Live-document overlay (TD-033 N-04): rename/references must never read stale disk text for
 *  an OPEN dirty document — the server passes the current buffer contents here. Overlay entries
 *  are computed fresh (never cached — they change per keystroke). */
export type TextOverlay = ReadonlyMap<string, string>;

function cachedScan(fsPath: string, overlay?: TextOverlay): CacheEntry | null {
  const key = normAbs(fsPath);
  const live = overlay?.get(key);
  if (live !== undefined) {
    return buildEntry(live, key, -1);
  }
  let mtimeMs: number;
  try {
    mtimeMs = fs.statSync(key).mtimeMs;
  } catch {
    return null;
  }
  const hit = scanCache.get(key);
  if (hit && hit.mtimeMs === mtimeMs) return hit;
  let source: string;
  try {
    source = fs.readFileSync(key, "utf8");
  } catch {
    return null;
  }
  const entry = buildEntry(source, key, mtimeMs);
  scanCache.set(key, entry);
  return entry;
}

/** The full cached index entry for a file (scan + refs + exact text) — the TD-033 query base. */
export function getFileIndex(
  fsPath: string,
  overlay?: TextOverlay,
): { scan: UetkxFileScanResult; refs: UetkxFileRef[]; text: string } | null {
  const entry = cachedScan(fsPath, overlay);
  return entry ? { scan: entry.scan, refs: entry.refs, text: entry.text } : null;
}

/** All top-level declarations of a .uetkx file (name -> kind/exported/nameAt), mtime-cached.
 *  Returns null when the file is unreadable. */
export function getDecls(fsPath: string): ExportedDecl[] | null {
  return cachedScan(fsPath)?.decls ?? null;
}

/** ES-modules (U-08): the file's `export default <Name>` target ("" = none / unreadable) —
 *  mirrors IUetkxImportResolver::DefaultExportOf. */
export function defaultExportOf(fsPath: string): string {
  return cachedScan(fsPath)?.defaultExportName ?? "";
}

// ── module + workspace roots ─────────────────────────────────────────────────────────────────

/** The Unreal module root of a file: the nearest ancestor directory containing a `*.Build.cs`
 *  (mirrors FUetkxConfig::ModuleRootFor). null when none is found. */
export function moduleRootFor(fsPath: string): string | null {
  let dir = path.dirname(normAbs(fsPath));
  for (let depth = 0; depth < 40; depth++) {
    try {
      if (fs.readdirSync(dir).some((e) => e.endsWith(".Build.cs"))) return dir;
    } catch {
      break;
    }
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return null;
}

/** The workspace search root for the FindExporter index + specifier completions: the nearest
 *  ancestor holding a `.uproject` (the driver's sweep universe ≈ the project). Falls back to the
 *  module root, then the file's own directory. */
export function workspaceRootFor(fsPath: string): string {
  let dir = path.dirname(normAbs(fsPath));
  for (let depth = 0; depth < 40; depth++) {
    try {
      if (fs.readdirSync(dir).some((e) => e.endsWith(".uproject"))) return dir;
    } catch {
      break;
    }
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return moduleRootFor(fsPath) ?? path.dirname(normAbs(fsPath));
}

// ── specifier resolution (FUetkxFsResolver::Resolve) ─────────────────────────────────────────

/** Resolve an import specifier from an importer file to an absolute .uetkx path, or null.
 *  `./` `../` are importer-relative; `~/` anchors at the config `root` (or the module root when no
 *  config declares one); implicit `.uetkx`. Bare + engine-native (res://, Source/, Assets/) are
 *  forbidden (return null) — imports address workspace .uetkx targets only. */
export function resolveSpecifier(importerFsPath: string, specifier: string): string | null {
  const importerDir = path.dirname(normAbs(importerFsPath));
  let target: string;
  if (specifier.startsWith("./") || specifier.startsWith("../")) {
    target = path.resolve(importerDir, specifier);
  } else if (specifier.startsWith("~/")) {
    const anchor = rootAnchorFor(importerDir) ?? moduleRootFor(importerFsPath);
    if (!anchor) return null;
    target = path.resolve(anchor, specifier.slice(2));
  } else {
    return null;
  }
  if (!target.endsWith(".uetkx")) target += ".uetkx";
  target = normAbs(target);
  return fs.existsSync(target) ? target : null;
}

/** A POSIX import specifier from an importer to a target file (the 2305 fix-it shape): importer-
 *  relative, `./`-prefixed, `.uetkx` dropped. Mirrors FUetkxFsResolver::SuggestSpecifier. */
export function suggestSpecifier(importerFsPath: string, targetFsPath: string): string {
  const importerDir = path.dirname(normAbs(importerFsPath));
  let rel = path.relative(importerDir, normAbs(targetFsPath)).replace(/\\/g, "/");
  if (rel.endsWith(".uetkx")) rel = rel.slice(0, -".uetkx".length);
  if (!rel.startsWith("./") && !rel.startsWith("../")) rel = "./" + rel;
  return rel;
}

// ── workspace file index (FindExporter + specifier completion) ───────────────────────────────

/** Every .uetkx file under a root (recursive), excluding the D-22 contract fixtures (harness-only,
 *  matching the driver's export universe) and node_modules/Intermediate/Saved noise. */
export function listUetkxFiles(rootDir: string): string[] {
  const out: string[] = [];
  const skip = new Set(["node_modules", "Intermediate", "Saved", "Binaries", "DerivedDataCache", ".git"]);
  const walk = (dir: string, depth: number) => {
    if (depth > 32) return;
    let entries: fs.Dirent[];
    try {
      entries = fs.readdirSync(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const e of entries) {
      const full = path.join(dir, e.name);
      if (e.isDirectory()) {
        if (!skip.has(e.name)) walk(full, depth + 1);
      } else if (e.name.endsWith(".uetkx") && !normAbs(full).includes("/ContractFixtures/")) {
        out.push(normAbs(full));
      }
    }
  };
  walk(normAbs(rootDir), 0);
  return out;
}

// ── imported-surface emission (TB-10 follow-through: real declarations, not suppression) ─────

/** What an importer can EMIT into its virtual C++ TU for one exporter file: hook signatures
 *  (declaration-only — clangd then types `auto [A,B] = UseX(…)` correctly) and module bodies
 *  (verbatim namespaces — constants keep their real types). Components are omitted: they are
 *  markup-position-only (UETKX0114), so code never references them. */
export interface ImportedSurface {
  hooks: Array<{ name: string; ret: string; params: string }>;
  modules: Array<{ name: string; body: string }>;
  /** Component names — their generated wrappers are fully-defaulted (`FRuiNode Name();` is a
   *  valid call shape), and CODE references them (router tables: `RouterHome()`). */
  components: string[];
  /** ES-modules (M6): value exports — emitted as `inline const <T> Name = <Init>;` (typed) or
   *  `= <Init>` under auto (inferred) so clangd types bare references correctly. */
  values: Array<{ name: string; type: string; init: string }>;
  /** ES-modules (M6): util functions — declaration-only, like hooks minus Ctx. */
  utils: Array<{ name: string; retType: string; params: string }>;
  /** ES-modules (U-08): the exporter's `export default` target ("" = none) — a default import
   *  binds this symbol. */
  defaultExportName: string;
}

const surfaceCache = new Map<string, { mtimeMs: number; surface: ImportedSurface }>();

/** The emittable surface of the exporter behind `specifier`, mtime-cached. Null when the
 *  specifier doesn't resolve or the exporter can't be read (the caller degrades to the
 *  suppression fallback — never an error). */
export function importedSurfaceFor(importerFsPath: string, specifier: string): ImportedSurface | null {
  const key = resolveSpecifier(importerFsPath, specifier);
  if (!key) return null;
  let mtimeMs: number;
  try {
    mtimeMs = fs.statSync(key).mtimeMs;
  } catch {
    return null;
  }
  const hit = surfaceCache.get(key);
  if (hit && hit.mtimeMs === mtimeMs) return hit.surface;
  let source: string;
  try {
    source = fs.readFileSync(key, "utf8");
  } catch {
    return null;
  }
  const scan = scanFile(source, path.basename(key, ".uetkx"), /*resyncOnBodyError*/ true);
  const surface: ImportedSurface = {
    hooks: scan.hooks.map((h) => ({ name: h.name, ret: h.ret, params: h.params })),
    modules: scan.modules.map((m) => ({ name: m.name, body: m.body })),
    components: scan.components.map((c) => c.name),
    values: scan.values.map((v) => ({ name: v.name, type: v.type, init: v.init })),
    utils: scan.utils.map((u) => ({ name: u.name, retType: u.retType, params: u.params })),
    defaultExportName: scan.defaultExportName,
  };
  surfaceCache.set(key, { mtimeMs, surface });
  return surface;
}

// ── host-include header index (TB-9) ─────────────────────────────────────────────────────────

const headerIndexCache = new Map<string, { at: number; headers: string[] }>();

/** Every .h/.hpp under the workspace's Source/ + Plugins/ (normalized, forward slashes), cached
 *  30s per root. The LSP-side half of UETKX2316: the COMPILER can't see UBT include paths (why
 *  2316 was long "reserved"), but the workspace tree it CAN see — enough for go-to-definition
 *  and for catching a misspelled project-local header. */
export function workspaceHeaders(importerFsPath: string): string[] {
  const root = workspaceRootFor(importerFsPath);
  const hit = headerIndexCache.get(root);
  const now = Date.now();
  if (hit && now - hit.at < 30_000) return hit.headers;
  const skip = new Set(["node_modules", "Intermediate", "Saved", "Binaries", "DerivedDataCache", ".git"]);
  const out: string[] = [];
  const walk = (dir: string, depth: number) => {
    if (depth > 32) return;
    let entries: fs.Dirent[];
    try {
      entries = fs.readdirSync(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const e of entries) {
      const full = path.join(dir, e.name);
      if (e.isDirectory()) {
        if (!skip.has(e.name)) walk(full, depth + 1);
      } else if (e.name.endsWith(".h") || e.name.endsWith(".hpp")) {
        out.push(normAbs(full));
      }
    }
  };
  const scanRoots = [path.join(root, "Source"), path.join(root, "Plugins")].filter((p) => fs.existsSync(p));
  for (const r of scanRoots.length ? scanRoots : [root]) walk(normAbs(r), 0);
  headerIndexCache.set(root, { at: now, headers: out });
  return out;
}

/** Resolve an `import "@X.h"` payload to a workspace header by path-suffix match
 *  (`RuiDemoSupport.h`, `Doom/DoomTypes.h`), or null when the workspace doesn't have it. */
export function resolveHostInclude(importerFsPath: string, specifier: string): string | null {
  const suffix = "/" + specifier.replace(/\\/g, "/");
  for (const h of workspaceHeaders(importerFsPath)) {
    if (h.endsWith(suffix)) return h;
  }
  return null;
}

export interface ExporterHit {
  file: string;
  kind: UetkxDeclKind;
  nameAt: number;
}

/** The compiler's sweep universe under the workspace root: the .uetkx files under `Source/` + `Plugins/`
 *  (mirrors RUICompileCommandlet::DefaultRoots). The LSP indexes ONLY these so it never offers or
 *  navigates to an export the compiler never sweeps (bughunt LSP-2). Falls back to the whole workspace
 *  root when neither Source/ nor Plugins/ exists (a bare fixture tree). Sorted for a stable order. */
export function sweptUetkxFiles(importerFsPath: string): string[] {
  const root = workspaceRootFor(importerFsPath);
  const roots = ["Source", "Plugins"].map((r) => path.join(root, r)).filter((d) => fs.existsSync(d));
  if (roots.length === 0) return listUetkxFiles(root);
  const out: string[] = [];
  for (const r of roots) out.push(...listUetkxFiles(r));
  return out.sort();
}

/** The file that EXPORTS `name` under the importer's sweep universe (first exporter wins,
 *  mirroring FUetkxFsResolver::EnsureIndex / FindExporter). null when no file exports it. */
export function findExporter(name: string, importerFsPath: string): ExporterHit | null {
  for (const file of sweptUetkxFiles(importerFsPath)) {
    const decls = getDecls(file);
    if (!decls) continue;
    for (const d of decls) {
      if (d.exported && d.name === name) return { file, kind: d.kind, nameAt: d.nameAt };
    }
  }
  return null;
}

// ── live resolution diagnostics (FUetkxResolve::Apply, import-name portion) ───────────────────

/** The import-name resolution diagnostics the compiler emits (2300 unknown specifier, 2308 cross-
 *  module, 2302 not-declared, 2301 not-exported), computed live from the workspace — instant import
 *  feedback without a recompile. The usage-policing codes (2304 unused / 2305 missing / 2307
 *  unknown) stay with the hash-gated sidecar (they need the emitter's full reference set). */
export function resolveDiagnostics(scan: UetkxFileScanResult, importerFsPath: string): ResolveDiag[] {
  const diags: ResolveDiag[] = [];
  const importerModule = moduleRootFor(importerFsPath);
  for (const imp of scan.imports) {
    // Host includes (`import "@Header.h"`, INCLUDE_RETIREMENT_PLAN.md §B) resolve to no .uetkx
    // file — imp.specifier is a header path, not a peer-file specifier. They must never 2300
    // (the C++ resolver skips the same way, UetkxResolve.cpp) — but the LSP CAN verify a
    // project-local name against the workspace tree (TB-9 / UETKX2316, LSP-only): slash-less
    // specifiers are the project-local convention; slashed ones are usually engine paths the
    // workspace can't see, so they stay undiagnosed. Auto-included names already hint 2317.
    if (imp.hostInclude) {
      if (
        !imp.specifier.includes("/") &&
        !AUTO_INCLUDED_HEADERS.includes(imp.specifier) &&
        !resolveHostInclude(importerFsPath, imp.specifier)
      ) {
        diags.push({
          code: "UETKX2316",
          severity: 0, // error (owner call 2026-07-17): a slash-less name missing from the workspace WILL fail the C++ build
          message: `header \`${imp.specifier}\` not found under this workspace's Source/ or Plugins/ — check the name (engine headers can't be verified here)`,
          off: imp.specifierAt,
          len: Math.max(1, imp.specifier.length + 3),
        });
      }
      continue;
    }
    const key = resolveSpecifier(importerFsPath, imp.specifier);
    if (!key) {
      diags.push({
        code: "UETKX2300",
        severity: 0,
        message: `unknown import specifier \`${imp.specifier}\` — no file at ${imp.specifier}(.uetkx)`,
        off: imp.specifierAt,
        len: Math.max(1, imp.specifier.length + 2),
      });
      continue;
    }
    const label = workspaceRelLabel(importerFsPath, key);
    // 2308: imports are module-scoped. Only flag when BOTH sides have a resolvable module (matches
    // FUetkxFsResolver::CrossesModuleBoundary — an indeterminate side never false-positives).
    const keyModule = moduleRootFor(key);
    if (importerModule && keyModule && importerModule !== keyModule) {
      diags.push({
        code: "UETKX2308",
        severity: 0,
        message: `import crosses a module/root boundary (${label}) — imports are module-scoped in v1`,
        off: imp.at,
        len: 1,
      });
      continue;
    }
    // ES-modules (G-05): a namespace import validates its MEMBERS at use sites (the compiler's
    // sidecar carries those — they need the code walk); nothing to name-check at the import line.
    if (imp.isNamespace) {
      continue;
    }
    // ES-modules (U-08): a default import needs the target to HAVE a default export — live 2326.
    if (imp.isDefault) {
      if (!defaultExportOf(key)) {
        diags.push({
          code: "UETKX2326",
          severity: 0,
          message: `${label} has no default export — use a named import: import { ... } from "${imp.specifier}"`,
          off: imp.defaultAliasAt,
          len: Math.max(1, imp.defaultAlias.length),
        });
      }
      continue;
    }
    const decls = getDecls(key) ?? [];
    const byName = new Map(decls.map((d) => [d.name, d]));
    // Rename imports (`{ A as B }`) validate the TARGET name A — the local alias is the
    // importer's business (2325 is the scanner's, collisions-wise).
    for (let n = 0; n < imp.names.length; n++) {
      const name = imp.names[n];
      const nameAt = imp.nameAts[n];
      const t = byName.get(name);
      if (!t) {
        diags.push({ code: "UETKX2302", severity: 0, message: `\`${name}\` is not declared in ${label}`, off: nameAt, len: name.length });
      } else if (!t.exported) {
        diags.push({
          code: "UETKX2301",
          severity: 0,
          message: `\`${name}\` is not exported by ${label} — add \`export\` to its declaration`,
          off: nameAt,
          len: name.length,
        });
      }
    }
  }
  return diags;
}

/** A stable project-relative label for messages (mirrors LabelForKey — relative to the workspace
 *  root, forward slashes). */
export function workspaceRelLabel(importerFsPath: string, targetFsPath: string): string {
  const root = workspaceRootFor(importerFsPath);
  return path.relative(root, normAbs(targetFsPath)).replace(/\\/g, "/");
}

// ── import cursor classification (completions / go-to-def) ────────────────────────────────────

export type ImportCursor =
  | { kind: "import-name"; specifier: string | null; partial: string }
  | { kind: "import-specifier"; partial: string }
  | { kind: "import-keyword" };

/** Classify a cursor sitting inside a preamble `import { … } from "…"` STATEMENT, which may span
 *  several physical lines (a multi-line `{ … }` name list). Operates on UTF-16 string offsets (import
 *  lines + POSIX specifiers are ASCII in practice). null when not in an import name-list or specifier
 *  string. Walks back to the statement's `import` keyword rather than only inspecting the cursor's own
 *  physical line (bughunt LSP-3 — a cursor on a continuation line saw no `import` and bailed). */
export function importCursorAt(text: string, off: number): ImportCursor | null {
  // Find the enclosing statement's start: the nearest line-start `import` at/above the cursor. Bounded
  // to a small window (imports live in the preamble; a name list never spans dozens of lines).
  let start = -1;
  let lineStart = off;
  while (lineStart > 0 && text[lineStart - 1] !== "\n") lineStart--;
  for (let scanned = 0; scanned < 64; scanned++) {
    const nl = text.indexOf("\n", lineStart);
    const lineEnd = nl < 0 ? text.length : nl;
    if (/^\s*import\b/.test(text.slice(lineStart, lineEnd))) {
      start = lineStart;
      break;
    }
    if (lineStart === 0) break;
    let prev = lineStart - 1; // the preceding '\n'
    while (prev > 0 && text[prev - 1] !== "\n") prev--;
    lineStart = prev;
  }
  if (start < 0) return null;
  const before = text.slice(start, off);
  // Cursor right after `import` (only whitespace so far, nothing typed yet) — before either the
  // `{` name-list or the `"..."` specifier form. Offers a discoverable `import "@Header.h"`
  // snippet (INCLUDE_RETIREMENT_PLAN.md §B, mirrors Unity's `import "@Namespace"` discoverability).
  if (/^\s*import\s*$/.test(before)) {
    return { kind: "import-keyword" };
  }
  // inside the `from "…"` string? (unterminated quote before the cursor)
  if (/\bfrom\s*"[^"]*$/.test(before)) {
    const q = before.lastIndexOf('"');
    return { kind: "import-specifier", partial: before.slice(q + 1) };
  }
  // A COMPLETE `from "…"` before the cursor means the import statement is already closed; the cursor is
  // in later code, so a subsequent `{` (e.g. a `component App {` body brace) must NOT be read as an open
  // import name-list. Without this, <Tag> completion inside a component that follows an import dropped all
  // host elements (routed to the import-name branch). Multi-line imports still work: their `from` is after
  // the cursor, so this does not match while the name list is being typed.
  if (/\bfrom\s*"[^"]*"/.test(before)) {
    return null;
  }
  // inside the `{ … }` name list (open brace with no matching close before the cursor)?
  if (before.lastIndexOf("{") > before.lastIndexOf("}")) {
    // The specifier may be on a LATER line — search the whole statement, not just `before`.
    const stmt = text.slice(start, Math.min(text.length, off + 400));
    const specMatch = /from\s*"([^"]*)"/.exec(stmt);
    const partMatch = /([A-Za-z0-9_]*)$/.exec(before);
    return { kind: "import-name", specifier: specMatch ? specMatch[1] : null, partial: partMatch ? partMatch[1] : "" };
  }
  return null;
}

// ── symbol resolution + find-all-references (TD-033 N0) ─────────────────────────────────────

/** What the cursor is on, resolved to the SYMBOL it means (N-04 semantics):
 *  - `global`: a declaration — rename/references span the whole workspace.
 *  - `local-alias`: a local import binding (`B` of `A as B`, a `* as X` alias, a default
 *    alias) — rename/references stay within the importer file.
 *  - `import-binding`: the cursor is ON a PLAIN named binding token (target == local) — global
 *    target semantics for references; rename OFFERS the alias-insertion form (N1). */
export type UetkxSymbol =
  | { type: "global"; file: string; name: string }
  | { type: "local-alias"; file: string; importIndex: number; name: string }
  | { type: "import-binding"; file: string; importIndex: number; bindingIndex: number; targetFile: string; name: string };

/** A single reference location (code points into `file`'s CURRENT text). */
export interface UetkxRefLocation {
  file: string;
  start: number;
  len: number;
  kind: string; // UetkxRefKind — surfaced for tests/rename edit shaping
}

const refAt = (refs: UetkxFileRef[], cpOffset: number): UetkxFileRef | null => {
  for (const r of refs) {
    if (cpOffset >= r.start && cpOffset <= r.start + r.len) return r;
  }
  return null;
};

function normAbsPath(p: string): string {
  return path.resolve(p).replace(/\\/g, "/");
}

/** Path EQUALITY for reference/rename resolution — case-insensitive on win32 (audit: a
 *  mis-cased specifier like `./chip` for `Chip.uetkx` passes existsSync but a case-sensitive
 *  compare silently dropped the importer from the rename set → incomplete rename). */
function samePath(a: string, b: string): boolean {
  if (a === b) return true;
  return process.platform === "win32" && a.toLowerCase() === b.toLowerCase();
}

/** Resolve the symbol at a cursor offset (code points). Null = not on a referenceable token. */
export function resolveSymbolAt(fsPath: string, cpOffset: number, overlay?: TextOverlay): UetkxSymbol | null {
  const index = getFileIndex(fsPath, overlay);
  if (!index) return null;
  const { scan, refs } = index;
  const here = normAbsPath(fsPath);
  const hit = refAt(refs, cpOffset);
  if (!hit) return null;

  const resolveImportTarget = (importIndex: number): string | null => {
    const imp = scan.imports[importIndex];
    return imp ? resolveSpecifier(fsPath, imp.specifier) : null;
  };

  switch (hit.kind) {
    case "decl-name":
    case "export-list":
    case "export-default":
      return { type: "global", file: here, name: hit.name };
    case "import-target": {
      const imp = scan.imports[hit.importIndex!];
      const target = resolveImportTarget(hit.importIndex!);
      if (!target || !imp) return null;
      const n = imp.names.findIndex((nm, idx) => nm === hit.name && imp.nameAts[idx] === hit.start);
      const plain = n >= 0 && (imp.localNames[n] ?? imp.names[n]) === imp.names[n];
      if (plain) {
        return { type: "import-binding", file: here, importIndex: hit.importIndex!, bindingIndex: n, targetFile: normAbsPath(target), name: hit.name };
      }
      return { type: "global", file: normAbsPath(target), name: hit.name };
    }
    case "import-alias":
      return { type: "local-alias", file: here, importIndex: hit.importIndex!, name: hit.name };
    case "qual-member": {
      const target = resolveImportTarget(hit.importIndex!);
      return target ? { type: "global", file: normAbsPath(target), name: hit.name } : null;
    }
    case "tag":
    case "code": {
      const name = hit.name;
      // same-file declaration wins
      if (scan.components.some((d) => d.name === name) || scan.hooks.some((d) => d.name === name) ||
          scan.modules.some((d) => d.name === name) || scan.values.some((d) => d.name === name) ||
          scan.utils.some((d) => d.name === name)) {
        return { type: "global", file: here, name };
      }
      // an import binding: plain → the target's symbol; renamed/namespace/default → the local alias
      for (let idx = 0; idx < scan.imports.length; idx++) {
        const imp = scan.imports[idx];
        if (imp.hostInclude) continue;
        if (imp.isNamespace && imp.namespaceAlias === name) return { type: "local-alias", file: here, importIndex: idx, name };
        if (imp.isDefault && imp.defaultAlias === name) return { type: "local-alias", file: here, importIndex: idx, name };
        for (let n = 0; n < imp.names.length; n++) {
          const local = imp.localNames[n] ?? imp.names[n];
          if (local !== name) continue;
          if (local === imp.names[n]) {
            const target = resolveSpecifier(fsPath, imp.specifier);
            return target ? { type: "global", file: normAbsPath(target), name: imp.names[n] } : null;
          }
          return { type: "local-alias", file: here, importIndex: idx, name };
        }
      }
      // unimported but exported somewhere (the 2305 shape) — still navigable/renamable
      const exporter = findExporter(name, fsPath);
      return exporter ? { type: "global", file: normAbsPath(exporter.file), name } : null;
    }
  }
  return null;
}

/** Every reference to `symbol` across the sweep universe (N-03/N-04). Declaration tokens are
 *  included (the server filters on includeDeclaration). */
export function findReferencesTo(symbol: UetkxSymbol, originFsPath: string, overlay?: TextOverlay): UetkxRefLocation[] {
  const out: UetkxRefLocation[] = [];
  const push = (file: string, r: UetkxFileRef) => out.push({ file, start: r.start, len: r.len, kind: r.kind });

  if (symbol.type === "local-alias") {
    const index = getFileIndex(symbol.file, overlay);
    if (!index) return out;
    for (const r of index.refs) {
      if (r.name !== symbol.name) continue;
      if (r.kind === "import-alias" && r.importIndex === symbol.importIndex) push(symbol.file, r);
      // tags/code refs spelled with the alias belong to it (a valid file has no same-named
      // decl or second import — 2325 polices that)
      else if (r.kind === "tag" || r.kind === "code") push(symbol.file, r);
    }
    return out;
  }

  const targetFile = symbol.type === "import-binding" ? symbol.targetFile : symbol.file;
  const name = symbol.name;

  // the declaring file: decl token + export list/default + its own tags/code refs
  const declIndex = getFileIndex(targetFile, overlay);
  if (declIndex) {
    for (const r of declIndex.refs) {
      if (r.name !== name) continue;
      if (r.kind === "decl-name" || r.kind === "export-list" || r.kind === "export-default" || r.kind === "tag" || r.kind === "code") {
        push(targetFile, r);
      }
    }
  }

  // every other swept file: bindings whose import resolves to the declaring file
  for (const file of sweptUetkxFiles(originFsPath)) {
    const fileNorm = normAbsPath(file);
    if (samePath(fileNorm, targetFile)) continue;
    const index = getFileIndex(file, overlay);
    if (!index) continue;
    // which of this file's imports point at the declaring file?
    const pointing = new Map<number, "named-plain" | "named-renamed" | "namespace" | "default">();
    index.scan.imports.forEach((imp, idx) => {
      if (imp.hostInclude) return;
      const key = resolveSpecifier(file, imp.specifier);
      if (!key || !samePath(normAbsPath(key), targetFile)) return;
      if (imp.isNamespace) pointing.set(idx, "namespace");
      else if (imp.isDefault) pointing.set(idx, "default");
      else {
        const n = imp.names.indexOf(name);
        if (n >= 0) pointing.set(idx, (imp.localNames[n] ?? imp.names[n]) === imp.names[n] ? "named-plain" : "named-renamed");
      }
    });
    if (pointing.size === 0) continue;
    let plainlyBound = false;
    for (const kind of pointing.values()) if (kind === "named-plain") plainlyBound = true;
    for (const r of index.refs) {
      if (r.name !== name) continue;
      if (r.kind === "import-target" && r.importIndex !== undefined && pointing.has(r.importIndex)) push(file, r);
      else if (r.kind === "qual-member" && r.importIndex !== undefined && pointing.get(r.importIndex) === "namespace") push(file, r);
      else if ((r.kind === "tag" || r.kind === "code") && plainlyBound) push(file, r);
    }
  }
  return out;
}

// ── rename (TD-033 N1) ──────────────────────────────────────────────────────────────────────

export interface UetkxRenameEdit {
  file: string;
  start: number; // code points
  len: number;
  newText: string;
}

export type UetkxRenameResult = { edits: UetkxRenameEdit[] } | { error: string };

const IDENT_RE = /^[A-Za-z_][A-Za-z0-9_]*$/;

/** Does `file` already bind `name` at its top level (a declaration or any import binding)?
 *  The N-05 refusal check — never produce an edit the compiler immediately rejects (2325/2106
 *  shapes). */
function fileBindsName(file: string, name: string, overlay?: TextOverlay): boolean {
  const index = getFileIndex(file, overlay);
  if (!index) return false;
  const { scan } = index;
  if (
    scan.components.some((d) => d.name === name) || scan.hooks.some((d) => d.name === name) ||
    scan.modules.some((d) => d.name === name) || scan.values.some((d) => d.name === name) ||
    scan.utils.some((d) => d.name === name)
  ) {
    return true;
  }
  for (const imp of scan.imports) {
    if (imp.hostInclude) continue;
    if (imp.isNamespace && imp.namespaceAlias === name) return true;
    if (imp.isDefault && imp.defaultAlias === name) return true;
    for (let n = 0; n < imp.names.length; n++) {
      if ((imp.localNames[n] ?? imp.names[n]) === name) return true;
    }
  }
  return false;
}

/** Rename the symbol at a cursor offset (N-04 semantics, N-05 validation). `hostTags` = the
 *  markup element vocabulary (renaming a component TO a host-tag name would shadow it in tag
 *  position — refused). */
export function renameSymbolAt(
  fsPath: string,
  cpOffset: number,
  newName: string,
  hostTags: ReadonlySet<string>,
  overlay?: TextOverlay,
): UetkxRenameResult {
  if (!IDENT_RE.test(newName)) {
    return { error: `\`${newName}\` is not a valid identifier` };
  }
  const symbol = resolveSymbolAt(fsPath, cpOffset, overlay);
  if (!symbol) {
    return { error: "nothing renameable at this position" };
  }
  if (symbol.name === newName) {
    return { edits: [] };
  }
  const refs = findReferencesTo(symbol, fsPath, overlay);

  // N-05 validation over every file an edit would ACTUALLY touch (audit: an import-binding
  // rename edits only the importer — inserting `A as NewLocal` — so a name bound or exported
  // ELSEWHERE is perfectly legal there, exactly like a local alias).
  const touched =
    symbol.type === "import-binding" ? new Set<string>([symbol.file]) : new Set<string>(refs.map((r) => r.file));
  if (symbol.type === "global") touched.add(symbol.file);
  for (const file of touched) {
    if (fileBindsName(file, newName, overlay)) {
      return { error: `\`${newName}\` is already bound in ${path.basename(file)} — pick another name` };
    }
  }
  if (symbol.type === "global") {
    // 2106 shape: an EXPORTED name must stay globally unique.
    const exporter = findExporter(newName, fsPath);
    if (exporter) {
      return { error: `\`${newName}\` is already exported by ${path.basename(exporter.file)} (one exported name, one file)` };
    }
    // a component renamed to a host tag would shadow the vocabulary in tag position
    const declIndex = getFileIndex(symbol.file, overlay);
    const isComponent = declIndex?.scan.components.some((d) => d.name === symbol.name) ?? false;
    if (isComponent && hostTags.has(newName)) {
      return { error: `\`${newName}\` is a host element name — components cannot shadow the markup vocabulary` };
    }
  }

  // Edits. `import-binding` (cursor ON a plain binding token) is the ES rename-at-binding
  // behavior: keep the target, insert an alias, rename the importer's local refs only.
  if (symbol.type === "import-binding") {
    const index = getFileIndex(symbol.file, overlay);
    if (!index) return { error: "importer unreadable" };
    const imp = index.scan.imports[symbol.importIndex];
    const bindingAt = imp.nameAts[symbol.bindingIndex];
    const edits: UetkxRenameEdit[] = [
      { file: symbol.file, start: bindingAt, len: symbol.name.length, newText: `${symbol.name} as ${newName}` },
    ];
    for (const r of index.refs) {
      if ((r.kind === "tag" || r.kind === "code") && r.name === symbol.name) {
        edits.push({ file: symbol.file, start: r.start, len: r.len, newText: newName });
      }
    }
    return { edits };
  }

  const edits: UetkxRenameEdit[] = refs.map((r) => ({ file: r.file, start: r.start, len: r.len, newText: newName }));
  return { edits };
}
