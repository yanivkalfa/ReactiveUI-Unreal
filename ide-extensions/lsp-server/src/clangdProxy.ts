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

/** A clangd publishDiagnostics payload (virtual-doc coordinates — the server maps them back).
 *  `version` echoes the didOpen/didChange version the diagnostics were computed FOR — the N6
 *  rename guard synchronizes on it (stale diagnostics must never gate a fresh rename). */
export interface ClangdPublishedDiagnostics {
  uri: string;
  version?: number;
  diagnostics: Array<{
    range: { start: ClangdPosition; end: ClangdPosition };
    severity?: number;
    code?: string | number;
    source?: string;
    message: string;
    tags?: number[];
  }>;
}

/** Locate a clangd binary (TB-10): explicit setting → PATH → the extension's managed install
 *  (%LOCALAPPDATA%/uetkx/clangd) → common LLVM/VS locations. Returns null when none exists —
 *  the caller degrades to markup-only AND tells the user what was probed (the silent-darkness
 *  fix). PATH is probed via existsSync over PATH entries so an absent binary never costs a
 *  spawn. */
export function findClangd(explicitPath?: string): string | null {
  const exe = process.platform === "win32" ? "clangd.exe" : "clangd";
  if (explicitPath && explicitPath.trim().length > 0) {
    return fs.existsSync(explicitPath) ? explicitPath : null; // an explicit setting is authoritative
  }
  // §5: the extension-BUNDLED clangd — before any machine discovery ("completely self
  // sustained"). The server runs from <ext-root>/server/, so the bundle sits one level up:
  // the VS Code platform vsix ships clangd/<platform>-<arch>/…, the (single-platform) VS2022
  // VSIX ships clangd/…. In the dev tree (lsp-server/out/) neither exists — falls through.
  for (const rel of [
    ["..", "clangd", `${process.platform}-${process.arch}`, "bin", exe],
    ["..", "clangd", "bin", exe],
  ]) {
    const bundled = path.resolve(__dirname, ...rel);
    if (fs.existsSync(bundled)) {
      return bundled;
    }
  }
  for (const dir of (process.env.PATH ?? "").split(path.delimiter)) {
    if (dir && fs.existsSync(path.join(dir, exe))) {
      return path.join(dir, exe);
    }
  }
  const candidates: string[] = [];
  if (process.env.LOCALAPPDATA) {
    candidates.push(path.join(process.env.LOCALAPPDATA, "uetkx", "clangd", "bin", exe));
  }
  // The official clangd VS Code extension's managed install (@clangd/install → globalStorage):
  // if the user already has it, reuse its binary — newest version first.
  const appData = process.env.APPDATA ?? (process.env.HOME ? path.join(process.env.HOME, ".config") : null);
  if (appData) {
    for (const product of ["Code", "Code - Insiders", "VSCodium"]) {
      const store = path.join(appData, product, "User", "globalStorage", "llvm-vs-code-extensions.vscode-clangd", "install");
      try {
        const versions = fs.readdirSync(store).sort().reverse();
        for (const v of versions) {
          const inner = path.join(store, v);
          for (const sub of fs.readdirSync(inner)) {
            candidates.push(path.join(inner, sub, "bin", exe));
          }
        }
      } catch {
        /* not installed — keep probing */
      }
    }
  }
  if (process.platform === "win32") {
    candidates.push("C:\\Program Files\\LLVM\\bin\\clangd.exe");
    for (const edition of ["Community", "Professional", "Enterprise"]) {
      candidates.push(
        `C:\\Program Files\\Microsoft Visual Studio\\2022\\${edition}\\VC\\Tools\\Llvm\\x64\\bin\\clangd.exe`,
      );
    }
  }
  for (const c of candidates) {
    if (fs.existsSync(c)) {
      return c;
    }
  }
  return null;
}

