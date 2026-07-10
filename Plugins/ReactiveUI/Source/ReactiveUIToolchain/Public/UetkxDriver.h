// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxDriver — the compiler's file layer (D-18/D-19): CompileFile/CompileAll/IsStale/
// HasStale, the ALWAYS-written diagnostics sidecar (`Foo.uetkx.diags.json`, schema v2, FNV-1a
// src_hash over CODE POINTS — identical constants to the LSP), the error-verdict rule (a
// failed compile deletes the stale .inl and the sidecar's same-hash error entry means
// "already compiled and reported broken" -> NOT stale; prevents busy loops), the mtime-tie
// content-hash break, the compiler fingerprint (Saved/ReactiveUI/compiler.fp — a codegen
// version bump treats every output as stale), and the stable per-module aggregator
// `<Module>.Uetkx.gen.cpp` whose CONTENTS change while the compiled file SET stays constant
// (the UBT makefile-caching pitfall, D-19.1).

#pragma once

#include "CoreMinimal.h"
#include "UetkxCodegen.h"

struct REACTIVEUITOOLCHAIN_API FUetkxFileResult
{
	bool bOk = false;
	bool bSkipped = false; // up-to-date or standing error verdict
	FString UetkxPath;
	FString InlPath;
	TArray<FUetkxDiag> Diags;
};

struct REACTIVEUITOOLCHAIN_API FUetkxSweepResult
{
	int32 Compiled = 0;
	int32 Errors = 0;
	int32 Skipped = 0;
	int32 Total = 0;
	bool bAggregatorsChanged = false;
	TArray<FUetkxFileResult> Files;
};

class REACTIVEUITOOLCHAIN_API FUetkxDriver
{
public:
	/** Bump when generated-code SHAPE changes — the fingerprint that re-stales everything. */
	static constexpr int32 CodegenVersion = 1;

	static FString InlPathFor(const FString& UetkxPath) { return UetkxPath + TEXT(".inl"); }
	static FString SidecarPathFor(const FString& UetkxPath) { return UetkxPath + TEXT(".diags.json"); }

	/** FNV-1a 2166136261/16777619 over Unicode code points, masked to 32 bits (LSP-identical). */
	static uint32 SrcHash(const FString& Source);

	/** Compile one file: writes the .inl on success (deletes it on failure — stale output
	 *  must never run) and the sidecar ALWAYS (unless payload-identical). bForce skips the
	 *  staleness check. */
	static FUetkxFileResult CompileFile(const FString& UetkxPath, bool bForce = false);

	/** True when UetkxPath needs a compile (missing/older .inl; error verdicts honored). */
	static bool IsStale(const FString& UetkxPath);

	/** Find every *.uetkx under RootDir (recursive). */
	static TArray<FString> FindAll(const FString& RootDir);

	/** Cheap poll: does anything under RootDir need work? */
	static bool HasStale(const FString& RootDir);

	/** Sweep-compile all (stale unless bForce) + regenerate aggregators + fingerprint. */
	static FUetkxSweepResult CompileAll(const FString& RootDir, bool bForce = false);

	/** Regenerate `<ModuleDirName>.Uetkx.gen.cpp` beside each module's .uetkx set. Returns
	 *  true when any aggregator changed on disk. */
	static bool RegenerateAggregators(const TArray<FString>& UetkxPaths);

	/** Fingerprint file check: mismatch/absent -> everything stale (returns true). Call
	 *  RefreshFingerprint after a clean sweep. */
	static bool FingerprintMismatch();
	static void RefreshFingerprint();
};
