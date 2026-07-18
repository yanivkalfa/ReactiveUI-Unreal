// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TD-033 N3: the code-action pure halves — unused-import removal spans, the import-insertion
// point, and the 2320 wrapper→plain rewrite, which is PINNED byte-identical to the codemod's
// per-declaration forms (RUIMigrateImportsCommandlet pass 3): the same input declaration must
// migrate to the same output whether the codemod or the lightbulb does it.

import { test } from "node:test";
import * as assert from "node:assert";
import { scanFile } from "../uetkxFileScan";
import { firstDeclStartCp, unusedImportRemoval, wrapperRewriteAt } from "../uetkxActions";

const cpSlice = (text: string, start: number, end: number) => [...text].slice(start, end).join("");
const applyRewrite = (text: string, r: { start: number; end: number; text: string }) =>
  [...text].slice(0, r.start).join("") + r.text + [...text].slice(r.end).join("");

test("2320 action: component rewrite matches the codemod's form (param colon-flip, () gained)", () => {
  const src = 'export component ScoreRow(Label: FString = FString(TEXT("x")), N: int32 = 0) {\n\treturn ( <Spacer /> );\n}\n';
  const scan = scanFile(src, "ScoreRow", true);
  const r = wrapperRewriteAt(scan, src.indexOf("component"));
  assert.ok(r, "rewrite offered");
  const migrated = applyRewrite(src, r!);
  // The codemod's exact output shape for the same declaration (M7 pass 3).
  assert.strictEqual(
    migrated,
    'export FRuiNode ScoreRow(FString Label = FString(TEXT("x")), int32 N = 0) {\n\treturn ( <Spacer /> );\n}\n',
  );
  // and it re-scans clean as a NEW-form component (no 2320)
  const rescanned = scanFile(migrated, "ScoreRow", true);
  assert.ok(rescanned.components.length === 1 && !rescanned.components[0].legacySyntax);
  assert.ok(!rescanned.diags.some((d) => d.code === "UETKX2320"));
});

test("2320 action: param-less component gains (); hook's -> flips to a leading return type", () => {
  const noParams = "component Tiny {\n\treturn ( <Spacer /> );\n}\n";
  const scanA = scanFile(noParams, "Tiny", true);
  const ra = wrapperRewriteAt(scanA, noParams.indexOf("component"));
  assert.strictEqual(applyRewrite(noParams, ra!), "FRuiNode Tiny() {\n\treturn ( <Spacer /> );\n}\n");

  const hook = "export hook UseTick(int32 Start) -> TTuple<int32, int32> {\n\treturn MakeTuple(Start, Start);\n}\n";
  const scanB = scanFile(hook, "UseTick", true);
  const rb = wrapperRewriteAt(scanB, hook.indexOf("hook"));
  assert.strictEqual(
    applyRewrite(hook, rb!),
    "export TTuple<int32, int32> UseTick(int32 Start) {\n\treturn MakeTuple(Start, Start);\n}\n",
  );

  const voidHook = "hook UseFire() {\n}\n";
  const scanC = scanFile(voidHook, "UseFire", true);
  const rc = wrapperRewriteAt(scanC, 0);
  assert.strictEqual(applyRewrite(voidHook, rc!), "void UseFire() {\n}\n");
});

test("2320 action: modules are NOT offered (the codemod owns cross-file hoists)", () => {
  const src = "export module Styles { inline const float P = 1.0f; }\n";
  const scan = scanFile(src, "Styles", true);
  assert.strictEqual(wrapperRewriteAt(scan, src.indexOf("module")), null);
});

test("unused-import removal: middle binding takes its comma; sole binding takes the line; namespace/default take the line", () => {
  const src = 'import { A, B, C } from "./X"\nexport FRuiNode T() {\n\treturn ( <A /> );\n}\n';
  const scan = scanFile(src, "T", true);
  const bAt = src.indexOf("B");
  const span = unusedImportRemoval(scan, src, bAt);
  assert.ok(span);
  assert.strictEqual(cpSlice(src, span!.start, span!.end), "B, ", "binding + its trailing comma");

  const sole = 'import { Only } from "./X"\nexport FRuiNode T() {\n\treturn ( <Spacer /> );\n}\n';
  const scanSole = scanFile(sole, "T", true);
  const spanSole = unusedImportRemoval(scanSole, sole, sole.indexOf("Only"));
  assert.strictEqual(cpSlice(sole, spanSole!.start, spanSole!.end), 'import { Only } from "./X"\n', "whole line");

  const ns = 'import * as P from "./X"\nexport FRuiNode T() {\n\treturn ( <Spacer /> );\n}\n';
  const scanNs = scanFile(ns, "T", true);
  const spanNs = unusedImportRemoval(scanNs, ns, ns.indexOf("P from"));
  assert.strictEqual(cpSlice(ns, spanNs!.start, spanNs!.end), 'import * as P from "./X"\n');
});

test("unused-import removal: a renamed binding removes `Name as Local` whole", () => {
  const src = 'import { A as B, C } from "./X"\nexport FRuiNode T() {\n\treturn ( <C /> );\n}\n';
  const scan = scanFile(src, "T", true);
  const span = unusedImportRemoval(scan, src, src.indexOf("B,"));
  assert.strictEqual(cpSlice(src, span!.start, span!.end), "A as B, ");
});

test("firstDeclStartCp: the import-insertion point is the first decl's true start (export included)", () => {
  const src = '#include "X.h"\n\nexport FRuiNode T() {\n\treturn ( <Spacer /> );\n}\n';
  const scan = scanFile(src, "T", true);
  assert.strictEqual(firstDeclStartCp(scan), [...src].join("").indexOf("export"));
});

test("unused-import removal: a MULTI-LINE sole-binding import removes the whole statement (audit)", () => {
  const src = 'import {\n\tOnly\n} from "./X"\nexport FRuiNode T() {\n\treturn ( <Spacer /> );\n}\n';
  const scan = scanFile(src, "T", true);
  const span = unusedImportRemoval(scan, src, src.indexOf("Only"));
  assert.ok(span, "span offered");
  assert.strictEqual(cpSlice(src, span!.start, span!.end), 'import {\n\tOnly\n} from "./X"\n', "no dangling `} from` left behind");
});

test("unused-import removal: a tab after the comma is cleaned up too (audit)", () => {
  const src = 'import { A,\tB } from "./X"\nexport FRuiNode T() {\n\treturn ( <A /> );\n}\n';
  const scan = scanFile(src, "T", true);
  const span = unusedImportRemoval(scan, src, src.indexOf("A,"));
  assert.strictEqual(cpSlice(src, span!.start, span!.end), "A,\t", "binding + comma + the tab");
});
