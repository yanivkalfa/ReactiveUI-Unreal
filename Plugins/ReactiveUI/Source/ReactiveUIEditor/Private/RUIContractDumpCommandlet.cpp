// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RUIContractDumpCommandlet.h"

#include "UetkxContract.h"

DEFINE_LOG_CATEGORY_STATIC(LogRUIContract, Log, All);

int32 URUIContractDumpCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);
	const bool bCheck =
		Switches.ContainsByPredicate([](const FString& S) { return S.Equals(TEXT("check"), ESearchCase::IgnoreCase); });
	const FUetkxContractResult Result = FUetkxContract::Run(FUetkxContract::DefaultFixtureDir(), /*bWrite*/ !bCheck);
	for (const FString& Message : Result.Messages)
	{
		UE_LOG(LogRUIContract, Error, TEXT("%s"), *Message);
	}
	if (bCheck)
	{
		UE_LOG(LogRUIContract, Display, TEXT("RUIContractDump -check: %d fixture(s), %d mismatched"), Result.Total,
			   Result.Mismatched);
		return Result.Passed() ? 0 : 1;
	}
	UE_LOG(LogRUIContract, Display, TEXT("RUIContractDump: %d golden(s) written for %d fixture(s)"), Result.Written,
		   Result.Total);
	return 0;
}
