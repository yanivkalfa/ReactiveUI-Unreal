// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TD-020 server wiring — the routing that sends embedded-C++ positions to the proxy over the
// virtual document, translates back, and degrades to the markup baseline when clangd is absent.

import { test } from "node:test";
import * as assert from "node:assert";
import {
  buildEmbeddedView,
  isEmbeddedOffset,
  embeddedPositionRequest,
  translateEmbeddedCompletion,
  virtualUriOf,
  type PositionResponder,
} from "../embeddedIntel";
import type { ClangdPosition } from "../clangdProxy";

const SOURCE = [
  'import { X } from "./x"', // line 0 — preamble (NOT embedded)
  "",
  "export hook UseThing(int32 Start) -> int32 {", // line 2 — decl header (NOT embedded)
  "\tconst int32 Doubled = Start * 2;", // line 3 — embedded C++ body
  "\treturn Doubled;",
  "}",
].join("\n");

/** A stub responder that records what it was asked and returns a canned result. */
class StubResponder implements PositionResponder {
  available: boolean;
  opened: { uri: string; text: string }[] = [];
  requests: { method: string; uri: string; position: ClangdPosition }[] = [];
  result: unknown;
  constructor(available: boolean, result: unknown) {
    this.available = available;
    this.result = result;
  }
  isAvailable(): boolean {
    return this.available;
  }
  didOpen(uri: string, text: string): void {
    this.opened.push({ uri, text });
  }
  async positionRequest(method: string, uri: string, position: ClangdPosition): Promise<unknown | null> {
    this.requests.push({ method, uri, position });
    return this.result;
  }
}

test("isEmbeddedOffset: true inside a body, false in preamble / decl header", () => {
  const bodyOffset = SOURCE.indexOf("Doubled = Start");
  assert.ok(bodyOffset > 0);
  assert.strictEqual(isEmbeddedOffset(SOURCE, bodyOffset), true);

  assert.strictEqual(isEmbeddedOffset(SOURCE, SOURCE.indexOf("import")), false);
  assert.strictEqual(isEmbeddedOffset(SOURCE, SOURCE.indexOf("export hook")), false);
});

test("buildEmbeddedView: source <-> virtual position round-trips inside the body", () => {
  const view = buildEmbeddedView(SOURCE);
  assert.ok(view.regionCount >= 1);
  const off = SOURCE.indexOf("Doubled = Start");
  const vpos = view.positionInVirtual(off);
  assert.notStrictEqual(vpos, null, "an embedded offset maps into the virtual doc");
  assert.strictEqual(view.sourceOffsetOf(vpos!), off, "and maps back to the same source offset");

  // A preamble offset does not map.
  assert.strictEqual(view.positionInVirtual(SOURCE.indexOf("import")), null);
});

test("embeddedPositionRequest: forwards an embedded position to the proxy over the virtual uri", async () => {
  const proxy = new StubResponder(true, { contents: "int32" });
  const off = SOURCE.indexOf("Doubled = Start");
  const result = await embeddedPositionRequest(proxy, "textDocument/hover", "file:///a.uetkx", SOURCE, off);

  assert.deepStrictEqual(result, { contents: "int32" }, "returns the proxy result");
  assert.strictEqual(proxy.requests.length, 1, "queried the proxy exactly once");
  assert.strictEqual(proxy.requests[0].method, "textDocument/hover");
  assert.strictEqual(proxy.requests[0].uri, virtualUriOf("file:///a.uetkx"), "used the virtual uri");
  assert.strictEqual(proxy.opened.length, 1, "opened the virtual doc first");
  assert.ok(proxy.opened[0].text.includes("Doubled = Start"), "opened the synthesized C++");
});

test("embeddedPositionRequest: a markup position is never forwarded (returns null)", async () => {
  const proxy = new StubResponder(true, { contents: "should not be used" });
  const off = SOURCE.indexOf("import");
  const result = await embeddedPositionRequest(proxy, "textDocument/hover", "file:///a.uetkx", SOURCE, off);
  assert.strictEqual(result, null);
  assert.strictEqual(proxy.requests.length, 0, "the proxy was never consulted for a markup position");
});

