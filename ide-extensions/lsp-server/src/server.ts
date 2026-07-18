// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// The .uetkx language server (stdio) — the markup-intelligence BASELINE, fully offline:
// completions/hover from the compiler-exported schema, live parse diagnostics from the
// TS front-end (corpus-locked to the C++ compiler), full compiler diagnostics from the
// hash-gated sidecar, document formatting via the golden-corpus formatter. Embedded-C++
// intelligence (clangd proxy over a virtual document) is the planned enhancement layer —
// markup-only is the family's fully-supported degradation baseline.

import {
  createConnection,
  ProposedFeatures,
  TextDocuments,
  TextDocumentSyncKind,
  CodeActionKind,
  CompletionItemKind,
  InsertTextFormat,
  DiagnosticSeverity,
  DiagnosticTag,
  SemanticTokensBuilder,
  SymbolKind,
  type CodeAction,
  type CompletionItem,
  type CompletionList,
  type Definition,
  type Diagnostic,
  type DocumentSymbol,
  type Hover,
  type InitializeParams,
  type Location,
  type SymbolInformation,
  type TextEdit,
  type WorkspaceEdit,
} from "vscode-languageserver/node";
import { TextDocument } from "vscode-languageserver-textdocument";
import { URI } from "./uri";
import * as fs from "node:fs";
import * as path from "node:path";
import { codePointToUtf16, toCodePoints, utf16ToCodePoint } from "./codePoints";
import { findMarkupRanges } from "./jsxScan";
import { sweepMarkupElements } from "./uetkxIndex";
import { classifyCursor, DIRECTIVES } from "./context";
import { enclosingAttrName, fieldForKind, PAYLOAD_FIELDS } from "./eventPayload";
import { readSidecarDiags } from "./diagsSidecar";
import { formatUetkx, UetkxFormatOptions, DEFAULT_FORMAT_OPTIONS } from "./formatUetkx";
import { scanFile } from "./uetkxFileScan";
import { declStartCpOf, firstDeclStartCp, unusedImportRemoval, wrapperRewriteAt } from "./uetkxActions";
import { importBindingTokens, SEMANTIC_TOKEN_TYPES } from "./semanticTokens";
import { loadFormatterConfig, schemaForFile, UetkxSchema } from "./uetkxSchema";
import {
  defaultExportOf,
  findExporter,
  findReferencesTo,
  getDecls,
  getFileIndex,
  importCursorAt,
  renameSymbolAt,
  resolveDiagnostics,
  resolveHostInclude,
  resolveSpecifier,
  resolveSymbolAt,
  suggestSpecifier,
  sweptUetkxFiles,
  workspaceRootFor,
} from "./uetkxWorkspace";
import { ClangdProxy, findClangd, type ClangdPublishedDiagnostics } from "./clangdProxy";
import {
  buildEmbeddedView,
  embeddedPositionRequest,
  isEmbeddedOffset,
  realUriOfVirtual,
  translateEmbeddedCompletion,
  translateEmbeddedRename,
  virtualUriOf,
} from "./embeddedIntel";

const IDENT_RE = /^[A-Za-z_][A-Za-z0-9_]*$/;

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

// ── §1 crash-hardening: the server must NEVER die silently ─────────────────────────────────
// The jsonrpc layer guards request/notification handlers, but OUR event-emitter callbacks and
// floating promises are outside it — in Node, one uncaught exception or unhandled rejection
// kills the process and every feature goes dark with zero feedback (the owner's
// zero-diagnostics session). Last-resort traps: log to the client's output channel, keep
// serving. Individual boundaries (clangd pump, syncEmbeddedDoc) carry their own guards so
// these traps are telemetry of a bug, not load-bearing control flow.
function logServerError(context: string, error: unknown): void {
  const detail = error instanceof Error ? (error.stack ?? error.message) : String(error);
  try {
    connection.console.error(`[uetkx-server] ${context}: ${detail}`);
  } catch {
    /* the connection itself is gone — nothing left to tell */
  }
}
process.on("uncaughtException", (e) => logServerError("uncaughtException", e));
process.on("unhandledRejection", (e) => logServerError("unhandledRejection", e));

// ── embedded-C++ intelligence: one clangd proxy per server, started lazily on the first request
//    that lands inside a setup/hook/module body. start() is idempotent and resolves false when
//    clangd is absent — every embedded path then returns null and falls back to the markup baseline
//    (the family's documented degradation), so the server never hard-depends on clangd.
//    TB-10: the binary is DISCOVERED (setting → PATH → managed install → LLVM/VS locations), the
//    degradation is ANNOUNCED once instead of silent, and clangd's diagnostics for the virtual
//    docs flow back into the real documents (mapped through the source map).
let clangd: ClangdProxy | null = null;
let clangdPathSetting: string | undefined;
let warnedNoClangd = false;
async function embeddedProxy(doc: TextDocument): Promise<ClangdProxy | null> {
  if (!clangd) {
    const found = findClangd(clangdPathSetting);
    if (!found) {
      if (!warnedNoClangd) {
        warnedNoClangd = true;
        connection.window.showWarningMessage(
          "UETKX: embedded C++ intelligence is OFF — clangd was not found. Set `uetkx.clangd.path`, " +
            "or install clangd (PATH, %LOCALAPPDATA%\\uetkx\\clangd\\bin, C:\\Program Files\\LLVM, or the " +
            "VS 2022 'C++ Clang tools' component). Markup intelligence is unaffected.",
        );
      }
      return null;
    }
    const root = workspaceRootFor(fsPathOf(doc)) ?? path.dirname(fsPathOf(doc));
    clangd = new ClangdProxy(found, root);
    clangd.onPublishDiagnostics = (params) => publishEmbeddedDiagnostics(params);
    clangd.onError = (context, error) => logServerError(context, error);
  }
  await clangd.start();
  return clangd.isAvailable() ? clangd : null;
}

connection.onInitialize((params: InitializeParams) => {
  const opts = (params.initializationOptions ?? {}) as { clangdPath?: string };
  clangdPathSetting = typeof opts.clangdPath === "string" ? opts.clangdPath : undefined;
  return {
    capabilities: {
      textDocumentSync: TextDocumentSyncKind.Incremental,
      completionProvider: { triggerCharacters: ["<", "@", ".", " ", "{", '"', "/"] },
      hoverProvider: true,
      definitionProvider: true,
      documentFormattingProvider: true,
      // TD-033 (LSP_COMPLETION_PLAN): the workspace-index features.
      referencesProvider: true,
      renameProvider: { prepareProvider: true },
      documentSymbolProvider: true,
      workspaceSymbolProvider: true,
      codeActionProvider: { codeActionKinds: [CodeActionKind.QuickFix] },
      // Kind-accurate coloring of imported binding names (Unity 0.9.1 field-find parity):
      // the tokens are import-line-only; everything else stays with the TextMate grammar.
      semanticTokensProvider: {
        legend: { tokenTypes: [...SEMANTIC_TOKEN_TYPES], tokenModifiers: [] },
        full: true,
        range: false,
      },
    },
  };
});

// ── semantic tokens: imported bindings colored by the KIND of the export they bind ─────────

connection.languages.semanticTokens.on((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return { data: [] };
  const text = doc.getText();
  const fsPath = fsPathOf(doc);
  const scan = scanFile(text, path.basename(fsPath, ".uetkx"), true);
  const builder = new SemanticTokensBuilder();
  for (const t of importBindingTokens(scan, fsPath)) {
    const pos = doc.positionAt(codePointToUtf16(text, t.cpStart));
    builder.push(pos.line, pos.character, t.length, t.type, 0);
  }
  return builder.build();
});

/** The live-document overlay (TD-033 N-04): index queries for OPEN documents must read the
 *  current buffer, never stale disk text. Keys are normalized fs paths (the index's key space). */
function liveOverlay(): Map<string, string> {
  const overlay = new Map<string, string>();
  for (const doc of documents.all()) {
    overlay.set(path.resolve(fsPathOf(doc)).replace(/\\/g, "/"), doc.getText());
  }
  return overlay;
}

function fsPathOf(doc: TextDocument): string {
  return URI.toFsPath(doc.uri);
}

