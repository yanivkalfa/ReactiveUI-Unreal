// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TD-020 — the embedded-C++ intelligence layer: the source map, the C++ virtual document, and
// the clangd proxy's graceful degradation.

import { test } from "node:test";
import * as assert from "node:assert";
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { SourceMap, offsetToPosition, positionToOffset } from "../sourceMap";
import { buildVirtualCpp } from "../virtualDoc";
import { ClangdProxy, findCompileCommands } from "../clangdProxy";

test("source map round-trips offsets across a span", () => {
  const map = new SourceMap([
    { srcStart: 100, virStart: 40, length: 20 },
    { srcStart: 200, virStart: 80, length: 10 },
  ]);
  assert.strictEqual(map.spanCount, 2);
  assert.strictEqual(map.sourceToVirtual(105), 45); // inside span 1
  assert.strictEqual(map.virtualToSource(45), 105);
  assert.strictEqual(map.sourceToVirtual(205), 85); // inside span 2
  assert.strictEqual(map.virtualToSource(85), 205);
  assert.strictEqual(map.sourceToVirtual(50), null); // before any span
  assert.strictEqual(map.sourceToVirtual(150), null); // between spans
  assert.strictEqual(map.virtualToSource(10), null); // inside the prelude scaffolding
});

test("offset <-> position conversions agree", () => {
  const text = "ab\ncde\nf";
  assert.deepStrictEqual(offsetToPosition(text, 0), { line: 0, character: 0 });
  assert.deepStrictEqual(offsetToPosition(text, 4), { line: 1, character: 1 }); // 'd'
  assert.deepStrictEqual(offsetToPosition(text, 7), { line: 2, character: 0 }); // 'f'
  assert.strictEqual(positionToOffset(text, { line: 1, character: 1 }), 4);
  assert.strictEqual(positionToOffset(text, { line: 2, character: 0 }), 7);
});

test("virtual C++ doc lifts an embedded hook body verbatim + maps it back", () => {
  const source = [
    'import { X } from "./x"',
    "",
    "export hook UseThing(int32 Start) -> int32 {",
    "\tauto [Count, SetCount] = UseState<int32>(Start);",
    "\tconst int32 Current = Count;",
    "\treturn Current;",
    "}",
    "",
  ].join("\n");

  const vd = buildVirtualCpp(source);
  assert.ok(vd.regionCount >= 1, "at least one embedded region lifted");

  // The hook body appears verbatim in the virtual doc.
  assert.ok(vd.text.includes("UseState<int32>(Start)"), "body copied into the virtual doc");

  // An identifier inside the body maps source -> virtual -> same identifier text, and back.
  const needle = "UseState<int32>(Start)";
  const srcOffset = source.indexOf(needle);
  assert.ok(srcOffset > 0);
  const virOffset = vd.map.sourceToVirtual(srcOffset);
  assert.ok(virOffset !== null, "the body offset maps into the virtual doc");
  assert.strictEqual(vd.text.slice(virOffset!, virOffset! + needle.length), needle);
  assert.strictEqual(vd.map.virtualToSource(virOffset!), srcOffset, "round-trips back");

  // An offset in the markup/decl header (the word `export`) is NOT embedded C++ -> no mapping.
  assert.strictEqual(vd.map.sourceToVirtual(source.indexOf("export")), null);
});

test("astral-plane char before a region keeps the map on the UTF-16 axis (bughunt B14)", () => {
  // 🎉 is ONE code point but TWO UTF-16 units. scanFile offsets are code points; the virtual side and
  // the server (doc.offsetAt/positionAt) are UTF-16, so srcStart must be stored in UTF-16 — else every
  // identifier after the emoji maps one unit too far and hover/definition hit the wrong token.
  const source = [
    "// 🎉 banner",
    "export hook UseThing(int32 Start) -> int32 {",
    "\tconst int32 Doubled = Start * 2;",
    "\treturn Doubled;",
    "}",
  ].join("\n");
  const vd = buildVirtualCpp(source);

  const needle = "Doubled = Start";
  const srcOffset = source.indexOf(needle); // JS indexOf is UTF-16
  assert.ok(srcOffset > 0);
  const virOffset = vd.map.sourceToVirtual(srcOffset);
  assert.notStrictEqual(virOffset, null, "the body offset maps into the virtual doc");
  assert.strictEqual(
    vd.text.slice(virOffset!, virOffset! + needle.length),
    needle,
    "the mapped virtual slice is the SAME identifier (no astral-char off-by-N)",
  );
  assert.strictEqual(vd.map.virtualToSource(virOffset!), srcOffset, "round-trips back to the UTF-16 source offset");
});

