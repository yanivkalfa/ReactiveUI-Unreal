// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Acceptance — the coherence gate: one suite that walks the WHOLE shipped chain
// end to end and fails loudly if any seam between subsystems drifts. Each numbered section
// mirrors an item on plans/OWNER_ACCEPTANCE_CHECKLIST.md — this suite is the headless half;
// the checklist is the interactive half (PIE, HMR-in-editor, IDE extensions).

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuiCoreElements.h"
#include "RuiDemoScreens.h"
#include "RuiHmr.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "UetkxCodegen.h"
#include "UetkxContract.h"
#include "UetkxDriver.h"
#include "UetkxFormatter.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AcceptanceTest
{
	static bool ContainsText(SWidget& RootWidget, const FString& Needle)
	{
		if (RootWidget.GetType() == FName(TEXT("STextBlock")) &&
			static_cast<STextBlock&>(RootWidget).GetText().ToString().Contains(Needle))
		{
			return true;
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (ContainsText(Children->GetChildAt(i).Get(), Needle))
			{
				return true;
			}
		}
		return false;
	}
} // namespace AcceptanceTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiAcceptanceTest, "ReactiveUI.Acceptance",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiAcceptanceTest::RunTest(const FString&)
{
	// 1. The committed generated code matches its sources (the CI drift gate, content-based).
	{
		TArray<FString> Roots;
		for (const FString& Candidate : {FPaths::ProjectDir() / TEXT("Source"), FPaths::ProjectDir() / TEXT("Plugins")})
		{
			Roots.Add(Candidate);
		}
		const FUetkxCheckResult Drift = FUetkxDriver::CheckDrift(Roots);
		TestEqual(TEXT("1. no .uetkx drift"), Drift.Drift, 0);
		TestEqual(TEXT("1. no .uetkx compile errors"), Drift.Errors, 0);
		// 17 = 11 screens + 2 subcomponents (DemoContextPanel, SignalPanel — one component per file,
		// components/ convention) + 2 support files (ContextDemo.style, SimpleCounter.hooks) + the
		// 2 CycleProof files (CycleA/CycleB — cross-file component-cycle parity, M6).
		TestEqual(TEXT("1. all 17 swept .uetkx files (gallery + cycle proof)"), Drift.Total, 17);
	}

	// 2. The contract goldens hold (codegen shape is what the fixtures pinned).
	{
		const FUetkxContractResult Contract = FUetkxContract::Run(FUetkxContract::DefaultFixtureDir(), false);
		TestTrue(TEXT("2. contract harness passes"), Contract.Passed() && Contract.Total >= 6);
	}

	// 3. Every gallery screen self-registered and mounts through the named-factory seam.
	{
		for (const FName& Name : RuiDemo::GetCompiledScreenNames())
		{
			TestTrue(FString::Printf(TEXT("3. '%s' registered"), *Name.ToString()), RUI::HasNamedFactory(Name));
		}
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("HelloWorld"))));
		Root->FlushSync();
		TestTrue(TEXT("3. compiled screen renders"),
				 AcceptanceTest::ContainsText(Root->GetWidget().Get(), TEXT("Hello, world!")));
		Root->Unmount();
	}

	// 4. The formatter round-trips every committed screen source (idempotent, no fallback).
	{
		TArray<FString> Screens = FUetkxDriver::FindAll(FPaths::ProjectDir() / TEXT("Source"));
		for (const FString& Screen : Screens)
		{
			FString Source;
			FFileHelper::LoadFileToString(Source, *Screen);
			const FUetkxFormatResult First = FUetkxFormatter::Format(Source);
			TestFalse(FString::Printf(TEXT("4. %s formats"), *FPaths::GetCleanFilename(Screen)), First.bFellBack);
			const FUetkxFormatResult Second = FUetkxFormatter::Format(First.Text);
			TestEqual(FString::Printf(TEXT("4. %s idempotent"), *FPaths::GetCleanFilename(Screen)), Second.Text,
					  First.Text);
		}
	}

	// 5. The HMR loop swaps a live definition (the save→screen chain, headless).
	{
		FRuiHmr::ResetSession();
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("HelloWorld"))));
		Root->FlushSync();
		const FRuiHmrStatus Status = FRuiHmr::ApplySource(
			TEXT("component HelloWorld {\n\treturn (\n\t\t<TextBlock Text=\"Hot swapped!\" />\n\t);\n}\n"),
			TEXT("HelloWorld"));
		TestTrue(TEXT("5. swap applied"), Status.Ok() && Status.Reloaded == 1);
		TestTrue(TEXT("5. live UI updated in place"),
				 AcceptanceTest::ContainsText(Root->GetWidget().Get(), TEXT("Hot swapped!")));
		Root->Unmount();
		FRuiHmr::ResetSession(); // back to the compiled definition
	}

	// 6. The schema export (what the IDE tooling consumes) stays coherent with the vocabulary.
	{
		const FString Schema = FUetkxCodegen::ExportSchemaJson();
		TestTrue(TEXT("6. schema exports"), Schema.Contains(TEXT("\"elements\"")) && Schema.Contains(TEXT("Button")));
		// the SHIPPED copy in the LSP must not drift from the live export
		FString Shipped;
		if (FFileHelper::LoadFileToString(
				Shipped, *(FPaths::ProjectDir() / TEXT("ide-extensions/lsp-server/src/uetkx-schema.json"))))
		{
			auto Normalize = [](FString S)
			{
				S.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
				S.TrimStartAndEndInline();
				return S;
			};
			TestEqual(TEXT("6. shipped LSP schema matches the compiler export"), Normalize(Shipped), Normalize(Schema));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
