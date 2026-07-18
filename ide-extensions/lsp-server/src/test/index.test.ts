// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TD-033 (LSP_COMPLETION_PLAN N0/N1): the workspace reference index, find-all-references and
// rename — collector shapes, scope tracking (the N-07 TS half), alias-aware cross-file queries,
// and the N-04/N-05 rename semantics incl. refusals.

import { test } from "node:test";
import * as assert from "node:assert";
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { scanFile } from "../uetkxFileScan";
import { collectFileReferences } from "../uetkxIndex";
import { findReferencesTo, renameSymbolAt, resolveSymbolAt } from "../uetkxWorkspace";

function refsOf(source: string, basename = "T") {
  return collectFileReferences(scanFile(source, basename, true), source);
}

test("index collector: tags (open + close), code refs, decl names, export markers", () => {
  const src = [
    'import { StatusChip } from "./StatusChip"',
    "",
    "export FRuiNode Panel() {",
    "\tauto A = FmtScore(1);",
    "\treturn (",
    "\t\t<VerticalBox>",
    "\t\t\t<StatusChip Label={ FmtScore(2) } />",
    "\t\t</VerticalBox>",
    "\t);",
    "}",
    "FString FmtScore(int32 S) {",
    "\treturn FString::FromInt(S);",
    "}",
    "export { FmtScore };",
    "",
  ].join("\n");
  const refs = refsOf(src, "Panel");
  const byKind = (k: string) => refs.filter((r) => r.kind === k);
  assert.ok(byKind("import-target").some((r) => r.name === "StatusChip"), "import target token");
  assert.deepStrictEqual(
    byKind("tag").map((r) => `${r.name}${r.closeTag ? "/close" : ""}`).sort(),
    ["StatusChip", "VerticalBox", "VerticalBox/close"],
    "open + close tags",
  );
  assert.strictEqual(byKind("code").filter((r) => r.name === "FmtScore").length, 2, "setup call + attr-expr call");
  assert.ok(byKind("decl-name").some((r) => r.name === "Panel") && byKind("decl-name").some((r) => r.name === "FmtScore"));
  assert.ok(byKind("export-list").some((r) => r.name === "FmtScore"), "export-list entry");
  // token positions are exact: the source substring at each ref equals its name
  for (const r of refs) {
    if (r.kind === "tag" || r.kind === "code" || r.kind === "decl-name") {
      assert.strictEqual([...src].slice(r.start, r.start + r.len).join(""), r.name, `${r.kind} ${r.name} span`);
    }
  }
});

test("index collector: scope tracking suppresses locals (params, auto, typed, bindings)", () => {
  const src = [
    "export FRuiNode Scoped(int32 Count) {",
    "\tauto Cool = 1;",              // auto local
    "\tFLinearColor Warm = Tint();", // typed local (Warm), Tint stays a ref
    "\tauto [A, B] = Pair();",       // structured bindings
    "\tint32 X = Count + Cool + Warm.R + A + B;",
    "\treturn ( <Spacer /> );",
    "}",
    "",
  ].join("\n");
  const names = refsOf(src, "Scoped").filter((r) => r.kind === "code").map((r) => r.name);
  assert.ok(!names.includes("Count"), "param suppressed");
  assert.ok(!names.includes("Cool"), "auto local suppressed");
  assert.ok(!names.includes("Warm"), "typed local suppressed");
  assert.ok(!names.includes("A") && !names.includes("B"), "structured bindings suppressed");
  assert.ok(!names.includes("X"), "typed local X suppressed");
  assert.ok(names.includes("Tint") && names.includes("Pair"), "real calls still referenced");
});

