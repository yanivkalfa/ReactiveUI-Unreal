// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxDriver.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UetkxLexer.h"
#include "UetkxToolchainLog.h"

namespace
{
	FString FingerprintPath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUI"), TEXT("compiler.fp"));
	}

	FString FingerprintValue()
	{
		return FString::Printf(TEXT("uetkx-codegen-v%d"), FUetkxDriver::CodegenVersion);
	}

	/** Write only when contents differ. For the SIDECAR (file watchers — the LSP watches
	 *  .diags.json) and the AGGREGATOR (D-19: an unchanged aggregator must not trip UBT into
	 *  rebuilding its TU). The .inl itself is written UNCONDITIONALLY on compile — the bumped
	 *  mtime is what settles staleness (a skipped identical write would leave src newer than
	 *  output forever: a poll busy loop). Returns true when the file changed. */
	bool WriteIfChanged(const FString& Path, const FString& Contents)
	{
		FString Existing;
		if (FFileHelper::LoadFileToString(Existing, *Path) && Existing == Contents)
		{
			return false;
		}
		return FFileHelper::SaveStringToFile(Contents, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	FString SerializeSidecar(uint32 SrcHash, const TArray<FUetkxDiag>& Diags, const TArray<FString>& ComponentNames,
							 const FString& InlName)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("v"), 2);
		Root->SetNumberField(TEXT("src_hash"), static_cast<double>(SrcHash));
		TArray<TSharedPtr<FJsonValue>> DiagArray;
		for (const FUetkxDiag& Diag : Diags)
		{
			TSharedRef<FJsonObject> D = MakeShared<FJsonObject>();
			D->SetStringField(TEXT("code"), Diag.Code);
			D->SetNumberField(TEXT("severity"), Diag.Severity);
			D->SetStringField(TEXT("message"), Diag.Message);
			D->SetNumberField(TEXT("off"), Diag.Offset);
			D->SetNumberField(TEXT("len"), Diag.Length);
			DiagArray.Add(MakeShared<FJsonValueObject>(D));
		}
		Root->SetArrayField(TEXT("diagnostics"), DiagArray);
		TSharedRef<FJsonObject> Refs = MakeShared<FJsonObject>();
		for (const FString& Name : ComponentNames)
		{
			Refs->SetStringField(Name, InlName);
		}
		Root->SetObjectField(TEXT("refs"), Refs);
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}

	/** Read the sidecar's {src_hash, had an error} — the compile-verdict memory. */
	bool ReadSidecarVerdict(const FString& SidecarPath, uint32& OutHash, bool& bOutHadError)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *SidecarPath))
		{
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->HasField(TEXT("src_hash")))
		{
			return false;
		}
		OutHash = static_cast<uint32>(Root->GetNumberField(TEXT("src_hash")));
		bOutHadError = false;
		const TArray<TSharedPtr<FJsonValue>>* Diags = nullptr;
		if (Root->TryGetArrayField(TEXT("diagnostics"), Diags))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Diags)
			{
				const TSharedPtr<FJsonObject>& Obj = Value->AsObject();
				if (Obj.IsValid() && static_cast<int32>(Obj->GetNumberField(TEXT("severity"))) == 0)
				{
					bOutHadError = true;
					break;
				}
			}
		}
		return true;
	}

	/** The component names a sidecar recorded (its refs keys) — how a SKIPPED file still
	 *  contributes to the duplicate-binding check without recompiling. */
	TArray<FString> ReadSidecarRefs(const FString& SidecarPath)
	{
		TArray<FString> Out;
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *SidecarPath))
		{
			return Out;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return Out;
		}
		const TSharedPtr<FJsonObject>* Refs = nullptr;
		if (Root->TryGetObjectField(TEXT("refs"), Refs))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Refs)->Values)
			{
				Out.Add(Pair.Key);
			}
		}
		return Out;
	}

	bool IsContractFixturePath(const FString& Path)
	{
		return Path.Replace(TEXT("\\"), TEXT("/")).Contains(TEXT("/ContractFixtures/"));
	}
} // namespace

uint32 FUetkxDriver::SrcHash(const FString& Source)
{
	const TArray<int32> Cp = FUetkxLexer::ToCodePoints(Source);
	uint32 Hash = 2166136261u;
	for (const int32 C : Cp)
	{
		// hash each code point's 4 bytes LE — deterministic across platforms, LSP-identical
		for (int32 Shift = 0; Shift < 32; Shift += 8)
		{
			Hash = (Hash ^ ((static_cast<uint32>(C) >> Shift) & 0xFFu)) * 16777619u;
		}
	}
	return Hash;
}