function schemaOf(doc: TextDocument): UetkxSchema {
  return schemaForFile(path.dirname(fsPathOf(doc)));
}

// ── diagnostics: live parse + hash-gated sidecar + forwarded clangd (TB-10) ────────────────

/** Last published diagnostics per REAL doc uri, split by producer — published merged so the
 *  markup pass and the async clangd pass never clobber each other. */
const markupDiagsByUri = new Map<string, Diagnostic[]>();
const embeddedDiagsByUri = new Map<string, Diagnostic[]>();
/** The virtual-doc VERSION the stored embedded diagnostics were computed for (N6): the rename
 *  guard must never gate on stale diagnostics, so it waits until this catches the version it
 *  just synced. */
const embeddedDiagsVersionByUri = new Map<string, number>();
const embeddedDiagsWaiters: Array<() => void> = [];

/** Resolve when the stored embedded diagnostics for `uri` reach `minVersion`; false on timeout
 *  (clangd still parsing — first parse of an engine-includes TU can take >10s). */
function waitForEmbeddedDiags(uri: string, minVersion: number, timeoutMs: number): Promise<boolean> {
  if ((embeddedDiagsVersionByUri.get(uri) ?? -1) >= minVersion) return Promise.resolve(true);
  return new Promise<boolean>((resolve) => {
    const timer = setTimeout(() => {
      const i = embeddedDiagsWaiters.indexOf(check);
      if (i >= 0) embeddedDiagsWaiters.splice(i, 1);
      resolve(false);
    }, timeoutMs);
    const check = () => {
      if ((embeddedDiagsVersionByUri.get(uri) ?? -1) >= minVersion) {
        clearTimeout(timer);
        const i = embeddedDiagsWaiters.indexOf(check);
        if (i >= 0) embeddedDiagsWaiters.splice(i, 1);
        resolve(true);
      }
    };
    embeddedDiagsWaiters.push(check);
  });
}

function publishMerged(uri: string): void {
  const merged = [...(markupDiagsByUri.get(uri) ?? []), ...(embeddedDiagsByUri.get(uri) ?? [])];
  connection.sendDiagnostics({ uri, diagnostics: merged });
}

/** clangd's diagnostics for a virtual doc, mapped back into its .uetkx (TB-10): ranges are
 *  translated through the source map; anything landing in the prelude/scaffolding (missing
 *  headers, wrapper braces) is DROPPED — those are the virtual doc's problems, not the
 *  user's. Tags (Unnecessary/Deprecated) pass through, so clangd-detected dead code fades. */
function publishEmbeddedDiagnostics(params: ClangdPublishedDiagnostics): void {
  const realUri = realUriOfVirtual(params.uri);
  if (!realUri) return;
  // clangd RE-SERIALIZES URIs (drive-letter case, `%3A` vs `:`), so a raw string lookup misses
  // VS Code's own spelling and every diagnostic silently vanishes (the "I mangled the whole
  // body and nothing squiggled" bug, round 2 — bughunt LSP-5's lesson applied here too).
  let doc = documents.get(realUri);
  if (!doc) {
    for (const d of documents.all()) {
      if (URI.sameUri(d.uri, realUri)) {
        doc = d;
        break;
      }
    }
  }
  if (!doc) return;
  const text = doc.getText();
  const view = buildEmbeddedView(text, fsPathOf(doc));
  // Names imported FROM OTHER .uetkx FILES exist only after aggregation — the virtual TU can't
  // see them, so clangd's "undeclared identifier" for an imported name is OUR gap, not the
  // user's typo. Drop those; a genuinely misspelled import already gets UETKX2305/2300.
  const importedNames = new Set<string>();
  for (const imp of scanFile(text, path.basename(fsPathOf(doc), ".uetkx")).imports) {
    for (const n of imp.names ?? []) importedNames.add(n);
  }
  const undeclaredRe = /undeclared identifier '([A-Za-z_][A-Za-z0-9_]*)'/;
  const mapped: Diagnostic[] = [];
  for (const d of params.diagnostics) {
    const startOff = view.sourceOffsetOf(d.range.start);
    const endOff = view.sourceOffsetOf(d.range.end);
    if (startOff === null || endOff === null || endOff < startOff) continue; // prelude/scaffold
    const undeclared = undeclaredRe.exec(d.message);
    if (undeclared && importedNames.has(undeclared[1])) continue; // cross-file import, not a typo
    mapped.push({
      range: { start: doc.positionAt(startOff), end: doc.positionAt(Math.max(endOff, startOff + 1)) },
      severity: (d.severity ?? 1) as DiagnosticSeverity,
      code: d.code,
      source: "clangd",
      message: d.message,
      ...(d.tags && d.tags.length ? { tags: d.tags as Diagnostic["tags"] } : {}),
    });
  }
  embeddedDiagsByUri.set(doc.uri, mapped); // keyed by the CLIENT's uri spelling, not clangd's
  if (typeof params.version === "number") {
    embeddedDiagsVersionByUri.set(doc.uri, params.version);
  } else {
    // A clangd that omits `version`: advance to whatever the proxy last sent — arrival still
    // means "diagnostics for the current text" in the fully-serialized single-doc flow.
    embeddedDiagsVersionByUri.set(doc.uri, clangd?.versionOf(params.uri) ?? 0);
  }
  for (const w of [...embeddedDiagsWaiters]) w();
  publishMerged(doc.uri);
}

/** Push the current virtual doc to clangd so its diagnostics track EDITS, not just position
 *  requests (the "I mangled the whole body and nothing squiggled" fix). Fire-and-forget; a
 *  markup-only doc (no embedded regions) is never opened. */
async function syncEmbeddedDoc(doc: TextDocument): Promise<void> {
  const text = doc.getText();
  const view = buildEmbeddedView(text, fsPathOf(doc));
  if (view.regionCount === 0) return;
  const proxy = await embeddedProxy(doc);
  if (proxy) proxy.didOpen(virtualUriOf(doc.uri), view.virtualText);
}