test("index collector: twin identity — `using`/`struct` head declarations; inner scopes expire at `}`", () => {
  // Mirrors FUetkxScopedLocals::Walk (N-07): only control keywords turn type-ish OFF, so
  // `using Alias = …;` and `struct Local { … }` both declare; a brace scope ends its shadows.
  const src = [
    "export FRuiNode Twin() {",
    "\tusing Alias = FLinearColor;",
    "\tstruct Local { int32 V; };",
    "\tauto A = Alias(Local());",
    "\tif (true) {",
    "\t\tauto Shadowed = 1;",
    "\t}",
    "\tauto B = Shadowed;", // the inner-scope local expired — this IS a reference again
    "\treturn ( <Spacer /> );",
    "}",
    "",
  ].join("\n");
  const names = refsOf(src, "Twin").filter((r) => r.kind === "code").map((r) => r.name);
  assert.ok(!names.includes("Alias"), "`using Alias =` declares (type-ish head)");
  assert.ok(!names.includes("Local"), "`struct Local {` declares");
  assert.ok(names.includes("Shadowed"), "inner-scope local expires at `}` — outer use is a ref");
});

test("index collector: a ternary's second arm is NOT a declaration (lone-colon IMPORT-3 rule)", () => {
  // `U = a ? Cool : Cool;` — the first arm sets the type-ish lookbehind and the lone `:` must
  // reset it, or `Cool ;` reads as `Type Name;` and the second-arm reference vanishes (the exact
  // C++-twin regression the Codegen ternary pin caught). A real `::` qual still keeps type-ish.
  const src = [
    "export FRuiNode Tern(bool bOn) {",
    "\tauto U = bOn ? Cool : Cool;",
    "\tstd::atomic Flag = 1;", // `::`-qualified type still declares Flag
    "\tauto F = Flag;",
    "\treturn ( <Spacer /> );",
    "}",
    "",
  ].join("\n");
  const names = refsOf(src, "Tern").filter((r) => r.kind === "code").map((r) => r.name);
  assert.strictEqual(names.filter((n) => n === "Cool").length, 2, "both ternary arms stay references");
  assert.ok(!names.includes("Flag"), "`std::atomic Flag` still declares through the `::` qual");
});

test("index collector: namespace quals emit base + member; ternary second arm still refs (IMPORT-3)", () => {
  const src = [
    'import * as Pal from "./Palette"',
    "",
    "export FRuiNode Q(bool bOn) {",
    "\tconst FLinearColor T = bOn ? Pal::Cool : Pal::Warm;",
    "\treturn ( <Spacer /> );",
    "}",
    "",
  ].join("\n");
  const refs = refsOf(src, "Q");
  const quals = refs.filter((r) => r.kind === "qual-member").map((r) => r.name).sort();
  assert.deepStrictEqual(quals, ["Cool", "Warm"], "BOTH ternary arms' members (the lone-colon rule)");
  assert.strictEqual(refs.filter((r) => r.kind === "code" && r.name === "Pal").length, 2, "both base tokens");
});

// ── a 3-file workspace for the cross-file queries ────────────────────────────────────────────

function makeWorkspace() {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "uetkx-idx-"));
  fs.writeFileSync(path.join(root, "Demo.uproject"), "{}");
  const chip = path.join(root, "Chip.uetkx");
  fs.writeFileSync(
    chip,
    'export FRuiNode Chip() {\n\treturn ( <Spacer /> );\n}\nexport FLinearColor Cool = FLinearColor(0.1f, 0.2f, 0.3f, 1.0f);\n',
  );
  const plain = path.join(root, "PlainUser.uetkx");
  fs.writeFileSync(
    plain,
    'import { Chip, Cool } from "./Chip"\n\nexport FRuiNode PlainUser() {\n\tauto C = Cool;\n\treturn ( <Border>\n\t\t<Chip />\n\t</Border> );\n}\n',
  );
  const renamed = path.join(root, "RenamedUser.uetkx");
  fs.writeFileSync(
    renamed,
    'import { Chip as Badge } from "./Chip"\n\nexport FRuiNode RenamedUser() {\n\treturn ( <Badge /> );\n}\n',
  );
  const ns = path.join(root, "NsUser.uetkx");
  fs.writeFileSync(
    ns,
    'import * as C from "./Chip"\n\nexport FRuiNode NsUser() {\n\tauto T = C::Cool;\n\treturn ( <Spacer /> );\n}\n',
  );
  return { root, chip, plain, renamed, ns };
}