bool FUetkxDriver::IsStale(const FString& UetkxPath)
{
	IFileManager& FM = IFileManager::Get();
	const FDateTime SrcTime = FM.GetTimeStamp(*UetkxPath);
	if (SrcTime == FDateTime::MinValue())
	{
		return false; // source unreadable — nothing to do (env hold: keep outputs, retry later)
	}
	// "This exact content already compiled and reported broken" — the busy-loop guard.
	// It only excuses a MISSING output: an on-disk .inl older than its source is always
	// stale (a failed recompile deletes it; stale generated code must never build).
	auto MatchesErrorVerdict = [&]()
	{
		uint32 SidecarHash = 0;
		bool bHadError = false;
		if (!ReadSidecarVerdict(SidecarPathFor(UetkxPath), SidecarHash, bHadError) || !bHadError)
		{
			return false;
		}
		FString Source;
		return FFileHelper::LoadFileToString(Source, *UetkxPath) && SrcHash(Source) == SidecarHash;
	};
	const FDateTime InlTime = FM.GetTimeStamp(*InlPathFor(UetkxPath));
	if (InlTime == FDateTime::MinValue())
	{
		return !MatchesErrorVerdict();
	}
	if (SrcTime < InlTime)
	{
		return false; // the fast path — no file reads
	}
	if (SrcTime > InlTime)
	{
		return true;
	}
	// Whole-second mtime tie (coarse filesystems): break by content. Fresh only when the
	// sidecar proves this exact content compiled CLEANLY; anything unprovable recompiles
	// once, which rewrites .inl + sidecar and settles the tie.
	uint32 SidecarHash = 0;
	bool bHadError = false;
	FString Source;
	if (!ReadSidecarVerdict(SidecarPathFor(UetkxPath), SidecarHash, bHadError) || bHadError ||
		!FFileHelper::LoadFileToString(Source, *UetkxPath))
	{
		return true;
	}
	return SrcHash(Source) != SidecarHash;
}

FUetkxFileResult FUetkxDriver::CompileFile(const FString& UetkxPath, bool bForce)
{
	FUetkxFileResult Out;
	Out.UetkxPath = UetkxPath;
	Out.InlPath = InlPathFor(UetkxPath);
	if (!bForce && !IsStale(UetkxPath))
	{
		Out.bOk = true;
		Out.bSkipped = true;
		Out.ComponentNames = ReadSidecarRefs(SidecarPathFor(UetkxPath));
		return Out;
	}
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *UetkxPath))
	{
		FUetkxDiag D;
		D.Code = TEXT("UETKX2507");
		D.Severity = 1;
		D.Message = TEXT("source unreadable (editor save race?) — outputs kept, will retry");
		Out.Diags.Add(MoveTemp(D));
		return Out; // env hold: never delete outputs, never record a verdict, for a failed READ
	}
	const FString Basename = FPaths::GetBaseFilename(UetkxPath);
	const uint32 Hash = SrcHash(Source);
	FUetkxCompileOutput Compiled = FUetkxCodegen::CompileSource(Source, Basename);
	Out.Diags = Compiled.Diags;
	Out.ComponentNames = Compiled.ComponentNames;
	if (Compiled.bOk)
	{
		// Unconditional write: the fresh mtime IS the staleness ledger (see WriteIfChanged).
		if (FFileHelper::SaveStringToFile(Compiled.Inl, *Out.InlPath,
										  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Out.bOk = true;
		}
		else
		{
			FUetkxDiag D;
			D.Code = TEXT("UETKX2508");
			D.Severity = 0;
			D.Message = FString::Printf(TEXT("could not write %s"), *Out.InlPath);
			Out.Diags.Add(MoveTemp(D));
		}
	}
	else
	{
		// A failed compile DELETES the stale sibling so old generated code can never build.
		IFileManager::Get().Delete(*Out.InlPath, /*RequireExists*/ false, /*EvenReadOnly*/ true,
								   /*Quiet*/ true);
	}
	WriteIfChanged(SidecarPathFor(UetkxPath),
				   SerializeSidecar(Hash, Out.Diags, Compiled.ComponentNames, FPaths::GetCleanFilename(Out.InlPath)));
	return Out;
}

TArray<FString> FUetkxDriver::FindAll(const FString& RootDir)
{
	TArray<FString> Out;
	IFileManager::Get().FindFilesRecursive(Out, *RootDir, TEXT("*.uetkx"), /*Files*/ true, /*Dirs*/ false);
	Out.RemoveAll(IsContractFixturePath);
	Out.Sort();
	return Out;
}

bool FUetkxDriver::HasStale(const FString& RootDir)
{
	if (FingerprintMismatch())
	{
		return !FindAll(RootDir).IsEmpty();
	}
	for (const FString& Path : FindAll(RootDir))
	{
		if (IsStale(Path))
		{
			return true;
		}
	}
	return false;
}

