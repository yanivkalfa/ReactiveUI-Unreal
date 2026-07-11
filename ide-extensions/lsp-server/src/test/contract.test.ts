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
  imports?: { names: string[]; specifier: string }[];
  components?: { name: string; exported: boolean }[];
  hooks?: { name: string; exported: boolean }[];
  modules?: { name: string; exported: boolean }[];
  order?: string[];
  diags?: string[];
  noError?: boolean;
};
type FileScanCase = { name: string; input: string; basename?: string; expect: FileScanExpect };

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
        const act = scan.imports.map((i) => `${i.names.join("|")}<-${i.specifier}`).join(";");
        const exp = e.imports.map((i) => `${i.names.join("|")}<-${i.specifier}`).join(";");
        assert.strictEqual(act, exp, `${label} imports`);
      }
      if (e.components) assert.strictEqual(scan.components.map(declStr).join(","), e.components.map(declStr).join(","), `${label} components`);
      if (e.hooks) assert.strictEqual(scan.hooks.map(declStr).join(","), e.hooks.map(declStr).join(","), `${label} hooks`);
      if (e.modules) assert.strictEqual(scan.modules.map(declStr).join(","), e.modules.map(declStr).join(","), `${label} modules`);
      if (e.order) {
        const act = scan.order
          .map((o) => `${o.kind}:${o.kind === "component" ? scan.components[o.index].name : o.kind === "hook" ? scan.hooks[o.index].name : scan.modules[o.index].name}`)
          .join(",");
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