function validate(doc: TextDocument): void {
  const text = doc.getText();
  const diags: Diagnostic[] = [];
  const push = (offCp: number, lenCp: number, severity: number, code: string, message: string) => {
    const start = doc.positionAt(codePointToUtf16(text, Math.max(offCp, 0)));
    const end = doc.positionAt(codePointToUtf16(text, Math.max(offCp, 0) + Math.max(lenCp, 1)));
    diags.push({
      range: { start, end },
      severity: severity === 0 ? DiagnosticSeverity.Error : severity === 1 ? DiagnosticSeverity.Warning : DiagnosticSeverity.Hint,
      code,
      source: "uetkx",
      message,
      // UETKX0107 (TB-5): the Unnecessary tag makes editors FADE the range — the family's
      // dead-code presentation (Unity styles its uitkxUnreachable semantic token the same way).
      ...(code === "UETKX0107" ? { tags: [DiagnosticTag.Unnecessary] } : {}),
    });
  };
  const scan = scanFile(text, path.basename(fsPathOf(doc), ".uetkx"));
  const seen = new Set<string>();
  for (const d of scan.diags) {
    seen.add(`${d.code}@${d.offset}:${d.length}`);
    push(d.offset, d.length, d.severity, d.code, d.message);
  }
  // B2 (F5 field test): live tag/attr validation against the schema, PARSE-ERROR RESILIENT —
  // previously unknown tags/attrs only ever surfaced via the hash-gated compiler sidecar, so a
  // dirty mangled buffer showed nothing but the first parse error. The sweep is textual
  // (markup-lexis + hole-aware), so a broken tree cannot mask it; `seen` dedupes against the
  // resolver's 2307 on clean parses.
  {
    const schema = schemaOf(doc);
    const validTags = new Set<string>(Object.keys(schema.elements));
    for (const c of scan.components) validTags.add(c.name);
    for (const imp of scan.imports) {
      if (imp.hostInclude) continue;
      for (const local of imp.localNames) validTags.add(local);
      if (imp.isDefault && imp.defaultAlias) validTags.add(imp.defaultAlias);
    }
    const universal = new Set<string>(["key", "classes", "Ref", ...schema.styleKeys, ...schema.slotKeys]);
    const sweepSpans = (bodyCp: readonly number[], baseAt: number, merged: Array<[number, number]>) => {
      for (const [s, e] of merged) {
        for (const el of sweepMarkupElements(bodyCp, s, e)) {
          const tagOff = baseAt + el.at;
          if (!validTags.has(el.tag)) {
            if (!findExporter(el.tag, fsPathOf(doc))) {
              const key = `UETKX2307@${tagOff}:${el.tag.length}`;
              if (!seen.has(key)) {
                seen.add(key);
                push(tagOff, el.tag.length, 0, "UETKX2307", `\`${el.tag}\` is used like a uetkx component/hook but no file exports it`);
              }
            }
            continue; // a component tag — its attrs are that component's params, not schema keys
          }
          const schemaEl = schema.elements[el.tag];
          if (!schemaEl) continue;
          for (const a of el.attrs) {
            if (a.name in schemaEl.attrs || universal.has(a.name)) continue;
            const attrOff = baseAt + a.at;
            const key = `UETKX0105@${attrOff}:${a.name.length}`;
            if (!seen.has(key)) {
              seen.add(key);
              push(attrOff, a.name.length, 0, "UETKX0105", `unknown attribute '${a.name}' on <${el.tag}>`);
            }
          }
        }
      }
    };
    if (scan.components.length === 0 && scan.diags.some((d) => d.severity === 0)) {
      // a BROKEN parse drops the component records entirely — sweep the whole file's markup
      // ranges instead, so the typos that broke the parse still surface (the B2 masking bug)
      const fileCp = toCodePoints(text);
      const spans: Array<[number, number]> = [];
      for (const r of findMarkupRanges(fileCp, 0, fileCp.length)) {
        spans.push([r.start, r.end === -1 ? fileCp.length : r.end]);
      }
      sweepSpans(fileCp, 0, spans);
    }
    for (const comp of scan.components) {
      const bodyCp = toCodePoints(comp.body);
      const spans: Array<[number, number]> = [];
      for (const sp of comp.returns ?? []) {
        if (sp.mStart >= 0 && sp.mEnd >= 0) spans.push([sp.mStart, sp.mEnd]);
      }
      for (const r of findMarkupRanges(bodyCp, 0, bodyCp.length)) {
        spans.push([r.start, r.end === -1 ? bodyCp.length : r.end]);
      }
      spans.sort((a, b) => a[0] - b[0]);
      const merged: Array<[number, number]> = [];
      for (const s of spans) {
        const last = merged[merged.length - 1];
        if (last && s[0] <= last[1]) last[1] = Math.max(last[1], s[1]);
        else merged.push([s[0], s[1]]);
      }
      sweepSpans(bodyCp, comp.bodyAt, merged);
    }
  }
  if (!scan.diags.some((d) => d.severity === 0)) {
    // clean parse: live import resolution (2300/2301/2302/2308) off the workspace, then the
    // compiler's full hash-gated verdict for the rest — de-duped by code+range so a code the live
    // pass already produced does not double when the sidecar is fresh.
    for (const d of resolveDiagnostics(scan, fsPathOf(doc))) {
      const key = `${d.code}@${d.off}:${d.len}`;
      if (!seen.has(key)) {
        seen.add(key);
        push(d.off, d.len, d.severity, d.code, d.message);
      }
    }
    for (const d of readSidecarDiags(fsPathOf(doc), text)) {
      if (!seen.has(`${d.code}@${d.off}:${d.len}`)) push(d.off, d.len, d.severity, d.code, d.message);
    }
  }
  markupDiagsByUri.set(doc.uri, diags);
  publishMerged(doc.uri);
  // Async clangd pass republishes merged when its diagnostics arrive — DEBOUNCED (B7):
  // per-keystroke didChange forced a TU rebuild per character, so the rebuild queue lagged
  // seconds behind the buffer. Position requests are unaffected — embeddedPositionRequest
  // syncs the CURRENT text itself before every query (hash-deduped).
  scheduleEmbeddedSync(doc);
}

const embedSyncTimers = new Map<string, ReturnType<typeof setTimeout>>();
function scheduleEmbeddedSync(doc: TextDocument): void {
  const prior = embedSyncTimers.get(doc.uri);
  if (prior) clearTimeout(prior);
  embedSyncTimers.set(
    doc.uri,
    setTimeout(() => {
      embedSyncTimers.delete(doc.uri);
      // The catch is load-bearing (§1): a floating rejection is a PROCESS KILL in Node.
      syncEmbeddedDoc(doc).catch((e) => logServerError("syncEmbeddedDoc", e));
    }, 300),
  );
}

documents.onDidChangeContent((change) => {
  try {
    validate(change.document);
  } catch (e) {
    logServerError("validate", e); // markup diagnostics silently missing = this bug class
  }
});
documents.onDidClose((e) => {
  const timer = embedSyncTimers.get(e.document.uri);
  if (timer) {
    clearTimeout(timer);
    embedSyncTimers.delete(e.document.uri);
  }
  markupDiagsByUri.delete(e.document.uri);
  embeddedDiagsByUri.delete(e.document.uri);
  embeddedDiagsVersionByUri.delete(e.document.uri);
  clangd?.didClose(virtualUriOf(e.document.uri));
  connection.sendDiagnostics({ uri: e.document.uri, diagnostics: [] });
});

// ── completions ────────────────────────────────────────────────────────────────────────────

