// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// §4 markup-everywhere — the jsx scan (position-gated markup detection in embedded C++), the
// TS mirror of UetkxJsxScan.cpp. Boundary set: start-of-expression, `(`, `[`, `,`, simple `=`,
// `&&`, `||`, `?`, ternary `:`, `return`, `else`.

import { test } from "node:test";
import * as assert from "node:assert";
import { toCodePoints, fromCodePoints } from "../codePoints";
import { findMarkupRanges } from "../jsxScan";

function rangesOf(src: string) {
  const cp = toCodePoints(src);
  return findMarkupRanges(cp, 0, cp.length).map((r) => ({
    text: r.end === -1 ? fromCodePoints(cp, r.start) : fromCodePoints(cp, r.start, r.end - r.start),
    op: r.op,
    unbalanced: r.end === -1,
  }));
}

test("jsxScan: boundary positions detect markup; operators do not", () => {
  // simple `=` value
  assert.deepStrictEqual(rangesOf("FRuiNode N = <Spacer />;"), [{ text: "<Spacer />", op: "", unbalanced: false }]);
  // parenthesized value
  assert.deepStrictEqual(rangesOf("auto X = (<VerticalBox><Spacer /></VerticalBox>);"), [
    { text: "<VerticalBox><Spacer /></VerticalBox>", op: "", unbalanced: false },
  ]);
  // call argument (`,` + `(` boundaries)
  assert.deepStrictEqual(rangesOf("Rows.Add(<Spacer />); Fn(1, <Spacer />);"), [
    { text: "<Spacer />", op: "", unbalanced: false },
    { text: "<Spacer />", op: "", unbalanced: false },
  ]);
  // ternary both branches
  assert.deepStrictEqual(rangesOf("cond ? <A /> : <B />"), [
    { text: "<A />", op: "", unbalanced: false },
    { text: "<B />", op: "", unbalanced: false },
  ]);
  // short-circuit ops carry the op
  assert.deepStrictEqual(rangesOf("bOpen && <Chip />"), [{ text: "<Chip />", op: "&&", unbalanced: false }]);
  assert.deepStrictEqual(rangesOf("Node || <Fallback />"), [{ text: "<Fallback />", op: "||", unbalanced: false }]);
  // return boundary
  assert.deepStrictEqual(rangesOf("return <Spacer />;"), [{ text: "<Spacer />", op: "", unbalanced: false }]);
  // a LESS-THAN never matches: no boundary, or non-tag char after `<`
  assert.deepStrictEqual(rangesOf("if (a<b) { c = a < b; }"), []);
  assert.deepStrictEqual(rangesOf("TArray<FString> Names;"), []);
  assert.deepStrictEqual(rangesOf("i < Names.Num()"), []);
  // `::` and `->` stay operators
  assert.deepStrictEqual(rangesOf("FMath::Min(a, b)->Get()"), []);
});

test("jsxScan: markup interiors are opaque (holes, strings, comments, fragments)", () => {
  // attr holes with braces + strings with markup-looking text don't end the element
  assert.deepStrictEqual(
    rangesOf('auto X = (<Button Label={ FString::Printf(TEXT("<not-a-tag>")) }>go</Button>);'),
    [{ text: '<Button Label={ FString::Printf(TEXT("<not-a-tag>")) }>go</Button>', op: "", unbalanced: false }],
  );
  // fragments
  assert.deepStrictEqual(rangesOf("auto F = (<><Spacer /></>);"), [{ text: "<><Spacer /></>", op: "", unbalanced: false }]);
  // nested elements track depth
  assert.deepStrictEqual(rangesOf("auto D = <A><B><C /></B></A>;"), [
    { text: "<A><B><C /></B></A>", op: "", unbalanced: false },
  ]);
  // markup that never closes reports end: -1 (UETKX2508 upstream)
  assert.deepStrictEqual(rangesOf("auto Bad = <VerticalBox><Spacer />;"), [
    { text: "<VerticalBox><Spacer />;", op: "", unbalanced: true },
  ]);
});