test("embeddedPositionRequest: graceful degradation — unavailable proxy returns null, never queries", async () => {
  const proxy = new StubResponder(false, { contents: "unreachable" });
  const off = SOURCE.indexOf("Doubled = Start");
  const result = await embeddedPositionRequest(proxy, "textDocument/hover", "file:///a.uetkx", SOURCE, off);
  assert.strictEqual(result, null);
  assert.strictEqual(proxy.requests.length, 0);
  assert.strictEqual(proxy.opened.length, 0);
});

// ── completion translation (the TD-020 tail) ────────────────────────────────────────────────

/** A virtual-doc range spanning `length` chars starting at the virtual position of `sourceOffset`. */
function virtualRangeAt(sourceOffset: number, length: number) {
  const view = buildEmbeddedView(SOURCE);
  const start = view.positionInVirtual(sourceOffset)!;
  const end = view.positionInVirtual(sourceOffset + length)!;
  assert.ok(start && end, "test range must sit inside an embedded region");
  return { start, end };
}

test("translateEmbeddedCompletion: a textEdit range maps back to .uetkx coordinates", () => {
  const off = SOURCE.indexOf("Doubled = Start");
  const result = [
    { label: "Doubled", kind: 6, textEdit: { range: virtualRangeAt(off, "Doubled".length), newText: "Doubled" } },
  ];
  const translated = translateEmbeddedCompletion(result, SOURCE);
  assert.notStrictEqual(translated, null);
  assert.strictEqual(translated!.isIncomplete, false);
  assert.strictEqual(translated!.items.length, 1);
  const item = translated!.items[0] as { label: string; textEdit: { range: { start: { line: number; character: number } }; newText: string } };
  assert.strictEqual(item.label, "Doubled");
  // SOURCE line 3 is the body line holding "Doubled" — the range must be in SOURCE coordinates.
  assert.strictEqual(item.textEdit.range.start.line, 3);
  assert.strictEqual(item.textEdit.range.start.character, SOURCE.split("\n")[3].indexOf("Doubled"));
});

test("translateEmbeddedCompletion: the insert/replace edit form maps both ranges", () => {
  const off = SOURCE.indexOf("Doubled = Start");
  const result = {
    isIncomplete: true,
    items: [
      {
        label: "Doubled",
        textEdit: {
          insert: virtualRangeAt(off, 3),
          replace: virtualRangeAt(off, "Doubled".length),
          newText: "Doubled",
        },
      },
    ],
  };
  const translated = translateEmbeddedCompletion(result, SOURCE);
  assert.notStrictEqual(translated, null);
  assert.strictEqual(translated!.isIncomplete, true, "isIncomplete passes through");
  const edit = (translated!.items[0] as { textEdit: { insert: { start: { line: number } }; replace: object } }).textEdit;
  assert.strictEqual(edit.insert.start.line, 3);
  assert.ok(edit.replace, "replace range survives");
});

test("translateEmbeddedCompletion: prelude-scaffolding edits are dropped from the item, not the item itself", () => {
  const preludeRange = { start: { line: 0, character: 0 }, end: { line: 0, character: 4 } }; // the prelude comment
  const result = [
    {
      label: "FVector",
      textEdit: { range: preludeRange, newText: "FVector" },
      additionalTextEdits: [{ range: preludeRange, newText: '#include "Math/Vector.h"\n' }],
    },
  ];
  const translated = translateEmbeddedCompletion(result, SOURCE);
  assert.notStrictEqual(translated, null);
  const item = translated!.items[0] as { label: string; textEdit?: unknown; additionalTextEdits?: unknown };
  assert.strictEqual(item.label, "FVector", "the item survives");
  assert.strictEqual(item.textEdit, undefined, "the untranslatable edit is dropped");
  assert.strictEqual(item.additionalTextEdits, undefined, "auto-include edits never pass through");
});

test("translateEmbeddedCompletion: empty or absent results fall through to the baseline (null)", () => {
  assert.strictEqual(translateEmbeddedCompletion(null, SOURCE), null);
  assert.strictEqual(translateEmbeddedCompletion([], SOURCE), null);
  assert.strictEqual(translateEmbeddedCompletion({ isIncomplete: false, items: [] }, SOURCE), null);
});