/** Walk up from `startDir` for a compile database. Checks the canonical name first
 *  (compile_commands.json at the root or build/), then UBT's VSCODE-GENERATOR OUTPUT
 *  (.vscode/compileCommands_*.json) — so a stock `-projectfiles -vscode` run works with NO
 *  manual copy step (TB-10; the project-named file wins over compileCommands_Default). Null
 *  when none. */
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
    const vscodeDir = path.join(dir, ".vscode");
    if (fs.existsSync(vscodeDir)) {
      try {
        const generated = fs
          .readdirSync(vscodeDir)
          .filter((f) => f.startsWith("compileCommands_") && f.endsWith(".json"))
          .sort((a, b) => (a === "compileCommands_Default.json" ? 1 : b === "compileCommands_Default.json" ? -1 : a.localeCompare(b)));
        if (generated.length > 0) {
          return path.join(vscodeDir, generated[0]);
        }
      } catch {
        /* unreadable .vscode — keep walking */
      }
    }
    const parent = path.dirname(dir);
    if (parent === dir) {
      return null;
    }
    dir = parent;
  }
}

/** Length-prefixed LSP message framing over a BYTE buffer. `Content-Length` is a UTF-8 byte count, so
 *  framing MUST slice the Buffer by bytes — slicing a decoded UTF-16 string by that length desyncs the
 *  stream on any non-ASCII frame (a multi-byte char counts as 1 UTF-16 unit but >1 byte), silently
 *  killing embedded intel (bughunt LSP-1). The body is decoded as UTF-8 only after the byte slice. */
export function drainMessages(buffer: Buffer): { messages: unknown[]; rest: Buffer } {
  const messages: unknown[] = [];
  let rest = buffer;
  const separator = Buffer.from("\r\n\r\n");
  for (;;) {
    const headerEnd = rest.indexOf(separator);
    if (headerEnd < 0) {
      break;
    }
    const header = rest.subarray(0, headerEnd).toString("ascii");
    const match = /Content-Length:\s*(\d+)/i.exec(header);
    if (!match) {
      rest = rest.subarray(headerEnd + separator.length);
      continue;
    }
    const length = parseInt(match[1], 10);
    const bodyStart = headerEnd + separator.length;
    if (rest.length < bodyStart + length) {
      break; // wait for more bytes
    }
    const body = rest.subarray(bodyStart, bodyStart + length).toString("utf8");
    rest = rest.subarray(bodyStart + length);
    try {
      messages.push(JSON.parse(body));
    } catch {
      /* ignore a malformed frame */
    }
  }
  return { messages, rest };
}

/** §2 root-cause fix: UBT's VSCode-generator commands force-include the module's SharedPCH
 *  HEADER — meant for MSVC's compiled-PCH machinery. Re-parsing that everything-header under
 *  clang fails partway and silently truncates core engine templates (TFunction's internals
 *  came back "undefined"); every downstream `undeclared identifier` was cascade. The mirror
 *  therefore EXPANDS each entry's @rsp into explicit arguments and drops the `/FI <SharedPCH>`
 *  pair (Definitions.*.h — the macro set — stays). Explicit arguments also remove clangd's
 *  rsp-expansion variability. Exported for tests. */
/** Split one rsp line into arguments, honoring double quotes (`/I "C:\Program Files\X"` →
 *  two args, quotes stripped). Exported for tests. */
export function tokenizeRspLine(line: string): string[] {
  const out: string[] = [];
  let cur = "";
  let inQuotes = false;
  let sawAny = false;
  for (const ch of line) {
    if (ch === '"') {
      inQuotes = !inQuotes;
      sawAny = true;
      continue;
    }
    if (!inQuotes && (ch === " " || ch === "\t")) {
      if (sawAny) {
        out.push(cur);
        cur = "";
        sawAny = false;
      }
      continue;
    }
    cur += ch;
    sawAny = true;
  }
  if (sawAny) {
    out.push(cur);
  }
  return out;
}