test("virtual C++ doc lifts a component setup block", () => {
  const source = [
    "export component Counter {",
    "\tauto [Count, SetCount] = UseState<int32>(0);",
    "\tconst int32 Doubled = Count * 2;",
    "\treturn (",
    "\t\t<TextBlock></TextBlock>",
    "\t);",
    "}",
  ].join("\n");
  const vd = buildVirtualCpp(source);
  assert.ok(vd.regionCount >= 1, "setup lifted");
  assert.ok(vd.text.includes("__rui_setup_Counter"), "setup wrapped in a named function");
  assert.ok(vd.text.includes("Doubled = Count * 2"), "setup body present");
});

test("findCompileCommands returns null in an empty temp tree", () => {
  // os.tmpdir() has no compile_commands.json above it in practice; assert it does not throw and
  // returns a string-or-null (walk-up terminates at the filesystem root).
  const found = findCompileCommands(os.tmpdir());
  assert.ok(found === null || typeof found === "string");
});

test("virtual doc emits REAL declarations for cross-file imports (hook signature + module namespace)", () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "uetkx-surf-"));
  fs.writeFileSync(path.join(root, "Demo.uproject"), "{}");
  const dir = path.join(root, "Source", "Demo");
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(
    path.join(dir, "Counter.hooks.uetkx"),
    "export hook UseCounter(int32 Start) -> TTuple<int32, TFunction<void()>> {\n\tauto [Count, SetCount] = UseState<int32>(Start);\n\treturn MakeTuple(Count, TFunction<void()>());\n}\nexport module CounterStyle {\n\tinline const int32 Max = 10;\n}\n",
  );
  const importer = path.join(dir, "App.uetkx");
  const src =
    'import { UseCounter, CounterStyle } from "./Counter.hooks"\nexport component App {\n\tauto [Count, Inc] = UseCounter(CounterStyle::Max);\n\treturn ( <Spacer /> );\n}\n';
  fs.writeFileSync(importer, src);
  const { buildEmbeddedView } = require("../embeddedIntel") as typeof import("../embeddedIntel");
  const view = buildEmbeddedView(src, importer);
  assert.ok(
    view.virtualText.includes("TTuple<int32, TFunction<void()>> UseCounter(int32 Start);"),
    "imported hook declared with its REAL signature",
  );
  assert.ok(view.virtualText.includes("namespace CounterStyle {"), "imported module declared as a namespace");
  assert.ok(view.virtualText.includes("inline const int32 Max = 10;"), "module body verbatim");
  // Without a path the surfaces are simply omitted (test-mode consistency).
  const bare = buildEmbeddedView(src);
  assert.ok(!bare.virtualText.includes("UseCounter(int32 Start);"), "no resolver, no emission");
  fs.rmSync(root, { recursive: true, force: true });
});

test("findCompileCommands picks up UBT's .vscode generator output, project file over Default (TB-10)", () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "uetkx-ccdb-"));
  const vscodeDir = path.join(root, ".vscode");
  fs.mkdirSync(vscodeDir);
  fs.writeFileSync(path.join(vscodeDir, "compileCommands_Default.json"), "[]");
  fs.writeFileSync(path.join(vscodeDir, "compileCommands_MyGame.json"), "[]");
  const nested = path.join(root, "Source", "MyGame");
  fs.mkdirSync(nested, { recursive: true });
  const found = findCompileCommands(nested);
  assert.ok(found !== null && found.endsWith("compileCommands_MyGame.json"), `project-named DB wins (${found})`);
  // A canonical file at the root beats the generated ones.
  fs.writeFileSync(path.join(root, "compile_commands.json"), "[]");
  const canonical = findCompileCommands(nested);
  assert.ok(canonical !== null && canonical.endsWith(`${path.sep}compile_commands.json`), `canonical wins (${canonical})`);
  fs.rmSync(root, { recursive: true, force: true });
});

test("clangd proxy degrades gracefully when clangd is absent", async () => {
  const proxy = new ClangdProxy("definitely-not-a-real-clangd-binary-xyz");
  const started = await proxy.start();
  assert.strictEqual(started, false, "start resolves false, never throws");
  assert.strictEqual(proxy.isAvailable(), false);
  const hover = await proxy.positionRequest("textDocument/hover", "file:///v.cpp", { line: 0, character: 0 });
  assert.strictEqual(hover, null, "requests return null under degradation");
  proxy.dispose();
});
