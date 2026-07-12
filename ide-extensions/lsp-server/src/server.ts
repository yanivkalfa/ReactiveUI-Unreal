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
  CompletionItemKind,
  DiagnosticSeverity,
  type CompletionItem,
  type Definition,
  type Diagnostic,
  type InitializeParams,
} from "vscode-languageserver/node";
import { TextDocument } from "vscode-languageserver-textdocument";
import { URI } from "./uri";
import * as fs from "node:fs";
import * as path from "node:path";
import { codePointToUtf16, utf16ToCodePoint } from "./codePoints";
import { classifyCursor, DIRECTIVES } from "./context";
import { enclosingAttrName, fieldForKind, PAYLOAD_FIELDS } from "./eventPayload";
import { readSidecarDiags } from "./diagsSidecar";
import { formatUetkx, UetkxFormatOptions, DEFAULT_FORMAT_OPTIONS } from "./formatUetkx";
import { scanFile } from "./uetkxFileScan";
import { loadFormatterConfig, schemaForFile, UetkxSchema } from "./uetkxSchema";
import {
  findExporter,
  getDecls,
  importCursorAt,
  listUetkxFiles,
  resolveDiagnostics,
  resolveSpecifier,
  suggestSpecifier,
  workspaceRootFor,
} from "./uetkxWorkspace";

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

connection.onInitialize((_params: InitializeParams) => ({
  capabilities: {
    textDocumentSync: TextDocumentSyncKind.Incremental,
    completionProvider: { triggerCharacters: ["<", "@", ".", " ", "{", '"', "/"] },
    hoverProvider: true,
    definitionProvider: true,
    documentFormattingProvider: true,
  },
}));

function fsPathOf(doc: TextDocument): string {
  return URI.toFsPath(doc.uri);
}

function schemaOf(doc: TextDocument): UetkxSchema {
  return schemaForFile(path.dirname(fsPathOf(doc)));
}

// ── diagnostics: live parse + hash-gated sidecar ───────────────────────────────────────────

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
    });
  };
  const scan = scanFile(text, path.basename(fsPathOf(doc), ".uetkx"));
  const seen = new Set<string>();
  for (const d of scan.diags) {
    seen.add(`${d.code}@${d.offset}:${d.length}`);
    push(d.offset, d.length, d.severity, d.code, d.message);
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
  connection.sendDiagnostics({ uri: doc.uri, diagnostics: diags });
}

documents.onDidChangeContent((change) => validate(change.document));
documents.onDidClose((e) => connection.sendDiagnostics({ uri: e.document.uri, diagnostics: [] }));

// ── completions ────────────────────────────────────────────────────────────────────────────

connection.onCompletion((params): CompletionItem[] => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return [];
  const text = doc.getText();

  // Import intelligence takes precedence in the preamble: a `import { … } from "…"` cursor
  // completes exported NAMES of the resolved target, or workspace-relative SPECIFIER paths.
  const imp = importCursorAt(text, doc.offsetAt(params.position));
  if (imp) {
    const fsPath = fsPathOf(doc);
    if (imp.kind === "import-specifier") {
      const importerDir = path.dirname(fsPath);
      const root = workspaceRootFor(fsPath);
      const items: CompletionItem[] = [];
      for (const file of listUetkxFiles(root)) {
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
    const items: CompletionItem[] = [];
    const add = (name: string, kind: string) => {
      if (seen.has(name)) return;
      seen.add(name);
      items.push({ label: name, kind: kind === "hook" ? CompletionItemKind.Function : kind === "module" ? CompletionItemKind.Module : CompletionItemKind.Class, detail: kind });
    };
    if (key) {
      for (const d of getDecls(key) ?? []) if (d.exported) add(d.name, d.kind);
    } else {
      for (const file of listUetkxFiles(workspaceRootFor(fsPath))) {
        if (path.resolve(file) === path.resolve(fsPath)) continue;
        for (const d of getDecls(file) ?? []) if (d.exported) add(d.name, d.kind);
      }
    }
    return items;
  }

  const schema = schemaOf(doc);

  // TD-016: typed event payload — `Value.<field>` inside an event handler expression completes the
  // FRuiValue field, with the ENCLOSING event's field first (OnTextChanged → Value.TextValue).
  const off = doc.offsetAt(params.position);
  const before = text.slice(Math.max(0, off - 48), off);
  if (/\bValue\.[A-Za-z0-9_]*$/.test(before)) {
    const attr = enclosingAttrName(text, off);
    const want = fieldForKind(attr && schema.eventPayloads ? schema.eventPayloads[attr] : undefined);
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
      const key = resolveSpecifier(fsPath, imp.specifier);
      const decls = key ? getDecls(key) : null;
      for (const nm of imp.names) {
        // A resolved import: offer it only when it is (or is presumed) a component; a known hook/
        // module is not a tag. Unresolved (decls null) → offer it, the author knows their intent.
        const d = decls?.find((x) => x.name === nm);
        if (!d || d.kind === "component") addComp(nm, "component (imported)");
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
    return items;
  }
  if (ctx.kind === "directive") {
    return DIRECTIVES.map((d) => ({ label: d, kind: CompletionItemKind.Keyword, detail: "@" + d }));
  }
  return schema.hooks.map((hook) => ({ label: hook, kind: CompletionItemKind.Function, detail: "hook" }));
});

// ── hover ──────────────────────────────────────────────────────────────────────────────────

connection.onHover((params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;
  const text = doc.getText();
  const offset = doc.offsetAt(params.position);
  let start = offset;
  let end = offset;
  const isWord = (c: string) => /[A-Za-z0-9_.]/.test(c);
  while (start > 0 && isWord(text[start - 1])) start--;
  while (end < text.length && isWord(text[end])) end++;
  const word = text.slice(start, end);
  if (!word) return null;
  const schema = schemaOf(doc);
  const el = schema.elements[word];
  if (el) {
    const attrs = Object.entries(el.attrs)
      .map(([a, t]) => `${a}: ${t}`)
      .join(", ");
    return {
      contents: {
        kind: "markdown" as const,
        value: `**<${word}>** → \`${el.factory}\`\n\n${el.children ? "container" : "leaf"} — attrs: ${attrs || "(none)"}`,
      },
    };
  }
  if (schema.hooks.includes(word)) {
    return { contents: { kind: "markdown" as const, value: `**${word}** — ReactiveUI hook (auto-prefixed to \`Ctx.\` by the compiler)` } };
  }
  if (schema.styleKeys.includes(word)) {
    return { contents: { kind: "markdown" as const, value: `**${word}** — style key (host-applied; resets on removal)` } };
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

connection.onDefinition((params): Definition | null => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return null;
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
    for (let n = 0; n < impDecl.names.length; n++) {
      const at = impDecl.nameAts[n];
      if (cpOff >= at && cpOff <= at + impDecl.names[n].length) {
        const key = resolveSpecifier(fsPath, impDecl.specifier);
        if (!key) return null;
        const d = (getDecls(key) ?? []).find((x) => x.name === impDecl.names[n]);
        return d ? locAtDecl(key, d.nameAt, d.name.length) : null;
      }
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
  // same-file decl first
  for (const c of scan.components) if (c.name === word) return locAtDecl(fsPath, c.nameAt, word.length);
  for (const h of scan.hooks) if (h.name === word) return locAtDecl(fsPath, h.nameAt, word.length);
  for (const m of scan.modules) if (m.name === word) return locAtDecl(fsPath, m.nameAt, word.length);
  const hit = findExporter(word, fsPath);
  return hit ? locAtDecl(hit.file, hit.nameAt, word.length) : null;
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
