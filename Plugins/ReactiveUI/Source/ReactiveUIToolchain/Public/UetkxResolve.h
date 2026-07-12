// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Import resolution (M4). IUetkxImportResolver abstracts "specifier -> file" + "file -> exports"
// so CompileSource can run resolution + STRICT usage diagnostics (2300-2308) without knowing about
// the filesystem: the driver and the contract-fixture harness each supply a concrete resolver over
// their own tree. FUetkxFsResolver is the filesystem implementation both use (base dir + search
// roots; a fixture mode with `~/` = base dir and no module boundary). FUetkxResolve::Apply is the
// resolution + strict-usage pass CompileSource invokes.

#pragma once

#include "CoreMinimal.h"
#include "UetkxFileScan.h"

/** A target file's declaration as the resolver reports it (kind + whether it is exported). */
struct FUetkxTargetDecl
{
	EUetkxDeclKind Kind = EUetkxDeclKind::Component;
	bool bExported = false;
};

class REACTIVEUITOOLCHAIN_API IUetkxImportResolver
{
public:
	virtual ~IUetkxImportResolver() = default;

	/** Resolve a specifier from an importer (identified by the path CompileSource was given).
	 *  Applies `./` `../` `~/` + implicit `.uetkx`; engine-native forms are forbidden. Returns an
	 *  opaque resolved key ("" = unresolved) that the other methods accept. */
	virtual FString Resolve(const FString& Spec, const FString& ImporterPath) const = 0;

	/** All declarations of a resolved key: name -> (kind, exported). false if unreadable. */
	virtual bool GetDecls(const FString& Key, TMap<FString, FUetkxTargetDecl>& Out) const = 0;

	/** True when Key lives OUTSIDE ImporterPath's module/root — imports are module-scoped (2308). */
	virtual bool CrossesModuleBoundary(const FString& ImporterPath, const FString& Key) const = 0;

	/** FNV-1a export hash of a key (sorted `name|kind|exported` lines) for staleness (M8). */
	virtual uint32 ExportHashOf(const FString& Key) const = 0;

	/** A machine-stable label for a key (project-relative path) for messages + sidecar dep_hashes. */
	virtual FString LabelForKey(const FString& Key) const = 0;

	/** The project-relative LABEL a specifier WOULD resolve to even if that file does NOT exist yet —
	 *  the reverse-staleness key an UNRESOLVED import records, so creating the missing file re-stales the
	 *  importer (M8 / bughunt DRV-2 / IMPORT-1). "" for a forbidden or anchorless specifier. */
	virtual FString WouldBeLabel(const FString& Spec, const FString& ImporterPath) const = 0;

	/** GLOBAL: the key of ANY file that EXPORTS Name (2305 vs 2307). "" if none exports it. */
	virtual FString FindExporter(const FString& Name, EUetkxDeclKind& OutKind) const = 0;

	/** A POSIX import specifier suggestion from ImporterPath to Key (the 2305 fix-it + codemod). */
	virtual FString SuggestSpecifier(const FString& ImporterPath, const FString& Key) const = 0;
};

/** Filesystem-backed resolver (driver + fixture harness + tests). ImporterPath is resolved as
 *  BaseDir/ImporterPath; `~/` anchors at FUetkxConfig::RootAnchorFor (or BaseDir in fixture mode);
 *  the FindExporter index + module-boundary universe are the .uetkx files under SearchRoots.
 *  Export tables + hashes are cached by (mtime, src_hash). */
class REACTIVEUITOOLCHAIN_API FUetkxFsResolver : public IUetkxImportResolver
{
public:
	FUetkxFsResolver(const FString& InBaseDir, const TArray<FString>& InSearchRoots, bool bInFixtureMode = false);

	virtual FString Resolve(const FString& Spec, const FString& ImporterPath) const override;
	virtual bool GetDecls(const FString& Key, TMap<FString, FUetkxTargetDecl>& Out) const override;
	virtual bool CrossesModuleBoundary(const FString& ImporterPath, const FString& Key) const override;
	virtual uint32 ExportHashOf(const FString& Key) const override;
	virtual FString LabelForKey(const FString& Key) const override;
	virtual FString WouldBeLabel(const FString& Spec, const FString& ImporterPath) const override;
	virtual FString FindExporter(const FString& Name, EUetkxDeclKind& OutKind) const override;
	virtual FString SuggestSpecifier(const FString& ImporterPath, const FString& Key) const override;

private:
	FString ImporterAbs(const FString& ImporterPath) const;
	/** The absolute .uetkx path a specifier maps to, WITHOUT the existence check ("" = forbidden/anchorless). */
	FString ComputeTargetPath(const FString& Spec, const FString& ImporterPath) const;
	const FUetkxPreambleScan* CachedScan(const FString& Key) const; // (mtime, hash)-cached; null if unreadable
	void EnsureIndex() const;

	FString BaseDir;
	TArray<FString> SearchRoots;
	bool bFixtureMode = false;

	struct FCacheEntry
	{
		FDateTime Mtime;
		uint32 Hash = 0;
		FUetkxPreambleScan Scan;
	};
	mutable TMap<FString, FCacheEntry> Cache;		   // abs key -> parsed
	mutable TMap<FString, EUetkxDeclKind> ExportIndex; // exported name -> kind (first exporter wins)
	mutable TMap<FString, FString> ExporterOf;		   // exported name -> key
	mutable bool bIndexBuilt = false;
};

class REACTIVEUITOOLCHAIN_API FUetkxResolve
{
public:
	/** FNV-1a export hash of a preamble scan: over its declarations' sorted `name|kind|exported`
	 *  lines. The importer records this per resolved dep (dep_hashes) and re-stales when it changes
	 *  (M8) — the ONE formula both the resolver and CompileSource use so a dep's stored hash always
	 *  matches what an importer recorded. */
	static uint32 ExportHash(const FUetkxPreambleScan& Scan);

	/** Import resolution + strict usage diagnostics for one compiled file. Uses/UseAts = the
	 *  component tags the markup references (from the emitter, name -> first offset). Appends
	 *  2300-2308 to Diags and fills DepHashes (dep label -> its export hash) for the M8 graph. */
	static void Apply(const FUetkxFileScanResult& Scan, const TMap<FString, int32>& UseAts, const FString& ImporterPath,
					  const IUetkxImportResolver& Resolver, TArray<FUetkxDiag>& Diags,
					  TMap<FString, uint32>& DepHashes);

	/** The imports the codemod (M10) should ADD to a file: every referenced name (component tags,
	 *  bare Use<Upper>( calls, module quals) that is exported elsewhere but not yet imported or
	 *  same-file, grouped by suggested POSIX specifier with names sorted. Cross-module edges are
	 *  reported separately (never auto-written — they surface a real ownership decision, 2308).
	 *  Tags = external component tags (from a resolver-free compile's Uses). */
	static void MissingImports(const FUetkxFileScanResult& Scan, const TSet<FString>& Tags, const FString& ImporterPath,
							   const IUetkxImportResolver& Resolver, TMap<FString, TArray<FString>>& OutBySpecifier,
							   TArray<FString>& OutCrossModule);
};
