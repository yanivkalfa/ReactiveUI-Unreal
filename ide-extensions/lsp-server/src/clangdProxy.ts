// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-020 — the clangd child-process proxy. Forwards hover/completion/definition for the C++
// VIRTUAL document (virtualDoc.ts) to a clangd instance, discovered flags via a compile_commands
// .json walk-up. GRACEFUL DEGRADATION is the contract: with no clangd on PATH (or a spawn/parse
// failure) the proxy reports unavailable and every request returns null, so the LSP falls back to
// the fully-supported markup-only baseline (the family's documented degradation notice) — never
// an error.

import { spawn, ChildProcessWithoutNullStreams } from "child_process";
import * as fs from "fs";
import * as path from "path";

export interface ClangdPosition {
  line: number;
  character: number;
}

/** Walk up from `startDir` for a compile_commands.json (the clang tooling DB). Null when none. */
export function findCompileCommands(startDir: string): string | null {
  let dir = path.resolve(startDir);
  for (;;) {
    const direct = path.join(dir, "compile_commands.json");
    if (fs.existsSync(direct)) {
      return direct;
    }
    const buildDir = path.join(dir, "build", "compile_commands.json");
    if (fs.existsSync(buildDir)) {
      return buildDir;
    }
    const parent = path.dirname(dir);
    if (parent === dir) {
      return null;
    }
    dir = parent;
  }
}

/** Length-prefixed LSP message framing over a byte buffer; returns parsed messages + the tail. */
function drainMessages(buffer: string): { messages: unknown[]; rest: string } {
  const messages: unknown[] = [];
  let rest = buffer;
  for (;;) {
    const headerEnd = rest.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
      break;
    }
    const header = rest.slice(0, headerEnd);
    const match = /Content-Length:\s*(\d+)/i.exec(header);
    if (!match) {
      rest = rest.slice(headerEnd + 4);
      continue;
    }
    const length = parseInt(match[1], 10);
    const bodyStart = headerEnd + 4;
    if (rest.length < bodyStart + length) {
      break; // wait for more bytes
    }
    const body = rest.slice(bodyStart, bodyStart + length);
    rest = rest.slice(bodyStart + length);
    try {
      messages.push(JSON.parse(body));
    } catch {
      /* ignore a malformed frame */
    }
  }
  return { messages, rest };
}

export class ClangdProxy {
  private proc: ChildProcessWithoutNullStreams | null = null;
  private buffer = "";
  private nextId = 1;
  private readonly pending = new Map<number, (result: unknown) => void>();
  private available = false;
  private starting: Promise<boolean> | null = null;

  constructor(
    private readonly clangdPath = "clangd",
    private readonly rootPath = process.cwd(),
  ) {}

  isAvailable(): boolean {
    return this.available;
  }

  /** Spawn + initialize clangd. Resolves false (never rejects) when clangd is unavailable. */
  async start(): Promise<boolean> {
    if (this.starting) {
      return this.starting;
    }
    this.starting = new Promise<boolean>((resolve) => {
      let proc: ChildProcessWithoutNullStreams;
      try {
        const args = ["--log=error"];
        const cc = findCompileCommands(this.rootPath);
        if (cc) {
          args.push(`--compile-commands-dir=${path.dirname(cc)}`);
        }
        proc = spawn(this.clangdPath, args, { stdio: "pipe" });
      } catch {
        resolve(false);
        return;
      }
      proc.on("error", () => {
        this.available = false;
        resolve(false);
      });
      proc.stdout.setEncoding("utf8");
      proc.stdout.on("data", (chunk: string) => this.onData(chunk));
      this.proc = proc;

      this.request("initialize", {
        processId: process.pid,
        rootUri: null,
        capabilities: {},
      })
        .then(() => {
          this.notify("initialized", {});
          this.available = true;
          resolve(true);
        })
        .catch(() => {
          this.available = false;
          resolve(false);
        });
    });
    return this.starting;
  }

  /** Open the virtual C++ doc so subsequent position requests resolve against it. */
  didOpen(uri: string, text: string): void {
    if (!this.available) {
      return;
    }
    this.notify("textDocument/didOpen", {
      textDocument: { uri, languageId: "cpp", version: 1, text },
    });
  }

  /** Forward a position request; null on any degradation (unavailable / timeout / error). */
  async positionRequest(method: string, uri: string, position: ClangdPosition): Promise<unknown | null> {
    if (!this.available) {
      return null;
    }
    try {
      return await this.request(method, { textDocument: { uri }, position });
    } catch {
      return null;
    }
  }

  dispose(): void {
    this.available = false;
    this.pending.clear();
    if (this.proc) {
      this.proc.kill();
      this.proc = null;
    }
  }

  // ── framing ─────────────────────────────────────────────────────────────────────────────

  private onData(chunk: string): void {
    this.buffer += chunk;
    const { messages, rest } = drainMessages(this.buffer);
    this.buffer = rest;
    for (const msg of messages) {
      const m = msg as { id?: number; result?: unknown };
      if (typeof m.id === "number" && this.pending.has(m.id)) {
        const resolve = this.pending.get(m.id)!;
        this.pending.delete(m.id);
        resolve(m.result ?? null);
      }
    }
  }

  private send(payload: object): void {
    if (!this.proc) {
      return;
    }
    const body = JSON.stringify(payload);
    this.proc.stdin.write(`Content-Length: ${Buffer.byteLength(body, "utf8")}\r\n\r\n${body}`);
  }

  private notify(method: string, params: object): void {
    this.send({ jsonrpc: "2.0", method, params });
  }

  private request(method: string, params: object): Promise<unknown> {
    const id = this.nextId++;
    return new Promise<unknown>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error("clangd request timed out"));
      }, 5000);
      this.pending.set(id, (result) => {
        clearTimeout(timer);
        resolve(result);
      });
      this.send({ jsonrpc: "2.0", id, method, params });
    });
  }
}
