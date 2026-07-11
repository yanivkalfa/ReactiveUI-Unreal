// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// `-run=RUIExportSchema` — writes the markup vocabulary (elements/attrs/style/slot/hooks) to
// Saved/ReactiveUI/schema.json for the LSP's local markup intelligence (Phase 5). The schema
// content is owned by FUetkxCodegen (the single vocabulary authority); this is CLI glue.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "RUIExportSchemaCommandlet.generated.h"

UCLASS()
class URUIExportSchemaCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URUIExportSchemaCommandlet()
	{
		IsClient = false;
		IsServer = false;
		IsEditor = true;
		LogToConsole = true;
	}

	virtual int32 Main(const FString& Params) override;
};
