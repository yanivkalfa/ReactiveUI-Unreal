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

int32 URUICompileCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);
	auto HasSwitch = [&Switches](const TCHAR* Name)
	{
		return Switches.ContainsByPredicate([Name](const FString& S)
											{ return S.Equals(Name, ESearchCase::IgnoreCase); });
	};
	const TArray<FString> Roots = DefaultRoots();

	if (HasSwitch(TEXT("check")))
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

	// Capture the fingerprint verdict ONCE — CompileAll refreshes it per root, and the second
	// root must not lose the "codegen version changed" force.
	const bool bForce = HasSwitch(TEXT("full")) || FUetkxDriver::FingerprintMismatch();
	int32 Compiled = 0, Errors = 0, Skipped = 0, Total = 0;
	for (const FString& Root : Roots)
	{
		const FUetkxSweepResult Sweep = FUetkxDriver::CompileAll(Root, bForce);
		Compiled += Sweep.Compiled;
		Errors += Sweep.Errors;
		Skipped += Sweep.Skipped;
		Total += Sweep.Total;
	}
	UE_LOG(LogRUICompile, Display, TEXT("RUICompile: %d file(s) — %d compiled, %d up-to-date, %d error(s)"), Total,
		   Compiled, Skipped, Errors);
	return Errors == 0 ? 0 : 1;
}