export function sanitizeCompileCommands(dbPath: string): Array<{ file: string; directory: string; arguments: string[] }> {
  const raw = JSON.parse(fs.readFileSync(dbPath, "utf8")) as Array<{
    file: string;
    directory: string;
    arguments?: string[];
    command?: string;
  }>;
  const out: Array<{ file: string; directory: string; arguments: string[] }> = [];
  for (const entry of raw) {
    let args = entry.arguments ?? (entry.command ? entry.command.split(/\s+/) : []);
    // Expand response files (`@path.rsp`). UBT's rsp lines hold flag+value PAIRS with quoted
    // paths (`/I "C:\Program Files\…"`) — tokenize per line honoring double quotes; one
    // line is NOT one argument.
    const expanded: string[] = [];
    for (const a of args) {
      if (a.startsWith("@")) {
        try {
          const rsp = fs.readFileSync(a.slice(1), "utf8");
          for (const line of rsp.split(/\r?\n/)) {
            expanded.push(...tokenizeRspLine(line));
          }
        } catch {
          expanded.push(a); // unreadable rsp — keep the reference, clangd can try
        }
      } else {
        expanded.push(a);
      }
    }
    // Drop the SharedPCH force-include: `/FI <path>`, `/FIpath`, `-include <path>` forms.
    const cleaned: string[] = [];
    for (let i = 0; i < expanded.length; i++) {
      const a = expanded[i];
      const next = expanded[i + 1] ?? "";
      if ((a === "/FI" || a === "-include") && /SharedPCH/i.test(next)) {
        i++; // skip the path too
        continue;
      }
      if (/^(\/FI|-include)/.test(a) && /SharedPCH/i.test(a)) {
        continue;
      }
      cleaned.push(a);
    }
    // The rsp carries NO STL/SDK paths — real cl.exe gets them from its ENVIRONMENT, which
    // clangd's spawn doesn't have, and driver auto-detection is unreliable outside a VS
    // prompt ('type_traits' file not found was the root under every cascade). Derive them
    // deterministically: MSVC's include dir from the entry's own cl.exe path, the Windows
    // SDK from its standard install root; append as -imsvc (clang-cl system includes).
    cleaned.push(...systemIncludeArgs(cleaned[0] ?? ""));
    out.push({ file: entry.file, directory: entry.directory, arguments: cleaned });
  }
  return out;
}

let cachedSystemIncludes: string[] | null = null;
/** `-imsvc` args for the MSVC STL (derived from the cl.exe path: …\VC\Tools\MSVC\<ver>\bin\…
 *  → …\include) and the newest installed Windows 10/11 SDK (ucrt/shared/um/winrt). Cached —
 *  identical for every entry. Exported for tests. */
export function systemIncludeArgs(clExePath: string): string[] {
  if (cachedSystemIncludes) {
    return cachedSystemIncludes;
  }
  const args: string[] = [];
  const msvcMatch = /^(.*\\VC\\Tools\\MSVC\\(\d+)\.(\d+)[^\\]*)\\bin\\/i.exec(clExePath);
  if (msvcMatch && fs.existsSync(path.join(msvcMatch[1], "include"))) {
    args.push("-imsvc", path.join(msvcMatch[1], "include"));
    // clang assumes _MSC_VER 1933 by default; UE's headers static_assert on the REAL
    // toolchain version (14.44 → 1944). Derive it from the same path.
    args.push(`-fmsc-version=19${msvcMatch[3]}`);
  }
  for (const kitsRoot of ["C:\\Program Files (x86)\\Windows Kits\\10\\Include", "C:\\Program Files\\Windows Kits\\10\\Include"]) {
    try {
      const versions = fs
        .readdirSync(kitsRoot)
        .filter((v) => /^\d+\./.test(v))
        .sort()
        .reverse();
      if (versions.length > 0) {
        for (const sub of ["ucrt", "shared", "um", "winrt"]) {
          const p = path.join(kitsRoot, versions[0], sub);
          if (fs.existsSync(p)) {
            args.push("-imsvc", p);
          }
        }
        break;
      }
    } catch {
      /* kit root absent — try the next */
    }
  }
  cachedSystemIncludes = args;
  return args;
}