TMap<FString, FString> FUetkxDriver::BuildAggregators(const TArray<FString>& UetkxPaths)
{
	// Group by MODULE directory: the nearest ancestor holding a *.Build.cs (dir name == module
	// name by UBT convention). The aggregator is the one stable TU the build system sees — its
	// CONTENTS track the .uetkx set so the compiled-file SET never changes (D-19.1).
	TMap<FString, TArray<FString>> ByModuleDir;
	for (const FString& Path : UetkxPaths)
	{
		FString Dir = FPaths::GetPath(Path);
		while (!Dir.IsEmpty())
		{
			TArray<FString> BuildFiles;
			IFileManager::Get().FindFiles(BuildFiles, *(Dir / TEXT("*.Build.cs")), true, false);
			if (!BuildFiles.IsEmpty())
			{
				ByModuleDir.FindOrAdd(Dir).Add(Path);
				break;
			}
			const FString Parent = FPaths::GetPath(Dir);
			if (Parent == Dir)
			{
				break;
			}
			Dir = Parent;
		}
	}
	TMap<FString, FString> Out;
	for (const TPair<FString, TArray<FString>>& Pair : ByModuleDir)
	{
		const FString ModuleName = FPaths::GetBaseFilename(Pair.Key);
		const FString AggregatorDir = Pair.Key / TEXT("Private");
		const FString AggregatorPath = AggregatorDir / FString::Printf(TEXT("%s.Uetkx.gen.cpp"), *ModuleName);
		FString Contents;
		Contents += TEXT("// AUTO-GENERATED aggregator - DO NOT EDIT. Regenerated by RUICompile; the\n");
		Contents += TEXT("// compiled source-file SET stays constant, only these contents change (D-19).\n");
		Contents += TEXT("// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.\n\n");
		Contents += TEXT("#include \"CoreMinimal.h\"\n");
		Contents += TEXT("#include \"RuiContext.h\"\n");
		Contents += TEXT("#include \"RuiCoreElements.h\"\n");
		Contents += TEXT("#include \"RuiSignal.h\"\n");
		Contents += TEXT("#include \"RuiSlateElements.h\"\n");
		Contents += TEXT("#include \"RuiStyle.h\"\n\n");
		TArray<FString> Sorted = Pair.Value;
		Sorted.Sort(); // deterministic; cross-component refs must respect alphabetical order (v1)
		for (const FString& Uetkx : Sorted)
		{
			FString Rel = InlPathFor(Uetkx);
			FPaths::MakePathRelativeTo(Rel, *(AggregatorDir + TEXT("/")));
			Contents += FString::Printf(TEXT("#include \"%s\"\n"), *Rel);
		}
		Out.Add(AggregatorPath, MoveTemp(Contents));
	}
	return Out;
}

bool FUetkxDriver::RegenerateAggregators(const TArray<FString>& UetkxPaths)
{
	bool bChanged = false;
	for (const TPair<FString, FString>& Pair : BuildAggregators(UetkxPaths))
	{
		bChanged |= WriteIfChanged(Pair.Key, Pair.Value);
	}
	return bChanged;
}

FUetkxCheckResult FUetkxDriver::CheckDrift(const TArray<FString>& Roots)
{
	auto Normalized = [](FString S)
	{
		S.ReplaceInline(TEXT("\r\n"), TEXT("\n")); // git autocrlf checkouts must not read as drift
		return S;
	};
	FUetkxCheckResult Out;
	TArray<FString> All;
	for (const FString& Root : Roots)
	{
		All.Append(FindAll(Root));
	}
	Out.Total = All.Num();
	TMap<FString, FString> NameToFile; // duplicate-binding ledger (UETKX2106)
	for (const FString& Path : All)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *Path))
		{
			++Out.Errors;
			Out.Messages.Add(FString::Printf(TEXT("%s: unreadable"), *Path));
			continue;
		}
		const FUetkxCompileOutput Compiled = FUetkxCodegen::CompileSource(Source, FPaths::GetBaseFilename(Path));
		for (const FString& Name : Compiled.ComponentNames)
		{
			if (const FString* Incumbent = NameToFile.Find(Name))
			{
				++Out.Errors;
				Out.Messages.Add(FString::Printf(TEXT("%s: UETKX2106: component `%s` is already bound by %s"), *Path,
												 *Name, **Incumbent));
			}
			else
			{
				NameToFile.Add(Name, Path);
			}
		}
		if (!Compiled.bOk)
		{
			++Out.Errors;
			for (const FUetkxDiag& Diag : Compiled.Diags)
			{
				if (Diag.Severity == 0)
				{
					Out.Messages.Add(FString::Printf(TEXT("%s: %s: %s"), *Path, *Diag.Code, *Diag.Message));
				}
			}
			continue;
		}
		FString OnDisk;
		if (!FFileHelper::LoadFileToString(OnDisk, *InlPathFor(Path)))
		{
			++Out.Drift;
			Out.Messages.Add(FString::Printf(TEXT("%s: generated output missing — run -run=RUICompile"), *Path));
		}
		else if (Normalized(OnDisk) != Normalized(Compiled.Inl))
		{
			++Out.Drift;
			Out.Messages.Add(FString::Printf(TEXT("%s: stale — .inl differs from a fresh compile"), *Path));
		}
	}
	for (const TPair<FString, FString>& Pair : BuildAggregators(All))
	{
		FString OnDisk;
		if (!FFileHelper::LoadFileToString(OnDisk, *Pair.Key))
		{
			++Out.Drift;
			Out.Messages.Add(FString::Printf(TEXT("%s: aggregator missing — run -run=RUICompile"), *Pair.Key));
		}
		else if (Normalized(OnDisk) != Normalized(Pair.Value))
		{
			++Out.Drift;
			Out.Messages.Add(FString::Printf(TEXT("%s: aggregator stale — include set changed"), *Pair.Key));
		}
	}
	return Out;
}

