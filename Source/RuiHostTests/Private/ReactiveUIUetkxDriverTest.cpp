// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Driver — the compiler's FILE layer, end-to-end on real scratch files under
// Saved/: compile writes .inl + sidecar (schema v3), a failed compile deletes the stale .inl
// and its same-hash error sidecar suppresses re-compiles (the busy-loop guard), content changes
// re-stale, sweeps count + settle (fingerprint), and aggregators regenerate deterministically.

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuiNode.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UetkxDriver.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxDriverTest, "ReactiveUI.Uetkx.Driver",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxDriverTest::RunTest(const FString&)
{
	IFileManager& FM = IFileManager::Get();
	const FString Scratch = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUI"), TEXT("DriverTest"));
	FM.DeleteDirectory(*Scratch, /*RequireExists*/ false, /*Tree*/ true);
	FM.MakeDirectory(*Scratch, /*Tree*/ true);

	const FString GoodBadge = TEXT("export component Badge(Label: FString = TEXT(\"hi\")) {\n"
								   "\tauto [Count, SetCount] = UseState(0);\n"
								   "\treturn (\n"
								   "\t\t<VerticalBox>\n"
								   "\t\t\t<TextBlock Text={ FText::FromString(Label) } />\n"
								   "\t\t</VerticalBox>\n"
								   "\t);\n"
								   "}\n");
	const FString GoodBadgeV2 = GoodBadge.Replace(TEXT("UseState(0)"), TEXT("UseState(42)"));
	const FString BrokenSrc = TEXT("component Broken { return ( <VerticalBox> ); }\n");
	const FString FixedSrc = TEXT("component Broken { return ( <Spacer /> ); }\n");

	// ── SrcHash: the cross-tool contract (LSP recomputes these exact values in Phase 5) ────
	{
		TestEqual(TEXT("fnv over 'ab' code points"), FUetkxDriver::SrcHash(TEXT("ab")), 0x221505E6u);
		FString Emoji; // U+1F3AE as a surrogate pair — must hash as ONE code point (D-18)
		Emoji.AppendChar((TCHAR)0xD83C);
		Emoji.AppendChar((TCHAR)0xDFAE);
		TestEqual(TEXT("fnv collapses surrogate pairs"), FUetkxDriver::SrcHash(Emoji), 0x6F5C2B7Fu);
		TestNotEqual(TEXT("content-sensitive"), FUetkxDriver::SrcHash(GoodBadge), FUetkxDriver::SrcHash(GoodBadgeV2));
	}

	// ── success path: .inl + sidecar, then fresh, then skip ────────────────────────────────
	const FString BadgePath = Scratch / TEXT("FakeModule/Widgets/Badge.uetkx");
	{
		FFileHelper::SaveStringToFile(GoodBadge, *BadgePath);
		const FUetkxFileResult R = FUetkxDriver::CompileFile(BadgePath);
		TestTrue(TEXT("compiles"), R.bOk && !R.bSkipped);
		FString Inl;
		TestTrue(TEXT(".inl written"), FFileHelper::LoadFileToString(Inl, *FUetkxDriver::InlPathFor(BadgePath)));
		TestTrue(TEXT(".inl holds the impl"), Inl.Contains(TEXT("Badge_UetkxImpl")));

		FString SidecarJson;
		TestTrue(TEXT("sidecar written"),
				 FFileHelper::LoadFileToString(SidecarJson, *FUetkxDriver::SidecarPathFor(BadgePath)));
		TSharedPtr<FJsonObject> Sidecar;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SidecarJson), Sidecar);
		if (TestTrue(TEXT("sidecar parses"), Sidecar.IsValid()))
		{
			TestEqual(TEXT("schema v3"), (int32)Sidecar->GetNumberField(TEXT("v")), 3);
			TestTrue(TEXT("v3 records an export_hash"), Sidecar->HasField(TEXT("export_hash")));
			TestEqual(TEXT("src_hash recorded"), (uint32)Sidecar->GetNumberField(TEXT("src_hash")),
					  FUetkxDriver::SrcHash(GoodBadge));
			// ES-modules (2320): `export component` is the legacy wrapper — one deprecation warning
			// is now the CORRECT "clean" outcome (no errors) for this still-legal syntax.
			TestEqual(TEXT("clean compile -> only the 2320 deprecation warning"),
					  Sidecar->GetArrayField(TEXT("diagnostics")).Num(), 1);
			TestEqual(TEXT("refs maps component -> inl"),
					  Sidecar->GetObjectField(TEXT("refs"))->GetStringField(TEXT("Badge")),
					  FString(TEXT("Badge.uetkx.inl")));
		}
		TestFalse(TEXT("fresh after compile"), FUetkxDriver::IsStale(BadgePath));
		TestTrue(TEXT("recompile skips"), FUetkxDriver::CompileFile(BadgePath).bSkipped);
	}

	// ── content change re-stales; recompile picks up the new content ───────────────────────
	{
		FFileHelper::SaveStringToFile(GoodBadgeV2, *BadgePath);
		TestTrue(TEXT("edit -> stale"), FUetkxDriver::IsStale(BadgePath));
		TestTrue(TEXT("recompiles"), !FUetkxDriver::CompileFile(BadgePath).bSkipped);
		FString Inl;
		FFileHelper::LoadFileToString(Inl, *FUetkxDriver::InlPathFor(BadgePath));
		TestTrue(TEXT(".inl updated"), Inl.Contains(TEXT("UseState(42)")));
		TestFalse(TEXT("settles"), FUetkxDriver::IsStale(BadgePath));
	}

	// ── error path: no .inl, error sidecar, and the busy-loop guard ────────────────────────
	const FString BrokenPath = Scratch / TEXT("Loose/Broken.uetkx");
	{
		FFileHelper::SaveStringToFile(BrokenSrc, *BrokenPath);
		const FUetkxFileResult R = FUetkxDriver::CompileFile(BrokenPath);
		TestFalse(TEXT("broken fails"), R.bOk);
		TestTrue(TEXT("carries error diags"),
				 R.Diags.Num() > 0 && R.Diags.ContainsByPredicate([](const FUetkxDiag& D) { return D.Severity == 0; }));
		TestFalse(TEXT("no .inl for broken source"), FM.FileExists(*FUetkxDriver::InlPathFor(BrokenPath)));
		TestFalse(TEXT("error verdict -> NOT stale (busy-loop guard)"), FUetkxDriver::IsStale(BrokenPath));
		TestTrue(TEXT("error verdict -> skip"), FUetkxDriver::CompileFile(BrokenPath).bSkipped);

		FFileHelper::SaveStringToFile(FixedSrc, *BrokenPath);
		TestTrue(TEXT("fix -> stale again"), FUetkxDriver::IsStale(BrokenPath));
		TestTrue(TEXT("fix compiles"), FUetkxDriver::CompileFile(BrokenPath).bOk);
		TestTrue(TEXT("fix wrote .inl"), FM.FileExists(*FUetkxDriver::InlPathFor(BrokenPath)));
	}

	// ── breaking a previously-good file DELETES its .inl (stale output must never build) ───
	{
		FFileHelper::SaveStringToFile(BrokenSrc, *BrokenPath);
		TestFalse(TEXT("re-broken fails"), FUetkxDriver::CompileFile(BrokenPath).bOk);
		TestFalse(TEXT("stale .inl deleted"), FM.FileExists(*FUetkxDriver::InlPathFor(BrokenPath)));
		FFileHelper::SaveStringToFile(FixedSrc, *BrokenPath); // restore for the sweep below
	}

	// ── FindAll: recursive, sorted, .uetkx only ────────────────────────────────────────────
	const FString DeepPath = Scratch / TEXT("FakeModule/Widgets/Nested/Deep.uetkx");
	{
		FFileHelper::SaveStringToFile(TEXT("component Deep { return ( <Spacer /> ); }\n"), *DeepPath);
		const TArray<FString> All = FUetkxDriver::FindAll(Scratch);
		TestEqual(TEXT("finds all three"), All.Num(), 3);
		TestTrue(TEXT("recurses"),
				 All.ContainsByPredicate([](const FString& P) { return P.EndsWith(TEXT("Deep.uetkx")); }));
	}

	// ── aggregator: one stable TU per module dir, deterministic includes ───────────────────
	{
		FFileHelper::SaveStringToFile(FString(), *(Scratch / TEXT("FakeModule/FakeModule.Build.cs")));
		const FString AggPath = Scratch / TEXT("FakeModule/Private/FakeModule.Uetkx.gen.cpp");
		TestTrue(TEXT("first regen writes"), FUetkxDriver::RegenerateAggregators(FUetkxDriver::FindAll(Scratch)));
		FString Agg;
		TestTrue(TEXT("aggregator exists"), FFileHelper::LoadFileToString(Agg, *AggPath));
		TestTrue(TEXT("includes Badge (relative)"), Agg.Contains(TEXT("#include \"../Widgets/Badge.uetkx.inl\"")));
		TestTrue(TEXT("includes Deep (relative)"), Agg.Contains(TEXT("#include \"../Widgets/Nested/Deep.uetkx.inl\"")));
		TestTrue(TEXT("badge before deep (sorted)"),
				 Agg.Find(TEXT("Badge.uetkx.inl")) < Agg.Find(TEXT("Deep.uetkx.inl")));
		TestFalse(TEXT("Broken outside any module -> no include"), Agg.Contains(TEXT("Broken")));
		TestFalse(TEXT("second regen is a no-op"), FUetkxDriver::RegenerateAggregators(FUetkxDriver::FindAll(Scratch)));
	}

	// ── sweep: counts, fingerprint settles, HasStale flips on edit ─────────────────────────
	{
		const FUetkxSweepResult Sweep = FUetkxDriver::CompileAll(Scratch, /*bForce*/ true);
		TestEqual(TEXT("sweep total"), Sweep.Total, 3);
		TestEqual(TEXT("sweep compiled"), Sweep.Compiled, 3);
		TestEqual(TEXT("sweep errors"), Sweep.Errors, 0);
		TestFalse(TEXT("fingerprint refreshed"), FUetkxDriver::FingerprintMismatch());
		TestFalse(TEXT("nothing stale after sweep"), FUetkxDriver::HasStale(Scratch));

		const FUetkxSweepResult Again = FUetkxDriver::CompileAll(Scratch);
		TestEqual(TEXT("second sweep skips all"), Again.Skipped, 3);

		FFileHelper::SaveStringToFile(TEXT("component Deep { return ( <Border /> ); }\n"), *DeepPath);
		TestTrue(TEXT("edit -> HasStale"), FUetkxDriver::HasStale(Scratch));
	}

	// ── CheckDrift: the CI gate is content-based, no mtimes, no writes ─────────────────────
	{
		// Deep.uetkx was just edited without recompiling -> exactly one drifted file.
		FUetkxCheckResult Check = FUetkxDriver::CheckDrift({Scratch});
		TestEqual(TEXT("check total"), Check.Total, 3);
		TestEqual(TEXT("edited-not-recompiled drifts"), Check.Drift, 1);
		TestEqual(TEXT("no errors"), Check.Errors, 0);
		TestFalse(TEXT("gate fails"), Check.Passed());

		FUetkxDriver::CompileFile(DeepPath);
		TestTrue(TEXT("recompile settles the gate"), FUetkxDriver::CheckDrift({Scratch}).Passed());

		// Hand-tampered .inl is NEWER than its source — mtime staleness would call it fresh;
		// only the content compare catches it.
		FString Inl;
		FFileHelper::LoadFileToString(Inl, *FUetkxDriver::InlPathFor(BadgePath));
		FFileHelper::SaveStringToFile(Inl + TEXT("// tampered\n"), *FUetkxDriver::InlPathFor(BadgePath));
		TestFalse(TEXT("mtime says fresh"), FUetkxDriver::IsStale(BadgePath));
		TestEqual(TEXT("content compare catches tampering"), FUetkxDriver::CheckDrift({Scratch}).Drift, 1);
		FUetkxDriver::CompileFile(BadgePath, /*bForce*/ true);

		// Aggregator drift: deleting it must fail the gate.
		const FString AggPath = Scratch / TEXT("FakeModule/Private/FakeModule.Uetkx.gen.cpp");
		FM.Delete(*AggPath);
		TestEqual(TEXT("missing aggregator drifts"), FUetkxDriver::CheckDrift({Scratch}).Drift, 1);
		FUetkxDriver::RegenerateAggregators(FUetkxDriver::FindAll(Scratch));

		// Broken source: the gate reports an ERROR (CI must fail on uncompilable .uetkx).
		FFileHelper::SaveStringToFile(BrokenSrc, *BrokenPath);
		Check = FUetkxDriver::CheckDrift({Scratch});
		TestEqual(TEXT("broken source is an error"), Check.Errors, 1);
		TestFalse(TEXT("gate fails on errors"), Check.Passed());
		TestTrue(TEXT("message names the file"),
				 Check.Messages.ContainsByPredicate([](const FString& M) { return M.Contains(TEXT("Broken.uetkx")); }));
	}

	// ── duplicate binding (UETKX2106) + the orphan sweep ───────────────────────────────────
	{
		// the sweep REPORTS the duplicate via UE_LOG(Error) — expected here, not a failure
		AddExpectedError(TEXT("UETKX2106"), EAutomationExpectedErrorFlags::Contains, 0);
		FFileHelper::SaveStringToFile(FixedSrc, *BrokenPath); // settle the previous block
		const FString DupPath = Scratch / TEXT("Loose/BadgeCopy.uetkx");
		FFileHelper::SaveStringToFile(TEXT("export component Badge { return ( <Spacer /> ); }\n"), *DupPath);
		const FUetkxSweepResult Dup = FUetkxDriver::CompileAll(Scratch, /*bForce*/ true);
		TestTrue(TEXT("duplicate EXPORTED binding is a sweep error"), Dup.Errors >= 1);
		TestTrue(TEXT("the drift gate flags duplicates too"),
				 FUetkxDriver::CheckDrift({Scratch}).Messages.ContainsByPredicate(
					 [](const FString& M) { return M.Contains(TEXT("UETKX2106")); }));

		FM.Delete(*DupPath);
		TestTrue(TEXT("orphan .inl lingers pre-sweep"), FM.FileExists(*FUetkxDriver::InlPathFor(DupPath)));
		FUetkxDriver::CompileAll(Scratch);
		TestFalse(TEXT("orphan .inl swept with its source"), FM.FileExists(*FUetkxDriver::InlPathFor(DupPath)));
		TestFalse(TEXT("orphan sidecar swept"), FM.FileExists(*FUetkxDriver::SidecarPathFor(DupPath)));
	}

	// ── verdict-poisoning fix (A5d/M8): an importer re-stales when a dep gains its export ─────────
	{
		const FString Dir = Scratch / TEXT("Retry");
		const FString APath = Dir / TEXT("A.uetkx");
		const FString BPath = Dir / TEXT("B.uetkx");
		// B does not export X yet; A imports X from ./B and renders <X/> — A errors (2302).
		FFileHelper::SaveStringToFile(TEXT("export component Placeholder { return ( <Spacer /> ); }\n"), *BPath);
		FFileHelper::SaveStringToFile(TEXT("import { X } from \"./B\"\nexport component A { return ( <X /> ); }\n"),
									  *APath);
		AddExpectedError(TEXT("UETKX2302"), EAutomationExpectedErrorFlags::Contains, 0);
		FUetkxDriver::CompileAll(Scratch, /*bForce*/ true);
		TestFalse(TEXT("A failed to resolve X -> no .inl"), FM.FileExists(*FUetkxDriver::InlPathFor(APath)));

		// B gains X. A is NOT touched. The dep_hashes graph un-poisons A's error verdict, and the
		// staleness FIXPOINT recompiles A in a SINGLE sweep even though A sorts before B (pass 1
		// settles B, pass 2 catches A) — no external re-sweep needed.
		FFileHelper::SaveStringToFile(
			TEXT("export component Placeholder { return ( <Spacer /> ); }\nexport component X { return ( <Spacer /> "
				 "); }\n"),
			*BPath);
		FUetkxDriver::CompileAll(Scratch);
		TestTrue(TEXT("A recompiled after B gained X in ONE sweep (verdict un-poisoned, fixpoint)"),
				 FM.FileExists(*FUetkxDriver::InlPathFor(APath)));
		FM.Delete(*APath);
		FM.Delete(*BPath);
		FUetkxDriver::CompileAll(Scratch);
	}

	// ── PRIVATE same-name decls across two files are LEGAL (A5e: the ledger keys exported only) ──
	{
		const FString PrivA = Scratch / TEXT("Loose/PrivA.uetkx");
		const FString PrivB = Scratch / TEXT("Loose/PrivB.uetkx");
		FFileHelper::SaveStringToFile(TEXT("component Helper { return ( <Spacer /> ); }\n"), *PrivA);
		FFileHelper::SaveStringToFile(TEXT("component Helper { return ( <Border /> ); }\n"), *PrivB);
		const FUetkxCheckResult Check = FUetkxDriver::CheckDrift({Scratch});
		TestFalse(TEXT("two PRIVATE same-name decls do not trip 2106"),
				  Check.Messages.ContainsByPredicate([](const FString& M) { return M.Contains(TEXT("UETKX2106")); }));
		FM.Delete(*PrivA);
		FM.Delete(*PrivB);
	}

	// ── UETKX2306: a VALUE-import cycle (module <-> module) errors; component cycles are legal ────
	{
		const FString Dir = Scratch / TEXT("Value");
		const FString MA = Dir / TEXT("MA.uetkx");
		const FString MB = Dir / TEXT("MB.uetkx");
		// MA's module reads MB's constant and vice versa — an eager-load init deadlock.
		FFileHelper::SaveStringToFile(
			TEXT("import { StyleB } from \"./MB\"\nexport module StyleA { inline const int32 X = StyleB::Y; }\n"), *MA);
		FFileHelper::SaveStringToFile(
			TEXT("import { StyleA } from \"./MA\"\nexport module StyleB { inline const int32 Y = StyleA::X; }\n"), *MB);
		AddExpectedError(TEXT("UETKX2306"), EAutomationExpectedErrorFlags::Contains, 0);
		const FUetkxSweepResult Sweep = FUetkxDriver::CompileAll(Scratch, /*bForce*/ true);
		TestTrue(TEXT("module<->module value cycle is a sweep error (2306)"), Sweep.Errors >= 1);
		FM.Delete(*MA);
		FM.Delete(*MB);
		FUetkxDriver::CompileAll(Scratch);
	}

	// ── ES-modules (U-06): a NEW-form VALUE-export cycle (A⇄B via value imports) is 2306 too ─────
	{
		const FString Dir = Scratch / TEXT("Value2");
		const FString VA = Dir / TEXT("VA.uetkx");
		const FString VB = Dir / TEXT("VB.uetkx");
		FFileHelper::SaveStringToFile(TEXT("import { BVal } from \"./VB\"\nexport int32 AVal = FMath::Max(BVal, 1);\n"),
									  *VA);
		FFileHelper::SaveStringToFile(TEXT("import { AVal } from \"./VA\"\nexport int32 BVal = FMath::Max(AVal, 1);\n"),
									  *VB);
		AddExpectedError(TEXT("UETKX2306"), EAutomationExpectedErrorFlags::Contains, 0);
		const FUetkxSweepResult Sweep = FUetkxDriver::CompileAll(Scratch, /*bForce*/ true);
		TestTrue(TEXT("value-export<->value-export cycle is a sweep error (2306)"), Sweep.Errors >= 1);
		FM.Delete(*VA);
		FM.Delete(*VB);
		FUetkxDriver::CompileAll(Scratch);
	}

	// ── ES-modules (M4): export_hash moves on a value edit → importer recompiles in ONE sweep ────
	{
		const FString Dir = Scratch / TEXT("ValueDep");
		const FString PalettePath = Dir / TEXT("Pal.uetkx");
		const FString UserPath = Dir / TEXT("PalUser.uetkx");
		FFileHelper::SaveStringToFile(TEXT("export FLinearColor Main = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);\n"),
									  *PalettePath);
		FFileHelper::SaveStringToFile(TEXT("import { Main } from \"./Pal\"\nexport component PalUser {\n\tauto C = ")
										  TEXT("Main;\n\treturn ( <Spacer /> );\n}\n"),
									  *UserPath);
		FUetkxDriver::CompileAll(Scratch);
		TestTrue(TEXT("value-importing component compiled"), FM.FileExists(*FUetkxDriver::InlPathFor(UserPath)));
		// Renaming the exported value moves Pal's export_hash; the ONE next sweep must recompile
		// the importer (whose old import is now 2302-broken) — the TD-025 fixpoint over value deps.
		FFileHelper::SaveStringToFile(TEXT("export FLinearColor Main2 = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);\n"),
									  *PalettePath);
		AddExpectedError(TEXT("UETKX2302"), EAutomationExpectedErrorFlags::Contains, 0);
		FUetkxDriver::CompileAll(Scratch);
		TestFalse(TEXT("importer re-verdicts on the value rename (stale .inl deleted in the same sweep)"),
				  FM.FileExists(*FUetkxDriver::InlPathFor(UserPath)));
		FM.Delete(*PalettePath);
		FM.Delete(*UserPath);
		FUetkxDriver::CompileAll(Scratch);
	}

	// ── ES-modules (M5): support-file blast radius — ImportersOf answers from the dep graph ──────
	{
		const FString Dir = Scratch / TEXT("Blast");
		const FString SupPath = Dir / TEXT("Theme.uetkx");
		const FString ImpPath = Dir / TEXT("ThemeUser.uetkx");
		FFileHelper::SaveStringToFile(TEXT("export FLinearColor Tone = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);\n"),
									  *SupPath);
		FFileHelper::SaveStringToFile(TEXT("import { Tone } from \"./Theme\"\nexport component ThemeUser {\n\tauto C ")
										  TEXT("= Tone;\n\treturn ( <Spacer /> );\n}\n"),
									  *ImpPath);
		FUetkxDriver::CompileAll(Scratch);
		const TArray<FString> Importers = FUetkxDriver::ImportersOf(FUetkxDriver::ProjectRelPath(SupPath), {Scratch});
		TestTrue(TEXT("ImportersOf names the value-importing file"), Importers.Contains(TEXT("ThemeUser")));
		FM.Delete(*SupPath);
		FM.Delete(*ImpPath);
		FUetkxDriver::CompileAll(Scratch);
	}

	// ── ES-modules (M5/TD-026): private HMR identity is per-FILE — signatures never alias ────────
	{
		// The M3 emission keys two files' private `Row`s as RuiPriv_<File>::Row; the HMR maps key
		// by that FName, so editing one file's private component can never flip the other's
		// signature (pre-M3 both keyed bare `Row` — last-swap-wins).
		const FName IdA(TEXT("RuiPriv_HmrPairA::Row"));
		const FName IdB(TEXT("RuiPriv_HmrPairB::Row"));
		RUI::RegisterHookSignature(IdA, 0x11111111u);
		RUI::RegisterHookSignature(IdB, 0x22222222u);
		RUI::RegisterHookSignature(IdA, 0x33333333u); // "edit" A's file — its signature moves
		TestEqual(TEXT("A's private signature updated"), RUI::FindHookSignature(IdA), 0x33333333u);
		TestEqual(TEXT("B's private signature untouched"), RUI::FindHookSignature(IdB), 0x22222222u);
	}

	// ── ES-modules (M5/G-01): renaming a file renames RuiPriv_<Basename> — privates remount ─────
	{
		const FString PairSrc = TEXT("FRuiNode Row() {\n\treturn ( <Spacer /> );\n}\n")
			TEXT("export FRuiNode RENAMED() {\n\treturn ( <VerticalBox> <Row /> </VerticalBox> );\n}\n");
		const FUetkxCompileOutput AsA =
			FUetkxCodegen::CompileSource(PairSrc.Replace(TEXT("RENAMED"), TEXT("RenameA")), TEXT("RenameA"));
		const FUetkxCompileOutput AsB =
			FUetkxCodegen::CompileSource(PairSrc.Replace(TEXT("RENAMED"), TEXT("RenameB")), TEXT("RenameB"));
		TestTrue(TEXT("rename gives the private a FRESH runtime id (remount semantic)"),
				 AsA.bOk && AsB.bOk && AsA.Inl.Contains(TEXT("RuiPriv_RenameA::Row")) &&
					 AsB.Inl.Contains(TEXT("RuiPriv_RenameB::Row")));
	}

	// ── ES-modules (U-08): the sidecar records the default-export marker ─────────────────────────
	{
		const FString DefPath = Scratch / TEXT("Loose/DefScreen.uetkx");
		FFileHelper::SaveStringToFile(TEXT("export component DefScreen { return ( <Spacer /> ); }\nexport default ")
										  TEXT("DefScreen;\n"),
									  *DefPath);
		TestTrue(TEXT("default-export file compiles"), FUetkxDriver::CompileFile(DefPath, /*bForce*/ true).bOk);
		FString SidecarJson;
		FFileHelper::LoadFileToString(SidecarJson, *FUetkxDriver::SidecarPathFor(DefPath));
		TestTrue(TEXT("sidecar carries the default marker"), SidecarJson.Contains(TEXT("\"default\":\"DefScreen\"")));
		FM.Delete(*DefPath);
		FUetkxDriver::CompileAll(Scratch);
	}

	FM.DeleteDirectory(*Scratch, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
