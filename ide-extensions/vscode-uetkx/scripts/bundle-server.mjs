// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// Bundle the language server into the extension: copy ../lsp-server/out + its runtime deps
// into ./server so the .vsix is self-contained (pure JS — one vsix serves every platform).
import { cpSync, mkdirSync, rmSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(dirname(fileURLToPath(import.meta.url)));
const lsp = join(dirname(here), "lsp-server");
const target = join(here, "server");

if (!existsSync(join(lsp, "out", "server.js"))) {
  console.error("lsp-server not built — run `npm run build` in ide-extensions/lsp-server first");
  process.exit(1);
}
rmSync(target, { recursive: true, force: true });
mkdirSync(target, { recursive: true });
cpSync(join(lsp, "out"), target, { recursive: true, filter: (p) => !p.includes(`${join("out", "test")}`) });
// runtime deps the server imports
for (const dep of ["vscode-languageserver", "vscode-languageserver-textdocument", "vscode-jsonrpc", "vscode-languageserver-protocol", "vscode-languageserver-types"]) {
  const src = join(lsp, "node_modules", dep);
  if (existsSync(src)) cpSync(src, join(target, "node_modules", dep), { recursive: true });
}
console.log("server bundled");
