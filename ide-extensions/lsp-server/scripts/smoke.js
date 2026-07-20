// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// End-to-end smoke: spawn the built server over stdio, run the LSP handshake, and exercise
// the v1 markup-baseline surface — tag/attr completion from the shipped schema, a live parse
// diagnostic (UETKX code), canonical formatting, and the hash-gated compiler sidecar
// (surfaced on match, suppressed once the buffer diverges). No Unreal editor required.
const cp = require("child_process");
const path = require("path");
const fs = require("fs");
const os = require("os");
const { srcHash } = require("../out/cppScanner");

const server = cp.spawn("node", [path.join(__dirname, "..", "out", "server.js"), "--stdio"]);
let buf = Buffer.alloc(0);
let contentLength = -1;
const waiters = new Map();
const diagnostics = {};

server.stdout.on("data", (d) => {
  buf = Buffer.concat([buf, d]);
  for (;;) {
    if (contentLength < 0) {
      const e = buf.indexOf("\r\n\r\n");
      if (e < 0) return;
      const m = /Content-Length:\s*(\d+)/i.exec(buf.slice(0, e).toString());
      contentLength = m ? +m[1] : 0;
      buf = buf.slice(e + 4);
    }
    if (buf.length < contentLength) return;
    const msg = JSON.parse(buf.slice(0, contentLength).toString());
    buf = buf.slice(contentLength);
    contentLength = -1;
    if (msg.id !== undefined && waiters.has(msg.id)) {
      waiters.get(msg.id)(msg);
      waiters.delete(msg.id);
    } else if (msg.method === "textDocument/publishDiagnostics") {
      diagnostics[msg.params.uri] = msg.params.diagnostics;
    }
  }
});
server.stderr.on("data", (d) => process.stderr.write(d));

let id = 0;
function send(msg) {
  const j = JSON.stringify({ jsonrpc: "2.0", ...msg });
  server.stdin.write(`Content-Length: ${Buffer.byteLength(j)}\r\n\r\n${j}`);
}
function request(method, params) {
  const myId = ++id;
  return new Promise((res) => {
    waiters.set(myId, res);
    send({ id: myId, method, params });
  });
}
function notify(method, params) {
  send({ method, params });
}
function fail(m) {
  console.error("SMOKE FAIL:", m);
  server.kill();
  process.exit(1);
}
const settle = (ms = 300) => new Promise((r) => setTimeout(r, ms));