/** The mirror refreshes when the source is newer OR when the existing target is not in
 *  sanitized form: a visible SharedPCH force-include, an UNEXPANDED `@rsp` reference (which
 *  can hide the SharedPCH inside the response file), or `command`-string entries. Content-
 *  based, so stale pre-sanitizer mirrors self-heal regardless of mtimes. */
export function mirrorNeedsRefresh(srcPath: string, targetPath: string): boolean {
  if (!fs.existsSync(targetPath)) {
    return true;
  }
  try {
    if (fs.statSync(targetPath).mtimeMs < fs.statSync(srcPath).mtimeMs) {
      return true;
    }
    const txt = fs.readFileSync(targetPath, "utf8");
    return /SharedPCH/i.test(txt) || txt.includes('"@') || txt.includes('"command"');
  } catch {
    return true;
  }
}

export class ClangdProxy {
  private proc: ChildProcessWithoutNullStreams | null = null;
  private buffer: Buffer = Buffer.alloc(0);
  private nextId = 1;
  private readonly pending = new Map<number, (result: unknown) => void>();
  private available = false;
  private starting: Promise<boolean> | null = null;
  /** Open virtual docs → last sent version (didOpen once, didChange after — clangd re-runs
   *  diagnostics per version; re-sending didOpen would leak duplicate TUs). */
  private readonly openVersions = new Map<string, number>();
  /** Open virtual docs → hash of the last sent text: an identical re-send is a no-op (N6 —
   *  the rename path syncs the doc before its guard; without dedup every rename would bump
   *  the version and force a full clangd re-parse of unchanged text). */
  private readonly openTextHashes = new Map<string, number>();
  /** The last ~2KB of clangd's stderr — drained continuously (B5) and surfaced on exit. */
  private stderrTail = "";
  /** clangd's semantic-token legend (from its initialize result) — needed to decode
   *  semanticTokens/full payloads (R5: embedded-C++ coloring). */
  private tokenLegend: { tokenTypes: string[]; tokenModifiers: string[] } | null = null;

  /** TB-10: clangd's publishDiagnostics for a VIRTUAL doc land here (virtual coordinates —
   *  the server maps them back through the source map before publishing). */
  onPublishDiagnostics: ((params: ClangdPublishedDiagnostics) => void) | null = null;

