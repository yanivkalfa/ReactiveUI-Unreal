// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// `-run=RUIContractDump [-check]` — the D-22 full-file contract harness's CLI. Default:
// regenerate every ContractFixtures golden (.inl.expected / .diags.expected). -check: compare
// only, exit non-zero on any drift or missing golden (CI). Thin glue over FUetkxContract.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "RUIContractDumpCommandlet.generated.h"

UCLASS()
class URUIContractDumpCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URUIContractDumpCommandlet()
	{
		IsClient = false;
		IsServer = false;
		IsEditor = true;
		LogToConsole = true;
	}

	virtual int32 Main(const FString& Params) override;
};