connection.onCompletion(async (params): Promise<CompletionItem[] | CompletionList> => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return [];
  const text = doc.getText();

  // TD-016 typed event payload FIRST — `Value.<field>` inside an event expression completes the
  // FRuiValue field with the enclosing event's payload field on top. This is MARKUP-domain
  // knowledge and must beat the embedded-C++ forward: the event expr is a lifted clangd region,
  // and clangd's generic member list (or, headerless, its identifier fallback) would otherwise
  // preempt the payload-first ordering (pre-existing smoke flake, fixed in ES-modules M6).
  const offEarly = doc.offsetAt(params.position);
  const beforeEarly = text.slice(Math.max(0, offEarly - 48), offEarly);
  if (/\bValue\.[A-Za-z0-9_]*$/.test(beforeEarly)) {
    const schemaEarly = schemaOf(doc);
    const attr = enclosingAttrName(text, offEarly);
    const want = fieldForKind(attr && schemaEarly.eventPayloads ? schemaEarly.eventPayloads[attr] : undefined);
    const items: CompletionItem[] = [];
    if (want) {
      items.push({ label: want.field, kind: CompletionItemKind.Field, detail: `${want.type} — ${attr} payload`, sortText: "0" });
    }
    for (const f of PAYLOAD_FIELDS) {
      if (want && f.field === want.field) continue;
      items.push({ label: f.field, kind: CompletionItemKind.Field, detail: f.type, sortText: "1" + f.field });
    }
    return items;
  }

  // Embedded C++ (the TD-020 tail — hover/definition shipped before completion): inside a
  // setup/hook/module body, clangd's completions (locals, engine symbols, members) beat the
  // markup baseline. textEdit ranges come back in VIRTUAL coordinates and are translated;
  // null (not embedded / clangd absent / nothing to offer) falls through to the baseline paths.
  const embeddedOffset = doc.offsetAt(params.position);
  if (isEmbeddedOffset(text, embeddedOffset)) {
    const proxy = await embeddedProxy(doc);
    if (proxy) {
      const result = await embeddedPositionRequest(proxy, "textDocument/completion", doc.uri, text, embeddedOffset);
      const translated = translateEmbeddedCompletion(result, text, fsPathOf(doc));
      if (translated) return translated as CompletionList;
    }
  }

  // Import intelligence takes precedence in the preamble: a `import { … } from "…"` cursor
  // completes exported NAMES of the resolved target, or workspace-relative SPECIFIER paths.
  const imp = importCursorAt(text, doc.offsetAt(params.position));
  if (imp) {
    const fsPath = fsPathOf(doc);
    if (imp.kind === "import-keyword") {
      // Discoverability nudge for the host-include form (INCLUDE_RETIREMENT_PLAN.md §B,
      // mirrors Unity's `import "@Namespace"` snippet) — snippet-expands to quotes with the
      // cursor placed between them, ready for a header path.
      return [
        {
          label: '"@Header.h"',
          kind: CompletionItemKind.Snippet,
          insertText: '"@$1"',
          insertTextFormat: InsertTextFormat.Snippet,
          detail: "host C++ #include — a nameless side-effect import",
        },
      ];
    }
    if (imp.kind === "import-specifier") {
      const importerDir = path.dirname(fsPath);
      const items: CompletionItem[] = [];
      for (const file of sweptUetkxFiles(fsPath)) {
        if (path.resolve(file) === path.resolve(fsPath)) continue; // never import yourself
        const decls = (getDecls(file) ?? []).filter((d) => d.exported);
        if (decls.length === 0) continue;
        const spec = suggestSpecifier(fsPath, file);
        items.push({
          label: spec,
          kind: CompletionItemKind.File,
          detail: decls.map((d) => d.name).join(", "),
          insertText: spec,
          documentation: path.relative(importerDir, file).replace(/\\/g, "/"),
        });
      }
      return items;
    }
    // import-name: exported decls of the resolved specifier (or, if the specifier is not yet
    // typed/resolvable, every exported name in the workspace so the author can pick then fix up).
    const key = imp.specifier ? resolveSpecifier(fsPath, imp.specifier) : null;
    const seen = new Set<string>();
    // Never re-suggest a binding the statement already carries (Unity 0.9.1 field find): the
    // names/aliases already listed in the braces (minus the token being typed), and — for the
    // COMBINED form — the default alias plus the member the default already binds.
    for (const listed of imp.listed) if (listed !== imp.partial) seen.add(listed);
    if (imp.defaultAlias) {
      seen.add(imp.defaultAlias);
      const def = key ? defaultExportOf(key) : "";
      if (def) seen.add(def);
    }
    const items: CompletionItem[] = [];
    const add = (name: string, kind: string) => {
      if (seen.has(name)) return;
      seen.add(name);
      const itemKind =
        kind === "hook" || kind === "util" ? CompletionItemKind.Function
        : kind === "module" ? CompletionItemKind.Module
        : kind === "value" ? CompletionItemKind.Variable
        : CompletionItemKind.Class;
      items.push({ label: name, kind: itemKind, detail: kind });
    };
    if (key) {
      for (const d of getDecls(key) ?? []) if (d.exported) add(d.name, d.kind);
    } else {
      for (const file of sweptUetkxFiles(fsPath)) {
        if (path.resolve(file) === path.resolve(fsPath)) continue;
        for (const d of getDecls(file) ?? []) if (d.exported) add(d.name, d.kind);
      }
    }
    return items;
  }

  const schema = schemaOf(doc);

  // (TD-016 `Value.` payload completion runs FIRST, before the embedded-C++ forward — see top.)

  const cp = utf16ToCodePoint(text, doc.offsetAt(params.position));
  const ctx = classifyCursor(text, cp);
  if (ctx.kind === "tag") {
    const items: CompletionItem[] = Object.keys(schema.elements).map((tag) => ({
      label: tag,
      kind: CompletionItemKind.Class,
      detail: schema.elements[tag].factory,
    }));
    // Import intelligence: a component declared in THIS file or imported here is renderable as a
    // `<Tag>` too — fold those in after the host elements (host names win a collision).
    const fsPath = fsPathOf(doc);
    const scan = scanFile(text, path.basename(fsPath, ".uetkx"));
    const seen = new Set<string>(Object.keys(schema.elements));
    const addComp = (name: string, detail: string) => {
      if (seen.has(name)) return;
      seen.add(name);
      items.push({ label: name, kind: CompletionItemKind.Class, detail });
    };
    for (const c of scan.components) addComp(c.name, "component (this file)");
    for (const imp of scan.imports) {
      // A namespace part offers no tag names (a `X::` qual is not a tag), but an ES COMBINED
      // import (`import Def, { A } from` / `import Def, * as X from`) still offers its
      // default/named parts — never skip the whole declaration on one part's shape.
      if (imp.hostInclude) continue;
      const key = resolveSpecifier(fsPath, imp.specifier);
      const decls = key ? getDecls(key) : null;
      // ES-modules (U-08): a default import's LOCAL alias is renderable when the target's default
      // symbol is (or is presumed) a component.
      if (imp.isDefault) {
        const def = key ? defaultExportOf(key) : "";
        const d = def ? decls?.find((x) => x.name === def) : undefined;
        if (!d || d.kind === "component") addComp(imp.defaultAlias, "component (default import)");
      }
      for (let n = 0; n < imp.names.length; n++) {
        const nm = imp.names[n];
        const local = imp.localNames[n] ?? nm;
        // A resolved import: offer it only when the TARGET is (or is presumed) a component; the
        // author renders the LOCAL alias (`<Badge/>` for `{ Chip as Badge }`). Unresolved (decls
        // null) → offer it, the author knows their intent.
        const d = decls?.find((x) => x.name === nm);
        if (!d || d.kind === "component") addComp(local, local === nm ? "component (imported)" : `component (imported as ${nm})`);
      }
    }
    return items;
  }
  if (ctx.kind === "attr") {
    const items: CompletionItem[] = [];
    const el = schema.elements[ctx.tag];
    if (el) {
      for (const [attr, type] of Object.entries(el.attrs)) {
        items.push({ label: attr, kind: CompletionItemKind.Property, detail: type });
      }
    }
    for (const key of schema.styleKeys) items.push({ label: key, kind: CompletionItemKind.Color, detail: "style" });
    for (const key of schema.slotKeys) items.push({ label: key, kind: CompletionItemKind.Unit, detail: "slot" });
    items.push({ label: "key", kind: CompletionItemKind.Keyword, detail: "reconciler identity" });
    items.push({ label: "classes", kind: CompletionItemKind.Keyword, detail: "style classes" });
    items.push({ label: "Ref", kind: CompletionItemKind.Keyword, detail: "host-handle capture (expr)" });
    return items;
  }
  if (ctx.kind === "directive") {
    return DIRECTIVES.map((d) => ({ label: d, kind: CompletionItemKind.Keyword, detail: "@" + d }));
  }
  return schema.hooks.map((hook) => ({ label: hook, kind: CompletionItemKind.Function, detail: "hook" }));
});

// ── hover ──────────────────────────────────────────────────────────────────────────────────

