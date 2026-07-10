#!/usr/bin/env node
// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
/**
 * CI gate: the plugin-local changelog is a BYTE-IDENTICAL mirror of the root one
 * (MASTER_PLAN D-31, Lane A). In the sibling Godot repo this tripwire lives inside a
 * headless engine test; here it must be ENGINE-FREE because engine CI is default-unarmed —
 * so it's a five-line node script in the always-on job instead.
 *
 * Resync after editing the root file:  cp CHANGELOG.md Plugins/ReactiveUI/CHANGELOG.md
 * (copy, never re-type — "byte-identical" is literal; .gitattributes pins LF on both).
 */
import { readFileSync, existsSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const ROOT_FILE = resolve(REPO_ROOT, 'CHANGELOG.md');
const MIRROR_FILE = resolve(REPO_ROOT, 'Plugins/ReactiveUI/CHANGELOG.md');

for (const f of [ROOT_FILE, MIRROR_FILE]) {
  if (!existsSync(f)) {
    console.error(`✗ missing: ${f}`);
    process.exit(1);
  }
}

const root = readFileSync(ROOT_FILE);
const mirror = readFileSync(MIRROR_FILE);

if (!root.equals(mirror)) {
  console.error('✗ Plugins/ReactiveUI/CHANGELOG.md is NOT byte-identical to root CHANGELOG.md.');
  console.error('  Resync it:  cp CHANGELOG.md Plugins/ReactiveUI/CHANGELOG.md');
  console.error('  (Edit only the root file; the plugin copy is a generated mirror.)');
  process.exit(1);
}

console.error('✓ changelog mirror is byte-identical.');
