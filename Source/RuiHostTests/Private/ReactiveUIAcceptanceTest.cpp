// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Acceptance — the coherence gate: one suite that walks the WHOLE shipped chain
// end to end and fails loudly if any seam between subsystems drifts. Each numbered section
// mirrors an item on plans/archive/OWNER_ACCEPTANCE_CHECKLIST.md — this suite is the headless half;
// the checklist is the interactive half (PIE, HMR-in-editor, IDE extensions).

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiDemoScreens.h"
#include "RuiNode.h"
#include "RuiReconciler.h"
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

	// A stateful component standing in for a live HMR-refreshed root: its UseState count is heap-resident,
	// so an HmrRefreshAll (what a Live Coding patch triggers) must re-render it WITHOUT resetting the count.
	static TRuiSetter<int32> GRefreshCounterSetter;
	static FRuiNodeArray RefreshCounter(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		auto [Count, SetCount] = Ctx.UseState<int32>(0);
		GRefreshCounterSetter = SetCount;
		return {RUI::TextBlock(FString::Printf(TEXT("count=%d"), Count))};
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
		// 40 = 15 screens (incl. RouterDemo, DoomGame, DoomGameScreen) + 4 interop screens (MvvmDemo,
		// CommonUiDemo, UmgHostDemo, InteropShowcase — the Epic-interop pillars authored in .uetkx)
		// + 12 subcomponents (DemoContextPanel, SignalPanel, LabCard, ActivationProbe, ShowcaseProbe,
		// UmgHostInner, RouterHome, RouterUser, DoomFace, DoomMainMenu, DoomHUD, DoomMinimap — one
		// component per file, components/ convention) + 4 support files (ContextDemo.style,
		// SimpleCounter.hooks, AcceptanceLab.style, AcceptanceLab.hooks) + 2 CycleProof (CycleA/CycleB)
		// + 2 ChildrenProof (ChildHost/ChildParent) + 1 GrammarProof (MultiReturnProof, wave G).
		TestEqual(TEXT("1. all 40 swept .uetkx files (gallery + interop + cycle + children + grammar proof)"),
				  Drift.Total, 40);
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

	// 5. The HMR v2 refresh seam re-renders every live root IN PLACE with state preserved (headless half
	//    of the save→patch→screen chain). The interpreter is deleted (D-HMR-8): in the editor a Live
	//    Coding patch swaps the compiled code and fires the patch-complete delegate, which drives exactly
	//    this — FRuiReconciler::ForEachLive → HmrRefreshAll → FlushSync. Because hook cells are heap-
	//    resident, a code patch cannot reset them; this asserts that invariant without the compiler.
	{
		RUI::RegisterNamedFactory(FName(TEXT("RefreshCounterAcc")),
								  []() { return RUI::FC(&AcceptanceTest::RefreshCounter); });
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("RefreshCounterAcc"))));
		Root->FlushSync();
		TestTrue(TEXT("5. counter mounts at 0"),
				 AcceptanceTest::ContainsText(Root->GetWidget().Get(), TEXT("count=0")));

		AcceptanceTest::GRefreshCounterSetter(7); // user interacted before the edit
		Root->FlushSync();
		TestTrue(TEXT("5. state advanced"), AcceptanceTest::ContainsText(Root->GetWidget().Get(), TEXT("count=7")));

		// Simulate the Live Coding patch-complete refresh (no code actually changed here).
		int32 RefreshedRoots = 0;
		FRuiReconciler::ForEachLive(
			[&RefreshedRoots](FRuiReconciler& Reconciler)
			{
				Reconciler.HmrRefreshAll();
				Reconciler.FlushSync();
				++RefreshedRoots;
			});
		TestTrue(TEXT("5. at least one live root refreshed"), RefreshedRoots >= 1);
		TestTrue(TEXT("5. hook state survived the HMR refresh (not reset to 0)"),
				 AcceptanceTest::ContainsText(Root->GetWidget().Get(), TEXT("count=7")));
		Root->Unmount();
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