connection.onHover(async (params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;
  const text = doc.getText();
  const offset = doc.offsetAt(params.position);

  // Embedded C++ takes precedence when the cursor is inside a setup/hook/module body: forward to
  // clangd over the virtual document. A null (not embedded, clangd absent, or no hover) falls
  // through to the markup baseline below.
  if (isEmbeddedOffset(text, offset)) {
    const proxy = await embeddedProxy(doc);
    if (proxy) {
      const hover = await embeddedPositionRequest(proxy, "textDocument/hover", doc.uri, text, offset);
      if (hover) return translateEmbeddedHover(hover as Hover, doc, text);
    }
  }

  // ── markup hover (TB-11): everything the schema row knows, context-aware ──────────────────
  let start = offset;
  let end = offset;
  const isWord = (c: string) => /[A-Za-z0-9_.]/.test(c);
  while (start > 0 && isWord(text[start - 1])) start--;
  while (end < text.length && isWord(text[end])) end++;
  const word = text.slice(start, end);
  if (!word) return null;
  const schema = schemaOf(doc);
  const md = (value: string) => ({ contents: { kind: "markdown" as const, value } });
  const KIND_DOC: Record<string, string> = {
    bool: "`bool` — a bare attribute means true, or `{ expr }`",
    color: '`FLinearColor` — `"R,G,B,A"` literal or `{ FLinearColor(…) }`',
    event: "event handler — `{ statements }` run when it fires",
    expr: "C++ expression — `{ … }`",
    float: "`float` — number literal or `{ expr }`",
    int: "`int32` — number literal or `{ expr }`",
    margin: '`FMargin` — `"All"`, `"H,V"`, `"L,T,R,B"` or `{ FMargin(…) }`',
    name: "`FName` — string literal or `{ expr }`",
    text: "`FText`/`FString` — string literal or `{ expr }`",
    vector2: "`FVector2D` — `{ FVector2D(X, Y) }`",
  };

  // 1. An element tag: class, container/leaf, engine gate, full attribute surface.
  const el = schema.elements[word];
  if (el) {
    const entries = Object.entries(el.attrs);
    const events = entries.filter(([, t]) => t === "event").map(([a]) => a);
    const props = entries.filter(([, t]) => t !== "event");
    const slate = el.factory.startsWith("RUI::Slate::") ? ` — \`S${word}\`` : "";
    const lines = [
      `**\`<${word}>\`**${slate} · ${el.children ? "container" : "leaf"}${el.sinceUE ? ` · **requires UE ${el.sinceUE}+**` : ""}`,
      "",
      `Factory \`${el.factory}\`. Attribute names are 1:1 the Unreal setter/argument names (D-33) — look up \`S${word}\` in the engine source for exact semantics.`,
    ];
    if (props.length) lines.push("", props.map(([a, t]) => `\`${a}\` *${t}*`).join(" · "));
    if (events.length) lines.push("", "**Events:** " + events.map((e) => `\`${e}\``).join(" · "));
    return md(lines.join("\n"));
  }

  // 2. Context-aware ATTRIBUTE hover — the enclosing tag names the exact schema row.
  const cursorCtx = classifyCursor(text, utf16ToCodePoint(text, offset));
  if (cursorCtx.kind === "attr" && schema.elements[cursorCtx.tag]) {
    const kind = schema.elements[cursorCtx.tag].attrs[word];
    if (kind !== undefined) {
      const lines = [
        `**\`${word}\`** — ${KIND_DOC[kind] ?? `\`${kind}\``}`,
        "",
        `On \`<${cursorCtx.tag}>\`: this IS the \`S${cursorCtx.tag}\` setter/argument/delegate name (D-33 loyalty — no aliases).`,
      ];
      if (kind === "event" && schema.eventPayloads && schema.eventPayloads[word] !== undefined) {
        const f = fieldForKind(schema.eventPayloads[word]);
        lines.push("", f ? `Payload: \`Value.${f.field}\` (\`${f.type}\`).` : "Zero-payload event — ignore `Value`.");
      }
      return md(lines.join("\n"));
    }
  }

  // 3. Slot keys — set on the CHILD, applied to the parent panel's slot.
  if (schema.slotKeys.includes(word)) {
    return md(
      `**\`${word}\`** — slot key: written on a CHILD, applied to the slot its PARENT panel allocates (in Slate, padding/alignment/fill live on the slot, not the widget). Removing it resets the slot to its default.`,
    );
  }

  if (schema.hooks.includes(word)) {
    return md(
      `**\`${word}\`** — ReactiveUI hook (a member of \`FRuiContext\`; markup calls it bare and the compiler prefixes \`Ctx.\`). Hook rules apply: unconditional call order, setup-only.`,
    );
  }
  if (schema.styleKeys.includes(word)) {
    return md(
      `**\`${word}\`** — style key (host-applied setter, 1:1 the Unreal property name). Unlike plain props, REMOVING a style key resets the widget to its default — family semantics.`,
    );
  }
  if (word === "key") {
    return md(
      "**`key`** — reconciler identity among siblings: keyed children MOVE instead of unmount/remount, so their state survives reorder. Value: string literal or `{ expr }` (FName/int).",
    );
  }
  if (word === "classes") {
    return md(
      "**`classes`** — style-class list resolved through the registered style registry (`RUI::Slate::RegisterStyleClass`); later classes win on conflicts.",
    );
  }
  if (word === "Ref") {
    return {
      contents: {
        kind: "markdown" as const,
        value:
          "**Ref** — universal reserved prop: `Ref={ expr }` captures the mounted widget's host handle (React ref lifecycle — called on attach, cleared on detach). Pass a `TFunction<void(const FRuiHostHandle&)>` or a `RUI::Slate::UseFocus` handle's `.Ref`.",
      },
    };
  }
  // TD-016: an event attribute — show the payload its handler's `Value` carries.
  if (schema.eventPayloads && schema.eventPayloads[word] !== undefined) {
    const f = fieldForKind(schema.eventPayloads[word]);
    const payload = f ? `the payload arrives as \`Value.${f.field}\` (\`${f.type}\`)` : "zero-payload — ignore `Value`";
    return { contents: { kind: "markdown" as const, value: `**${word}** — event; ${payload}` } };
  }
  return null;
});

// ── go-to-definition (imports + workspace-exported references) ───────────────────────────────

connection.onDefinition(async (params): Promise<Definition | null> => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;
  const markup = markupDefinition(doc, params);
  if (markup) return markup;

  // Fallback: a cursor inside embedded C++ that the markup index could not resolve — forward to
  // clangd. Locations clangd returns INTO the virtual doc are translated back to .uetkx; real
  // engine-header locations pass through. Null (not embedded / clangd absent / no result) is fine.
  const text = doc.getText();
  const offset = doc.offsetAt(params.position);
  if (isEmbeddedOffset(text, offset)) {
    const proxy = await embeddedProxy(doc);
    if (proxy) {
      const result = await embeddedPositionRequest(proxy, "textDocument/definition", doc.uri, text, offset);
      const translated = translateEmbeddedDefinition(result, doc, text);
      if (translated) return translated;
    }
  }
  return null;
});

/** Translate an embedded (clangd) hover: its `range` is in VIRTUAL-doc coordinates, so map it back to
 *  the .uetkx source range before returning (bughunt LSP-2 — an untranslated range highlights the wrong
 *  span, or lands out of bounds). A range in the prelude scaffolding is dropped (keep the contents). */
function translateEmbeddedHover(hover: Hover, doc: TextDocument, source: string): Hover {
  const h = hover as Hover & {
    range?: { start: { line: number; character: number }; end: { line: number; character: number } };
  };
  if (!h.range) return hover;
  const view = buildEmbeddedView(source, fsPathOf(doc));
  const startOff = view.sourceOffsetOf(h.range.start);
  const endOff = view.sourceOffsetOf(h.range.end);
  if (startOff === null || endOff === null) {
    return { contents: h.contents };
  }
  return { contents: h.contents, range: { start: doc.positionAt(startOff), end: doc.positionAt(endOff) } };
}

/** Translate a clangd definition result: locations pointing back into THIS document's virtual C++
 *  are mapped to .uetkx ranges; locations in real files (engine headers) pass through unchanged;
 *  anything landing in the prelude scaffolding is dropped. */
function translateEmbeddedDefinition(result: unknown, doc: TextDocument, source: string): Definition | null {
  if (!result) return null;
  const virtualUri = virtualUriOf(doc.uri);
  const view = buildEmbeddedView(source, fsPathOf(doc));
  const locs = (Array.isArray(result) ? result : [result]) as Location[];
  const out: Location[] = [];
  for (const loc of locs) {
    if (!loc || typeof loc.uri !== "string") continue;
    if (!URI.sameUri(loc.uri, virtualUri)) {
      out.push(loc); // a real file (engine header) — pass through
      continue;
    }
    const startOff = view.sourceOffsetOf(loc.range.start);
    const endOff = view.sourceOffsetOf(loc.range.end);
    if (startOff === null || endOff === null) continue; // in the prelude — not addressable in .uetkx
    out.push({
      uri: doc.uri,
      range: { start: doc.positionAt(startOff), end: doc.positionAt(endOff) },
    });
  }
  if (out.length === 0) return null;
  return out.length === 1 ? out[0] : out;
}

