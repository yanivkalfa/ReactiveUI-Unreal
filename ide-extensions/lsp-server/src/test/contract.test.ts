// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// The cross-language contract (D-22): replay the SAME corpora the C++ automation suites run.
// A case failing here while passing in C++ means the two scanners have drifted — fix the
// PORT, never the fixture.

import { test } from "node:test";
import * as assert from "node:assert";
import * as fs from "node:fs";
import * as path from "node:path";
import { toCodePoints } from "../codePoints";
import { findMatching, findMatchingMarkup, skipNoncode, skipNoncodeMarkup, srcHash } from "../cppScanner";
import { scanFile, hasError } from "../uetkxFileScan";

const fixtures = path.join(__dirname, "..", "..", "test-fixtures");

type ScannerCase = { name: string; input: string; at: number; expect: number; section?: string };

const scannerFns: Record<string, (src: number[], at: number) => number> = {
  skipNoncode,
  findMatching,
  skipNoncodeMarkup,
  findMatchingMarkup,
};

test("uetkx-scanner-cases.json replays byte-identically", () => {
  const corpus = JSON.parse(fs.readFileSync(path.join(fixtures, "uetkx-scanner-cases.json"), "utf8"));
  let total = 0;
  for (const section of Object.keys(scannerFns)) {
    for (const c of corpus[section] as ScannerCase[]) {
      const got = scannerFns[section](toCodePoints(c.input), c.at);
      assert.strictEqual(got, c.expect, `[${section}] ${c.name}`);
      total++;
    }
  }
  for (const c of corpus.nonBmp as ScannerCase[]) {
    const fn = scannerFns[c.section!];
    assert.ok(fn, `nonBmp section ${c.section}`);
    assert.strictEqual(fn(toCodePoints(c.input), c.at), c.expect, `[nonBmp] ${c.name}`);
    total++;
  }
  assert.ok(total >= 50, `corpus is substantial (${total})`);
});

type FileScanExpect = {
  imports?: { names?: string[]; localNames?: string[]; namespace?: string; default?: string; specifier: string }[];
  components?: { name: string; exported: boolean; hookCalls?: string[] }[];
  hooks?: { name: string; exported: boolean }[];
  modules?: { name: string; exported: boolean }[];
  values?: { name: string; exported: boolean }[];
  utils?: { name: string; exported: boolean }[];
  order?: string[];
  diags?: string[];
  noError?: boolean;
  defaultExport?: string;
};
type FileScanCase = { name: string; input: string; basename?: string; expect: FileScanExpect };

// ES-modules (M1): a single canonical import stringification used for BOTH the expected (JSON)
// and actual (UetkxImportDecl) sides, so rename/namespace/default forms compare exactly.
function expectedImportKey(o: { names?: string[]; localNames?: string[]; namespace?: string; default?: string; specifier: string }): string {
  if (o.namespace !== undefined) return `*${o.namespace}<-${o.specifier}`;
  if (o.default !== undefined) return `default:${o.default}<-${o.specifier}`;
  const names = o.names ?? [];
  const locals = o.localNames ?? names;
  const pieces = names.map((name, n) => {
    const local = locals[n] ?? name;
    return local === name ? name : `${name}=>${local}`;
  });
  return `${pieces.join("|")}<-${o.specifier}`;
}
function actualImportKey(i: {
  isNamespace: boolean;
  namespaceAlias: string;
  isDefault: boolean;
  defaultAlias: string;
  names: string[];
  localNames: string[];
  specifier: string;
}): string {
  if (i.isNamespace) return `*${i.namespaceAlias}<-${i.specifier}`;
  if (i.isDefault) return `default:${i.defaultAlias}<-${i.specifier}`;
  const pieces = i.names.map((name, n) => {
    const local = i.localNames[n] ?? name;
    return local === name ? name : `${name}=>${local}`;
  });
  return `${pieces.join("|")}<-${i.specifier}`;
}

function orderName(scan: ReturnType<typeof scanFile>, kind: string, index: number): string {
  switch (kind) {
    case "component":
      return scan.components[index].name;
    case "hook":
      return scan.hooks[index].name;
    case "module":
      return scan.modules[index].name;
    case "value":
      return scan.values[index].name;
    case "util":
      return scan.utils[index].name;
    default:
      return "";
  }
}

test("fileScan corpus replays byte-identically (import/export/mixed-decl grammar)", () => {
  const corpus = JSON.parse(fs.readFileSync(path.join(fixtures, "uetkx-scanner-cases.json"), "utf8"));
  const declStr = (d: { name: string; exported: boolean }) => `${d.name}:${d.exported ? "E" : "P"}`;
  let total = 0;
  for (const section of ["fileScan", "fileScanLeg"]) {
    for (const c of corpus[section] as FileScanCase[]) {
      const scan = scanFile(c.input, c.basename ?? "");
      const label = `[${section}] ${c.name}`;
      const e = c.expect;
      if (e.imports) {
        const act = scan.imports.map(actualImportKey).join(";");
        const exp = e.imports.map(expectedImportKey).join(";");
        assert.strictEqual(act, exp, `${label} imports`);
      }
      if (e.defaultExport !== undefined) {
        assert.strictEqual(scan.defaultExportName, e.defaultExport, `${label} defaultExport`);
      }
      if (e.components) {
        assert.strictEqual(scan.components.map(declStr).join(","), e.components.map(declStr).join(","), `${label} components`);
        // §4.3 HMR protection pin: the ordered hook-call kinds (the signature input).
        e.components.forEach((exp, x) => {
          if (exp.hookCalls && x < scan.components.length) {
            assert.strictEqual(scan.components[x].hookCalls.join(","), exp.hookCalls.join(","), `${label} hookCalls`);
          }
        });
      }
      if (e.hooks) assert.strictEqual(scan.hooks.map(declStr).join(","), e.hooks.map(declStr).join(","), `${label} hooks`);
      if (e.modules) assert.strictEqual(scan.modules.map(declStr).join(","), e.modules.map(declStr).join(","), `${label} modules`);
      if (e.values) assert.strictEqual(scan.values.map(declStr).join(","), e.values.map(declStr).join(","), `${label} values`);
      if (e.utils) assert.strictEqual(scan.utils.map(declStr).join(","), e.utils.map(declStr).join(","), `${label} utils`);
      if (e.order) {
        const act = scan.order.map((o) => `${o.kind}:${orderName(scan, o.kind, o.index)}`).join(",");
        assert.strictEqual(act, e.order.join(","), `${label} order`);
      }
      if (e.diags) {
        const act = scan.diags.map((d) => d.code).sort().join(",");
        assert.strictEqual(act, [...e.diags].sort().join(","), `${label} diags`);
      }
      if (e.noError) assert.strictEqual(hasError(scan), false, `${label} noError`);
      total++;
    }
  }
  assert.ok(total >= 15, `fileScan corpus is substantial (${total})`);
});

test("src_hash contract pins (sidecar gating)", () => {
  assert.strictEqual(srcHash("ab"), 0x221505e6);
  assert.strictEqual(srcHash("\u{1F3AE}"), 0x6f5c2b7f); // surrogate pair = ONE code point
});
