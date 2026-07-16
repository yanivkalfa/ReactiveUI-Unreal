// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-013 drift gate: every style key + slot key the markup compiler exports (schema.json,
// the same vocabulary the LSP serves) must have a typed fluent method on RUI::Style() /
// RUI::Slot() in RuiStyle.h — a key that exists in markup but not in the C++ authoring
// builders is exactly how `Clipping` shipped builder-less in 0.4.0. The methods stay
// hand-written (each needs a human-chosen C++ parameter type); THIS gate makes forgetting
// one a CI failure instead of a doc footnote.
import { readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const schema = JSON.parse(
  readFileSync(resolve(ROOT, 'ide-extensions/lsp-server/src/uetkx-schema.json'), 'utf8'),
);
const header = readFileSync(
  resolve(ROOT, 'Plugins/ReactiveUI/Source/ReactiveUISlate/Public/RuiStyle.h'),
  'utf8',
);

let failures = 0;
// FName comparison is case-insensitive in UE — the header may legitimately spell a key in a
// different case than the schema (the slot builder predates the PascalCase vocabulary).
const headerLower = header.toLowerCase();
const requireKey = (key, label) => {
  // A builder method registers its key via Set(FName(TEXT("<Key>")), ...).
  if (!headerLower.includes(`set(fname(text("${key}"`.toLowerCase())) {
    console.error(`✗ ${label} key '${key}' has NO RUI::Style()/RUI::Slot() builder method in RuiStyle.h`);
    failures++;
  }
};

for (const key of schema.styleKeys ?? []) requireKey(key, 'style');
for (const key of schema.slotKeys ?? []) requireKey(key, 'slot');

if (failures > 0) {
  console.error(`check-style-builders: ${failures} missing builder method(s).`);
  process.exit(1);
}
console.log(
  `✓ style/slot builders cover the schema (${(schema.styleKeys ?? []).length} style + ${(schema.slotKeys ?? []).length} slot keys).`,
);
