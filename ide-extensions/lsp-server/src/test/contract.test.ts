// The cross-language contract (D-22): replay the SAME corpora the C++ automation suites run.
// A case failing here while passing in C++ means the two scanners have drifted — fix the
// PORT, never the fixture.

import { test } from "node:test";
import * as assert from "node:assert";
import * as fs from "node:fs";
import * as path from "node:path";
import { toCodePoints } from "../codePoints";
import { findMatching, findMatchingMarkup, skipNoncode, skipNoncodeMarkup, srcHash } from "../cppScanner";

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

test("src_hash contract pins (sidecar gating)", () => {
  assert.strictEqual(srcHash("ab"), 0x221505e6);
  assert.strictEqual(srcHash("\u{1F3AE}"), 0x6f5c2b7f); // surrogate pair = ONE code point
});