function markupDefinition(doc: TextDocument, params: { position: { line: number; character: number } }): Definition | null {
  const text = doc.getText();
  const fsPath = fsPathOf(doc);
  const cpOff = utf16ToCodePoint(text, doc.offsetAt(params.position));
  const scan = scanFile(text, path.basename(fsPath, ".uetkx"));

  // A location at a name inside a target file, given the name's code-point offset there.
  const locAtDecl = (targetFsPath: string, nameAt: number, nameLen: number): Definition | null => {
    let src: string;
    try {
      src = fs.readFileSync(targetFsPath, "utf8");
    } catch {
      return null;
    }
    const startU16 = codePointToUtf16(src, nameAt);
    const tdoc = TextDocument.create(URI.fromFsPath(targetFsPath), "uetkx", 0, src);
    return { uri: tdoc.uri, range: { start: tdoc.positionAt(startU16), end: tdoc.positionAt(startU16 + nameLen) } };
  };

  // 1. Cursor on an import: a NAME jumps to its export in the target; the SPECIFIER opens the file.
  for (const impDecl of scan.imports) {
    // Host includes (`import "@Header.h"`) have no names, but the SPECIFIER can jump to the
    // header itself when the workspace has it (TB-9 — engine paths simply return null).
    if (impDecl.hostInclude) {
      if (cpOff >= impDecl.specifierAt && cpOff <= impDecl.specifierAt + impDecl.specifier.length + 3) {
        const header = resolveHostInclude(fsPath, impDecl.specifier);
        if (header) {
          const zero = { line: 0, character: 0 };
          return { uri: URI.fromFsPath(header), range: { start: zero, end: zero } };
        }
      }
      continue;
    }
    for (let n = 0; n < impDecl.names.length; n++) {
      const at = impDecl.nameAts[n];
      if (cpOff >= at && cpOff <= at + impDecl.names[n].length) {
        const key = resolveSpecifier(fsPath, impDecl.specifier);
        if (!key) return null;
        const d = (getDecls(key) ?? []).find((x) => x.name === impDecl.names[n]);
        return d ? locAtDecl(key, d.nameAt, d.name.length) : null;
      }
    }
    // ES-modules (G-05): the `* as X` / default aliases jump to their target — the namespace
    // alias opens the file; a default alias jumps to the target's default-export declaration.
    if (impDecl.isNamespace && cpOff >= impDecl.namespaceAliasAt && cpOff <= impDecl.namespaceAliasAt + impDecl.namespaceAlias.length) {
      const key = resolveSpecifier(fsPath, impDecl.specifier);
      if (!key) return null;
      const zero = { line: 0, character: 0 };
      return { uri: URI.fromFsPath(key), range: { start: zero, end: zero } };
    }
    if (impDecl.isDefault && cpOff >= impDecl.defaultAliasAt && cpOff <= impDecl.defaultAliasAt + impDecl.defaultAlias.length) {
      const key = resolveSpecifier(fsPath, impDecl.specifier);
      if (!key) return null;
      const def = defaultExportOf(key);
      const d = def ? (getDecls(key) ?? []).find((x) => x.name === def) : undefined;
      if (d) return locAtDecl(key, d.nameAt, d.name.length);
      const zero = { line: 0, character: 0 };
      return { uri: URI.fromFsPath(key), range: { start: zero, end: zero } };
    }
    const sq = impDecl.specifierAt; // opening quote; specifier text runs (sq, sq+len]
    if (cpOff > sq && cpOff <= sq + impDecl.specifier.length + 1) {
      const key = resolveSpecifier(fsPath, impDecl.specifier);
      if (!key) return null;
      const tdoc = TextDocument.create(URI.fromFsPath(key), "uetkx", 0, "");
      return { uri: tdoc.uri, range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } } };
    }
  }

  // 2. Cursor on a bare identifier (markup tag / hook / module ref): jump to its workspace exporter.
  let start = cpOff;
  let end = cpOff;
  const srcCp = [...text].map((c) => c.codePointAt(0)!);
  const isIdent = (c: number) => c === 95 || (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122);
  while (start > 0 && isIdent(srcCp[start - 1])) start--;
  while (end < srcCp.length && isIdent(srcCp[end])) end++;
  if (end <= start) return null;
  const word = String.fromCodePoint(...srcCp.slice(start, end));
  // same-file decl first (ES-modules: values/utils join the set)
  for (const c of scan.components) if (c.name === word) return locAtDecl(fsPath, c.nameAt, word.length);
  for (const h of scan.hooks) if (h.name === word) return locAtDecl(fsPath, h.nameAt, word.length);
  for (const m of scan.modules) if (m.name === word) return locAtDecl(fsPath, m.nameAt, word.length);
  for (const v of scan.values) if (v.name === word) return locAtDecl(fsPath, v.nameAt, word.length);
  for (const u of scan.utils) if (u.name === word) return locAtDecl(fsPath, u.nameAt, word.length);
  // ES-modules (G-05): a LOCAL alias resolves THROUGH the import — `<Badge/>` (Chip as Badge),
  // a default alias, or `X` of `* as X` all land on their target, not a same-named exporter.
  for (const impDecl of scan.imports) {
    if (impDecl.hostInclude) continue;
    if (impDecl.isNamespace && impDecl.namespaceAlias === word) {
      const key = resolveSpecifier(fsPath, impDecl.specifier);
      if (!key) return null;
      const zero = { line: 0, character: 0 };
      return { uri: URI.fromFsPath(key), range: { start: zero, end: zero } };
    }
    if (impDecl.isDefault && impDecl.defaultAlias === word) {
      const key = resolveSpecifier(fsPath, impDecl.specifier);
      if (!key) return null;
      const def = defaultExportOf(key);
      const d = def ? (getDecls(key) ?? []).find((x) => x.name === def) : undefined;
      if (d) return locAtDecl(key, d.nameAt, d.name.length);
      const zero = { line: 0, character: 0 };
      return { uri: URI.fromFsPath(key), range: { start: zero, end: zero } };
    }
    for (let n = 0; n < impDecl.names.length; n++) {
      const local = impDecl.localNames[n] ?? impDecl.names[n];
      if (local === word) {
        const key = resolveSpecifier(fsPath, impDecl.specifier);
        if (!key) return null;
        const d = (getDecls(key) ?? []).find((x) => x.name === impDecl.names[n]);
        return d ? locAtDecl(key, d.nameAt, d.name.length) : null;
      }
    }
  }
  const hit = findExporter(word, fsPath);
  return hit ? locAtDecl(hit.file, hit.nameAt, word.length) : null;
}


// ── TD-033: references / rename / symbols / code actions (LSP_COMPLETION_PLAN N0–N3) ────────

/** A cp-offset range in a file → an LSP Location (reads the file's CURRENT text — overlay for
 *  open docs, disk otherwise — for the UTF-16 conversion). */
function locationOfCp(file: string, startCp: number, lenCp: number, overlay: Map<string, string>): Location | null {
  const key = path.resolve(file).replace(/\\/g, "/");
  let text = overlay.get(key);
  if (text === undefined) {
    try {
      text = fs.readFileSync(key, "utf8");
    } catch {
      return null;
    }
  }
  const startU16 = codePointToUtf16(text, startCp);
  const endU16 = codePointToUtf16(text, startCp + lenCp);
  const tdoc = TextDocument.create(URI.fromFsPath(key), "uetkx", 0, text);
  return { uri: tdoc.uri, range: { start: tdoc.positionAt(startU16), end: tdoc.positionAt(endU16) } };
}

connection.onReferences((params): Location[] => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return [];
  const overlay = liveOverlay();
  const cpOff = utf16ToCodePoint(doc.getText(), doc.offsetAt(params.position));
  const symbol = resolveSymbolAt(fsPathOf(doc), cpOff, overlay);
  if (!symbol) return [];
  const refs = findReferencesTo(symbol, fsPathOf(doc), overlay);
  const out: Location[] = [];
  for (const r of refs) {
    if (!params.context.includeDeclaration && r.kind === "decl-name") continue;
    const loc = locationOfCp(r.file, r.start, r.len, overlay);
    if (loc) out.push(loc);
  }
  return out;
});

/** The C++/uetkx identifier at a utf16 offset — the prepare-rename range fallback for embedded
 *  locals (clangd's prepareRename may answer `defaultBehavior`). Null when not on an identifier. */
function identRangeAt(text: string, offset: number): { start: number; end: number } | null {
  const isIdent = (ch: string) => /[A-Za-z0-9_]/.test(ch);
  let s = offset;
  while (s > 0 && isIdent(text[s - 1])) s--;
  let e = offset;
  while (e < text.length && isIdent(text[e])) e++;
  if (s === e || /[0-9]/.test(text[s])) return null;
  return { start: s, end: e };
}

