// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TD-020 server wiring — the routing that sends embedded-C++ positions to the proxy over the
// virtual document, translates back, and degrades to the markup baseline when clangd is absent.

import { test } from "node:test";
import * as assert from "node:assert";
import {
  buildEmbeddedView,
  isEmbeddedOffset,
  embeddedPositionRequest,
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
