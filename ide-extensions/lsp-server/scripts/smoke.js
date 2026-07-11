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

  // live resolution diagnostic: importing a NON-exported name flags UETKX2301
  const aBadText = 'import { Hidden } from "./B"\ncomponent App {\n\treturn ( <Hidden /> );\n}\n';
  notify("textDocument/didChange", { textDocument: { uri: aUri, version: 2 }, contentChanges: [{ text: aBadText }] });
  await settle();
  if (!(diagnostics[aUri] || []).some((d) => String(d.code) === "UETKX2301"))
    fail("live 2301 (not-exported) missing: " + JSON.stringify(diagnostics[aUri]));
  console.log("live import diagnostic OK (2301 not-exported)");
  fs.rmSync(ws, { recursive: true, force: true });

  await request("shutdown", null);
  notify("exit", null);
  clearTimeout(timeout);
  console.log("SMOKE PASSED");
  server.kill();
  process.exit(0);
})().catch((e) => fail(e && e.stack ? e.stack : String(e)));
