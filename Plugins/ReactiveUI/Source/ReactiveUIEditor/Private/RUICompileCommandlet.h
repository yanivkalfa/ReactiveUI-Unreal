// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// `-run=RUICompile [-full] [-check]` — the CLI/CI entry to the .uetkx compiler.
//   (default) incremental sweep of Source/ + Plugins/ (stale files only)
//   -full     recompile everything
//   -check    the CI drift gate: NO writes; exits non-zero when any committed .inl or
//             aggregator differs from a fresh in-memory compile, or any source has errors.
// Thin glue: all semantics live in FUetkxDriver (tested by ReactiveUI.Uetkx.Driver).

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "RUICompileCommandlet.generated.h"

UCLASS()
class URUICompileCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URUICompileCommandlet()
	{
		IsClient = false;
		IsServer = false;
		IsEditor = true;
		LogToConsole = true;
	}

	virtual int32 Main(const FString& Params) override;

	/** The sweep roots: <Project>/Source and <Project>/Plugins. */
	static TArray<FString> DefaultRoots();
};
