#!/usr/bin/env node
// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
/**
 * CI gate: every source file carries the copyright line (MASTER_PLAN D-01 — Fab review checks
 * per-file copyright; retrofitting after generated code is committed is a full-tree sweep, so
 * the lint exists from commit one).
 *
 * Rules:
 *  - .h/.cpp/.inl/.cs/.mjs/.ts/.tsx/.js — first 3 lines must contain `Copyright (c) ... Yaniv Kalfa`
 *    (a `//` comment; generated .inl files inside THIS repo carry it too — the codegen emits it).
 *  - .ps1 — same, `#` comment form.
 *  - Scanned trees: Source/, Plugins/<plugin>/Source/, templates/, scripts/, ide-extensions/
 *    (excluding node_modules/, out/, dist/ — third-party and build output are not ours to stamp).
 *  - ReactiveUIUnrealDocs~/ is deliberately NOT scanned: the docs site is a vendored family shell (MIT'd wholesale
 *    by the repo LICENSE); stamping every .tsx there adds noise without a Fab requirement.
 */
import { readFileSync, readdirSync, statSync } from 'fs';
import { resolve, dirname, extname, join, relative } from 'path';
import { fileURLToPath } from 'url';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const SCAN_ROOTS = ['Source', 'Plugins', 'templates', 'scripts', 'ide-extensions'];
const EXTS_SLASH = new Set(['.h', '.cpp', '.inl', '.cs', '.mjs', '.ts', '.tsx', '.js']);
const EXTS_HASH = new Set(['.ps1']);
const EXCLUDE_DIRS = new Set(['node_modules', 'out', 'dist', 'Binaries', 'Intermediate', 'Saved', '.git']);
const NEEDLE = /Copyright \(c\) .*Yaniv Kalfa/;

const failures = [];

function scan(dir) {
  let names;
  try { names = readdirSync(dir); } catch { return; }
  for (const name of names) {
    const p = join(dir, name);
    let st;
    try { st = statSync(p); } catch { continue; }
    if (st.isDirectory()) {
      if (!EXCLUDE_DIRS.has(name)) scan(p);
      continue;
    }
    const ext = extname(name);
    const isSlash = EXTS_SLASH.has(ext);
    const isHash = EXTS_HASH.has(ext);
    if (!isSlash && !isHash) continue;

    const head = readFileSync(p, 'utf8').split('\n', 3).join('\n');
    const marker = isSlash ? '//' : '#';
    if (!(NEEDLE.test(head) && head.includes(marker))) {
      failures.push(relative(REPO_ROOT, p).replace(/\\/g, '/'));
    }
  }
}

for (const root of SCAN_ROOTS) scan(resolve(REPO_ROOT, root));

if (failures.length) {
  console.error(`✗ ${failures.length} file(s) missing the copyright header:`);
  for (const f of failures) console.error(`  ${f}`);
  console.error('\nAdd as the first line:');
  console.error('  // Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.   (or the # form for .ps1)');
  process.exit(1);
}

console.error('✓ copyright headers present in all scanned source files.');