(async () => {
  const timeout = setTimeout(() => fail("smoke timed out after 30s"), 30000);

  await request("initialize", { processId: process.pid, rootUri: null, capabilities: {} });
  notify("initialized", {});

  // a valid doc: clean live parse (URI basename matches the component -> no 0103 warn)
  const uri = "file:///tmp/Counter.uetkx";
  const text =
    "component Counter {\n\tauto [Count, SetCount] = UseState<int32>(0);\n\treturn (\n\t\t<VerticalBox>\n\t\t\t<But\n\t\t\t<Button  />\n\t\t</VerticalBox>\n\t);\n}\n";
  notify("textDocument/didOpen", { textDocument: { uri, languageId: "uetkx", version: 1, text } });

  // tag completion at "<But|" (line 4, char = tab*3 + "<But".length = 3 + 4 = 7)
  const tagRes = await request("textDocument/completion", { textDocument: { uri }, position: { line: 4, character: 7 } });
  const tagItems = Array.isArray(tagRes.result) ? tagRes.result : (tagRes.result && tagRes.result.items) || [];
  if (!tagItems.some((i) => i.label === "Button")) fail("tag completion missing Button: " + JSON.stringify(tagItems.slice(0, 5)));
  if (!tagItems.some((i) => i.label === "VerticalBox")) fail("tag completion missing VerticalBox");
  console.log(`tag completion OK (${tagItems.length} elements, includes Button/VerticalBox)`);

  // attr completion inside "<Button |" (line 5, char = 3 tabs + "<Button ".length = 3 + 8 = 11)
  const attrRes = await request("textDocument/completion", { textDocument: { uri }, position: { line: 5, character: 11 } });
  const attrItems = Array.isArray(attrRes.result) ? attrRes.result : (attrRes.result && attrRes.result.items) || [];
  const has = (n) => attrItems.some((i) => i.label === n);
  if (!has("OnClicked")) fail("attr completion missing typed attr OnClicked: " + JSON.stringify(attrItems.slice(0, 8).map((i) => i.label)));
  if (!has("ContentPadding")) fail("attr completion missing ContentPadding");
  if (!has("RenderOpacity")) fail("attr completion missing style key RenderOpacity");
  if (!has("Slot.Padding")) fail("attr completion missing slot key Slot.Padding");
  if (!has("key")) fail("attr completion missing structural 'key'");
  if (!has("classes")) fail("attr completion missing 'classes'");
  console.log(`attr completion OK (${attrItems.length} items: typed attrs + style/slot keys + key/classes)`);

  // TD-016: typed event payload — `Value.` inside OnTextChanged completes TextValue first.
  const uriEvt = "file:///tmp/Evt.uetkx";
  const evtText =
    'component Evt {\n\tauto [T, SetT] = UseState<FString>(FString());\n\treturn (\n\t\t<EditableTextBox OnTextChanged={ SetT(Value.) } />\n\t);\n}\n';
  notify("textDocument/didOpen", { textDocument: { uri: uriEvt, languageId: "uetkx", version: 1, text: evtText } });
  // position right after `Value.` (line 3): "\t\t<EditableTextBox OnTextChanged={ SetT(Value." then cursor
  const evtLine = evtText.split("\n")[3];
  const evtCh = evtLine.indexOf("Value.") + "Value.".length;
  const evtRes = await request("textDocument/completion", { textDocument: { uri: uriEvt }, position: { line: 3, character: evtCh } });
  const evtItems = Array.isArray(evtRes.result) ? evtRes.result : (evtRes.result && evtRes.result.items) || [];
  const textVal = evtItems.find((i) => i.label === "TextValue");
  if (!textVal) fail("Value. completion missing TextValue: " + JSON.stringify(evtItems.map((i) => i.label)));
  if (!/OnTextChanged payload/.test(textVal.detail || "")) fail("TextValue not marked as the OnTextChanged payload: " + JSON.stringify(textVal));
  console.log("event payload completion OK (Value. → TextValue first, typed by OnTextChanged)");

  // live parse diagnostic: an unclosed tag flags the family code UETKX0301
  const uriBroken = "file:///tmp/Broken.uetkx";
  notify("textDocument/didOpen", {
    textDocument: { uri: uriBroken, languageId: "uetkx", version: 1, text: "component Broken {\n\treturn ( <Box> );\n}\n" },
  });
  await settle();
  const dBroken = diagnostics[uriBroken] || [];
  if (!dBroken.some((x) => String(x.code) === "UETKX0301")) fail("unclosed-tag diagnostic missing: " + JSON.stringify(dBroken));
  console.log(`live parse diagnostic OK (${dBroken.length} diag(s), includes UETKX0301)`);

  // formatting: a messy doc canonicalizes (tab default), idempotence implied by the corpus
  const uriFmt = "file:///tmp/Fmt.uetkx";
  const messy = 'component Fmt {\n  return (\n    <VerticalBox><TextBlock Text="hi"/></VerticalBox>\n  );\n}\n';
  notify("textDocument/didOpen", { textDocument: { uri: uriFmt, languageId: "uetkx", version: 1, text: messy } });
  const fmtRes = await request("textDocument/formatting", { textDocument: { uri: uriFmt }, options: { tabSize: 4, insertSpaces: false } });
  const edits = fmtRes.result || [];
  if (!edits.length || !edits[0].newText.includes("\t\t<VerticalBox>") || !edits[0].newText.includes('<TextBlock Text="hi" />'))
    fail("formatting edit wrong: " + JSON.stringify(edits));
  console.log("formatting OK (canonical tabs, spaced self-close)");

  // compiler sidecar: hash-matched diagnostics surface; a diverged buffer suppresses them
  const scText = "component W { return ( <Spacer /> ); }\n";
  const scFsPath = path.join(os.tmpdir(), "W.uetkx");
  fs.writeFileSync(
    scFsPath + ".diags.json",
    JSON.stringify({
      v: 2,
      src_hash: srcHash(scText),
      diagnostics: [{ code: "UETKX0103", severity: 1, message: "component `W` differs from file name `X`", off: 10, len: 1 }],
      refs: {},
    }),
  );
  const scUri = "file:///" + scFsPath.replace(/\\/g, "/");
  notify("textDocument/didOpen", { textDocument: { uri: scUri, languageId: "uetkx", version: 1, text: scText } });
  await settle();
  if (!(diagnostics[scUri] || []).some((d) => String(d.code) === "UETKX0103"))
    fail("hash-matched sidecar not surfaced: " + JSON.stringify(diagnostics[scUri]));
  notify("textDocument/didChange", { textDocument: { uri: scUri, version: 2 }, contentChanges: [{ text: scText + "// edited\n" }] });
  await settle();
  if ((diagnostics[scUri] || []).some((d) => String(d.code) === "UETKX0103"))
    fail("stale sidecar must be suppressed after the buffer diverges");
  fs.unlinkSync(scFsPath + ".diags.json");
  console.log("compiler sidecar OK (surfaced on hash match, suppressed when stale)");

  // ── F5 round-2 pins (B1/B2): no basename nudge; broken parses still validate markup ──────
  const mmUri = "file:///tmp/LabCard.uetkx";
  notify("textDocument/didOpen", { textDocument: { uri: mmUri, languageId: "uetkx", version: 1,
    text: "export FRuiNode LasCard() {\n\treturn ( <Spacer /> );\n}\n" } });
  await settle();
  if ((diagnostics[mmUri] || []).some((d) => String(d.code) === "UETKX0103"))
    fail("0103 basename nudge must be GONE under ES modules");
  console.log("no 0103 basename nudge OK");

  const brokenUri = "file:///tmp/Broken.uetkx";
  notify("textDocument/didOpen", { textDocument: { uri: brokenUri, languageId: "uetkx", version: 1,
    text: 'export FRuiNode Broken() {\n\treturn ( <Bosrder>\n\t\t<Button ContesntPadding="12,4" />\n\t</Border> );\n}\n' } });
  await settle();
  const brokenDiags = diagnostics[brokenUri] || [];
  if (!brokenDiags.some((d) => String(d.code) === "UETKX2307"))
    fail("broken parse must still flag the unknown element (B2): " + JSON.stringify(brokenDiags.map((d) => d.code)));
  if (!brokenDiags.some((d) => String(d.code) === "UETKX0105"))
    fail("broken parse must still flag the unknown attribute (B2): " + JSON.stringify(brokenDiags.map((d) => d.code)));
  console.log("parse-error-resilient markup validation OK (2307 + 0105 on a broken tree)");

  // R5-4: component-prop validation — an unknown prop on a USER component 0105s
  const cpDir = fs.mkdtempSync(path.join(os.tmpdir(), "uetkx-props-"));
  fs.writeFileSync(path.join(cpDir, "Demo.uproject"), "{}");
  fs.writeFileSync(path.join(cpDir, "Card.uetkx"), 'export FRuiNode Card(FString Label = FString()) {\n\treturn ( <Spacer /> );\n}\n');
  const userPath = path.join(cpDir, "User.uetkx").replace(/\\/g, "/");
  const userUri = "file:///" + userPath;
  notify("textDocument/didOpen", { textDocument: { uri: userUri, languageId: "uetkx", version: 1,
    text: 'import { Card } from "./Card"\nexport FRuiNode User() {\n\treturn ( <Card Labsssel="x" /> );\n}\n' } });
  await settle();
  const propDiags = diagnostics[userUri] || [];
  if (!propDiags.some((d) => String(d.code) === "UETKX0105" && /Labsssel/.test(d.message)))
    fail("unknown component prop must 0105: " + JSON.stringify(propDiags.map((d) => d.code)));
  console.log("component-prop validation OK (0105 on an unknown prop)");

  // R6: local-typo lint — a near-miss of an in-scope LOCAL flags 2310 even when the
  // binding initializer is broken (clang suppresses the callee error in that cascade)
  const typoUri = "file:///tmp/Typo.uetkx";
  notify("textDocument/didOpen", { textDocument: { uri: typoUri, languageId: "uetkx", version: 1,
    text: "export FRuiNode Typo() {\n\tauto [bOpen, SetOpen] = UseSstate<bool>(true);\n\treturn ( <Button OnClicked={ SetOpsen(!bOpen) }>x</Button> );\n}\n" } });
  await settle();
  const typoDiags = diagnostics[typoUri] || [];
  if (!typoDiags.some((d) => String(d.code) === "UETKX2310" && /SetOpsen/.test(d.message)))
    fail("local-typo lint missing 2310: " + JSON.stringify(typoDiags.map((d) => d.code)));
  console.log("local-typo lint OK (2310 did-you-mean, cascade-proof)");

  // R10: attr VALUE validation — enum vocabularies (runtime parses fall back SILENTLY, so the
  // LSP is the only place a typo'd value can surface), numeric/margin formats, expr-only kinds.
  const valUri = "file:///tmp/Vals.uetkx";
  notify("textDocument/didOpen", { textDocument: { uri: valUri, languageId: "uetkx", version: 1,
    text: 'export FRuiNode Vals() {\n\treturn ( <Border HAlign="cesssssnter" VAlign="Top" Padding="1">\n\t\t<TextBlock Text="x" Slot.HAlign="cesssssnter" Justification="centre" />\n\t\t<Box WidthOverride="abc" />\n\t\t<Button ContentPadding="1,2,3">y</Button>\n\t\t<Spacer Size="1,2" />\n\t</Border> );\n}\n' } });
  await settle();
  const valDiags = diagnostics[valUri] || [];
  const has2311 = (re) => valDiags.some((d) => String(d.code) === "UETKX2311" && re.test(d.message));
  if (!has2311(/cesssssnter.*HAlign/)) fail("enum typo on element attr must 2311: " + JSON.stringify(valDiags.map((d) => d.message)));
  if (!has2311(/cesssssnter.*Slot\.HAlign/)) fail("enum typo on slot key must 2311: " + JSON.stringify(valDiags.map((d) => d.message)));
  if (!has2311(/centre.*Justification/)) fail("enum typo on style key must 2311");
  if (!has2311(/abc.*WidthOverride/)) fail("non-numeric float string must 2311");
  if (!has2311(/1,2,3.*margin/)) fail("3-component margin must 2311");
  if (valDiags.some((d) => String(d.code) === "UETKX2311" && /VAlign|'1'/.test(d.message)))
    fail("valid values must NOT flag (VAlign=Top is case-insensitively valid; Padding=1 is a uniform margin): " + JSON.stringify(valDiags.map((d) => d.message)));
  if (!valDiags.some((d) => String(d.code) === "UETKX0105" && /Size.*needs an \{expr\} value/.test(d.message)))
    fail("string form of a vector2 attr must surface codegen's 0105 LIVE: " + JSON.stringify(valDiags.map((d) => d.message)));
  console.log("attr value validation OK (2311 enums + formats, live 0105 expr-only, case-insensitive)");

  // R10: value completion — ctrl+space inside `HAlign="|"` offers the closed vocabulary
  const vcUri = "file:///tmp/ValComp.uetkx";
  const vcText = 'export FRuiNode ValComp() {\n\treturn ( <Border HAlign="" /> );\n}\n';
  notify("textDocument/didOpen", { textDocument: { uri: vcUri, languageId: "uetkx", version: 1, text: vcText } });
  const vcPos = { line: 1, character: vcText.split("\n")[1].indexOf('""') + 1 };
  const vcRes = await request("textDocument/completion", { textDocument: { uri: vcUri }, position: vcPos });
  const vcItems = (vcRes.result && (vcRes.result.items || vcRes.result)) || [];
  const vcLabels = vcItems.map((it) => it.label);
  if (!vcLabels.includes("fill") || !vcLabels.includes("center"))
    fail("enum value completion must offer the vocabulary: " + JSON.stringify(vcLabels));
  console.log("enum value completion OK (vocabulary inside the quotes)");
  fs.rmSync(cpDir, { recursive: true, force: true });

  // import intelligence: a real on-disk workspace (.uproject root + exporter B + importer A) —
  // name completion inside `import { | }`, go-to-definition on the name, and a live 2301 diag.
  const ws = fs.mkdtempSync(path.join(os.tmpdir(), "uetkx-smoke-ws-"));
  fs.writeFileSync(path.join(ws, "Demo.uproject"), "{}");
  const bPath = path.join(ws, "B.uetkx");
  fs.writeFileSync(bPath, "export component Badge { return ( <Spacer /> ); }\ncomponent Hidden { return ( <Spacer /> ); }\n");
  const aPath = path.join(ws, "A.uetkx");
  const toUri = (p) => "file:///" + p.replace(/\\/g, "/");
  const aText = 'import { Badge } from "./B"\ncomponent App {\n\treturn ( <Badge /> );\n}\n';
  fs.writeFileSync(aPath, aText);
  const aUri = toUri(aPath);
  notify("textDocument/didOpen", { textDocument: { uri: aUri, languageId: "uetkx", version: 1, text: aText } });

  // name completion at `import { Badge| }` (line 0, just after "Badge" at char 15)
  const impRes = await request("textDocument/completion", { textDocument: { uri: aUri }, position: { line: 0, character: 14 } });
  const impItems = Array.isArray(impRes.result) ? impRes.result : (impRes.result && impRes.result.items) || [];
  if (!impItems.some((i) => i.label === "Badge")) fail("import-name completion missing exported Badge: " + JSON.stringify(impItems.map((i) => i.label)));
  if (impItems.some((i) => i.label === "Hidden")) fail("import-name completion must not offer non-exported Hidden");
  console.log(`import name completion OK (offers exported Badge, hides Hidden)`);

  // go-to-definition on `Badge` in the import (line 0, char 10) → B.uetkx
  const defRes = await request("textDocument/definition", { textDocument: { uri: aUri }, position: { line: 0, character: 10 } });
  const def = Array.isArray(defRes.result) ? defRes.result[0] : defRes.result;
  if (!def || !def.uri.endsWith("B.uetkx")) fail("go-to-def did not land in B.uetkx: " + JSON.stringify(defRes.result));
  console.log("go-to-definition OK (import name → exporter file)");

  // tag completion folds in the imported component: `\treturn ( <Badge /> );` — cursor mid-name
  // at char 14 (`<Bad|ge`) is a tag position
  const tagWs = await request("textDocument/completion", { textDocument: { uri: aUri }, position: { line: 2, character: 14 } });
  const tagWsItems = Array.isArray(tagWs.result) ? tagWs.result : (tagWs.result && tagWs.result.items) || [];
  if (!tagWsItems.some((i) => i.label === "Badge")) fail("tag completion missing imported component Badge: " + JSON.stringify(tagWsItems.map((i) => i.label)));
  if (!tagWsItems.some((i) => i.label === "VerticalBox")) fail("tag completion dropped host elements when folding in components");
  console.log("tag completion OK (imported component Badge + host elements)");

  // TD-033: find-all-references on `Badge` (the import binding, line 0 char 10) — must find the
  // decl in B.uetkx plus A's binding + tag.
  const refRes = await request("textDocument/references", {
    textDocument: { uri: aUri },
    position: { line: 0, character: 10 },
    context: { includeDeclaration: true },
  });
  const refLocs = refRes.result || [];
  if (!refLocs.some((l) => l.uri.endsWith("B.uetkx"))) fail("references missing the declaration in B.uetkx: " + JSON.stringify(refLocs));
  if (refLocs.filter((l) => l.uri.endsWith("A.uetkx")).length < 2) fail("references missing A's binding + tag: " + JSON.stringify(refLocs));
  console.log(`find-all-references OK (${refLocs.length} locations across both files)`);

  // TD-033: prepareRename + rename `Badge` → `Crest` (cursor on the tag, line 2 char 12)
  const prep = await request("textDocument/prepareRename", { textDocument: { uri: aUri }, position: { line: 2, character: 12 } });
  if (!prep.result || !prep.result.placeholder) fail("prepareRename refused a renameable tag: " + JSON.stringify(prep));
  const ren = await request("textDocument/rename", { textDocument: { uri: aUri }, position: { line: 2, character: 12 }, newName: "Crest" });
  const changes = (ren.result && ren.result.changes) || {};
  const changedUris = Object.keys(changes);
  if (!changedUris.some((u) => u.endsWith("B.uetkx")) || !changedUris.some((u) => u.endsWith("A.uetkx")))
    fail("rename must edit BOTH files: " + JSON.stringify(changedUris));
  if (!Object.values(changes).flat().every((e) => e.newText === "Crest")) fail("rename edits wrong: " + JSON.stringify(changes));
  console.log(`rename OK (WorkspaceEdit across ${changedUris.length} files)`);

  // TD-033: document symbols for A.uetkx
  const symRes = await request("textDocument/documentSymbol", { textDocument: { uri: aUri } });
  if (!(symRes.result || []).some((s) => s.name === "App")) fail("documentSymbol missing App: " + JSON.stringify(symRes.result));
  console.log("document symbols OK");

  // TD-033: workspace symbols find Badge by partial query
  const wsSym = await request("workspace/symbol", { query: "bad" });
  if (!(wsSym.result || []).some((s) => s.name === "Badge")) fail("workspace symbol missing Badge: " + JSON.stringify(wsSym.result));
  console.log("workspace symbols OK");

  // live resolution diagnostic: importing a NON-exported name flags UETKX2301
  const aBadText = 'import { Hidden } from "./B"\ncomponent App {\n\treturn ( <Hidden /> );\n}\n';
  notify("textDocument/didChange", { textDocument: { uri: aUri, version: 2 }, contentChanges: [{ text: aBadText }] });
  await settle();
  const d2301 = (diagnostics[aUri] || []).find((d) => String(d.code) === "UETKX2301");
  if (!d2301) fail("live 2301 (not-exported) missing: " + JSON.stringify(diagnostics[aUri]));
  console.log("live import diagnostic OK (2301 not-exported)");

  // TD-033 N3: the 2301 code action offers the cross-file `export ` insertion into B.uetkx
  const caRes = await request("textDocument/codeAction", {
    textDocument: { uri: aUri },
    range: d2301.range,
    context: { diagnostics: [d2301] },
  });
  const exportAction = (caRes.result || []).find((a) => /Add export/.test(a.title));
  if (!exportAction) fail("2301 code action missing: " + JSON.stringify((caRes.result || []).map((a) => a.title)));
  const caUris = Object.keys((exportAction.edit && exportAction.edit.changes) || {});
  if (!caUris.some((u) => u.endsWith("B.uetkx"))) fail("2301 action must edit the TARGET file: " + JSON.stringify(caUris));
  console.log("code action OK (2301 → cross-file export insertion)");
  fs.rmSync(ws, { recursive: true, force: true });

  await request("shutdown", null);
  notify("exit", null);
  clearTimeout(timeout);
  console.log("SMOKE PASSED");
  server.kill();
  process.exit(0);
})().catch((e) => fail(e && e.stack ? e.stack : String(e)));
