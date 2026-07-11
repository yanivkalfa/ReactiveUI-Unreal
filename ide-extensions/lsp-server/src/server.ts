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
  type Diagnostic,
  type InitializeParams,
} from "vscode-languageserver/node";
import { TextDocument } from "vscode-languageserver-textdocument";
import { URI } from "./uri";
import * as path from "node:path";
import { codePointToUtf16, utf16ToCodePoint } from "./codePoints";
import { classifyCursor, DIRECTIVES } from "./context";
import { readSidecarDiags } from "./diagsSidecar";
import { formatUetkx, UetkxFormatOptions, DEFAULT_FORMAT_OPTIONS } from "./formatUetkx";
import { scanFile } from "./uetkxFileScan";
import { loadFormatterConfig, schemaForFile, UetkxSchema } from "./uetkxSchema";

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

connection.onInitialize((_params: InitializeParams) => ({
  capabilities: {
    textDocumentSync: TextDocumentSyncKind.Incremental,
    completionProvider: { triggerCharacters: ["<", "@", ".", " "] },
    hoverProvider: true,
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
    // clean parse: surface the compiler's full verdict for THIS content (hash-gated) — minus
    // entries the live scan already produced (same code+range), which would double the hover.
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
  const cp = utf16ToCodePoint(text, doc.offsetAt(params.position));
  const ctx = classifyCursor(text, cp);
  const schema = schemaOf(doc);
  if (ctx.kind === "tag") {
    return Object.keys(schema.elements).map((tag) => ({
      label: tag,
      kind: CompletionItemKind.Class,
      detail: schema.elements[tag].factory,
    }));
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
  return null;
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
