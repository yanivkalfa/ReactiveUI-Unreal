// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Contract — the D-22 full-file harness: every ContractFixtures/*.uetkx compiles
// through the real pipeline and must match its committed golden byte-for-byte (normalized
// line endings). A codegen-shape change without regenerated goldens fails here AND in the
// RUIContractDump -check CI lane. Also pins that the RUICompile sweep NEVER touches fixtures.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UetkxContract.h"
#include "UetkxDriver.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxContractTest, "ReactiveUI.Contract",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxContractTest::RunTest(const FString&)
{
	const FUetkxContractResult Result = FUetkxContract::Run(FUetkxContract::DefaultFixtureDir(), /*bWrite*/ false);
	TestTrue(TEXT("harness has fixtures"), Result.Total >= 4);
	for (const FString& Message : Result.Messages)
	{
		AddError(Message);
	}
	TestEqual(TEXT("no contract drift"), Result.Mismatched, 0);

	// The sweep must never compile fixtures (they'd gain stray .inl siblings + aggregators).
	const TArray<FString> Swept = FUetkxDriver::FindAll(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")));
	TestFalse(TEXT("fixtures excluded from the sweep"),
			  Swept.ContainsByPredicate([](const FString& P) { return P.Contains(TEXT("ContractFixtures")); }));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