test("find-all-references: a declaration collects across the whole workspace, alias-aware", () => {
  const { root, chip, plain } = makeWorkspace();
  try {
    const chipText = fs.readFileSync(chip, "utf8");
    const symbol = resolveSymbolAt(chip, chipText.indexOf("Chip()") + 1);
    assert.ok(symbol && symbol.type === "global" && symbol.name === "Chip", "decl resolves to a global symbol");
    const refs = findReferencesTo(symbol!, chip);
    const byFile = (f: string) => refs.filter((r) => r.file === path.resolve(f).replace(/\\/g, "/"));
    // declaring file: the decl token
    assert.ok(byFile(chip).some((r) => r.kind === "decl-name"));
    // plain importer: the binding token AND the local tag (open + close would be 1 self-closing tag here)
    const plainRefs = byFile(plain);
    assert.ok(plainRefs.some((r) => r.kind === "import-target"), "plain binding token");
    assert.ok(plainRefs.some((r) => r.kind === "tag"), "plain importer's tag counts");
    // renamed importer: ONLY the target token (its <Badge/> spelling is the alias's, not Chip's)
    const renamedRefs = refs.filter((r) => r.file.endsWith("RenamedUser.uetkx"));
    assert.strictEqual(renamedRefs.length, 1, "renamed importer contributes exactly the target token");
    assert.strictEqual(renamedRefs[0].kind, "import-target");
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test("find-all-references: values through namespace quals; local alias stays local", () => {
  const { root, chip, plain, ns, renamed } = makeWorkspace();
  try {
    const chipText = fs.readFileSync(chip, "utf8");
    const coolSym = resolveSymbolAt(chip, chipText.indexOf("Cool"));
    assert.ok(coolSym && coolSym.type === "global" && coolSym.name === "Cool");
    const coolRefs = findReferencesTo(coolSym!, chip);
    assert.ok(coolRefs.some((r) => r.file.endsWith("NsUser.uetkx") && r.kind === "qual-member"), "C::Cool member counts");
    assert.ok(coolRefs.some((r) => r.file.endsWith("PlainUser.uetkx") && r.kind === "code"), "plain value ref counts");

    // the local alias Badge: references confined to RenamedUser.uetkx
    const renamedText = fs.readFileSync(renamed, "utf8");
    const aliasSym = resolveSymbolAt(renamed, renamedText.indexOf("Badge"));
    assert.ok(aliasSym && aliasSym.type === "local-alias", "alias token resolves locally");
    const aliasRefs = findReferencesTo(aliasSym!, renamed);
    assert.ok(aliasRefs.length >= 2, "alias token + tag");
    assert.ok(aliasRefs.every((r) => r.file.endsWith("RenamedUser.uetkx")), "never leaves the importer");
    void plain;
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test("rename: declaration rename edits decl + plain importers (binding AND local refs) + renamed importers (target only)", () => {
  const { root, chip } = makeWorkspace();
  try {
    const chipText = fs.readFileSync(chip, "utf8");
    const result = renameSymbolAt(chip, chipText.indexOf("Chip()") + 1, "Pip", new Set());
    assert.ok("edits" in result, `rename succeeded: ${JSON.stringify(result)}`);
    const edits = (result as { edits: Array<{ file: string; newText: string }> }).edits;
    const inFile = (suffix: string) => edits.filter((e) => e.file.endsWith(suffix));
    assert.ok(inFile("Chip.uetkx").length >= 1, "decl token edited");
    assert.strictEqual(inFile("PlainUser.uetkx").length, 2, "plain importer: the binding token + the self-closing tag");
    assert.strictEqual(inFile("RenamedUser.uetkx").length, 1, "renamed importer: target token only");
    assert.ok(edits.every((e) => e.newText === "Pip"), "simple token replacement");
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test("rename: local alias renames within one file; plain-binding cursor inserts `as`", () => {
  const { root, renamed, plain } = makeWorkspace();
  try {
    const renamedText = fs.readFileSync(renamed, "utf8");
    const aliasResult = renameSymbolAt(renamed, renamedText.indexOf("<Badge") + 2, "Crest", new Set());
    assert.ok("edits" in aliasResult);
    const aliasEdits = (aliasResult as { edits: Array<{ file: string; newText: string }> }).edits;
    assert.ok(aliasEdits.every((e) => e.file.endsWith("RenamedUser.uetkx")), "alias rename stays local");
    assert.ok(aliasEdits.length >= 2, "alias token + tag");

    // cursor ON the plain binding token `Cool` in PlainUser's import → `Cool as NewName`
    const plainText = fs.readFileSync(plain, "utf8");
    const bindResult = renameSymbolAt(plain, plainText.indexOf("Cool"), "Accent", new Set());
    assert.ok("edits" in bindResult);
    const bindEdits = (bindResult as { edits: Array<{ file: string; start: number; newText: string }> }).edits;
    assert.ok(bindEdits.some((e) => e.newText === "Cool as Accent"), "alias-insertion at the binding");
    assert.ok(bindEdits.some((e) => e.newText === "Accent"), "local refs follow");
    assert.ok(bindEdits.every((e) => e.file.endsWith("PlainUser.uetkx")), "importer-only");
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test("rename refusals: invalid ident, collision in an edited file, exported elsewhere, host tag", () => {
  const { root, chip, plain } = makeWorkspace();
  try {
    const chipText = fs.readFileSync(chip, "utf8");
    const at = chipText.indexOf("Chip()") + 1;
    assert.ok("error" in renameSymbolAt(chip, at, "not an ident", new Set()), "invalid identifier refused");
    // `Cool` is bound in Chip.uetkx AND PlainUser.uetkx — collision in an edited file
    assert.ok("error" in renameSymbolAt(chip, at, "Cool", new Set()), "collision refused");
    // `PlainUser` is exported by another file — the 2106 shape
    assert.ok("error" in renameSymbolAt(chip, at, "PlainUser", new Set()), "cross-file export collision refused");
    // a host element name
    assert.ok("error" in renameSymbolAt(chip, at, "Border", new Set(["Border"])), "host-tag shadowing refused");
    void plain;
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test("rename reads DIRTY text through the overlay, never stale disk state", () => {
  const { root, chip } = makeWorkspace();
  try {
    // Disk still says Chip; the overlay renames the component to Zip and adds a use.
    const live = 'export FRuiNode Zip() {\n\treturn ( <Spacer /> );\n}\nexport FLinearColor Cool = FLinearColor(0.1f, 0.2f, 0.3f, 1.0f);\n';
    const overlay = new Map([[path.resolve(chip).replace(/\\/g, "/"), live]]);
    const sym = resolveSymbolAt(chip, live.indexOf("Zip"), overlay);
    assert.ok(sym && sym.type === "global" && sym.name === "Zip", "overlay text is what resolves");
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

// ── audit 2026-07-18: phantom-ref elimination + tracker parity pins ─────────────────────────

test("audit: markup islands see setup locals — no phantom refs (rename must never edit them)", () => {
  const src = [
    "export FRuiNode Panel() {",
    "\tauto Total = Compute();",
    "\tFRuiNode Chip = <Spacer />;",
    "\tauto AfterRange = Total;", // post-range fragment still knows Total
    "\treturn (",
    "\t\t<VerticalBox>",
    "\t\t\t<TextBlock Text={ FText::AsNumber(Total) } />",
    "\t\t\t{ Chip }",
    "\t\t</VerticalBox>",
    "\t);",
    "}",
    "",
  ].join("\n");
  const names = refsOf(src, "Panel").filter((r) => r.kind === "code").map((r) => r.name);
  assert.ok(!names.includes("Total"), "setup local used in an attr expr is NOT a code ref");
  assert.ok(!names.includes("Chip"), "value-markup local spliced as a child is NOT a code ref");
  assert.ok(names.includes("Compute"), "the real call is still referenced");
});

test("audit: range-for vars, lambda params, and @for loop vars are locals everywhere", () => {
  const src = [
    "export FRuiNode Loops(TArray<FString> Items) {",
    "\tfor (const FString& Item : Items) {",
    "\t\tUse(Item);",
    "\t}",
    "\tauto Fn = [](const FLinearColor& Tint) { return Tint.R; };",
    "\treturn (",
    "\t\t<VerticalBox>",
    "\t\t\t@for (int32 Row = 0; Row < 3; ++Row) {",
    "\t\t\t\treturn ( <TextBlock Text={ FText::AsNumber(Row) } /> )",
    "\t\t\t}",
    "\t\t</VerticalBox>",
    "\t);",
    "}",
    "",
  ].join("\n");
  const names = refsOf(src, "Loops").filter((r) => r.kind === "code").map((r) => r.name);
  assert.ok(!names.includes("Item"), "range-for var is a local (header + body uses)");
  assert.ok(!names.includes("Tint"), "lambda param is a local");
  assert.ok(!names.includes("Row"), "@for loop var is a local inside the nested attr expr");
});

test("audit: directive-lead locals are live in the nested window; comma resets type-ish", () => {
  const src = [
    "export FRuiNode Lead() {",
    "\tauto T = MakeTuple(1, Primary);", // B-arg of a call is a REF, not a decl
    "\treturn (",
    "\t\t<VerticalBox>",
    "\t\t\t@if (true) {",
    "\t\t\t\tFLinearColor Warm = Palette();",
    "\t\t\t\treturn ( <Border BorderBackgroundColor={ Warm } /> )",
    "\t\t\t}",
    "\t\t</VerticalBox>",
    "\t);",
    "}",
    "",
  ].join("\n");
  const names = refsOf(src, "Lead").filter((r) => r.kind === "code").map((r) => r.name);
  assert.ok(!names.includes("Warm"), "directive-lead local stays local inside the nested attr expr");
  assert.ok(names.includes("Primary"), "a call argument after a comma is still a reference");
});

test("audit: comments inside an import name list parse (the C++ SCAN-2 behavior)", () => {
  const scan = scanFile('import { A, /* note */ B as C, // tail\n\tD } from "./m"\nexport FRuiNode T() {\n\treturn ( <Spacer /> );\n}\n', "T", true);
  assert.strictEqual(scan.imports.length, 1);
  assert.deepStrictEqual(scan.imports[0].names, ["A", "B", "D"]);
  assert.deepStrictEqual(scan.imports[0].localNames, ["A", "C", "D"]);
  assert.ok(!scan.diags.some((d) => d.code === "UETKX0300"), "no spurious bad-name diagnostic");
});

test("audit: a plain-binding rename validates only the IMPORTER it edits (no global over-refusal)", () => {
  const { root, plain } = makeWorkspace();
  try {
    // `NsUser` is exported by another file and bound in NsUser.uetkx — but this rename edits
    // ONLY PlainUser.uetkx (`Cool as NsUser`), so it must be allowed (N-04/N-05: "any EDITED
    // file"). It still refuses names bound in the importer itself.
    const plainText = fs.readFileSync(plain, "utf8");
    const at = plainText.indexOf("Cool");
    const ok = renameSymbolAt(plain, at, "NsUser", new Set());
    assert.ok("edits" in ok, "a name bound/exported elsewhere is legal as a local alias");
    const edits = (ok as { edits: Array<{ file: string; newText: string }> }).edits;
    assert.ok(edits.some((e) => e.newText === "Cool as NsUser") && edits.every((e) => e.file.endsWith("PlainUser.uetkx")));
    const clash = renameSymbolAt(plain, at, "PlainUser", new Set());
    assert.ok("error" in clash, "a name bound in the importer itself still refuses");
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});
