// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// §1 (MARKUP_EVERYWHERE_PLAN) — the STDIO end-to-end test: spawn the ACTUAL compiled server,
// speak real LSP frames, and assert diagnostics ARRIVE. Exists because two bugs shipped that
// were invisible to unit tests ("works in the harness, dead in the editor"): the URI
// re-serialization silent drop, and server-process death taking every feature dark. The doc
// URI deliberately uses VS Code's percent-encoded drive spelling (`c%3A`) so the round-trip
// path is the one real clients exercise.

import { test } from "node:test";
import * as assert from "node:assert";
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { spawn } from "node:child_process";
import { drainMessages } from "../clangdProxy";

const SERVER = path.join(__dirname, "..", "server.js");

function vsCodeStyleUri(fsPath: string): string {
  // file:///c%3A/dir/file — lowercase drive, percent-encoded colon (VS Code's spelling).
  const p = fsPath.replace(/\\/g, "/");
  const m = /^([A-Za-z]):(.*)$/.exec(p);
  if (!m) return "file://" + p;
  const tail = m[2]
    .split("/")
    .map((seg) => encodeURIComponent(seg))
    .join("/");
  return `file:///${m[1].toLowerCase()}%3A${tail}`;
}

test("stdio E2E: real server publishes markup diagnostics for a mangled document", async () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "uetkx-e2e-"));
  fs.writeFileSync(path.join(root, "Demo.uproject"), "{}");
  const file = path.join(root, "Broken.uetkx");
  // Open tag mangled, close tag left alone — the owner's exact repro shape (UETKX0302).
  const source = "component Broken {\n\treturn (\n\t\t<Bordesr Padding=\"12\">\n\t\t\t<Spacer />\n\t\t</Border>\n\t);\n}\n";
  fs.writeFileSync(file, source);
  const uri = vsCodeStyleUri(file);

  const proc = spawn(process.execPath, [SERVER, "--stdio"], { stdio: "pipe" });
  let buffer: Buffer = Buffer.alloc(0);
  const inbox: unknown[] = [];
  const waiters: Array<() => void> = [];
  proc.stdout.on("data", (chunk: Buffer) => {
    buffer = Buffer.concat([buffer, chunk]);
    const { messages, rest } = drainMessages(buffer);
    buffer = rest;
    inbox.push(...messages);
    for (const w of waiters.splice(0)) w();
  });
  const send = (payload: object) => {
    const body = JSON.stringify(payload);
    proc.stdin.write(`Content-Length: ${Buffer.byteLength(body, "utf8")}\r\n\r\n${body}`);
  };
  const nextMessage = () => new Promise<void>((resolve) => waiters.push(resolve));
  const until = async <T>(pick: () => T | undefined, timeoutMs: number, what: string): Promise<T> => {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
      const hit = pick();
      if (hit !== undefined) return hit;
      if (Date.now() > deadline) throw new Error(`E2E timeout waiting for ${what}; inbox=${JSON.stringify(inbox).slice(0, 600)}`);
      await Promise.race([nextMessage(), new Promise((r) => setTimeout(r, 250))]);
    }
  };

  try {
    send({ jsonrpc: "2.0", id: 1, method: "initialize", params: { processId: process.pid, rootUri: null, capabilities: {} } });
    await until(
      () => inbox.find((m) => (m as { id?: number }).id === 1),
      10_000,
      "initialize result",
    );
    send({ jsonrpc: "2.0", method: "initialized", params: {} });
    send({
      jsonrpc: "2.0",
      method: "textDocument/didOpen",
      params: { textDocument: { uri, languageId: "uetkx", version: 1, text: source } },
    });

    type Publish = { method?: string; params?: { uri: string; diagnostics: Array<{ code?: string }> } };
    const publish = await until(
      () => {
        const hit = inbox.find((m) => {
          const p = m as Publish;
          return p.method === "textDocument/publishDiagnostics" && (p.params?.diagnostics.length ?? 0) > 0;
        }) as Publish | undefined;
        return hit?.params;
      },
      15_000,
      "a non-empty publishDiagnostics",
    );
    assert.ok(
      publish.diagnostics.some((d) => d.code === "UETKX0302"),
      `expected UETKX0302 in ${JSON.stringify(publish.diagnostics)}`,
    );

    // §1 survival check: a hover at a hostile position must not kill the server, and the
    // server must still answer afterwards.
    send({ jsonrpc: "2.0", id: 2, method: "textDocument/hover", params: { textDocument: { uri }, position: { line: 0, character: 0 } } });
    await until(() => inbox.find((m) => (m as { id?: number }).id === 2), 10_000, "hover response (server alive)");
    assert.strictEqual(proc.exitCode, null, "server process is still running");
  } finally {
    proc.kill();
    fs.rmSync(root, { recursive: true, force: true });
  }
});