connection.onPrepareRename(async (params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;
  const overlay = liveOverlay();
  const text = doc.getText();
  const u16Off = doc.offsetAt(params.position);
  const cpOff = utf16ToCodePoint(text, u16Off);
  const symbol = resolveSymbolAt(fsPathOf(doc), cpOff, overlay);
  if (symbol) {
    // The token under the cursor (its own file's index has the exact range).
    const index = getFileIndex(fsPathOf(doc), overlay);
    if (!index) return null;
    const hit = index.refs.find((r) => cpOff >= r.start && cpOff <= r.start + r.len);
    if (!hit) return null;
    const startU16 = codePointToUtf16(text, hit.start);
    const endU16 = codePointToUtf16(text, hit.start + hit.len);
    return { range: { start: doc.positionAt(startU16), end: doc.positionAt(endU16) }, placeholder: hit.name };
  }
  // N6: not a .uetkx symbol — an embedded-C++ LOCAL is still renamable through clangd. Offer the
  // identifier range under the cursor; the rename request itself does the real (refusable) work.
  if (isEmbeddedOffset(text, u16Off)) {
    const proxy = await embeddedProxy(doc);
    if (proxy) {
      const r = identRangeAt(text, u16Off);
      if (r) {
        return {
          range: { start: doc.positionAt(r.start), end: doc.positionAt(r.end) },
          placeholder: text.slice(r.start, r.end),
        };
      }
    }
  }
  return null;
});

connection.onRenameRequest(async (params): Promise<WorkspaceEdit | null> => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;
  const overlay = liveOverlay();
  const text = doc.getText();
  const u16Off = doc.offsetAt(params.position);
  const cpOff = utf16ToCodePoint(text, u16Off);
  const symbol = resolveSymbolAt(fsPathOf(doc), cpOff, overlay);
  if (!symbol) {
    // N6 (N-10): a cursor on embedded C++ that is NOT a .uetkx symbol — a body local, a member,
    // an engine name — forwards to clangd over the virtual doc. The translation is all-or-
    // nothing: any edit outside this file or inside generated glue refuses the whole rename.
    if (!isEmbeddedOffset(text, u16Off)) {
      connection.window.showWarningMessage("Rename refused: not a renamable symbol (imports, declarations, tags, aliases, or embedded C++ locals)");
      return null;
    }
    if (!IDENT_RE.test(params.newName)) {
      connection.window.showWarningMessage(`Rename refused: \`${params.newName}\` is not a valid identifier`);
      return null;
    }
    const proxy = await embeddedProxy(doc);
    if (!proxy) {
      connection.window.showWarningMessage("Rename refused: embedded C++ rename needs clangd (set `uetkx.clangd.path`)");
      return null;
    }
    const view = buildEmbeddedView(text, fsPathOf(doc));
    const virtualPosition = view.positionInVirtual(u16Off);
    if (virtualPosition === null) return null;
    const vuri = virtualUriOf(doc.uri);
    proxy.didOpen(vuri, view.virtualText); // no-op when clangd already holds this text
    // Partial-AST guard (N6 bughunt): clang's recovery DROPS statements it cannot type (an
    // unresolved engine type without a compile database), and clangd then renames only the
    // occurrences still in the AST — a silently PARTIAL rename. Two layers, evaluated against
    // diagnostics for the CURRENT version (waited for — stale diags must not gate):
    //   1. the enclosing embedded block carries clangd ERROR diagnostics → refuse;
    //   2. clangd's edits inside the block cover fewer word-boundary occurrences of the old
    //      name than the block's text holds → refuse (catches statements dropped past clang's
    //      error limit, where no mapped diagnostic survives). A same-name SHADOW in one block
    //      legitimately trips 2 — rare, and a refusal is safe where a partial rename corrupts.
    if (!(await waitForEmbeddedDiags(doc.uri, proxy.versionOf(vuri), 15000))) {
      connection.window.showWarningMessage(
        "Rename refused: clangd is still analyzing this file — try again in a moment.",
      );
      return null;
    }
    const region = view.sourceRegionOf(u16Off);
    if (region) {
      const hasRegionError = (embeddedDiagsByUri.get(doc.uri) ?? []).some((d) => {
        if (d.severity !== DiagnosticSeverity.Error) return false;
        const s = doc.offsetAt(d.range.start);
        const e = doc.offsetAt(d.range.end);
        return s < region.end && e > region.start;
      });
      if (hasRegionError) {
        connection.window.showWarningMessage(
          "Rename refused: this embedded C++ block has unresolved errors (clangd) — a rename could be partial. Fix them (or add a compile database for engine types) first.",
        );
        return null;
      }
    }
    const raw = await proxy.positionRequest("textDocument/rename", vuri, virtualPosition, {
      newName: params.newName,
    });
    const translated = translateEmbeddedRename(raw, doc.uri, text, fsPathOf(doc));
    if (!translated) return null; // clangd had nothing to rename here
    if (translated.kind === "refused") {
      connection.window.showWarningMessage(`Rename refused: ${translated.reason}`);
      return null;
    }
    if (region) {
      const oldIdent = identRangeAt(text, u16Off);
      const oldName = oldIdent ? text.slice(oldIdent.start, oldIdent.end) : null;
      if (oldName) {
        const occurrences = (text.slice(region.start, region.end).match(new RegExp(`\\b${oldName}\\b`, "g")) ?? []).length;
        const editsInRegion = translated.edits.filter((e) => {
          const s = doc.offsetAt(e.range.start);
          return s >= region.start && s < region.end;
        }).length;
        if (editsInRegion < occurrences) {
          connection.window.showWarningMessage(
            `Rename refused: only ${editsInRegion} of ${occurrences} \`${oldName}\` occurrences in this block could be verified — clangd's view of this code is incomplete (unresolved types?).`,
          );
          return null;
        }
      }
    }
    return { changes: { [doc.uri]: translated.edits.map((e) => ({ range: e.range, newText: e.newText })) } };
  }
  const hostTags = new Set(Object.keys(schemaOf(doc).elements));
  const result = renameSymbolAt(fsPathOf(doc), cpOff, params.newName, hostTags, overlay);
  if ("error" in result) {
    connection.window.showWarningMessage(`Rename refused: ${result.error}`);
    return null;
  }
  const changes: Record<string, TextEdit[]> = {};
  for (const edit of result.edits) {
    const loc = locationOfCp(edit.file, edit.start, edit.len, overlay);
    if (!loc) continue;
    (changes[loc.uri] ??= []).push({ range: loc.range, newText: edit.newText });
  }
  return { changes };
});

const SYMBOL_KIND: Record<string, SymbolKind> = {
  component: SymbolKind.Class,
  hook: SymbolKind.Function,
  module: SymbolKind.Namespace,
  value: SymbolKind.Constant,
  util: SymbolKind.Function,
};

connection.onDocumentSymbol((params): DocumentSymbol[] => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return [];
  const text = doc.getText();
  const scan = scanFile(text, path.basename(fsPathOf(doc), ".uetkx"), /*resyncOnBodyError*/ true);
  const out: DocumentSymbol[] = [];
  const symbolOf = (name: string, kind: string, nameAt: number, endCp: number): DocumentSymbol => {
    const s = doc.positionAt(codePointToUtf16(text, nameAt));
    const e = doc.positionAt(codePointToUtf16(text, Math.max(nameAt + name.length, endCp)));
    const sel = { start: s, end: doc.positionAt(codePointToUtf16(text, nameAt + name.length)) };
    return { name, kind: SYMBOL_KIND[kind] ?? SymbolKind.Object, range: { start: s, end: e }, selectionRange: sel, detail: kind };
  };
  for (const d of scan.components) out.push(symbolOf(d.name, "component", d.nameAt, d.next));
  for (const d of scan.hooks) out.push(symbolOf(d.name, "hook", d.nameAt, d.next));
  for (const d of scan.modules) out.push(symbolOf(d.name, "module", d.nameAt, d.next));
  for (const d of scan.values) out.push(symbolOf(d.name, "value", d.nameAt, d.next));
  for (const d of scan.utils) out.push(symbolOf(d.name, "util", d.nameAt, d.next));
  out.sort((a, b) => (a.range.start.line - b.range.start.line) || (a.range.start.character - b.range.start.character));
  return out;
});

