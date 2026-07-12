// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// `-run=RUIMigrateImports` — the one-shot, IDEMPOTENT, re-runnable codemod (A8d) that migrates a
// .uetkx tree to the strict import/export world: it adds `export` to EVERY existing declaration
// (export-everything, A3 — by-name C++/UMG consumers keep resolving) and inserts the `import { … }
// from "…"` lines every file needs, derived from a FRESH reference scan (component tags + user
// hook calls + module quals against the export tables — NOT sidecar edges, A3). Cross-module edges
// are reported (UETKX2308) and NOT written — they surface a real ownership decision. Finishes by
// compiling the whole tree WITH resolution and requiring ZERO diagnostics (the strict-day-one
// merge gate). Re-running is a no-op (already-exported decls + already-imported names are skipped).

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "RUIMigrateImportsCommandlet.generated.h"

UCLASS()
class URUIMigrateImportsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URUIMigrateImportsCommandlet()
	{
		IsClient = false;
		IsServer = false;
		IsEditor = true;
		LogToConsole = true;
	}

	virtual int32 Main(const FString& Params) override;
};
