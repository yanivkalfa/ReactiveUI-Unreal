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

test("§2 lifter: markup expressions become mapped code regions (events, directives, children)", () => {
  const src = [
    "export component Lift {",
    "\tauto [Count, SetCount] = UseState<int32>(0);",
    "\treturn (",
    "\t\t<VerticalBox>",
    '\t\t\t<Button OnClicked={ SetCount(Count + 1) } ContentPadding="12,4">Go</Button>',
    "\t\t\t{ RUI::TextBlock(FString(TEXT(\"hi\"))) }",
    "\t\t\t@for (int32 i = 0; i < 3; ++i) {",
    "\t\t\t\tconst FString Row = FString::FromInt(i);",
    "\t\t\t\treturn ( <TextBlock Text={ Row } /> );",
    "\t\t\t}",
    "\t\t</VerticalBox>",
    "\t);",
    "}",
    "",
  ].join("\n");
  const vd = buildVirtualCpp(src, "Lift");
  assert.ok(vd.text.includes("(void)[=](const FRuiValue& Value)"), "event handler lifted in codegen's lambda shape");
  assert.ok(vd.text.includes("for (int32 i = 0; i < 3; ++i) {"), "directive header lifted as a REAL loop");
  assert.ok(vd.text.includes("const FString Row = FString::FromInt(i);"), "directive-body statement lifted inside the loop scope");
  assert.ok(vd.text.includes('RUI::TextBlock(FString(TEXT("hi")))'), "expr child lifted");
  // Round-trip: an offset INSIDE the OnClicked expression maps into the virtual doc and back.
  const at = src.indexOf("SetCount(Count + 1)");
  const vir = vd.map.sourceToVirtual(at);
  assert.ok(vir !== null && vd.map.virtualToSource(vir) === at, "attr-expr offsets round-trip");
  // Empty-setup components still scaffold (HelloWorld shape).
  const hw = buildVirtualCpp('component Tiny { return ( <Spacer Size={ FVector2D(1.0f, 2.0f) } /> ); }', "Tiny");
  assert.ok(hw.text.includes("__rui_setup_Tiny"), "empty-setup scaffold present");
  assert.ok(hw.text.includes("FVector2D(1.0f, 2.0f)"), "its attr expr lifted");
});

test("§2 prefix: qualified real-header hooks get decltype adapters; imported components declared", () => {
  const src = [
    'import "@Doom/DoomGameHook.h"',
    "export component Q {",
    "\tauto View = RuiDoom::UseDoomGame(1, 2, 3);",
    "\treturn ( <Spacer /> );",
    "}",
    "",
  ].join("\n");
  const vd = buildVirtualCpp(src, "Q");
  assert.ok(
    vd.text.includes("namespace RuiDoom { template <typename... TArgs> auto UseDoomGame(TArgs&&... Args) -> decltype(UseDoomGame(Ctx, static_cast<TArgs&&>(Args)...)); }"),
    "qualified-hook adapter emitted inside its namespace",
  );
  // A comment mentioning a qualified hook must NOT produce an adapter (code-aware scan).
  const commented = buildVirtualCpp('component C {\n\t// see Fake::UseGhost(1) for details\n\treturn ( <Spacer /> );\n}\n', "C");
  // The comment itself rides along in the verbatim setup region — only the ADAPTER must not exist.
  assert.ok(!commented.text.includes("decltype(UseGhost"), "commented pseudo-call produces no adapter");
  // Imported components appear as fully-defaulted wrapper declarations via the resolver.
  const resolver = () => ({ hooks: [], modules: [], components: ["RouterHome"] });
  const withComp = buildVirtualCpp('import { RouterHome } from "./R"\ncomponent W {\n\tauto N = RouterHome();\n\treturn ( <Spacer /> );\n}\n', "W", resolver);
  assert.ok(withComp.text.includes("FRuiNode RouterHome();"), "imported component declared");
});

test("§2 sanitizer: rsp expansion honors quotes; SharedPCH dropped; system includes appended", () => {
  const { tokenizeRspLine, sanitizeCompileCommands, mirrorNeedsRefresh } = require("../clangdProxy") as typeof import("../clangdProxy");
  assert.deepStrictEqual(tokenizeRspLine('/I "C:\\Program Files\\X" '), ["/I", "C:\\Program Files\\X"]);
  assert.deepStrictEqual(tokenizeRspLine("/DFOO=1"), ["/DFOO=1"]);
  assert.deepStrictEqual(tokenizeRspLine(""), []);

  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "uetkx-sani-"));
  const rsp = path.join(dir, "M.1.rsp");
  fs.writeFileSync(
    rsp,
    '/FI "C:\\x\\Definitions.M.h" \n/FI "C:\\x\\SharedPCH.Big.h" \n/I "C:\\Program Files\\Inc" \n/std:c++20\n',
  );
  const db = path.join(dir, "compileCommands_T.json");
  fs.writeFileSync(db, JSON.stringify([{ file: "C:\\x\\a.cpp", directory: "C:\\x", arguments: ["cl.exe", `@${rsp}`] }]));
  const entries = sanitizeCompileCommands(db);
  const args = entries[0].arguments;
  assert.ok(!args.some((a) => /SharedPCH/i.test(a)), "SharedPCH force-include dropped");
  assert.ok(args.includes("C:\\x\\Definitions.M.h"), "Definitions force-include kept");
  assert.ok(args.includes("C:\\Program Files\\Inc"), "quoted path with spaces survives");

  // mirrorNeedsRefresh: rsp-form and command-form targets self-heal; sanitized form does not.
  const target = path.join(dir, "compile_commands.json");
  fs.writeFileSync(target, JSON.stringify([{ file: "a", directory: "b", arguments: ["cl.exe", "@x.rsp"] }]));
  assert.strictEqual(mirrorNeedsRefresh(db, target), true, "unexpanded @rsp target refreshes");
  fs.writeFileSync(target, JSON.stringify(entries, null, "\t"));
  assert.strictEqual(mirrorNeedsRefresh(db, target), false, "sanitized target is stable");
  fs.rmSync(dir, { recursive: true, force: true });
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

