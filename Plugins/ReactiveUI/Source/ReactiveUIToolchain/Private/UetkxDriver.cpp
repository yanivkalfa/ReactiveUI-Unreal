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
#include "UetkxConfig.h"
#include "UetkxLexer.h"
#include "UetkxResolve.h"
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
							 const FString& InlName, const TArray<FString>& Uses, bool bSupportFile, uint32 ExportHash,
							 const TMap<FString, uint32>& DepHashes)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("v"), 3);
		Root->SetNumberField(TEXT("src_hash"), static_cast<double>(SrcHash));
		// v3 staleness graph (M8): this file's export shape hash + the export hash of every resolved
		// dependency at compile time. An importer re-stales when a dep's export_hash moves (A5d).
		Root->SetNumberField(TEXT("export_hash"), static_cast<double>(ExportHash));
		TSharedRef<FJsonObject> Deps = MakeShared<FJsonObject>();
		for (const TPair<FString, uint32>& Dep : DepHashes)
		{
			Deps->SetNumberField(Dep.Key, static_cast<double>(Dep.Value));
		}
		Root->SetObjectField(TEXT("dep_hashes"), Deps);
		// Optional (schema-v2-compatible) ordering fields: which components this file's markup
		// REFERENCES + whether it is a hook/module support file. BuildAggregators reads these
		// to order #includes (direct component calls need callee-before-caller in the TU).
		Root->SetStringField(TEXT("kind"), bSupportFile ? TEXT("support") : TEXT("component"));
		TArray<TSharedPtr<FJsonValue>> UsesArray;
		for (const FString& Use : Uses)
		{
			UsesArray.Add(MakeShared<FJsonValueString>(Use));
		}
		Root->SetArrayField(TEXT("uses"), UsesArray);
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

	uint32 ReadSidecarExportHash(const FString& SidecarPath)
	{
		FString Json;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Json, *SidecarPath) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			return 0;
		}
		double H = 0.0;
		Root->TryGetNumberField(TEXT("export_hash"), H);
		return static_cast<uint32>(H);
	}

	/** True when any dependency this file recorded (dep_hashes) now has a DIFFERENT export_hash in
	 *  its own sidecar — the reverse-edge staleness + verdict-poisoning signal (M8/A5d). Labels are
	 *  project-relative paths; a missing dep sidecar reads as export_hash 0. */
	bool DepsChanged(const FString& SidecarPath)
	{
		FString Json;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Json, *SidecarPath) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* Deps = nullptr;
		if (!Root->TryGetObjectField(TEXT("dep_hashes"), Deps))
		{
			return false; // v2 sidecar (pre-M8) — no dep graph recorded
		}
		// `const auto&` + `*Key`: FJsonObject::Values' key type changed in UE 5.8 (FString →
		// UE::FSharedString); dereference-to-TCHAR* is the surface both types share.
		for (const auto& Dep : (*Deps)->Values)
		{
			const FString DepUetkx = FPaths::Combine(FPaths::ProjectDir(), FString(*Dep.Key));
			const uint32 Current = ReadSidecarExportHash(FUetkxDriver::SidecarPathFor(DepUetkx));
			if (Current != static_cast<uint32>(Dep.Value->AsNumber()))
			{
				return true;
			}
		}
		return false;
	}

	/** The dependency labels a sidecar recorded (dep_hashes keys) — the resolved imports of a file,
	 *  the reverse-edge source for the HMR blast radius (M9). */
	TArray<FString> ReadSidecarDepKeys(const FString& SidecarPath)
	{
		TArray<FString> Out;
		FString Json;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Json, *SidecarPath) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			return Out;
		}
		const TSharedPtr<FJsonObject>* Deps = nullptr;
		if (Root->TryGetObjectField(TEXT("dep_hashes"), Deps))
		{
			// Not GetKeys(): the map's key type is engine-version-dependent (UE 5.8: FSharedString).
			for (const auto& Pair : (*Deps)->Values)
			{
				Out.Add(FString(*Pair.Key));
			}
		}
		return Out;
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
			for (const auto& Pair : (*Refs)->Values) // key type is engine-version-dependent
			{
				Out.Add(FString(*Pair.Key));
			}
		}
		return Out;
	}

	bool IsContractFixturePath(const FString& Path)
	{
		return Path.Replace(TEXT("\\"), TEXT("/")).Contains(TEXT("/ContractFixtures/"));
	}

	/** A .uetkx source path relative to the project dir, forward-slashed — the stable, machine-
	 *  independent form emitted into `#line` directives (M7) and stored in the sidecar. */
	FString ProjectRelPathFor(const FString& UetkxPath)
	{
		FString Rel = UetkxPath;
		FPaths::MakePathRelativeTo(Rel, *FPaths::ProjectDir());
		Rel.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Rel;
	}

	/** Detect a VALUE-import cycle (imported HOOKS/MODULES, which load eagerly — a cycle among them is a
	 *  real init deadlock, TDZ parity; component cycles are legal via the two-phase decl pass). Returns
	 *  the cycle's filename chain, or an empty array when there is none. Shared by CompileAllRoots and
	 *  CheckDrift so `-check` enforces the SAME UETKX2306 invariant the full sweep does (bughunt CHECK-1). */
	TArray<FString> DetectValueImportCycleNames(const TArray<FString>& All, const FUetkxFsResolver& Resolver)
	{
		auto NormFull = [](FString P)
		{
			P = FPaths::ConvertRelativePathToFull(P);
			FPaths::NormalizeFilename(P);
			return P;
		};
		TMap<FString, FString> NormToOrig;
		TMap<FString, TArray<FString>> ValueEdges; // normalized path -> normalized paths it value-imports
		for (const FString& Path : All)
		{
			const FString Norm = NormFull(Path);
			NormToOrig.Add(Norm, Path);
			TArray<FString> Edges;
			FString Source;
			if (FFileHelper::LoadFileToString(Source, *Path))
			{
				const FString ProjRel = ProjectRelPathFor(Path);
				for (const FUetkxImportDecl& Imp : FUetkxFileScan::ScanPreamble(Source).Imports)
				{
					// Host includes (INCLUDE_RETIREMENT_PLAN.md §B) resolve to no file -- skip explicitly
					if (Imp.bHostInclude)
					{
						continue;
					}
					const FString Target = Resolver.Resolve(Imp.Specifier, ProjRel);
					if (Target.IsEmpty())
					{
						continue;
					}
					TMap<FString, FUetkxTargetDecl> Decls;
					if (!Resolver.GetDecls(Target, Decls))
					{
						continue;
					}
					const FString NormTarget = NormFull(Target);
					if (NormTarget == Norm)
					{
						continue; // a self-import is not a value cycle (bughunt IMPORT-4) — never a self-edge
					}
					for (const FString& Name : Imp.Names)
					{
						const FUetkxTargetDecl* T = Decls.Find(Name);
						if (T && (T->Kind == EUetkxDeclKind::Hook || T->Kind == EUetkxDeclKind::Module))
						{
							Edges.AddUnique(NormTarget);
							break;
						}
					}
				}
			}
			ValueEdges.Add(Norm, MoveTemp(Edges));
		}
		TSet<FString> Visited, OnStack;
		TArray<FString> Chain;
		TFunction<bool(const FString&)> Dfs = [&](const FString& Node) -> bool
		{
			Visited.Add(Node);
			OnStack.Add(Node);
			Chain.Push(Node);
			for (const FString& Next : ValueEdges.FindRef(Node))
			{
				if (OnStack.Contains(Next))
				{
					Chain.Push(Next); // close the cycle
					return true;
				}
				if (!Visited.Contains(Next) && Dfs(Next))
				{
					return true;
				}
			}
			OnStack.Remove(Node);
			Chain.Pop();
			return false;
		};
		for (const FString& Path : All)
		{
			const FString Norm = NormFull(Path);
			if (!Visited.Contains(Norm))
			{
				Chain.Reset();
				if (Dfs(Norm))
				{
					const int32 Start = Chain.IndexOfByKey(Chain.Last());
					TArray<FString> Names;
					for (int32 i = FMath::Max(0, Start); i < Chain.Num(); ++i)
					{
						Names.Add(FPaths::GetCleanFilename(NormToOrig.FindRef(Chain[i])));
					}
					return Names;
				}
			}
		}
		return {};
	}

	// NOTE (M6): the aggregator no longer orders by the sidecar `uses`/`kind` cache — ordering is now
	// source-truth (fresh preamble import edges, A2). The sidecar's `uses`/`kind` fields remain in
	// the payload as a verifier hint but are read by no code path here (ReadSidecarOrdering removed).
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
	// Verdict-poisoning fix (A5d): a standing error verdict is INVALID once a dependency's export
	// shape moved — e.g. A errored importing {X} from ./B before B exported X; B now exports X, so
	// A must recompile even though A's own source is byte-identical.
	auto MatchesErrorVerdict = [&]()
	{
		uint32 SidecarHash = 0;
		bool bHadError = false;
		if (!ReadSidecarVerdict(SidecarPathFor(UetkxPath), SidecarHash, bHadError) || !bHadError)
		{
			return false;
		}
		if (DepsChanged(SidecarPathFor(UetkxPath)))
		{
			return false;
		}
		FString Source;
		return FFileHelper::LoadFileToString(Source, *UetkxPath) && SrcHash(Source) == SidecarHash;
	};
	// Reverse-edge staleness (M8): a dependency's export_hash moved → recompile, regardless of mtime.
	if (DepsChanged(SidecarPathFor(UetkxPath)))
	{
		return true;
	}
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

