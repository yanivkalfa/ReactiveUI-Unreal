// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxContract.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UetkxCodegen.h"

namespace
{
	FString Normalized(FString S)
	{
		S.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		return S;
	}
} // namespace

FString FUetkxContract::DefaultFixtureDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/RuiHostTests/ContractFixtures"));
}

FString FUetkxContract::SerializeDiags(const FUetkxCompileOutput& Output)
{
	FString Out;
	for (const FUetkxDiag& Diag : Output.Diags)
	{
		Out += FString::Printf(TEXT("%s %d @%d+%d: %s\n"), *Diag.Code, Diag.Severity, Diag.Offset, Diag.Length,
							   *Diag.Message);
	}
	return Out;
}

FUetkxContractResult FUetkxContract::Run(const FString& FixtureDir, bool bWrite)
{
	FUetkxContractResult Out;
	TArray<FString> Fixtures;
	IFileManager::Get().FindFilesRecursive(Fixtures, *FixtureDir, TEXT("*.uetkx"), true, false);
	Fixtures.Sort();
	Out.Total = Fixtures.Num();
	for (const FString& Fixture : Fixtures)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *Fixture))
		{
			++Out.Mismatched;
			Out.Messages.Add(FString::Printf(TEXT("%s: unreadable"), *Fixture));
			continue;
		}
		const FUetkxCompileOutput Compiled = FUetkxCodegen::CompileSource(Source, FPaths::GetBaseFilename(Fixture));
		const FString GoldenPath = Compiled.bOk ? Fixture + TEXT(".inl.expected") : Fixture + TEXT(".diags.expected");
		const FString StalePath = Compiled.bOk ? Fixture + TEXT(".diags.expected") : Fixture + TEXT(".inl.expected");
		const FString Actual = Compiled.bOk ? Compiled.Inl : SerializeDiags(Compiled);
		if (bWrite)
		{
			IFileManager::Get().Delete(*StalePath, false, true, true); // verdict flipped: old golden goes
			FFileHelper::SaveStringToFile(Actual, *GoldenPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			++Out.Written;
			continue;
		}
		FString Golden;
		if (!FFileHelper::LoadFileToString(Golden, *GoldenPath))
		{
			++Out.Mismatched;
			Out.Messages.Add(
				FString::Printf(TEXT("%s: golden missing (%s) — run -run=RUIContractDump"), *Fixture, *GoldenPath));
			continue;
		}
		if (Normalized(Golden) != Normalized(Actual))
		{
			++Out.Mismatched;
			Out.Messages.Add(
				FString::Printf(TEXT("%s: output differs from its golden — codegen contract changed"), *Fixture));
		}
	}
	return Out;
}
