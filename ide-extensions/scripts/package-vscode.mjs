// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// §5 clangd bundling — build BOTH VS Code vsix flavors from one invocation:
//   1. `uetkx-<ver>-win32-x64.vsix`  — platform-specific, clangd 22.1.6 bundled (~28MB):
//      C++ intelligence lights up with ZERO machine setup ("completely self sustained").
//   2. `uetkx-<ver>.vsix`            — universal fallback, no clangd: full markup
//      intelligence everywhere; embedded C++ uses the discovery chain (PATH/LLVM/VS/…).
//
// vsce has ONE .vscodeignore, so the flavor gate is physical: the clangd staging dir is
// present for the platform build and moved aside for the universal build. The marketplace
// serves the platform-specific vsix to matching clients automatically and the universal one
// to everyone else — publish BOTH (publish.yml does; this script only packages).
//
//   node ide-extensions/scripts/package-vscode.mjs            both flavors
//   node ide-extensions/scripts/package-vscode.mjs --skip-fetch   reuse a staged clangd

import { execSync } from "node:child_process";
import { existsSync, renameSync, rmSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const IDE_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const EXT = join(IDE_ROOT, "vscode-uetkx");
const CLANGD_DIR = join(EXT, "clangd");
// OUTSIDE the extension dir — vsce packages any non-ignored folder under the extension root,
// dot-named or not (verified: an in-tree aside dir shipped 28MB of clangd in the universal vsix).
const CLANGD_ASIDE = join(IDE_ROOT, ".clangd-staged-aside");

const run = (cmd, cwd = EXT) => execSync(cmd, { cwd, stdio: "inherit" });

// 1. one server build for both flavors
run("npm run build");

// 2. stage clangd (pinned + verified) unless told to reuse
if (!process.argv.includes("--skip-fetch")) {
  run(`node ${JSON.stringify(join(IDE_ROOT, "scripts", "fetch-clangd.mjs"))} --target vscode`, IDE_ROOT);
} else if (!existsSync(join(CLANGD_DIR, "win32-x64", "bin", "clangd.exe"))) {
  throw new Error("--skip-fetch but no staged clangd — run fetch-clangd.mjs first");
}

// 3. platform-specific flavor WITH the bundle
run("npx @vscode/vsce package --target win32-x64");

// 4. universal flavor WITHOUT it — move the staging dir aside so vsce never sees it
rmSync(CLANGD_ASIDE, { recursive: true, force: true });
renameSync(CLANGD_DIR, CLANGD_ASIDE);
try {
  run("npx @vscode/vsce package");
} finally {
  renameSync(CLANGD_ASIDE, CLANGD_DIR);
}

console.log("\nPackaged both flavors — install the win32-x64 vsix locally; publish BOTH.");