FUetkxFileResult FUetkxDriver::CompileFile(const FString& UetkxPath, bool bForce, const IUetkxImportResolver* Resolver)
{
	FUetkxFileResult Out;
	Out.UetkxPath = UetkxPath;
	Out.InlPath = InlPathFor(UetkxPath);
	if (!bForce && !IsStale(UetkxPath))
	{
		Out.bSkipped = true;
		// A skip over a STANDING ERROR verdict (source unchanged since the last failed compile) is NOT a
		// success — mark it not-OK so the sweep/-check exit code reflects the still-broken tree instead of
		// silently passing (bughunt DRV-3). A clean up-to-date file skips as OK.
		uint32 VerdictHash = 0;
		bool bHadError = false;
		const bool bStandingError = ReadSidecarVerdict(SidecarPathFor(UetkxPath), VerdictHash, bHadError) && bHadError;
		Out.bOk = !bStandingError;
		Out.ComponentNames = ReadSidecarRefs(SidecarPathFor(UetkxPath));
		// Populate the EXPORTED-name set even on a skip (cheap preamble scan) so the cross-file
		// UETKX2106 duplicate-export ledger still counts a SKIPPED incumbent's exports — otherwise a
		// new file exporting the same name collides silently and only surfaces on a full recompile
		// (bughunt DRV-1). Skips are unchanged, so a fresh scan matches the committed output.
		FString SkipSource;
		if (FFileHelper::LoadFileToString(SkipSource, *UetkxPath))
		{
			for (const FUetkxPreambleDecl& D : FUetkxFileScan::ScanPreamble(SkipSource).Decls)
			{
				if (D.bExported)
				{
					Out.ExportedNames.Add(D.Name);
				}
			}
		}
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
	FUetkxCompileOutput Compiled =
		FUetkxCodegen::CompileSource(Source, Basename, ProjectRelPathFor(UetkxPath), Resolver);
	Out.Diags = Compiled.Diags;
	Out.ComponentNames = Compiled.ComponentNames;
	Out.ExportedNames = Compiled.ExportedNames;
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
				   SerializeSidecar(Hash, Out.Diags, Compiled.ComponentNames, FPaths::GetCleanFilename(Out.InlPath),
									Compiled.Uses, Compiled.bSupportFile, Compiled.ExportHash, Compiled.DepHashes));
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
		// The nearest *.Build.cs ancestor (dir name == module name, UBT convention). Shared with
		// the import resolver's `~/` module-root default via FUetkxConfig::ModuleRootFor (M1).
		const FString ModuleDir = FUetkxConfig::ModuleRootFor(Path);
		if (!ModuleDir.IsEmpty())
		{
			ByModuleDir.FindOrAdd(ModuleDir).Add(Path);
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
		// D-32(a) context-aware banner (bughunt CG-1): seller copyright in THIS repo, neutral in a
		// customer project — the same seller-repo signal CompileSource uses for the per-file .inl.
		Contents += FUetkxCodegen::IsSellerRepo()
						? TEXT("// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.\n\n")
						: TEXT("// Generated by ReactiveUI — this generated aggregator belongs to your project.\n\n");
		Contents += TEXT("#include \"CoreMinimal.h\"\n");
		Contents += TEXT("#include \"RuiContext.h\"\n");
		Contents += TEXT("#include \"RuiCoreElements.h\"\n");
		Contents += TEXT("#include \"RuiSignal.h\"\n");
		Contents += TEXT("#include \"RuiSlateElements.h\"\n");
		Contents += TEXT("#include \"RuiStyle.h\"\n");
		Contents += TEXT("#include \"RuiRouter.h\"\n"); // ReactiveUICore/Public — hard dep, always available
		// INCLUDE_RETIREMENT_PLAN.md §A: the rest are optional-module or engine headers, guarded
		// so a consuming module that depends only on ReactiveUICore+ReactiveUISlate (no UMG/
		// CommonUI/MVVM interop, no CoreUObject) still compiles. A header added here must ALSO be
		// added to virtualDoc.ts's PRELUDE and the docs' auto-included table (§D).
		Contents +=
			TEXT("#if __has_include(\"RuiAssetBrush.h\")\n#include \"RuiAssetBrush.h\"\n#endif\n"); // ReactiveUIUMG
		Contents += TEXT("#if __has_include(\"RuiFieldHooks.h\")\n#include \"RuiFieldHooks.h\"\n#endif\n");
		Contents += TEXT("#if __has_include(\"RuiUmgElement.h\")\n#include \"RuiUmgElement.h\"\n#endif\n");
		Contents += TEXT("#if __has_include(\"RuiSignalViewModel.h\")\n#include \"RuiSignalViewModel.h\"\n#endif\n");
		Contents += TEXT("#if __has_include(\"RuiHostWidget.h\")\n#include \"RuiHostWidget.h\"\n#endif\n");
		Contents += TEXT("#if __has_include(\"RuiWorldSubsystem.h\")\n#include \"RuiWorldSubsystem.h\"\n#endif\n");
		Contents += TEXT(
			"#if __has_include(\"RuiActivation.h\")\n#include \"RuiActivation.h\"\n#endif\n"); // ReactiveUICommonUI
		Contents +=
			TEXT("#if __has_include(\"RuiActivatableScreen.h\")\n#include \"RuiActivatableScreen.h\"\n#endif\n");
		Contents += TEXT(
			"#if __has_include(\"RuiMvvmViewModel.h\")\n#include \"RuiMvvmViewModel.h\"\n#endif\n"); // ReactiveUIMVVMBridge
		Contents += TEXT("#if __has_include(\"UObject/StrongObjectPtr.h\")\n#include "
						 "\"UObject/StrongObjectPtr.h\"\n#endif\n"); // CoreUObject
		Contents += TEXT("#if __has_include(\"Engine/World.h\")\n#include \"Engine/World.h\"\n#endif\n\n"); // Engine
		// TWO-PHASE include (M6): every .inl is `#include`d once in the DECL phase (complete props
		// structs + defaulted wrapper decls + hook fwd-decls + module bodies) and once in the BODY
		// phase. Because the decl pass forward-declares EVERY wrapper before any body, cross-file
		// component references — INCLUDING CYCLES — all resolve; the old UETKX2107 error retires.
		//
		// Order is SOURCE-TRUTH (A2): each file's fresh PREAMBLE-scan imports, resolved to the files
		// it depends on (not the per-machine sidecar `uses` cache). Topo-sorted (Kahn) so a
		// dependency's declarations precede its importer — the only hard constraint is a module body
		// preceding a struct whose member defaults read its constants — alphabetical tie-break, and
		// the cycle remainder appended alphabetically (now legal under the two-phase decl pass).
		auto NormFull = [](FString P)
		{
			P = FPaths::ConvertRelativePathToFull(P);
			FPaths::NormalizeFilename(P);
			return P;
		};
		TArray<FString> Sorted = Pair.Value;
		Sorted.Sort();
		const FUetkxFsResolver Resolver(FPaths::ProjectDir(), {}, /*bFixtureMode*/ false);
		TMap<FString, FString> NormToPath;
		for (const FString& Uetkx : Sorted)
		{
			NormToPath.Add(NormFull(Uetkx), Uetkx);
		}
		TMap<FString, TArray<FString>> DepsByPath; // path -> the group files it imports
		for (const FString& Uetkx : Sorted)
		{
			TArray<FString> Deps;
			FString Source;
			if (FFileHelper::LoadFileToString(Source, *Uetkx))
			{
				const FString ProjRel = ProjectRelPathFor(Uetkx);
				for (const FUetkxImportDecl& Imp : FUetkxFileScan::ScanPreamble(Source).Imports)
				{
					// Host includes (INCLUDE_RETIREMENT_PLAN.md §B) resolve to no file -- skip explicitly
					if (Imp.bHostInclude)
					{
						continue;
					}
					const FString Target = Resolver.Resolve(Imp.Specifier, ProjRel);
					if (const FString* Dep = Target.IsEmpty() ? nullptr : NormToPath.Find(NormFull(Target)))
					{
						Deps.AddUnique(*Dep);
					}
				}
			}
			DepsByPath.Add(Uetkx, MoveTemp(Deps));
		}
		TArray<FString> Ordered;
		TSet<FString> Emitted;
		while (Ordered.Num() < Sorted.Num())
		{
			bool bProgress = false;
			for (const FString& Uetkx : Sorted)
			{
				if (Emitted.Contains(Uetkx))
				{
					continue;
				}
				bool bReady = true;
				for (const FString& Dep : DepsByPath[Uetkx])
				{
					if (Dep != Uetkx && !Emitted.Contains(Dep))
					{
						bReady = false;
						break;
					}
				}
				if (bReady)
				{
					Ordered.Add(Uetkx);
					Emitted.Add(Uetkx);
					bProgress = true;
				}
			}
			if (!bProgress)
			{
				// Cycle remainder. A component-import edge creates a LEGAL cycle (the two-phase decl pass
				// forward-declares every wrapper), so it imposes NO decl-order constraint. But a VALUE
				// import (a module whose constants a struct member defaults from — bughunt CG-4) still
				// must precede its consumer. Value edges are acyclic (else UETKX2306), so order the
				// remainder by THOSE edges (alphabetical tie-break) instead of pure alphabetical, which
				// ignored a module-default whose provider is itself inside the component cycle.
				TArray<FString> Remainder;
				for (const FString& Uetkx : Sorted)
				{
					if (!Emitted.Contains(Uetkx))
					{
						Remainder.Add(Uetkx);
					}
				}
				TMap<FString, TArray<FString>> ValueDeps;
				for (const FString& Uetkx : Remainder)
				{
					TArray<FString> VDeps;
					FString Source;
					if (FFileHelper::LoadFileToString(Source, *Uetkx))
					{
						const FString ProjRel = ProjectRelPathFor(Uetkx);
						for (const FUetkxImportDecl& Imp : FUetkxFileScan::ScanPreamble(Source).Imports)
						{
							// Host includes (INCLUDE_RETIREMENT_PLAN.md §B) resolve to no file — skip explicitly
							if (Imp.bHostInclude)
							{
								continue;
							}
							const FString Target = Resolver.Resolve(Imp.Specifier, ProjRel);
							const FString* Dep = Target.IsEmpty() ? nullptr : NormToPath.Find(NormFull(Target));
							if (Dep == nullptr || *Dep == Uetkx || Emitted.Contains(*Dep))
							{
								continue; // only unemitted remainder deps constrain the remainder order
							}
							TMap<FString, FUetkxTargetDecl> Decls;
							if (!Resolver.GetDecls(Target, Decls))
							{
								continue;
							}
							for (const FString& Name : Imp.Names)
							{
								const FUetkxTargetDecl* T = Decls.Find(Name);
								if (T && (T->Kind == EUetkxDeclKind::Hook || T->Kind == EUetkxDeclKind::Module))
								{
									VDeps.AddUnique(*Dep);
									break;
								}
							}
						}
					}
					ValueDeps.Add(Uetkx, MoveTemp(VDeps));
				}
				for (;;) // Kahn over the (acyclic) value edges; Remainder is alphabetical → stable tie-break
				{
					bool bAny = false;
					for (const FString& Uetkx : Remainder)
					{
						if (Emitted.Contains(Uetkx))
						{
							continue;
						}
						bool bReady = true;
						for (const FString& D : ValueDeps[Uetkx])
						{
							if (!Emitted.Contains(D))
							{
								bReady = false;
								break;
							}
						}
						if (bReady)
						{
							Ordered.Add(Uetkx);
							Emitted.Add(Uetkx);
							bAny = true;
						}
					}
					if (!bAny)
					{
						break;
					}
				}
				for (const FString& Uetkx : Remainder) // any value-cycle leftover (2306 would flag it): alphabetical
				{
					if (!Emitted.Contains(Uetkx))
					{
						Ordered.Add(Uetkx);
						Emitted.Add(Uetkx);
					}
				}
			}
		}
		auto RelOf = [&](const FString& Uetkx)
		{
			FString Rel = InlPathFor(Uetkx);
			FPaths::MakePathRelativeTo(Rel, *(AggregatorDir + TEXT("/")));
			return Rel;
		};
		Contents += TEXT("#define RUI_UETKX_DECL_PHASE\n");
		for (const FString& Uetkx : Ordered)
		{
			Contents += FString::Printf(TEXT("#include \"%s\"\n"), *RelOf(Uetkx));
		}
		Contents += TEXT("#undef RUI_UETKX_DECL_PHASE\n");
		for (const FString& Uetkx : Ordered)
		{
			Contents += FString::Printf(TEXT("#include \"%s\"\n"), *RelOf(Uetkx));
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
	const FUetkxFsResolver Resolver(FPaths::ProjectDir(), Roots, /*bFixtureMode*/ false); // strict resolution
	for (const FString& Path : All)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *Path))
		{
			++Out.Errors;
			Out.Messages.Add(FString::Printf(TEXT("%s: unreadable"), *Path));
			continue;
		}
		const FUetkxCompileOutput Compiled =
			FUetkxCodegen::CompileSource(Source, FPaths::GetBaseFilename(Path), ProjectRelPathFor(Path), &Resolver);
		for (const FString& Name : Compiled.ExportedNames)
		{
			if (const FString* Incumbent = NameToFile.Find(Name))
			{
				++Out.Errors;
				Out.Messages.Add(FString::Printf(TEXT("%s: UETKX2106: exported binding `%s` is already bound by %s"),
												 *Path, *Name, **Incumbent));
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
	// Enforce the SAME cross-file value-import-cycle invariant the full sweep does — otherwise a 2306
	// cycle passes `-check` but fails the real RUICompile (bughunt CHECK-1).
	if (const TArray<FString> Cycle = DetectValueImportCycleNames(All, Resolver); Cycle.Num() > 0)
	{
		++Out.Errors;
		Out.Messages.Add(FString::Printf(TEXT("UETKX2306: value-import cycle: %s (hooks/modules load eagerly — "
											  "break the chain or move to component refs)"),
										 *FString::Join(Cycle, TEXT(" -> "))));
	}
	return Out;
}

TArray<FString> FUetkxDriver::ImportersOf(const FString& ProjRelPath, const TArray<FString>& Roots)
{
	TArray<FString> Out;
	for (const FString& Root : Roots)
	{
		for (const FString& Path : FindAll(Root))
		{
			if (ReadSidecarDepKeys(SidecarPathFor(Path)).Contains(ProjRelPath))
			{
				Out.AddUnique(FPaths::GetBaseFilename(Path));
			}
		}
	}
	Out.Sort();
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
	return CompileAllRoots({RootDir}, bForce);
}

FUetkxSweepResult FUetkxDriver::CompileAllRoots(const TArray<FString>& Roots, bool bForce)
{
	FUetkxSweepResult Out;
	const bool bFingerprintStale = FingerprintMismatch();
	TArray<FString> All;
	for (const FString& Root : Roots)
	{
		All.Append(FindAll(Root));
	}
	Out.Total = All.Num();
	bool bAnyHeld = false;
	// STRICT enforcement (A1): the compiler resolves imports over the sweep roots. Migrated files
	// resolve clean; a broken import fails the file (its .inl is deleted) — strict day one.
	const FUetkxFsResolver Resolver(FPaths::ProjectDir(), Roots, /*bFixtureMode*/ false);
	auto NormPath = [](FString P)
	{
		P = FPaths::ConvertRelativePathToFull(P);
		FPaths::NormalizeFilename(P);
		return P;
	};

	// One compile pass over every file. Each file's result is kept in ByPath: a real compile
	// (non-skip) always overwrites, a skip is kept only when the file was never compiled — so a file
	// compiled in pass 1 and skipped in pass 2 (up-to-date) still reports as COMPILED with its
	// ExportedNames intact for the 2106 ledger.
	TMap<FString, FUetkxFileResult> ByPath;
	auto RunPass = [&](bool bForcePass)
	{
		for (const FString& Path : All)
		{
			FUetkxFileResult R = CompileFile(Path, bForcePass, &Resolver);
			if (!R.bSkipped || !ByPath.Contains(Path))
			{
				ByPath.Add(Path, MoveTemp(R));
			}
		}
	};

	// Reverse-edge staleness FIXPOINT (M8/TD-025): snapshot each file's export_hash, run pass 1, and
	// if any export shape MOVED, an importer that sorted BEFORE its dep saw the stale hash — re-sweep
	// ONCE so its dep-hash IsStale check (now reading the updated dep sidecar) recompiles it. In
	// practice this converges in 2 passes (an importer's own export_hash does not change from merely
	// resolving, so there is no further cascade).
	TMap<FString, uint32> OldExport;
	for (const FString& Path : All)
	{
		OldExport.Add(NormPath(Path), ReadSidecarExportHash(SidecarPathFor(Path)));
	}
	RunPass(bForce || bFingerprintStale);
	bool bExportsMoved = false;
	for (const FString& Path : All)
	{
		if (ReadSidecarExportHash(SidecarPathFor(Path)) != OldExport.FindRef(NormPath(Path)))
		{
			bExportsMoved = true;
			break;
		}
	}
	if (bExportsMoved)
	{
		RunPass(false);
	}

	// Tally + the UETKX2106 exported-name ledger from the CONVERGED per-file state.
	TMap<FString, FString> NameToFile;
	for (const FString& Path : All)
	{
		const FUetkxFileResult& R = ByPath[Path];
		for (const FString& Name : R.ExportedNames)
		{
			if (const FString* Incumbent = NameToFile.Find(Name))
			{
				++Out.Errors;
				UE_LOG(
					LogRuiToolchain, Error,
					TEXT("%s: UETKX2106: exported binding `%s` is already bound by %s (one exported name, one file)"),
					*Path, *Name, **Incumbent);
			}
			else
			{
				NameToFile.Add(Name, Path);
			}
		}
		if (R.bSkipped && R.bOk)
		{
			++Out.Skipped;
		}
		else if (R.bSkipped) // skipped over a STANDING ERROR verdict — the tree is still broken (DRV-3)
		{
			++Out.Errors;
			UE_LOG(LogRuiToolchain, Error,
				   TEXT("%s: standing compile error (source unchanged since the last failed compile) — fix the "
						"source, or run -run=RUICompile -full to re-surface the diagnostics"),
				   *R.UetkxPath);
		}
		else if (R.bOk)
		{
			++Out.Compiled;
		}
		else
		{
			// An env-hold (unreadable source — UETKX2507) is a TRANSIENT retry, not a sweep error (bughunt
			// DRV-4): counting it as an error contradicts the "keep outputs, retry later" design.
			const bool bHeld =
				R.Diags.ContainsByPredicate([](const FUetkxDiag& D) { return D.Code == TEXT("UETKX2507"); });
			if (bHeld)
			{
				bAnyHeld = true;
			}
			else
			{
				++Out.Errors;
			}
			for (const FUetkxDiag& Diag : R.Diags)
			{
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
		Out.Files.Add(R);
	}
	// UETKX2306: a VALUE-import cycle (imported hooks/modules load eagerly — an init deadlock; component
	// cycles are legal via the two-phase decl pass). The DFS is shared with CheckDrift (CHECK-1).
	if (const TArray<FString> Cycle = DetectValueImportCycleNames(All, Resolver); Cycle.Num() > 0)
	{
		++Out.Errors;
		UE_LOG(LogRuiToolchain, Error,
			   TEXT("UETKX2306: value-import cycle: %s (hooks/modules load eagerly — break the chain or move to "
					"component refs)"),
			   *FString::Join(Cycle, TEXT(" -> ")));
	}
	// Orphan sweep: a deleted Foo.uetkx takes its generated siblings with it — a committed
	// .inl with no source would otherwise keep BUILDING through the aggregator's next regen
	// cycle... the aggregator drops the include (built from FindAll), but the stale file
	// itself must not linger for a hand include or a future rename to resurrect.
	for (const FString& Root : Roots)
	{
		TArray<FString> Inls;
		IFileManager::Get().FindFilesRecursive(Inls, *Root, TEXT("*.uetkx.inl"), true, false);
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
