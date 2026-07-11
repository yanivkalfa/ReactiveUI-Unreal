"use strict";
// The .uetkx language server (stdio) — the markup-intelligence BASELINE, fully offline:
// completions/hover from the compiler-exported schema, live parse diagnostics from the
// TS front-end (corpus-locked to the C++ compiler), full compiler diagnostics from the
// hash-gated sidecar, document formatting via the golden-corpus formatter. Embedded-C++
// intelligence (clangd proxy over a virtual document) is the planned enhancement layer —
// markup-only is the family's fully-supported degradation baseline.
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
const node_1 = require("vscode-languageserver/node");
const vscode_languageserver_textdocument_1 = require("vscode-languageserver-textdocument");
const uri_1 = require("./uri");
const path = __importStar(require("node:path"));
const codePoints_1 = require("./codePoints");
const context_1 = require("./context");
const diagsSidecar_1 = require("./diagsSidecar");
const formatUetkx_1 = require("./formatUetkx");
const uetkxFileScan_1 = require("./uetkxFileScan");
const uetkxSchema_1 = require("./uetkxSchema");
const connection = (0, node_1.createConnection)(node_1.ProposedFeatures.all);
const documents = new node_1.TextDocuments(vscode_languageserver_textdocument_1.TextDocument);
connection.onInitialize((_params) => ({
    capabilities: {
        textDocumentSync: node_1.TextDocumentSyncKind.Incremental,
        completionProvider: { triggerCharacters: ["<", "@", ".", " "] },
        hoverProvider: true,
        documentFormattingProvider: true,
    },
}));
function fsPathOf(doc) {
    return uri_1.URI.toFsPath(doc.uri);
}
function schemaOf(doc) {
    return (0, uetkxSchema_1.schemaForFile)(path.dirname(fsPathOf(doc)));
}
// ── diagnostics: live parse + hash-gated sidecar ───────────────────────────────────────────
function validate(doc) {
    const text = doc.getText();
    const diags = [];
    const push = (offCp, lenCp, severity, code, message) => {
        const start = doc.positionAt((0, codePoints_1.codePointToUtf16)(text, Math.max(offCp, 0)));
        const end = doc.positionAt((0, codePoints_1.codePointToUtf16)(text, Math.max(offCp, 0) + Math.max(lenCp, 1)));
        diags.push({
            range: { start, end },
            severity: severity === 0 ? node_1.DiagnosticSeverity.Error : severity === 1 ? node_1.DiagnosticSeverity.Warning : node_1.DiagnosticSeverity.Hint,
            code,
            source: "uetkx",
            message,
        });
    };
    const scan = (0, uetkxFileScan_1.scanFile)(text, path.basename(fsPathOf(doc), ".uetkx"));
    for (const d of scan.diags)
        push(d.offset, d.length, d.severity, d.code, d.message);
    if (!scan.diags.some((d) => d.severity === 0)) {
        // clean parse: surface the compiler's full verdict for THIS content (hash-gated)
        for (const d of (0, diagsSidecar_1.readSidecarDiags)(fsPathOf(doc), text))
            push(d.off, d.len, d.severity, d.code, d.message);
    }
    connection.sendDiagnostics({ uri: doc.uri, diagnostics: diags });
}
documents.onDidChangeContent((change) => validate(change.document));
documents.onDidClose((e) => connection.sendDiagnostics({ uri: e.document.uri, diagnostics: [] }));
// ── completions ────────────────────────────────────────────────────────────────────────────
connection.onCompletion((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return [];
    const text = doc.getText();
    const cp = (0, codePoints_1.utf16ToCodePoint)(text, doc.offsetAt(params.position));
    const ctx = (0, context_1.classifyCursor)(text, cp);
    const schema = schemaOf(doc);
    if (ctx.kind === "tag") {
        return Object.keys(schema.elements).map((tag) => ({
            label: tag,
            kind: node_1.CompletionItemKind.Class,
            detail: schema.elements[tag].factory,
        }));
    }
    if (ctx.kind === "attr") {
        const items = [];
        const el = schema.elements[ctx.tag];
        if (el) {
            for (const [attr, type] of Object.entries(el.attrs)) {
                items.push({ label: attr, kind: node_1.CompletionItemKind.Property, detail: type });
            }
        }
        for (const key of schema.styleKeys)
            items.push({ label: key, kind: node_1.CompletionItemKind.Color, detail: "style" });
        for (const key of schema.slotKeys)
            items.push({ label: key, kind: node_1.CompletionItemKind.Unit, detail: "slot" });
        items.push({ label: "key", kind: node_1.CompletionItemKind.Keyword, detail: "reconciler identity" });
        items.push({ label: "classes", kind: node_1.CompletionItemKind.Keyword, detail: "style classes" });
        return items;
    }
    if (ctx.kind === "directive") {
        return context_1.DIRECTIVES.map((d) => ({ label: d, kind: node_1.CompletionItemKind.Keyword, detail: "@" + d }));
    }
    return schema.hooks.map((hook) => ({ label: hook, kind: node_1.CompletionItemKind.Function, detail: "hook" }));
});
// ── hover ──────────────────────────────────────────────────────────────────────────────────
connection.onHover((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return null;
    const text = doc.getText();
    const offset = doc.offsetAt(params.position);
    let start = offset;
    let end = offset;
    const isWord = (c) => /[A-Za-z0-9_.]/.test(c);
    while (start > 0 && isWord(text[start - 1]))
        start--;
    while (end < text.length && isWord(text[end]))
        end++;
    const word = text.slice(start, end);
    if (!word)
        return null;
    const schema = schemaOf(doc);
    const el = schema.elements[word];
    if (el) {
        const attrs = Object.entries(el.attrs)
            .map(([a, t]) => `${a}: ${t}`)
            .join(", ");
        return {
            contents: {
                kind: "markdown",
                value: `**<${word}>** → \`${el.factory}\`\n\n${el.children ? "container" : "leaf"} — attrs: ${attrs || "(none)"}`,
            },
        };
    }
    if (schema.hooks.includes(word)) {
        return { contents: { kind: "markdown", value: `**${word}** — ReactiveUI hook (auto-prefixed to \`Ctx.\` by the compiler)` } };
    }
    if (schema.styleKeys.includes(word)) {
        return { contents: { kind: "markdown", value: `**${word}** — style key (host-applied; resets on removal)` } };
    }
    return null;
});
// ── formatting (uetkx.config.json walk-up, tab default) ────────────────────────────────────
connection.onDocumentFormatting((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return [];
    const text = doc.getText();
    const config = (0, uetkxSchema_1.loadFormatterConfig)(path.dirname(fsPathOf(doc)));
    const opts = {};
    if (typeof config.printWidth === "number")
        opts.printWidth = config.printWidth;
    if (config.indentStyle === "tab" || config.indentStyle === "space")
        opts.indentStyle = config.indentStyle;
    if (typeof config.indentSize === "number")
        opts.indentSize = config.indentSize;
    if (typeof config.singleAttributePerLine === "boolean")
        opts.singleAttributePerLine = config.singleAttributePerLine;
    if (typeof config.insertSpaceBeforeSelfClose === "boolean")
        opts.insertSpaceBeforeSelfClose = config.insertSpaceBeforeSelfClose;
    const result = (0, formatUetkx_1.formatUetkx)(text, { ...formatUetkx_1.DEFAULT_FORMAT_OPTIONS, ...opts });
    if (result.fellBack || !result.changed)
        return [];
    return [
        {
            range: { start: doc.positionAt(0), end: doc.positionAt(text.length) },
            newText: result.text,
        },
    ];
});
documents.listen(connection);
connection.listen();
//# sourceMappingURL=server.js.map