test("clangd pump guard: a throwing diagnostics consumer surfaces via onError, never escapes (§1)", () => {
  const proxy = new ClangdProxy("unused");
  const errors: string[] = [];
  proxy.onError = (context) => errors.push(context);
  proxy.onPublishDiagnostics = () => {
    throw new Error("consumer bug");
  };
  const frame = JSON.stringify({ jsonrpc: "2.0", method: "textDocument/publishDiagnostics", params: { uri: "x", diagnostics: [] } });
  const chunk = Buffer.from(`Content-Length: ${Buffer.byteLength(frame)}\r\n\r\n${frame}`);
  // Reach the private pump the way the process would: via the data path.
  (proxy as unknown as { onData(c: Buffer): void }).onData(chunk);
  assert.deepStrictEqual(errors, ["clangd message pump"], "the throw was contained and reported");
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

// ── §4 markup-everywhere: the virtual doc is jsx-aware everywhere code is emitted ──────────

test("virtual doc: value markup in setup neutralizes to __rui_rn and lifts its expressions", () => {
  const source = [
    "export component CardStack {",
    "\tint32 Total = 3;",
    "\tauto Card = (<VerticalBox><TextBlock Text={ FText::AsNumber(Total) } /></VerticalBox>);",
    "\tFRuiNode Bare = <Spacer />;",
    "\treturn (",
    "\t\t<Border>",
    "\t\t\t{ Card }",
    "\t\t</Border>",
    "\t);",
    "}",
    "",
  ].join("\n");
  const doc = buildVirtualCpp(source, "CardStack");
  assert.ok(!doc.text.includes("<VerticalBox"), "no raw markup reaches the virtual TU");
  assert.ok(!doc.text.includes("<Spacer"), "bare `=` markup neutralized too");
  assert.ok(doc.text.includes("auto Card = (__rui_rn);"), "paren value markup reads as a typed placeholder");
  assert.ok(doc.text.includes("FRuiNode Bare = __rui_rn;"), "bare value markup reads as a typed placeholder");
  assert.ok(doc.text.includes("extern FRuiNode __rui_rn;"), "the placeholder is declared in the prelude");
  // the nested attr expr got its own mapped region
  const attrExpr = "\nFText::AsNumber(Total)\n";
  const virAt = doc.text.indexOf(attrExpr);
  assert.ok(virAt >= 0, "nested attr expr lifted");
  const srcAt = doc.map.virtualToSource(virAt + 1);
  assert.strictEqual(srcAt, source.indexOf("FText::AsNumber(Total)"), "lifted expr maps byte-exactly to source");
});

test("virtual doc: short-circuit markup in an attr expr desugars to a ternary placeholder", () => {
  const source = [
    "export component Gate(bOpen: bool = false) {",
    "\treturn (",
    "\t\t<Border Content={ bOpen && <TextBlock Text={ GetLabel() } /> } />",
    "\t);",
    "}",
    "",
  ].join("\n");
  const doc = buildVirtualCpp(source, "Gate");
  assert.ok(!doc.text.includes("<TextBlock"), "nested markup neutralized");
  assert.ok(doc.text.includes("? __rui_rn : __rui_rn"), "short-circuit desugars like codegen's ternary");
  assert.ok(doc.text.includes("\nGetLabel()\n"), "the nested markup's own attr expr still lifts");
});

test("virtual doc: statement-level early markup returns neutralize without double-lifting", () => {
  const source = [
    "export component Early(Flag: bool = false) {",
    "\tif (Flag) {",
    "\t\treturn ( <TextBlock Text={ FText::AsNumber(1) } /> );",
    "\t}",
    "\treturn (",
    "\t\t<Box />",
    "\t);",
    "}",
    "",
  ].join("\n");
  const doc = buildVirtualCpp(source, "Early");
  assert.ok(!doc.text.includes("<TextBlock"), "early-return window neutralized in setup");
  assert.ok(doc.text.includes("return ( __rui_rn );") || doc.text.includes("return (__rui_rn);"), "the early return still reads as a real return");
  const first = doc.text.indexOf("FText::AsNumber(1)");
  const second = doc.text.indexOf("FText::AsNumber(1)", first + 1);
  assert.ok(first >= 0, "the early window's attr expr lifts once (via comp.returns)");
  assert.strictEqual(second, -1, "…and exactly once — no duplicate region");
});