connection.onWorkspaceSymbol((params): SymbolInformation[] => {
  // EVERY open document anchors a sweep universe (deduped) — an editor session can have docs
  // from several roots open (the smoke does), and "the first open doc" picked the wrong one.
  const overlay = liveOverlay();
  const query = params.query.toLowerCase();
  const out: SymbolInformation[] = [];
  const seenFiles = new Set<string>();
  for (const doc of documents.all()) {
    for (const file of sweptUetkxFiles(fsPathOf(doc))) {
      const norm = path.resolve(file).replace(/\\/g, "/");
      if (seenFiles.has(norm)) continue;
      seenFiles.add(norm);
      const decls = getDecls(file);
      if (!decls) continue;
      for (const d of decls) {
        if (query && !d.name.toLowerCase().includes(query)) continue;
        const loc = locationOfCp(file, d.nameAt, d.name.length, overlay);
        if (loc) out.push({ name: d.name, kind: SYMBOL_KIND[d.kind] ?? SymbolKind.Object, location: loc, containerName: path.basename(file) });
        if (out.length >= 200) return out; // capped (plan watch-list)
      }
    }
  }
  return out;
});

// ── code actions (N3): the fix-its as applicable edits — derived from published diagnostics ──

connection.onCodeAction((params): CodeAction[] => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return [];
  const text = doc.getText();
  const fsPath = fsPathOf(doc);
  const actions: CodeAction[] = [];

  for (const diag of params.context.diagnostics) {
    const code = String(diag.code ?? "");
    if (code === "UETKX2305") {
      // The message carries the exact fix: `… — add: import { X } from "spec"`.
      const m = /add: import \{ ([A-Za-z0-9_]+) \} from "([^"]+)"/.exec(diag.message);
      if (!m) continue;
      const [, name, spec] = m;
      const scan = scanFile(text, path.basename(fsPath, ".uetkx"), true);
      // Merge into an existing same-spec import that carries a name list (a COMBINED
      // declaration's braces qualify too) when one exists; else a new line above the first
      // declaration (the canonical preamble position).
      const existing = scan.imports.find((imp) => !imp.hostInclude && imp.names.length > 0 && imp.specifier === spec);
      let edit: TextEdit;
      if (existing && existing.names.length > 0) {
        const lastAt = existing.nameAts[existing.nameAts.length - 1];
        const lastName = existing.names[existing.names.length - 1];
        const lastLocal = existing.localNames[existing.localNames.length - 1] ?? lastName;
        const insertCp = (lastLocal === lastName ? lastAt : existing.localNameAts[existing.localNameAts.length - 1]) + lastLocal.length;
        const pos = doc.positionAt(codePointToUtf16(text, insertCp));
        edit = { range: { start: pos, end: pos }, newText: `, ${name}` };
      } else {
        const firstDeclCp = firstDeclStartCp(scan);
        const pos = doc.positionAt(codePointToUtf16(text, Math.max(0, firstDeclCp)));
        edit = { range: { start: pos, end: pos }, newText: `import { ${name} } from "${spec}"\n` };
      }
      actions.push({
        title: `Add import { ${name} } from "${spec}"`,
        kind: CodeActionKind.QuickFix,
        diagnostics: [diag],
        edit: { changes: { [doc.uri]: [edit] } },
      });
      continue;
    }
    if (code === "UETKX2304") {
      // The diagnostic range sits on the unused LOCAL binding token. Remove the binding — or
      // the whole import line when it was the only one.
      const scan = scanFile(text, path.basename(fsPath, ".uetkx"), true);
      const diagCp = utf16ToCodePoint(text, doc.offsetAt(diag.range.start));
      const removal = unusedImportRemoval(scan, text, diagCp);
      if (removal) {
        const start = doc.positionAt(codePointToUtf16(text, removal.start));
        const end = doc.positionAt(codePointToUtf16(text, removal.end));
        actions.push({
          title: "Remove unused import",
          kind: CodeActionKind.QuickFix,
          diagnostics: [diag],
          edit: { changes: { [doc.uri]: [{ range: { start, end }, newText: "" }] } },
        });
      }
      continue;
    }
    if (code === "UETKX2301") {
      // `X is not exported by <label> — add export …`: the diagnostic sits on the import's
      // TARGET-name token; the fix edits the TARGET file (a cross-file WorkspaceEdit).
      const scan = scanFile(text, path.basename(fsPath, ".uetkx"), true);
      const diagCp = utf16ToCodePoint(text, doc.offsetAt(diag.range.start));
      const imp = scan.imports.find((im) => !im.hostInclude && im.nameAts.some((at, n) => diagCp >= at && diagCp <= at + im.names[n].length));
      if (!imp) continue;
      const n = imp.nameAts.findIndex((at, idx) => diagCp >= at && diagCp <= at + imp.names[idx].length);
      const target = resolveSpecifier(fsPath, imp.specifier);
      if (!target || n < 0) continue;
      const targetIndex = getFileIndex(target, liveOverlay());
      const decl = targetIndex?.refs.find((r) => r.kind === "decl-name" && r.name === imp.names[n]);
      if (!targetIndex || !decl) continue;
      // insert `export ` at the DECLARATION's line start (its own At, which precedes the name)
      const declRecord = declStartCpOf(targetIndex.scan, imp.names[n]);
      if (declRecord < 0) continue;
      const tpos = (() => {
        const tdoc = TextDocument.create(URI.fromFsPath(target), "uetkx", 0, targetIndex.text);
        const p = tdoc.positionAt(codePointToUtf16(targetIndex.text, declRecord));
        return { uri: tdoc.uri, pos: p };
      })();
      actions.push({
        title: `Add export to \`${imp.names[n]}\` in ${path.basename(target)}`,
        kind: CodeActionKind.QuickFix,
        diagnostics: [diag],
        edit: { changes: { [tpos.uri]: [{ range: { start: tpos.pos, end: tpos.pos }, newText: "export " }] } },
      });
      continue;
    }
    if (code === "UETKX2320") {
      // Migrate THIS declaration to the plain grammar — the codemod's per-decl rewrite,
      // reproduced for components and hooks (modules hoist cross-file; the codemod owns those).
      const scan = scanFile(text, path.basename(fsPath, ".uetkx"), true);
      const diagCp = utf16ToCodePoint(text, doc.offsetAt(diag.range.start));
      const rewrite = wrapperRewriteAt(scan, diagCp);
      if (!rewrite) continue;
      const start = doc.positionAt(codePointToUtf16(text, rewrite.start));
      const end = doc.positionAt(codePointToUtf16(text, rewrite.end));
      actions.push({
        title: "Migrate to the plain declaration grammar",
        kind: CodeActionKind.QuickFix,
        diagnostics: [diag],
        edit: { changes: { [doc.uri]: [{ range: { start, end }, newText: rewrite.text }] } },
      });
      continue;
    }
  }
  return actions;
});

// ── formatting (uetkx.config.json walk-up, tab default) ────────────────────────────────────

connection.onDocumentFormatting((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return [];
  const text = doc.getText();
  const config = loadFormatterConfig(path.dirname(fsPathOf(doc)));
  const opts: Partial<UetkxFormatOptions> = {};
  if (typeof config.printWidth === "number") opts.printWidth = config.printWidth;
  if (config.indentStyle === "tab" || config.indentStyle === "space") opts.indentStyle = config.indentStyle;
  if (typeof config.indentSize === "number") opts.indentSize = config.indentSize;
  if (typeof config.singleAttributePerLine === "boolean") opts.singleAttributePerLine = config.singleAttributePerLine;
  if (typeof config.insertSpaceBeforeSelfClose === "boolean")
    opts.insertSpaceBeforeSelfClose = config.insertSpaceBeforeSelfClose;
  const result = formatUetkx(text, { ...DEFAULT_FORMAT_OPTIONS, ...opts });
  if (result.fellBack || !result.changed) return [];
  return [
    {
      range: { start: doc.positionAt(0), end: doc.positionAt(text.length) },
      newText: result.text,
    },
  ];
});

documents.listen(connection);
connection.listen();
