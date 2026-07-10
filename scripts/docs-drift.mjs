#!/usr/bin/env node
// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
/**
 * CI gate: docs-drift tripwire (MASTER_PLAN Phase 0 step 3; consumed by the docs-sync skill
 * and Phase 8's Verify). Greps user-facing claimed counts ("N hooks", "M widgets") against
 * the actual registries, so the README can never again claim numbers the code doesn't have —
 * the automated fix for the sibling repo's "stale README claimed MVP / 10 elements / 6 hooks"
 * episode.
 *
 * CHECKS is intentionally empty in Phase 0: the registries it reads (the prop-map schema's
 * widget list, the hooks registry) don't exist yet, and the README deliberately makes no
 * count claims. WIRE A CHECK IN THE SAME PR THAT INTRODUCES A COUNT CLAIM — that's the rule
 * docs-sync enforces. Each check: { file, pattern (regex with one capture group = the claimed
 * number), source (function returning the true count) }.
 *
 * Example (Phase 2+, when the prop-map schema exists):
 *   {
 *     file: 'README.md',
 *     pattern: /(\d+) wrapped Slate widgets/,
 *     source: () => JSON.parse(readFileSync(resolve(REPO_ROOT, 'templates/prop-map.schema.json'), 'utf8')).widgets.length,
 *   }
 */
import { readFileSync, existsSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');

const CHECKS = [
  // ── none yet (Phase 0) — see the doc comment for the wiring rule ──
];

let failures = 0;
for (const check of CHECKS) {
  const filePath = resolve(REPO_ROOT, check.file);
  if (!existsSync(filePath)) {
    console.error(`✗ ${check.file}: file missing`);
    failures++;
    continue;
  }
  const text = readFileSync(filePath, 'utf8');
  const m = text.match(check.pattern);
  if (!m) {
    console.error(`✗ ${check.file}: pattern ${check.pattern} not found (claim removed? update CHECKS)`);
    failures++;
    continue;
  }
  const claimed = parseInt(m[1], 10);
  const actual = check.source();
  if (claimed !== actual) {
    console.error(`✗ ${check.file}: claims ${claimed} but the registry says ${actual} (${check.pattern})`);
    failures++;
  } else {
    console.error(`✓ ${check.file}: ${claimed} matches the registry`);
  }
}

if (failures) {
  console.error(`\ndocs-drift FAILED (${failures}).`);
  process.exit(1);
}
console.error(`✓ docs-drift OK (${CHECKS.length} check(s) configured).`);
