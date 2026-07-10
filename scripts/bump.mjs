#!/usr/bin/env node
// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
/**
 * Version bump with lockstep partners + changelog scaffolding hints, in one command
 * (godot-ecosystem "IMPROVE FOR UNREAL" #4 — the sibling repo bumps five hand-edited
 * manifests with lockstep rules enforced only by skill prose; here it's a script).
 *
 * Usage:
 *   node scripts/bump.mjs plugin 0.2.0       # ReactiveUI.uplugin VersionName + integer Version
 *   node scripts/bump.mjs vscode 0.1.1       # vscode-uetkx/package.json + LOCKSTEP lsp-server/package.json
 *   node scripts/bump.mjs vs2022 0.1.1       # visual-studio vsixmanifest Identity/@Version
 *
 * Policy (MASTER_PLAN D-30): patch by default; perf-only changes to shipped bytes still bump.
 * This script does NOT write changelogs — it prints the exact Lane A/B follow-ups.
 */
import { readFileSync, writeFileSync, existsSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const SEMVERISH = /^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$/;

const [artifact, version] = process.argv.slice(2);

function die(msg) { console.error(`✗ ${msg}`); process.exit(1); }
function readText(p) { return readFileSync(p, 'utf8'); }
function writeText(p, t) { writeFileSync(p, t, 'utf8'); }

function bumpPlugin(v) {
  const p = resolve(REPO_ROOT, 'Plugins/ReactiveUI/ReactiveUI.uplugin');
  const json = JSON.parse(readText(p));
  const old = json.VersionName;
  json.VersionName = v;
  json.Version = (json.Version | 0) + 1; // monotonic integer, per FPluginDescriptor
  // Preserve tab-indented formatting (the descriptor is committed tab-indented like UE's own).
  writeText(p, JSON.stringify(json, null, '\t') + '\n');
  console.error(`✓ plugin: ${old} → ${v} (integer Version → ${json.Version})`);
  console.error('\nFollow-ups (Lane A — release-process skill §changelogs):');
  console.error(`  1. Write the ## [${v}] section at the top of root CHANGELOG.md`);
  console.error('  2. cp CHANGELOG.md Plugins/ReactiveUI/CHANGELOG.md   (byte-identical mirror)');
  console.error('  3. node scripts/verify-mirror.mjs');
}

function bumpVscode(v) {
  const ext = resolve(REPO_ROOT, 'ide-extensions/vscode-uetkx/package.json');
  const server = resolve(REPO_ROOT, 'ide-extensions/lsp-server/package.json');
  if (!existsSync(ext)) die('ide-extensions/vscode-uetkx/package.json not found — the VS Code extension lands in Phase 5.');
  for (const p of [ext, server]) {
    if (!existsSync(p)) { console.error(`  (skipping missing ${p})`); continue; }
    const text = readText(p);
    const updated = text.replace(/"version"\s*:\s*"[^"]+"/, `"version": "${v}"`);
    if (updated === text) die(`no "version" field found in ${p}`);
    writeText(p, updated);
    console.error(`✓ ${p} → ${v}`);
  }
  console.error('\nLockstep rule (D-30): the vendored lsp-server version-locks to vscode-uetkx (done above).');
  console.error('Any lsp-server/src change means BOTH extensions bump — check whether vs2022 should match:');
  console.error(`  node scripts/bump.mjs vs2022 ${v}`);
  console.error('\nFollow-ups (Lane B):');
  console.error(`  node ide-extensions/scripts/changelog.mjs add --scope <shared|vscode> --message-file <msg.txt> --vscode ${v}`);
  console.error('  node ide-extensions/scripts/changelog.mjs extract --ide vscode --out ide-extensions/vscode-uetkx/CHANGELOG.md');
  console.error('  node ide-extensions/scripts/changelog.mjs verify');
}

function bumpVs2022(v) {
  const manifest = resolve(REPO_ROOT, 'ide-extensions/visual-studio/UetkxVsix/source.extension.vsixmanifest');
  if (!existsSync(manifest)) die('vsixmanifest not found — the VS2022 extension lands in Phase 5.');
  const text = readText(manifest);
  const updated = text.replace(/(<Identity[^>]*\sVersion=")[^"]+(")/, `$1${v}$2`);
  if (updated === text) die('no Identity Version attribute found in the vsixmanifest');
  writeText(manifest, updated);
  console.error(`✓ vs2022 vsixmanifest → ${v}`);
  console.error('\nFollow-ups (Lane B):');
  console.error(`  node ide-extensions/scripts/changelog.mjs add --scope <shared|vs2022> --message-file <msg.txt> --vs2022 ${v}`);
  console.error('  node ide-extensions/scripts/changelog.mjs extract --ide vs2022 --out ide-extensions/visual-studio/CHANGELOG.md');
  console.error('  node ide-extensions/scripts/changelog.mjs verify');
}

const TABLE = { plugin: bumpPlugin, vscode: bumpVscode, vs2022: bumpVs2022 };

if (!artifact || !version || !TABLE[artifact]) {
  console.error('Usage: node scripts/bump.mjs <plugin|vscode|vs2022> <X.Y.Z[-suffix]>');
  process.exit(1);
}
if (!SEMVERISH.test(version)) die(`'${version}' is not X.Y.Z[-suffix]`);
TABLE[artifact](version);
