// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// Pure-function server pieces: cursor classification, schema fallbacks, sidecar hash gating,
// and the config walk-up mapping — the testable core under the LSP plumbing.

import { test } from "node:test";
import * as assert from "node:assert";
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { classifyCursor } from "../context";
import { readSidecarDiags } from "../diagsSidecar";
import { srcHash } from "../cppScanner";
import { shippedSchema } from "../uetkxSchema";
import { scanFile, hasError, hookSignature } from "../uetkxFileScan";
import { loadUetkxConfig, loadFormatterConfig, rootAnchorFor } from "../uetkxSchema";

test("cursor classification", () => {
  const doc = 'component A {\n\treturn (\n\t\t<VerticalBox>\n\t\t\t<But\n\t\t\t<Button ContentP\n\t\t</VerticalBox>\n\t);\n}\n';
  const tagPos = doc.indexOf("<But") + 4;
  assert.deepStrictEqual(classifyCursor(doc, tagPos), { kind: "tag", partial: "But" });
  const attrPos = doc.indexOf("ContentP") + "ContentP".length;
  const attrCtx = classifyCursor(doc, attrPos);
  assert.strictEqual(attrCtx.kind, "attr");
  assert.strictEqual((attrCtx as { tag: string }).tag, "Button");
  const dirDoc = "component A {\n\treturn (\n\t\t<Box>\n\t\t\t@fo\n\t\t</Box>\n\t);\n}\n";
  const dirPos = dirDoc.indexOf("@fo") + 3;
  assert.deepStrictEqual(classifyCursor(dirDoc, dirPos), { kind: "directive", partial: "fo" });
});

test("shipped schema is the committed compiler export", () => {
  const schema = shippedSchema();
  assert.strictEqual(schema.v, 1);
  assert.strictEqual(Object.keys(schema.elements).length, 15);
  assert.strictEqual(schema.elements.Button.attrs.OnClicked, "event");
  assert.strictEqual(schema.hooks.length, 20);
  assert.ok(schema.styleKeys.includes("RenderOpacity"));
});

test("sidecar diags gate on src_hash", () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "uetkx-"));
  const file = path.join(dir, "X.uetkx");
  const source = "component X { return ( <Spacer /> ); }\n";
  fs.writeFileSync(file, source);
  fs.writeFileSync(
    file + ".diags.json",
    JSON.stringify({ v: 2, src_hash: srcHash(source), diagnostics: [{ code: "UETKX0103", severity: 1, message: "w", off: 0, len: 1 }], refs: {} }),
  );
  assert.strictEqual(readSidecarDiags(file, source).length, 1);
  assert.strictEqual(readSidecarDiags(file, source + "// edited").length, 0, "stale sidecar suppressed");
  fs.rmSync(dir, { recursive: true, force: true });
});

test("uetkx.config.json walk-up: root key, no-merge shadowing, missing-root default", () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "uetkx-cfg-"));
  const screens = path.join(root, "MyMod", "Screens");
  const sub = path.join(screens, "Sub");
  const noRoot = path.join(root, "MyMod", "NoRoot");
  for (const d of [sub, noRoot]) fs.mkdirSync(d, { recursive: true });
  const modRoot = path.join(root, "MyMod");
  fs.writeFileSync(path.join(modRoot, "uetkx.config.json"), JSON.stringify({ root: "Screens", printWidth: 80 }));
  fs.writeFileSync(path.join(sub, "uetkx.config.json"), JSON.stringify({ indentSize: 4 })); // no root
  fs.writeFileSync(path.join(noRoot, "uetkx.config.json"), JSON.stringify({ printWidth: 42 }));

  // Nearest wins + config dir surfaced.
  const fromScreens = loadUetkxConfig(screens);
  assert.strictEqual(fromScreens.config.printWidth, 80);
  assert.strictEqual(fromScreens.config.root, "Screens");
  assert.strictEqual(fromScreens.configDir, modRoot);
  assert.strictEqual(loadFormatterConfig(screens).printWidth, 80);

  // Declared root resolves relative to the config dir.
  assert.strictEqual(rootAnchorFor(screens), path.resolve(modRoot, "Screens"));
  // No-merge shadowing: the nearer formatter-only config has no root -> null (caller falls back
  // to the module root, which the TS side resolves in the workspace index, M11).
  assert.strictEqual(rootAnchorFor(sub), null);
  assert.strictEqual(rootAnchorFor(noRoot), null);
  // No config at all -> defaults, null anchor.
  assert.strictEqual(rootAnchorFor(root), null);
  assert.strictEqual(loadUetkxConfig(root).configDir, null);

  fs.rmSync(root, { recursive: true, force: true });
});

test("file scan parity spot checks", () => {
  const scan = scanFile("component Two { auto [A, SetA] = UseState<int32>(0);\n\treturn ( <Spacer /> ); }", "Two");
  assert.strictEqual(hasError(scan), false);
  assert.deepStrictEqual(scan.components[0].hookCalls, ["UseState"]);
  assert.notStrictEqual(hookSignature(["UseState"]), hookSignature(["UseState", "UseEffect"]));
});
