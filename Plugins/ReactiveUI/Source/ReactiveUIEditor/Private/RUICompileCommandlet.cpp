// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RUICompileCommandlet.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UetkxDriver.h"

DEFINE_LOG_CATEGORY_STATIC(LogRUICompile, Log, All);

TArray<FString> URUICompileCommandlet::DefaultRoots()
{
	TArray<FString> Roots;
	for (const FString& Candidate : {FPaths::ProjectDir() / TEXT("Source"), FPaths::ProjectDir() / TEXT("Plugins")})
	{
		if (IFileManager::Get().DirectoryExists(*Candidate))
		{
			Roots.Add(Candidate);
		}
	}
	return Roots;
}

static bool HasSwitch(const TArray<FString>& Switches, const TCHAR* Name)
{
	for (const FString& Switch : Switches)
	{
		if (Switch.Equals(Name, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

int32 URUICompileCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);
	const TArray<FString> Roots = DefaultRoots();

	if (HasSwitch(Switches, TEXT("check")))
	{
		const FUetkxCheckResult Check = FUetkxDriver::CheckDrift(Roots);
		for (const FString& Message : Check.Messages)
		{
			UE_LOG(LogRUICompile, Error, TEXT("%s"), *Message);
		}
		UE_LOG(LogRUICompile, Display, TEXT("RUICompile -check: %d file(s), %d drifted, %d error(s)"), Check.Total,
			   Check.Drift, Check.Errors);
		return Check.Passed() ? 0 : 1;
	}

	// One sweep over all roots under a SINGLE ledger (A5e): the UETKX2106 exported-name table spans
	// Source + Plugins so a name exported in both is a collision.
	const bool bForce = HasSwitch(Switches, TEXT("full")) || FUetkxDriver::FingerprintMismatch();
	const FUetkxSweepResult Sweep = FUetkxDriver::CompileAllRoots(Roots, bForce);
	UE_LOG(LogRUICompile, Display, TEXT("RUICompile: %d file(s) — %d compiled, %d up-to-date, %d error(s)"),
		   Sweep.Total, Sweep.Compiled, Sweep.Skipped, Sweep.Errors);
	return Sweep.Errors == 0 ? 0 : 1;
}
