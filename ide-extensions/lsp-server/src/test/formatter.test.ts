// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// Replay the SHARED formatter golden corpus (the C++ ReactiveUI.Uetkx.Formatter suite runs
// the same file) — byte-identical output required, every non-fellback case an idempotence pin.

import { test } from "node:test";
import * as assert from "node:assert";
import * as fs from "node:fs";
import * as path from "node:path";
import { formatUetkx, UetkxFormatOptions } from "../formatUetkx";

type FormatterCase = {
  name: string;
  input: string;
  expect?: string;
  fellBack?: boolean;
  opts?: Partial<UetkxFormatOptions>;
};

test("uetkx-formatter-cases.json replays byte-identically", () => {
  const corpus = JSON.parse(
    fs.readFileSync(path.join(__dirname, "..", "..", "test-fixtures", "uetkx-formatter-cases.json"), "utf8"),
  );
  const cases = corpus.cases as FormatterCase[];
  assert.ok(cases.length >= 12, "corpus is substantial");
  for (const c of cases) {
    const result = formatUetkx(c.input, c.opts);
    if (c.fellBack) {
      assert.strictEqual(result.fellBack, true, `[${c.name}] fell back`);
      assert.strictEqual(result.text, c.input, `[${c.name}] verbatim`);
      continue;
    }
    assert.strictEqual(result.fellBack, false, `[${c.name}] no fallback`);
    assert.strictEqual(result.text, c.expect, `[${c.name}] output`);
    assert.strictEqual(result.changed, c.input !== c.expect, `[${c.name}] changed flag`);
    const second = formatUetkx(result.text, c.opts);
    assert.strictEqual(second.text, result.text, `[${c.name}] idempotent`);
  }
});