// ── embedded rename translation (LSP_COMPLETION_PLAN N6, decision N-10) ─────────────────────

test("translateEmbeddedRename: clangd edits over the virtual doc map back to .uetkx, all occurrences", async () => {
  const { translateEmbeddedRename } = await import("../embeddedIntel");
  const uri = "file:///a.uetkx";
  // Rename the local `Doubled` (decl + return use) — two virtual-range edits, as clangd sends.
  const declOff = SOURCE.indexOf("Doubled = Start");
  const useOff = SOURCE.indexOf("Doubled;", declOff);
  const result = translateEmbeddedRename(
    {
      changes: {
        [virtualUriOf(uri)]: [
          { range: virtualRangeAt(declOff, "Doubled".length), newText: "Tripled" },
          { range: virtualRangeAt(useOff, "Doubled".length), newText: "Tripled" },
        ],
      },
    },
    uri,
    SOURCE,
  );
  assert.ok(result && result.kind === "edits", "translated to edits");
  assert.strictEqual(result.edits.length, 2);
  // The translated ranges point at the .uetkx occurrences (line/char of the source text).
  const lineOfDecl = SOURCE.slice(0, declOff).split("\n").length - 1;
  assert.strictEqual(result.edits[0].range.start.line, lineOfDecl);
  assert.strictEqual(result.edits.every((e) => e.newText === "Tripled"), true);
});

test("translateEmbeddedRename: an edit in another file (engine header) refuses the WHOLE rename", async () => {
  const { translateEmbeddedRename } = await import("../embeddedIntel");
  const uri = "file:///a.uetkx";
  const declOff = SOURCE.indexOf("Doubled = Start");
  const result = translateEmbeddedRename(
    {
      changes: {
        [virtualUriOf(uri)]: [{ range: virtualRangeAt(declOff, 7), newText: "X" }],
        "file:///C:/Engine/Source/Runtime/Core/Public/Math/Color.h": [
          { range: { start: { line: 0, character: 0 }, end: { line: 0, character: 1 } }, newText: "X" },
        ],
      },
    },
    uri,
    SOURCE,
  );
  assert.ok(result && result.kind === "refused", "refused, not partially applied");
  assert.match(result.reason, /outside this file/);
});

test("translateEmbeddedRename: an edit landing in generated glue refuses the WHOLE rename", async () => {
  const { translateEmbeddedRename } = await import("../embeddedIntel");
  const uri = "file:///a.uetkx";
  const declOff = SOURCE.indexOf("Doubled = Start");
  const result = translateEmbeddedRename(
    {
      changes: {
        [virtualUriOf(uri)]: [
          { range: virtualRangeAt(declOff, 7), newText: "X" },
          // {0,0} in the virtual doc is prelude scaffolding — unmapped by the source map.
          { range: { start: { line: 0, character: 0 }, end: { line: 0, character: 1 } }, newText: "X" },
        ],
      },
    },
    uri,
    SOURCE,
  );
  assert.ok(result && result.kind === "refused");
  assert.match(result.reason, /generated glue/);
});

test("translateEmbeddedRename: null result / empty edit set degrade to null (no crash, no edit)", async () => {
  const { translateEmbeddedRename } = await import("../embeddedIntel");
  assert.strictEqual(translateEmbeddedRename(null, "file:///a.uetkx", SOURCE), null);
  assert.strictEqual(translateEmbeddedRename({}, "file:///a.uetkx", SOURCE), null);
  assert.strictEqual(translateEmbeddedRename({ changes: {} }, "file:///a.uetkx", SOURCE), null);
});

test("embeddedPositionRequest: rename's newName rides the extra params to the proxy", async () => {
  class ExtraRecorder extends StubResponder {
    extras: unknown[] = [];
    async positionRequest(method: string, uri: string, position: ClangdPosition, extra?: object): Promise<unknown | null> {
      this.extras.push(extra);
      return super.positionRequest(method, uri, position);
    }
  }
  const proxy = new ExtraRecorder(true, { changes: {} });
  const off = SOURCE.indexOf("Doubled = Start");
  await embeddedPositionRequest(proxy, "textDocument/rename", "file:///a.uetkx", SOURCE, off, { newName: "Tripled" });
  assert.deepStrictEqual(proxy.extras, [{ newName: "Tripled" }]);
});