bool FUetkxDriver::FingerprintMismatch()
{
	FString Existing;
	if (!FFileHelper::LoadFileToString(Existing, *FingerprintPath()))
	{
		return true;
	}
	return Existing.TrimStartAndEnd() != FingerprintValue();
}

void FUetkxDriver::RefreshFingerprint()
{
	FFileHelper::SaveStringToFile(FingerprintValue(), *FingerprintPath(),
								  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FUetkxSweepResult FUetkxDriver::CompileAll(const FString& RootDir, bool bForce)
{
	FUetkxSweepResult Out;
	const bool bFingerprintStale = FingerprintMismatch();
	const TArray<FString> All = FindAll(RootDir);
	Out.Total = All.Num();
	bool bAnyHeld = false;
	TMap<FString, FString> NameToFile; // duplicate-binding ledger (UETKX2106)
	for (const FString& Path : All)
	{
		FUetkxFileResult R = CompileFile(Path, bForce || bFingerprintStale);
		for (const FString& Name : R.ComponentNames)
		{
			if (const FString* Incumbent = NameToFile.Find(Name))
			{
				++Out.Errors;
				UE_LOG(LogRuiToolchain, Error,
					   TEXT("%s: UETKX2106: component `%s` is already bound by %s (one name, one file)"), *Path, *Name,
					   **Incumbent);
			}
			else
			{
				NameToFile.Add(Name, Path);
			}
		}
		if (R.bSkipped)
		{
			++Out.Skipped;
		}
		else if (R.bOk)
		{
			++Out.Compiled;
		}
		else
		{
			++Out.Errors;
			for (const FUetkxDiag& Diag : R.Diags)
			{
				if (Diag.Code == TEXT("UETKX2507"))
				{
					bAnyHeld = true;
				}
				if (Diag.Severity == 0)
				{
					UE_LOG(LogRuiToolchain, Error, TEXT("%s: %s: %s"), *R.UetkxPath, *Diag.Code, *Diag.Message);
				}
				else
				{
					UE_LOG(LogRuiToolchain, Warning, TEXT("%s: %s: %s"), *R.UetkxPath, *Diag.Code, *Diag.Message);
				}
			}
		}
		Out.Files.Add(MoveTemp(R));
	}
	// Orphan sweep: a deleted Foo.uetkx takes its generated siblings with it — a committed
	// .inl with no source would otherwise keep BUILDING through the aggregator's next regen
	// cycle... the aggregator drops the include (built from FindAll), but the stale file
	// itself must not linger for a hand include or a future rename to resurrect.
	{
		TArray<FString> Inls;
		IFileManager::Get().FindFilesRecursive(Inls, *RootDir, TEXT("*.uetkx.inl"), true, false);
		for (const FString& Inl : Inls)
		{
			const FString SourcePath = Inl.LeftChop(4); // strip ".inl"
			if (!IsContractFixturePath(Inl) && !IFileManager::Get().FileExists(*SourcePath))
			{
				IFileManager::Get().Delete(*Inl, false, true, true);
				IFileManager::Get().Delete(*SidecarPathFor(SourcePath), false, true, true);
				UE_LOG(LogRuiToolchain, Display, TEXT("orphan swept: %s (source gone)"), *Inl);
			}
		}
	}
	Out.bAggregatorsChanged = RegenerateAggregators(All);
	if (!bAnyHeld)
	{
		RefreshFingerprint(); // only a hold-free sweep certifies this codegen version
	}
	return Out;
}
