// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Resolve — the import resolver + strict-usage diagnostics (M4). Builds scratch
// .uetkx trees under Saved/ and pins the resolution of `./ ../ ~/` specifiers plus every
// CompileSource-level family diagnostic (2300-2309). The 2308 module-boundary case uses a second
// tree with real `*.Build.cs` module roots.

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UetkxCodegen.h"
#include "UetkxResolve.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UetkxResolveTest
{
	static void Write(const FString& Path, const FString& Body)
	{
		FFileHelper::SaveStringToFile(Body, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	static FString Norm(FString P)
	{
		P = FPaths::ConvertRelativePathToFull(P);
		FPaths::NormalizeFilename(P);
		return P;
	}
	static TArray<FString> Codes(const FString& Source, const FString& Rel, const IUetkxImportResolver& R)
	{
		const FString Base = FPaths::GetBaseFilename(Rel);
		const FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, Base, Rel, &R);
		TArray<FString> C;
		for (const FUetkxDiag& D : Out.Diags)
		{
			C.Add(D.Code);
		}
		return C;
	}
} // namespace UetkxResolveTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxResolveTest, "ReactiveUI.Uetkx.Resolve",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxResolveTest::RunTest(const FString&)
{
	using namespace UetkxResolveTest;
	IFileManager& FM = IFileManager::Get();
	auto Has = [this](const TArray<FString>& C, const TCHAR* Code)
	{ return TestTrue(FString::Printf(TEXT("diags contain %s"), Code), C.Contains(FString(Code))); };
	auto HasNot = [this](const TArray<FString>& C, const TCHAR* Code)
	{ return TestFalse(FString::Printf(TEXT("diags do NOT contain %s"), Code), C.Contains(FString(Code))); };

	// ── scratch tree (fixture-mode resolver: ~/ = root, flat, no module boundary) ─────────────
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUI"), TEXT("ResolveTest"));
	FM.DeleteDirectory(*Root, false, true);
	FM.MakeDirectory(*(Root / TEXT("Screens")), true);
	FM.MakeDirectory(*(Root / TEXT("hooks")), true);
	Write(Root / TEXT("Screens/Chip.uetkx"), TEXT("export component Chip {\n\treturn ( <Spacer /> );\n}\n"));
	Write(Root / TEXT("Screens/Mixed.uetkx"),
		  TEXT("export component Mixed {\n\treturn ( <Spacer /> );\n}\nhook UsePriv() {\n}\n"));
	Write(Root / TEXT("hooks/Thing.uetkx"), TEXT("export hook UseThing() {\n}\n"));

	const FUetkxFsResolver R(Root, {Root}, /*bFixtureMode*/ true);

	// ── Resolve() specifier forms ─────────────────────────────────────────────────────────────
	TestEqual(TEXT("./ resolves"), Norm(R.Resolve(TEXT("./Chip"), TEXT("Screens/App.uetkx"))),
			  Norm(Root / TEXT("Screens/Chip.uetkx")));
	TestEqual(TEXT("../ resolves"), Norm(R.Resolve(TEXT("../hooks/Thing"), TEXT("Screens/App.uetkx"))),
			  Norm(Root / TEXT("hooks/Thing.uetkx")));
	TestEqual(TEXT("~/ resolves at the fixture root"),
			  Norm(R.Resolve(TEXT("~/hooks/Thing"), TEXT("Screens/App.uetkx"))),
			  Norm(Root / TEXT("hooks/Thing.uetkx")));
	TestTrue(TEXT("unknown specifier is unresolved"), R.Resolve(TEXT("./Nope"), TEXT("Screens/App.uetkx")).IsEmpty());
	TestTrue(TEXT("bare/engine-native specifier is forbidden"),
			 R.Resolve(TEXT("Chip"), TEXT("Screens/App.uetkx")).IsEmpty());
	TestTrue(TEXT("res:// specifier is forbidden"), R.Resolve(TEXT("res://Chip"), TEXT("Screens/App.uetkx")).IsEmpty());

	// ── strict diagnostics, one per case (rel = Screens/T.uetkx so ./ anchors in Screens) ─────
	const FString Rel = TEXT("Screens/T.uetkx");
	{ // clean: import a used component (the `component`/`hook` wrapper keywords are still legal —
	  // ES-modules 2320 is the one expected deprecation warning, not an error)
		const TArray<FString> C =
			Codes(TEXT("import { Chip } from \"./Chip\"\ncomponent T {\n\treturn ( <Chip /> );\n}\n"), Rel, R);
		TestEqual(TEXT("clean import has only the 2320 deprecation warning"), C.Num(), 1);
	}
	{ // clean: ~/ hook import + use
		const TArray<FString> C = Codes(TEXT("import { UseThing } from \"~/hooks/Thing\"\ncomponent T "
											 "{\n\tUseThing();\n\treturn ( <Spacer /> );\n}\n"),
										Rel, R);
		TestEqual(TEXT("~/ hook import + use has only the 2320 deprecation warning"), C.Num(), 1);
	}
	Has(Codes(TEXT("import { Chip } from \"./Nope\"\ncomponent T {\n\treturn ( <Spacer /> );\n}\n"), Rel, R),
		TEXT("UETKX2300")); // unknown specifier
	Has(Codes(TEXT("import { UsePriv } from \"./Mixed\"\ncomponent T {\n\tUsePriv();\n\treturn ( <Spacer /> );\n}\n"),
			  Rel, R),
		TEXT("UETKX2301")); // imported name is not exported
	Has(Codes(TEXT("import { Ghost } from \"./Chip\"\ncomponent T {\n\treturn ( <Ghost /> );\n}\n"), Rel, R),
		TEXT("UETKX2302")); // imported name is not declared
	Has(Codes(TEXT("import { Chip } from \"./Chip\"\nimport { Chip } from \"./Chip\"\ncomponent T {\n\treturn ( "
				   "<Chip /> );\n}\n"),
			  Rel, R),
		TEXT("UETKX2303")); // duplicate import
	Has(Codes(TEXT("import { Chip } from \"./Chip\"\ncomponent T {\n\treturn ( <Spacer /> );\n}\n"), Rel, R),
		TEXT("UETKX2304")); // unused import (warn)
	Has(Codes(TEXT("component T {\n\treturn ( <Chip /> );\n}\n"), Rel, R),
		TEXT("UETKX2305")); // used but not imported (exported elsewhere)
	Has(Codes(TEXT("component T {\n\treturn ( <Nonexistent /> );\n}\n"), Rel, R),
		TEXT("UETKX2307")); // used, exported by no file
	Has(Codes(TEXT("component T {\n\treturn ( <Spacer /> );\n}\nimport { Chip } from \"./Chip\"\n"), Rel, R),
		TEXT("UETKX2309")); // import after a declaration

	// same-file references are exempt from the import requirement.
	{
		const TArray<FString> C =
			Codes(TEXT("component T {\n\tUseLocal();\n\treturn ( <Spacer /> );\n}\nhook UseLocal() {\n}\n"), Rel, R);
		HasNot(C, TEXT("UETKX2305"));
		HasNot(C, TEXT("UETKX2307"));
	}
	// a hand-written C++ namespace qual (exported by no file) is ambient — never 2305/2307.
	{
		const TArray<FString> C =
			Codes(TEXT("component T {\n\tint32 X = RuiDemo::Value;\n\treturn ( <Spacer /> );\n}\n"), Rel, R);
		HasNot(C, TEXT("UETKX2305"));
		HasNot(C, TEXT("UETKX2307"));
	}

	// ── ES-modules (M4): rename / `* as` / default resolution matrix + 2326 + value/util refs ────
	{
		// Fixtures live BESIDE the importer (Screens/T.uetkx — every specifier is ./-relative), and
		// a FRESH resolver is constructed after writing them: the exporter index is built once per
		// resolver (by design — the driver makes one per sweep), so R's index predates these files.
		Write(Root / TEXT("Screens/Palette.uetkx"),
			  TEXT("export FLinearColor Cool = FLinearColor(0.2f, 0.6f, 0.9f, 1.0f);\n")
				  TEXT("FLinearColor Hidden = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);\n")
					  TEXT("export FString FmtP(int32 S) {\n\treturn FString::FromInt(S);\n}\n"));
		Write(Root / TEXT("Screens/Screen.uetkx"),
			  TEXT("export FRuiNode Screen() {\n\treturn ( <Spacer /> );\n}\n") TEXT("export default Screen;\n"));
		Write(Root / TEXT("Screens/Deck.uetkx"),
			  TEXT("export FRuiNode Card() {\n\treturn ( <Spacer /> );\n}\n")
				  TEXT("export FLinearColor Tint = FLinearColor(0.1f, 0.2f, 0.3f, 1.0f);\n")
					  TEXT("export default Card;\n"));
		Write(Root / TEXT("Screens/Deck2.uetkx"),
			  TEXT("export FLinearColor Hue = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);\n")
				  TEXT("FString FmtD(int32 S) {\n\treturn FString::FromInt(S);\n}\n") TEXT("export default FmtD;\n"));
		const FUetkxFsResolver R3(Root, {Root}, /*bFixtureMode*/ true);

		// Rename import: TARGET validated, LOCAL used — clean modulo the legacy-wrapper 2320.
		{
			const TArray<FString> C = Codes(TEXT("import { Chip as Badge } from \"./Chip\"\ncomponent T {\n\treturn ( "
												 "<Badge /> );\n}\n"),
											Rel, R3);
			HasNot(C, TEXT("UETKX2301"));
			HasNot(C, TEXT("UETKX2302"));
			HasNot(C, TEXT("UETKX2304"));
			HasNot(C, TEXT("UETKX2305"));
			HasNot(C, TEXT("UETKX2307"));
		}
		// Rename import validates the TARGET name, not the alias.
		Has(Codes(TEXT("import { Ghost as Badge } from \"./Chip\"\ncomponent T {\n\treturn ( <Badge /> );\n}\n"), Rel,
				  R3),
			TEXT("UETKX2302"));
		// Unused rename import warns on the LOCAL alias.
		Has(Codes(TEXT("import { Chip as Badge } from \"./Chip\"\ncomponent T {\n\treturn ( <Spacer /> );\n}\n"), Rel,
				  R3),
			TEXT("UETKX2304"));

		// Namespace import: member validated against the target's export surface.
		{
			const TArray<FString> C = Codes(TEXT("import * as P from \"./Palette\"\ncomponent T {\n\tauto X = "
												 "P::Cool;\n\treturn ( <Spacer /> );\n}\n"),
											Rel, R3);
			HasNot(C, TEXT("UETKX2301"));
			HasNot(C, TEXT("UETKX2302"));
			HasNot(C, TEXT("UETKX2304"));
		}
		Has(Codes(TEXT("import * as P from \"./Palette\"\ncomponent T {\n\tauto X = P::Ghost;\n\treturn ( <Spacer /> "
					   ");\n}\n"),
				  Rel, R3),
			TEXT("UETKX2302")); // member not declared
		Has(Codes(TEXT("import * as P from \"./Palette\"\ncomponent T {\n\tauto X = P::Hidden;\n\treturn ( <Spacer /> "
					   ");\n}\n"),
				  Rel, R3),
			TEXT("UETKX2301")); // member declared, not exported
		Has(Codes(TEXT("import * as P from \"./Palette\"\ncomponent T {\n\treturn ( <Spacer /> );\n}\n"), Rel, R3),
			TEXT("UETKX2304")); // namespace alias never used

		// Default import: resolves through `export default`; 2326 when the target has none.
		{
			const TArray<FString> C =
				Codes(TEXT("import Home from \"./Screen\"\ncomponent T {\n\treturn ( <Home /> );\n}\n"), Rel, R3);
			HasNot(C, TEXT("UETKX2326"));
			HasNot(C, TEXT("UETKX2305"));
			HasNot(C, TEXT("UETKX2307"));
		}
		Has(Codes(TEXT("import Home from \"./Chip\"\ncomponent T {\n\treturn ( <Home /> );\n}\n"), Rel, R3),
			TEXT("UETKX2326")); // Chip.uetkx has no default export

		// ES COMBINED forms: ONE declaration carrying default + named/star — every part resolves,
		// polices usage, and diagnoses independently (Unity 0.9.1 field-find parity).
		auto CountOf = [](const TArray<FString>& C, const TCHAR* Code)
		{
			int32 N = 0;
			for (const FString& D : C)
			{
				if (D == Code)
				{
					++N;
				}
			}
			return N;
		};
		{ // default + named, both parts used → clean
			const TArray<FString> C = Codes(TEXT("import Home, { Tint } from \"./Deck\"\ncomponent T {\n\tauto X = ")
												TEXT("Tint;\n\treturn ( <Home /> );\n}\n"),
											Rel, R3);
			HasNot(C, TEXT("UETKX2301"));
			HasNot(C, TEXT("UETKX2302"));
			HasNot(C, TEXT("UETKX2304"));
			HasNot(C, TEXT("UETKX2305"));
			HasNot(C, TEXT("UETKX2326"));
		}
		{ // only the default used → exactly ONE 2304, on the named part
			const TArray<FString> C = Codes(
				TEXT("import Home, { Tint } from \"./Deck\"\ncomponent T {\n\treturn ( <Home /> );\n}\n"), Rel, R3);
			TestEqual(TEXT("combined: unused NAMED part alone warns"), CountOf(C, TEXT("UETKX2304")), 1);
		}
		{ // only the named part used → exactly ONE 2304, on the default
			const TArray<FString> C = Codes(TEXT("import Home, { Tint } from \"./Deck\"\ncomponent T {\n\tauto X = ")
												TEXT("Tint;\n\treturn ( <Spacer /> );\n}\n"),
											Rel, R3);
			TestEqual(TEXT("combined: unused DEFAULT part alone warns"), CountOf(C, TEXT("UETKX2304")), 1);
		}
		{ // default + star, both used → clean; the star's members validate
			const TArray<FString> C = Codes(TEXT("import Home, * as D from \"./Deck\"\ncomponent T {\n\tauto X = ")
												TEXT("D::Tint;\n\treturn ( <Home /> );\n}\n"),
											Rel, R3);
			HasNot(C, TEXT("UETKX2302"));
			HasNot(C, TEXT("UETKX2304"));
			HasNot(C, TEXT("UETKX2326"));
		}
		Has(Codes(TEXT("import Home, * as D from \"./Deck\"\ncomponent T {\n\tauto X = D::Ghost;\n\treturn ( <Home /> ")
					  TEXT(");\n}\n"),
				  Rel, R3),
			TEXT("UETKX2302")); // combined star member still validated
		{ // combined against a default-less target: the DEFAULT part 2326s, the NAMED part stays valid
			const TArray<FString> C = Codes(TEXT("import P, { Cool } from \"./Palette\"\ncomponent T {\n\tauto X = ")
												TEXT("Cool;\n\treturn ( <P /> );\n}\n"),
											Rel, R3);
			Has(C, TEXT("UETKX2326"));
			HasNot(C, TEXT("UETKX2302"));
		}
		// duplicate binding across the parts (`import a, { b as a }`) is a 2325 collision
		Has(Codes(TEXT("import a, { b as a } from \"./Deck\"\ncomponent T {\n\treturn ( <Spacer /> );\n}\n"), Rel, R3),
			TEXT("UETKX2325"));

		// Same-name MEMBER default (Unity 0.9.1 CS0121 repro shape): `import FmtD, { Hue }` where
		// FmtD IS the target's default-export name. This dialect lowers imports as a RENAME PLANE —
		// no consumer-side declaration exists to collide with, and a same-name default adds no
		// rename at all: the .inl carries the bare reference and NO declaration of the binding.
		{
			const FString Src = TEXT("import FmtD, { Hue } from \"./Deck2\"\nexport FRuiNode T() {\n")
				TEXT("\tauto X = Hue;\n\tauto S = FmtD(1);\n\treturn ( <Spacer /> );\n}\n");
			const FUetkxCompileOutput Out2 = FUetkxCodegen::CompileSource(Src, TEXT("T"), Rel, &R3);
			TArray<FString> C;
			for (const FUetkxDiag& D : Out2.Diags)
			{
				C.Add(D.Code);
			}
			HasNot(C, TEXT("UETKX2302"));
			HasNot(C, TEXT("UETKX2304"));
			HasNot(C, TEXT("UETKX2305"));
			HasNot(C, TEXT("UETKX2326"));
			TestTrue(TEXT("same-name default emits the bare reference"), Out2.Inl.Contains(TEXT("FmtD(1)")));
			TestFalse(TEXT("no consumer-side declaration of the default binding (single lowering)"),
					  Out2.Inl.Contains(TEXT("FString FmtD")));
		}
		// The default-exported name stays NAME-IMPORT-private (ES parity): `import { FmtD }` is 2301.
		Has(Codes(TEXT("import { FmtD } from \"./Deck2\"\ncomponent T {\n\tauto S = FmtD(1);\n\treturn ( <Spacer /> ")
					  TEXT(");\n}\n"),
				  Rel, R3),
			TEXT("UETKX2301"));
		// Unused PLAIN default binding fires — and pins that a binding never self-counts: the
		// reference walk scans declaration bodies only, never the preamble's import lines.
		Has(Codes(TEXT("import Home from \"./Screen\"\ncomponent T {\n\treturn ( <Spacer /> );\n}\n"), Rel, R3),
			TEXT("UETKX2304"));

		// UETKX2304 is ERROR-tier (owner decision, family f130cbbb): an unused binding fails the
		// build like the other import diagnostics, and the finding spans the WHOLE binding token —
		// a renamed entry anchors its LOCAL alias (what the author must delete), not the target.
		{
			const FString Src2304 =
				TEXT("import { Chip as Badge } from \"./Chip\"\ncomponent T {\n\treturn ( <Spacer /> );\n}\n");
			const FUetkxCompileOutput Out4 = FUetkxCodegen::CompileSource(Src2304, TEXT("T"), Rel, &R3);
			const FUetkxDiag* D2304 =
				Out4.Diags.FindByPredicate([](const FUetkxDiag& D) { return D.Code == TEXT("UETKX2304"); });
			if (TestNotNull(TEXT("unused renamed import diagnosed"), D2304))
			{
				TestEqual(TEXT("2304 is error-tier"), D2304->Severity, 0);
				TestEqual(TEXT("2304 anchors the LOCAL alias token"), D2304->Offset, Src2304.Find(TEXT("Badge")));
				TestEqual(TEXT("2304 spans the whole alias token"), D2304->Length, 5);
			}
			TestFalse(TEXT("an unused import fails the compile (error tier)"), Out4.bOk);
		}
		// Error-tier safety: the "used" scan OVER-approximates — param DEFAULTS are real uses
		// that live outside the scanned bodies (they emit into the generated code).
		{ // a value used ONLY in a component param default is used
			const TArray<FString> C =
				Codes(TEXT("import { Tint } from \"./Deck\"\nexport FRuiNode T(FLinearColor C2 = Tint) {\n")
						  TEXT("\treturn ( <Spacer /> );\n}\n"),
					  Rel, R3);
			HasNot(C, TEXT("UETKX2304"));
		}
		{ // a value used ONLY in a util's param default is used
			const TArray<FString> C =
				Codes(TEXT("import { Tint } from \"./Deck\"\nexport FString F2(FLinearColor C2 = Tint) {\n")
						  TEXT("\treturn FString();\n}\nexport FRuiNode T() {\n\treturn ( <Spacer /> );\n}\n"),
					  Rel, R3);
			HasNot(C, TEXT("UETKX2304"));
		}

		// Value/util strict usage: an exported value/util referenced WITHOUT an import is 2305.
		Has(Codes(TEXT("component T {\n\tauto X = Cool;\n\treturn ( <Spacer /> );\n}\n"), Rel, R3), TEXT("UETKX2305"));
		Has(Codes(TEXT("component T {\n\tauto X = FmtP(1);\n\treturn ( <Spacer /> );\n}\n"), Rel, R3),
			TEXT("UETKX2305"));
		{
			const TArray<FString> C = Codes(TEXT("import { Cool, FmtP } from \"./Palette\"\ncomponent T {\n\tauto X = ")
												TEXT("Cool;\n\tauto Y = FmtP(1);\n\treturn ( <Spacer /> );\n}\n"),
											Rel, R3);
			HasNot(C, TEXT("UETKX2304"));
			HasNot(C, TEXT("UETKX2305"));
		}

		// TD-034 #2 (N4): a body LOCAL shadowing an exported name is not an external reference —
		// no 2305 (and no codemod import). A param shadow behaves the same; the un-shadowed
		// spelling (a reference BEFORE the local declaration) still diagnoses.
		{
			const TArray<FString> C =
				Codes(TEXT("export FRuiNode T() {\n\tFLinearColor Cool = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);\n")
						  TEXT("\tauto X = Cool;\n\treturn ( <Spacer /> );\n}\n"),
					  Rel, R3);
			HasNot(C, TEXT("UETKX2305"));
		}
		{
			const TArray<FString> C =
				Codes(TEXT("export FRuiNode T(FLinearColor Cool) {\n\tauto X = Cool;\n\treturn ( <Spacer /> );\n}\n"),
					  Rel, R3);
			HasNot(C, TEXT("UETKX2305"));
		}
		{ // the reference sits BEFORE the shadowing declaration → still an external ref → 2305
			const TArray<FString> C = Codes(TEXT("export FRuiNode T() {\n\tauto X = Cool;\n")
												TEXT("\tFLinearColor Cool = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);\n")
													TEXT("\treturn ( <Spacer /> );\n}\n"),
											Rel, R3);
			Has(C, TEXT("UETKX2305"));
		}
		{ // a RANGE-FOR variable named like an exported value is a local — no 2305 (audit)
			const TArray<FString> C =
				Codes(TEXT("export FRuiNode T() {\n\tTArray<FLinearColor> Tints;\n")
						  TEXT("\tfor (const FLinearColor& Cool : Tints) {\n\t\tauto X = Cool;\n\t}\n")
							  TEXT("\treturn ( <Spacer /> );\n}\n"),
					  Rel, R3);
			HasNot(C, TEXT("UETKX2305"));
		}
		{ // a LAMBDA parameter named like an exported value is a local — no 2305 (audit)
			const TArray<FString> C =
				Codes(TEXT("export FRuiNode T() {\n")
						  TEXT("\tauto Fn = [](const FLinearColor& Cool) { auto X = Cool; return X; };\n")
							  TEXT("\treturn ( <Spacer /> );\n}\n"),
					  Rel, R3);
			HasNot(C, TEXT("UETKX2305"));
		}
		{ // an attr NAME never masks a real missing-import (audit: the junk-decl bug) — `Value`
		  // is exported, appears as a Slider prop in an early window, and the TAIL's bare use
		  // must still 2305
			Write(Root / TEXT("Screens/Props.uetkx"),
				  TEXT("export FLinearColor Value = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);\n"));
			const FUetkxFsResolver R4(Root, {Root}, /*bFixtureMode*/ true);
			const TArray<FString> C = Codes(TEXT("export FRuiNode T(bool bOn) {\n")
												TEXT("\tif (bOn) {\n\t\treturn ( <Slider Value={ 0.5f } /> );\n\t}\n")
													TEXT("\tauto X = Value;\n\treturn ( <Spacer /> );\n}\n"),
											Rel, R4);
			Has(C, TEXT("UETKX2305"));
		}
		{ // a directive-lead local shadowing an exported value stays local inside the nested
		  // window's attr expression — no 2305 (audit)
			const TArray<FString> C =
				Codes(TEXT("export FRuiNode T() {\n\treturn (\n\t\t<VerticalBox>\n") TEXT("\t\t\t@if (true) {\n")
						  TEXT("\t\t\t\tFLinearColor Cool = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);\n")
							  TEXT("\t\t\t\treturn ( <Border BorderBackgroundColor={ Cool } /> )\n\t\t\t}\n")
								  TEXT("\t\t</VerticalBox>\n\t);\n}\n"),
					  Rel, R3);
			HasNot(C, TEXT("UETKX2305"));
		}

		// N5 (TD-034 #3): markup-window usage joins the reference set — attr expressions,
		// directive headers/bodies. Text children never count (N-08).
		{ // a value used ONLY in an attr expression: its import is USED (no false 2304)
			const TArray<FString> C = Codes(TEXT("import { Cool } from \"./Palette\"\nexport FRuiNode T() {\n")
												TEXT("\treturn ( <Border BorderBackgroundColor={ Cool } /> );\n}\n"),
											Rel, R3);
			HasNot(C, TEXT("UETKX2304"));
			HasNot(C, TEXT("UETKX2305"));
		}
		{ // …and WITHOUT the import, the attr-expr use is 2305 (silent before N5)
			const TArray<FString> C = Codes(TEXT("export FRuiNode T() {\n")
												TEXT("\treturn ( <Border BorderBackgroundColor={ Cool } /> );\n}\n"),
											Rel, R3);
			Has(C, TEXT("UETKX2305"));
		}
		{ // a util called inside an @if directive body → 2305 without the import
			const TArray<FString> C =
				Codes(TEXT("export FRuiNode T() {\n\treturn (\n\t\t<VerticalBox>\n")
						  TEXT("\t\t\t@if (true) {\n\t\t\t\treturn ( <TextBlock Text={ FText::FromString(FmtP(1)) } /> "
							   ")\n\t\t\t}\n") TEXT("\t\t</VerticalBox>\n\t);\n}\n"),
					  Rel, R3);
			Has(C, TEXT("UETKX2305"));
		}
		{ // an @for loop var named like an exported value is a LOCAL — never a false 2305
			const TArray<FString> C =
				Codes(TEXT("export FRuiNode T() {\n\treturn (\n\t\t<VerticalBox>\n")
						  TEXT("\t\t\t@for (int32 Cool = 0; Cool < 3; ++Cool) {\n")
							  TEXT("\t\t\t\treturn ( <TextBlock Text={ FText::AsNumber(Cool) } /> )\n\t\t\t}\n")
								  TEXT("\t\t</VerticalBox>\n\t);\n}\n"),
					  Rel, R3);
			HasNot(C, TEXT("UETKX2305"));
		}
		{ // a setup local is live inside markup islands (the conservative union seed)
			const TArray<FString> C = Codes(
				TEXT("export FRuiNode T() {\n") TEXT("\tFLinearColor Cool = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);\n")
					TEXT("\treturn ( <Border BorderBackgroundColor={ Cool } /> );\n}\n"),
				Rel, R3);
			HasNot(C, TEXT("UETKX2305"));
		}
	}

	// ── 2308: module boundary (real-mode resolver with *.Build.cs module roots) ────────────────
	const FString Root2 = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUI"), TEXT("ResolveTest2"));
	FM.DeleteDirectory(*Root2, false, true);
	FM.MakeDirectory(*(Root2 / TEXT("ModA")), true);
	FM.MakeDirectory(*(Root2 / TEXT("ModB")), true);
	Write(Root2 / TEXT("ModA/ModA.Build.cs"), TEXT("// stub"));
	Write(Root2 / TEXT("ModB/ModB.Build.cs"), TEXT("// stub"));
	Write(Root2 / TEXT("ModB/Chip.uetkx"), TEXT("export component Chip {\n\treturn ( <Spacer /> );\n}\n"));
	const FUetkxFsResolver R2(Root2, {Root2}, /*bFixtureMode*/ false);
	Has(Codes(TEXT("import { Chip } from \"../ModB/Chip\"\ncomponent App {\n\treturn ( <Chip /> );\n}\n"),
			  TEXT("ModA/App.uetkx"), R2),
		TEXT("UETKX2308")); // import crosses a module boundary

	FM.DeleteDirectory(*Root, false, true);
	FM.DeleteDirectory(*Root2, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
