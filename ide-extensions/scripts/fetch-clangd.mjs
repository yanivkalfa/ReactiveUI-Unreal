// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// §5 clangd bundling — fetch the PINNED clangd release, verify its sha256, and stage it for
// packaging. Binaries NEVER enter git (the staging dirs are gitignored); this script runs at
// package time and in CI. The pin is deliberate (MARKUP_EVERYWHERE_PLAN §0): bumping the
// version means updating VERSION + SHA256 here, re-verifying the embedded-C++ sweep against
// the new build, and a changelog entry.
//
//   node ide-extensions/scripts/fetch-clangd.mjs                 stage for vscode (win32-x64)
//   node ide-extensions/scripts/fetch-clangd.mjs --target vs2022 stage for the VS2022 VSIX
//   node ide-extensions/scripts/fetch-clangd.mjs --target all    stage both
//
// Layout staged (per target):
//   vscode: ide-extensions/vscode-uetkx/clangd/win32-x64/{bin/clangd.exe, lib/clang/…, LICENSE.TXT}
//   vs2022: ide-extensions/visual-studio/UetkxVsix/clangd/{bin/clangd.exe, lib/clang/…, LICENSE.TXT}
//
// The zip is cached in the system temp dir keyed by version+sha so repeated packaging runs
// don't redownload 28MB.

import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync, rmSync, cpSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const VERSION = "22.1.6";
const SHA256 = "ce54f16e0b4fd76d450eeda9664420b195360b73febcfe40e661108fa57f2ce1";
const URL = `https://github.com/clangd/clangd/releases/download/${VERSION}/clangd-windows-${VERSION}.zip`;

// Shipped beside the binary as BUNDLED-CLANGD.txt — the zip's own LICENSE.TXT (the full
// Apache-2.0-with-LLVM-exceptions text) is preserved as-is; writing this to LICENSE.txt would
// CLOBBER it on Windows' case-insensitive filesystem.
const LICENSE_NOTE = `This directory contains clangd ${VERSION}, part of the LLVM Project
(https://github.com/clangd/clangd), redistributed under the Apache License v2.0 with LLVM
Exceptions (https://llvm.org/LICENSE.txt). clangd is © the LLVM Project contributors and is
NOT covered by this extension's own license.
`;

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const TARGETS = {
  vscode: join(ROOT, "vscode-uetkx", "clangd", "win32-x64"),
  vs2022: join(ROOT, "visual-studio", "UetkxVsix", "clangd"),
};

function sha256Of(path) {
  return createHash("sha256").update(readFileSync(path)).digest("hex");
}

async function download(dest) {
  console.log(`fetching clangd ${VERSION} … (${URL})`);
  const res = await fetch(URL, { redirect: "follow" });
  if (!res.ok) throw new Error(`download failed: HTTP ${res.status}`);
  writeFileSync(dest, Buffer.from(await res.arrayBuffer()));
}

async function ensureZip() {
  const cached = join(tmpdir(), `clangd-windows-${VERSION}-${SHA256.slice(0, 12)}.zip`);
  if (existsSync(cached) && sha256Of(cached) === SHA256) {
    console.log(`✓ using cached zip: ${cached}`);
    return cached;
  }
  await download(cached);
  const actual = sha256Of(cached);
  if (actual !== SHA256) {
    rmSync(cached, { force: true });
    throw new Error(`sha256 MISMATCH for ${URL}\n  expected ${SHA256}\n  actual   ${actual}`);
  }
  console.log(`✓ downloaded + verified sha256 ${SHA256.slice(0, 12)}…`);
  return cached;
}

function stage(zip, destDir) {
  rmSync(destDir, { recursive: true, force: true });
  mkdirSync(destDir, { recursive: true });
  // Windows' bsdtar (System32) handles zip; the ABSOLUTE path matters — a Git-Bash GNU tar
  // earlier on PATH can't read zip and parses `C:` as a remote host.
  const tar =
    process.platform === "win32" ? join(process.env.SystemRoot ?? "C:\\Windows", "System32", "tar.exe") : "tar";
  execFileSync(tar, ["-xf", zip, "-C", destDir, "--strip-components", "1"], { stdio: "inherit" });
  const exe = join(destDir, "bin", "clangd.exe");
  if (!existsSync(exe)) throw new Error(`staging failed — ${exe} missing after extract`);
  writeFileSync(join(destDir, "BUNDLED-CLANGD.txt"), LICENSE_NOTE);
  console.log(`✓ staged ${exe}`);
}

const targetArg = process.argv.includes("--target") ? process.argv[process.argv.indexOf("--target") + 1] : "vscode";
const targets = targetArg === "all" ? Object.keys(TARGETS) : [targetArg];
for (const t of targets) {
  if (!TARGETS[t]) throw new Error(`unknown --target ${t} (vscode | vs2022 | all)`);
}
const zip = await ensureZip();
for (const t of targets) stage(zip, TARGETS[t]);
console.log(`clangd ${VERSION} staged for: ${targets.join(", ")}`);
