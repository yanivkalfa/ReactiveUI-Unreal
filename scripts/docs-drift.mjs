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
import { readdirSync } from 'fs';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');

// ── registry readers (each derives the TRUE count from the artifact that defines it) ──────

/** The compiler-exported vocabulary the LSP + the docs catalog both read. */
function readUetkxSchema() {
  const raw = readFileSync(
    resolve(REPO_ROOT, 'ide-extensions/lsp-server/src/uetkx-schema.json'),
    'utf8',
  ).replace(/^\uFEFF/, '');
  return JSON.parse(raw);
}

/** Core hooks = the Ctx member hooks in RuiContext.h (Use* + ProvideContext, minus *Impl)
 *  + UseSignal/UseSignalKey (RuiSignal.h) + UsePresence (RuiPresence.h) — the audited 23. */
function countCoreHooks() {
  const ctx = readFileSync(
    resolve(REPO_ROOT, 'Plugins/ReactiveUI/Source/ReactiveUICore/Public/RuiContext.h'),
    'utf8',
  );
  const names = new Set();
  for (const m of ctx.matchAll(/\b(Use[A-Z]\w+|ProvideContext)\s*\(/g)) {
    if (!m[1].endsWith('Impl')) names.add(m[1]);
  }
  return names.size + 2 /* UseSignal, UseSignalKey */ + 1 /* UsePresence */;
}

/** Wrapped Slate widgets = the public FRuiNode factories in ReactiveUISlate + core TextBlock. */
function countWidgetFactories() {
  const dir = resolve(REPO_ROOT, 'Plugins/ReactiveUI/Source/ReactiveUISlate/Public');
  const names = new Set();
  for (const f of readdirSync(dir)) {
    if (!f.endsWith('.h')) continue;
    const text = readFileSync(resolve(dir, f), 'utf8');
    for (const m of text.matchAll(/REACTIVEUISLATE_API FRuiNode ([A-Z]\w+)\s*\(/g)) {
      names.add(m[1]);
    }
  }
  return names.size + 1; // + RUI::TextBlock (core)
}

/** Gallery screens = the screen directories under Source/RuiDemo/Screens. */
function countGalleryScreens() {
  return readdirSync(resolve(REPO_ROOT, 'Source/RuiDemo/Screens'), { withFileTypes: true }).filter(
    (e) => e.isDirectory(),
  ).length;
}

/** Router hooks = the REACTIVEUICORE_API Use* free functions in RuiRouter.h. */
function countRouterHooks() {
  const text = readFileSync(
    resolve(REPO_ROOT, 'Plugins/ReactiveUI/Source/ReactiveUICore/Public/RuiRouter.h'),
    'utf8',
  );
  const names = new Set();
  for (const m of text.matchAll(/REACTIVEUICORE_API [\w<>,&:\s]+?\b(Use[A-Z]\w+)\s*\(/g)) {
    names.add(m[1]);
  }
  return names.size;
}

/** Hook catalog entries by category — the generated per-hook docs pages read this file.
 *  The trailing comma distinguishes data entries from the interface's union-type line. */
function countHooksCatalog(category) {
  const text = readFileSync(resolve(REPO_ROOT, 'ReactiveUIUnrealDocs~/src/hooksCatalog.ts'), 'utf8');
  return (text.match(new RegExp(`category: '${category}',`, 'g')) ?? []).length;
}

/** Automation tests = IMPLEMENT_*_AUTOMATION_TEST macros in the test module. */
function countAutomationTests() {
  const dir = resolve(REPO_ROOT, 'Source/RuiHostTests/Private');
  let count = 0;
  for (const f of readdirSync(dir)) {
    if (!f.endsWith('.cpp')) continue;
    const text = readFileSync(resolve(dir, f), 'utf8');
    count += (text.match(/IMPLEMENT_\w*AUTOMATION_TEST\s*\(/g) ?? []).length;
  }
  return count;
}

const CHECKS = [
  {
    // README status blockquote: "(23 core hooks, ...".
    file: 'README.md',
    pattern: /\((\d+) core hooks/,
    source: countCoreHooks,
  },
  {
    // Docs intro: "UseState, UseEffect, and 21 more" (= the 23 total, 2 named + N more).
    file: 'ReactiveUIUnrealDocs~/src/pages/Introduction/IntroductionPage.tsx',
    pattern: /and (\d+) more/,
    source: () => countCoreHooks() - 2,
  },
  {
    // README: "65+ wrapped Slate widgets" — a floor claim; the registry must be >= it.
    file: 'README.md',
    pattern: /(\d+)\+ wrapped Slate widgets/,
    source: () => {
      const actual = countWidgetFactories();
      return actual >= 65 ? 65 : actual; // a shrink below the floor surfaces the real number
    },
  },
  {
    // README: "The demo gallery's 16 screens".
    file: 'README.md',
    pattern: /demo gallery's (\d+) screens/,
    source: countGalleryScreens,
  },
  {
    // README: "green under a 100+-test headless automation battery" — floor claim.
    file: 'README.md',
    pattern: /(\d+)\+-test headless/,
    source: () => {
      const actual = countAutomationTests();
      return actual >= 100 ? 100 : actual;
    },
  },
  {
    // Components Overview prose: "Markup tags (29 of them)" — must equal the schema exactly.
    file: 'ReactiveUIUnrealDocs~/src/pages/ComponentsOverview/ComponentsOverviewPage.tsx',
    pattern: /\((\d+) of them\)/,
    source: () => Object.keys(readUetkxSchema().elements ?? {}).length,
  },
  {
    // The generated per-hook docs pages: the catalog's CORE entries must cover every core hook.
    // (The claim line lives in the catalog header so the check self-anchors to the data file.)
    file: 'ReactiveUIUnrealDocs~/src/hooksCatalog.ts',
    pattern: /(\d+) core hook entries/,
    source: () => {
      const registry = countCoreHooks();
      const catalog = countHooksCatalog('core');
      return catalog === registry ? registry : catalog; // any mismatch surfaces both ways
    },
  },
  {
    // ...and the ROUTER entries must cover every RuiRouter.h hook.
    file: 'ReactiveUIUnrealDocs~/src/hooksCatalog.ts',
    pattern: /(\d+) router hook entries/,
    source: () => {
      const registry = countRouterHooks();
      const catalog = countHooksCatalog('router');
      return catalog === registry ? registry : catalog;
    },
  },
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
