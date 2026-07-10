#!/usr/bin/env node
// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
/**
 * CI gate: every skill under .claude/skills/<name>/SKILL.md is well-formed —
 *  1. YAML frontmatter with non-empty `name:` and `description:` (the harness needs both);
 *  2. the `name:` matches its directory;
 *  3. a `## Scar tissue` section exists (house style: every skill documents WHY its steps
 *     exist — the incidents behind the rules — so future edits don't optimize away the fence).
 */
import { readFileSync, readdirSync, existsSync, statSync } from 'fs';
import { resolve, dirname, join } from 'path';
import { fileURLToPath } from 'url';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const SKILLS_ROOT = resolve(REPO_ROOT, '.claude/skills');

if (!existsSync(SKILLS_ROOT)) {
  console.error(`✗ ${SKILLS_ROOT} does not exist.`);
  process.exit(1);
}

const failures = [];
const dirs = readdirSync(SKILLS_ROOT).filter(d => {
  try { return statSync(join(SKILLS_ROOT, d)).isDirectory(); } catch { return false; }
});

if (dirs.length === 0) {
  console.error('✗ no skills found under .claude/skills/.');
  process.exit(1);
}

for (const dir of dirs) {
  const file = join(SKILLS_ROOT, dir, 'SKILL.md');
  const label = `.claude/skills/${dir}/SKILL.md`;
  if (!existsSync(file)) {
    failures.push(`${label}: missing SKILL.md`);
    continue;
  }
  const text = readFileSync(file, 'utf8').replace(/\r\n/g, '\n');

  const fm = text.match(/^---\n([\s\S]*?)\n---\n/);
  if (!fm) {
    failures.push(`${label}: missing YAML frontmatter (--- ... ---)`);
    continue;
  }
  const nameMatch = fm[1].match(/^name:\s*(\S.*)$/m);
  const descMatch = fm[1].match(/^description:\s*(\S.*)$/m);
  if (!nameMatch) failures.push(`${label}: frontmatter missing 'name:'`);
  if (!descMatch) failures.push(`${label}: frontmatter missing 'description:'`);
  if (nameMatch && nameMatch[1].trim() !== dir) {
    failures.push(`${label}: frontmatter name '${nameMatch[1].trim()}' != directory '${dir}'`);
  }
  if (!/^## Scar tissue/m.test(text)) {
    failures.push(`${label}: missing '## Scar tissue' section (document why the steps exist)`);
  }
}

if (failures.length) {
  console.error(`✗ skills lint failed (${failures.length}):`);
  for (const f of failures) console.error(`  ${f}`);
  process.exit(1);
}

console.error(`✓ ${dirs.length} skills well-formed (frontmatter + scar tissue).`);