  /** §1 crash-hardening: errors thrown INSIDE the stdout pump (incl. the publish callback)
   *  surface here instead of dying as an uncaughtException — the message pump is an
   *  EventEmitter context, OUTSIDE the jsonrpc layer's handler guards. */
  onError: ((context: string, error: unknown) => void) | null = null;

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
        // --background-index=false is LOAD-BEARING (F5 field test B3): with a UE compile
        // database present, clangd's default background indexer starts parsing the ENTIRE
        // project's TUs (thousands of engine-header compiles) and pegs every core for hours —
        // the whole server "barely moves". We only ever query our per-file virtual docs;
        // a cross-TU index of the engine buys nothing.
        const args = ["--log=error", "--background-index=false"];
        const cc = findCompileCommands(this.rootPath);
        if (cc) {
          // clangd only reads the CANONICAL filename from --compile-commands-dir. When the hit
          // is UBT's .vscode/compileCommands_*.json, mirror it to <root>/compile_commands.json
          // SANITIZED (no manual copy step for the user, TB-10; no MSVC-only flags, §2).
          let ccDir: string | null = null;
          if (path.basename(cc) === "compile_commands.json") {
            ccDir = path.dirname(cc);
          } else {
            const target = path.join(path.dirname(path.dirname(cc)), "compile_commands.json");
            try {
              if (mirrorNeedsRefresh(cc, target)) {
                fs.writeFileSync(target, JSON.stringify(sanitizeCompileCommands(cc), null, "\t"));
              }
              ccDir = path.dirname(target);
            } catch {
              ccDir = null; // unwritable root — clangd falls back to heuristics
            }
          }
          if (ccDir) {
            args.push(`--compile-commands-dir=${ccDir}`);
          }
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
      // Keep stdout as raw Buffers (no setEncoding) so LSP framing can slice by BYTE length (LSP-1).
      proc.stdout.on("data", (chunk: Buffer) => this.onData(chunk));
      // DRAIN stderr (F5 field test B5 — LOAD-BEARING): clangd logs lines like
      // "IncludeCleaner: Failed to get an entry …" on EVERY parse of our virtual TUs. Unread,
      // the OS pipe buffer (64KB) fills after enough edits and clangd BLOCKS mid-write — the
      // owner-reported arc of "2-5s latency, then features disappear entirely". Keep a small
      // tail for post-mortems; never let the pipe back up.
      proc.stderr.on("data", (chunk: Buffer) => {
        this.stderrTail = (this.stderrTail + chunk.toString("utf8")).slice(-2000);
      });
      // A DEAD clangd must not masquerade as a live one (B6): mark unavailable, fail the
      // in-flight requests, and reset the spawn latch so the NEXT request lazily respawns a
      // fresh clangd (crash resilience without a supervision loop).
      proc.on("exit", (code, signal) => {
        if (this.proc !== proc) return; // an old instance we already replaced
        this.available = false;
        this.proc = null;
        this.starting = null;
        this.openVersions.clear();
        this.openTextHashes.clear();
        for (const [, respond] of this.pending) respond(null);
        this.pending.clear();
        this.onError?.("clangd exited", `code=${code} signal=${signal} stderr-tail: ${this.stderrTail.slice(-400)}`);
      });
      this.proc = proc;

      this.request("initialize", {
        processId: process.pid,
        rootUri: null,
        // Advertise diagnostic-tag support so clangd sends Unnecessary/Deprecated tags —
        // the client renders Unnecessary as faded (TB-10/TB-5 presentation).
        capabilities: {
          textDocument: {
            publishDiagnostics: { relatedInformation: false, tagSupport: { valueSet: [1, 2] } },
            // R5 embedded coloring: without this clangd never enables its token provider.
            inlayHint: {}, // R7: type/param hints — the compiler's OWN types for auto/bindings
            semanticTokens: {
              requests: { full: true },
              tokenTypes: [
                "namespace", "type", "class", "enum", "interface", "struct", "typeParameter",
                "parameter", "variable", "property", "enumMember", "function", "method", "macro",
                "keyword", "modifier", "comment", "string", "number", "regexp", "operator",
              ],
              tokenModifiers: [],
              formats: ["relative"],
            },
          },
        },
      })
        .then((initResult: unknown) => {
          const caps = (initResult as { capabilities?: { semanticTokensProvider?: { legend?: { tokenTypes: string[]; tokenModifiers: string[] } } } })?.capabilities;
          this.tokenLegend = caps?.semanticTokensProvider?.legend ?? null;
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

  /** Sync the virtual C++ doc: didOpen on first sight, versioned didChange after (clangd
   *  re-diagnoses per version — the TB-10 diagnostics loop rides this). An UNCHANGED text is
   *  a no-op (see openTextHashes). */
  didOpen(uri: string, text: string): void {
    if (!this.available) {
      return;
    }
    const hash = ClangdProxy.textHash(text);
    const prev = this.openVersions.get(uri);
    if (prev !== undefined && this.openTextHashes.get(uri) === hash) {
      return; // same text clangd already holds — don't force a re-parse
    }
    this.openTextHashes.set(uri, hash);
    if (prev === undefined) {
      this.openVersions.set(uri, 1);
      this.notify("textDocument/didOpen", {
        textDocument: { uri, languageId: "cpp", version: 1, text },
      });
      return;
    }
    const version = prev + 1;
    this.openVersions.set(uri, version);
    this.notify("textDocument/didChange", {
      textDocument: { uri, version },
      contentChanges: [{ text }], // full-sync — the virtual doc is rebuilt per edit anyway
    });
  }

  /** The version clangd currently holds for a virtual doc (0 = never opened) — the N6 rename
   *  guard waits for THIS version's diagnostics before trusting the TU's health. */
  versionOf(uri: string): number {
    return this.openVersions.get(uri) ?? 0;
  }

  /** Close a virtual doc (its .uetkx closed) so clangd drops the TU and clears diagnostics. */
  didClose(uri: string): void {
    if (!this.available || !this.openVersions.has(uri)) {
      return;
    }
    this.openVersions.delete(uri);
    this.openTextHashes.delete(uri);
    this.notify("textDocument/didClose", { textDocument: { uri } });
  }

  /** FNV-1a over UTF-16 units — cheap dedup key for didOpen (not cryptographic). */
  private static textHash(text: string): number {
    let h = 2166136261;
    for (let i = 0; i < text.length; i++) {
      h = (h ^ text.charCodeAt(i)) >>> 0;
      h = Math.imul(h, 16777619) >>> 0;
    }
    return h;
  }

  /** Forward a position request; null on any degradation (unavailable / timeout / error).
   *  `extra` merges additional request params (N6: rename's `newName`). */
  async positionRequest(method: string, uri: string, position: ClangdPosition, extra?: object): Promise<unknown | null> {
    if (!this.available) {
      return null;
    }
    try {
      return await this.request(method, { textDocument: { uri }, position, ...(extra ?? {}) });
    } catch {
      return null;
    }
  }

  /** clangd's semantic-token legend, or null before initialize / when unsupported. */
  semanticLegend(): { tokenTypes: string[]; tokenModifiers: string[] } | null {
    return this.tokenLegend;
  }

  /** textDocument/inlayHint over a virtual-doc range — the compiler's deduced TYPES for
   *  auto/structured bindings and resolved-call parameter names (R7: type-true coloring). */
  async inlayHints(
    uri: string,
    range: { start: ClangdPosition; end: ClangdPosition },
  ): Promise<Array<{ position: ClangdPosition; kind?: number; label: string | Array<{ value: string }> }> | null> {
    if (!this.available) {
      return null;
    }
    try {
      const res = await this.request("textDocument/inlayHint", { textDocument: { uri }, range });
      return Array.isArray(res) ? (res as Array<{ position: ClangdPosition; kind?: number; label: string | Array<{ value: string }> }>) : null;
    } catch {
      return null;
    }
  }

  /** textDocument/semanticTokens/full for a virtual doc — null on degradation. Long timeout:
   *  the first request per TU waits behind the initial parse. */
  async semanticTokensFull(uri: string): Promise<{ data: number[] } | null> {
    if (!this.available) {
      return null;
    }
    try {
      const res = (await this.request("textDocument/semanticTokens/full", { textDocument: { uri } })) as { data?: number[] } | null;
      return res && Array.isArray(res.data) ? { data: res.data } : null;
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

  private onData(chunk: Buffer): void {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    const { messages, rest } = drainMessages(this.buffer);
    this.buffer = rest;
    for (const msg of messages) {
      // Per-message guard (§1): one bad frame or a throwing consumer must never kill the
      // pump — and DEFINITELY not the server process (an exception here is fatal in Node).
      try {
        const m = msg as { id?: number; result?: unknown; method?: string; params?: unknown };
        if (typeof m.id === "number" && this.pending.has(m.id)) {
          const resolve = this.pending.get(m.id)!;
          this.pending.delete(m.id);
          resolve(m.result ?? null);
          continue;
        }
        if (m.method === "textDocument/publishDiagnostics" && this.onPublishDiagnostics && m.params) {
          this.onPublishDiagnostics(m.params as ClangdPublishedDiagnostics);
        }
      } catch (e) {
        this.onError?.("clangd message pump", e);
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