// ── F5 field-test fixes (2026-07-18): hand-header hooks, event sinks, texture types ─────────

test("virtual doc: a bare hand-header hook call gets the variadic Ctx adapter", () => {
  const src = [
    "export FRuiNode RouterHome() {",
    "\tauto Navigate = UseNavigate();",
    "\tconst FString Path = UsePathname();",
    "\treturn ( <Spacer /> );",
    "}",
    "",
  ].join("\n");
  const { buildVirtualCpp } = require("../virtualDoc") as typeof import("../virtualDoc");
  const vd = buildVirtualCpp(src, "RouterHome");
  assert.ok(
    vd.text.includes("auto UseNavigate(TArgs&&... Args) -> decltype(UseNavigate(Ctx, static_cast<TArgs&&>(Args)...));"),
    "UseNavigate adapter emitted",
  );
  assert.ok(vd.text.includes("decltype(UsePathname(Ctx,"), "UsePathname adapter emitted");
  // built-ins stay #define'd, never adapted (the adapter would fight the macro)
  assert.ok(!vd.text.includes("decltype(UseState(Ctx,"), "no adapter for a built-in");
});

test("virtual doc: same-file and imported hooks keep source arity (no adapter)", () => {
  const src = [
    "export int32 UseTick(int32 Start) {",
    "\treturn Start;",
    "}",
    "export FRuiNode Panel() {",
    "\tauto V = UseTick(1);",
    "\treturn ( <Spacer /> );",
    "}",
    "",
  ].join("\n");
  const { buildVirtualCpp } = require("../virtualDoc") as typeof import("../virtualDoc");
  const vd = buildVirtualCpp(src, "Panel");
  assert.ok(!vd.text.includes("decltype(UseTick(Ctx,"), "same-file hook keeps its lifted arity");
});

test("virtual doc: event-attr lifts sink into (void)(…) — bare callback values never warn", () => {
  const src = [
    "export FRuiNode Menu() {",
    "\tFRuiCallback OnStart = UseStableAction([](const FRuiValue&) {});",
    "\treturn ( <Button OnClicked={ OnStart }>go</Button> );",
    "}",
    "",
  ].join("\n");
  const { buildVirtualCpp } = require("../virtualDoc") as typeof import("../virtualDoc");
  const vd = buildVirtualCpp(src, "Menu");
  assert.ok(vd.text.includes("(void)[=](const FRuiValue& Value) { (void)("), "event lambda body is a void sink");
});

test("virtual doc: the prelude carries a guarded Engine/Texture2D.h (fwd-declared UTexture2D completes)", () => {
  const { buildVirtualCpp } = require("../virtualDoc") as typeof import("../virtualDoc");
  const vd = buildVirtualCpp("export FRuiNode T() {\n\treturn ( <Spacer /> );\n}\n", "T");
  assert.ok(vd.text.includes('#include "Engine/Texture2D.h"'), "guarded texture include present");
});

test("virtual doc: hook adapters are recursion-guarded (a TYPO'D hook must not fatal the TU)", async () => {
  // Field test round 4: the unguarded adapter's decltype found ITSELF for a nonexistent
  // hook ('recursive template instantiation exceeded maximum depth' — a FATAL that killed
  // every other diagnostic in the file). The __rui_ctx_first guard SFINAEs the adapter out
  // once the decltype re-enters with Ctx first.
  const { buildVirtualCpp } = await import("../virtualDoc");
  const vd = buildVirtualCpp("export FRuiNode T() {\n\tauto A = UseSsstate<bool>(true);\n\treturn ( <Spacer /> );\n}\n", "T");
  assert.ok(vd.text.includes("__rui_ctx_first<TArgs...>"), "the guard rides every adapter");
  assert.ok(!/template <typename\.\.\. TArgs> auto UseSsstate/.test(vd.text), "no unguarded adapter shape");
  assert.ok(vd.text.includes("__is_same(typename __rui_strip"), "the Ctx-first detector glue is emitted");
});
