// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RUIMigrateImportsCommandlet.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RUICompileCommandlet.h"
#include "UetkxCodegen.h"
#include "UetkxDriver.h"
#include "UetkxFileScan.h"
#include "UetkxLexer.h"
#include "UetkxResolve.h"

DEFINE_LOG_CATEGORY_STATIC(LogRUIMigrate, Log, All);

namespace
{
	FString ProjectRelPathFor(const FString& UetkxPath)
	{
		FString Rel = UetkxPath;
		FPaths::MakePathRelativeTo(Rel, *FPaths::ProjectDir());
		Rel.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Rel;
	}

	/** Insert Text at a CODE-POINT offset (the scan's units) and return the new source. */
	FString InsertCp(const FString& Source, int32 CpOffset, const FString& Text)
	{
		TArray<int32> Cp = FUetkxLexer::ToCodePoints(Source);
		const TArray<int32> Ins = FUetkxLexer::ToCodePoints(Text);
		Cp.Insert(Ins, FMath::Clamp(CpOffset, 0, Cp.Num()));
		return FUetkxLexer::FromCodePoints(Cp, 0, Cp.Num());
	}

	bool WriteSource(const FString& Path, const FString& Body)
	{
		// LF, no BOM (.gitattributes pins LF on .uetkx). Skip the write when unchanged (idempotent).
		FString Existing;
		if (FFileHelper::LoadFileToString(Existing, *Path) && Existing == Body)
		{
			return false;
		}
		return FFileHelper::SaveStringToFile(Body, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
} // namespace

int32 URUIMigrateImportsCommandlet::Main(const FString& Params)
{
	const TArray<FString> Roots = URUICompileCommandlet::DefaultRoots();
	TArray<FString> Files;
	for (const FString& Root : Roots)
	{
		Files.Append(FUetkxDriver::FindAll(Root)); // ContractFixtures auto-excluded (hand-edited)
	}
	Files.Sort();

	// ── Pass 1: export EVERYTHING (A3). By-name C++/UMG consumers keep resolving; privacy is
	// opt-in going forward. Inserting `export ` before each un-exported decl keyword, descending, so
	// earlier inserts don't shift later offsets. Idempotent: already-exported decls are skipped.
	int32 ExportsAdded = 0;
	for (const FString& File : Files)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *File))
		{
			continue;
		}
		const FUetkxPreambleScan Scan = FUetkxFileScan::ScanPreamble(Source);
		TArray<int32> Ats;
		for (const FUetkxPreambleDecl& D : Scan.Decls)
		{
			if (!D.bExported && D.At >= 0)
			{
				Ats.Add(D.At);
			}
		}
		if (Ats.IsEmpty())
		{
			continue;
		}
		Ats.Sort();
		for (int32 idx = Ats.Num() - 1; idx >= 0; --idx)
		{
			Source = InsertCp(Source, Ats[idx], TEXT("export "));
		}
		if (WriteSource(File, Source))
		{
			ExportsAdded += Ats.Num();
		}
	}
	UE_LOG(LogRUIMigrate, Display, TEXT("pass 1: added `export` to %d declaration(s)"), ExportsAdded);

	// The resolver over the freshly-exported tree: base = project dir (importer paths are
	// project-relative), search roots = the sweep roots (the export index universe).
	const FUetkxFsResolver Resolver(FPaths::ProjectDir(), Roots, /*bFixtureMode*/ false);

	// ── Pass 2: insert the imports each file needs (fresh reference scan, A3). ──
	int32 ImportsAdded = 0, CrossModule = 0;
	for (const FString& File : Files)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *File))
		{
			continue;
		}
		const FString Rel = ProjectRelPathFor(File);
		const FString Basename = FPaths::GetBaseFilename(File);
		const FUetkxFileScanResult Scan = FUetkxFileScan::Scan(Source, Basename);
		if (Scan.HasError())
		{
			continue; // a broken source — leave it for the author (the gate will still flag it)
		}
		// external component tags = a resolver-free compile's Uses (self-components already removed).
		const FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, Basename, Rel);
		TSet<FString> Tags(Out.Uses);

		TMap<FString, TArray<FString>> BySpec;
		TArray<FString> Cross;
		FUetkxResolve::MissingImports(Scan, Tags, Rel, Resolver, BySpec, Cross);
		for (const FString& Edge : Cross)
		{
			UE_LOG(LogRUIMigrate, Error,
				   TEXT("%s: UETKX2308: import crosses a module/root boundary (%s) — NOT written; move the "
						"dependency or the file"),
				   *Rel, *Edge);
			++CrossModule;
		}
		if (BySpec.IsEmpty())
		{
			continue;
		}
		TArray<FString> Specs;
		BySpec.GetKeys(Specs);
		Specs.Sort();
		FString Block;
		for (const FString& Spec : Specs)
		{
			Block +=
				FString::Printf(TEXT("import { %s } from \"%s\"\n"), *FString::Join(BySpec[Spec], TEXT(", ")), *Spec);
			ImportsAdded += BySpec[Spec].Num();
		}
		// Insert the import block right before the first declaration (after any #include block +
		// its blank line), then a blank line before the decl — the canonical preamble order.
		const int32 InsertAt = FMath::Max(0, FUetkxFileScan::ScanPreamble(Source).FirstDeclAt);
		Source = InsertCp(Source, InsertAt, Block + TEXT("\n"));
		WriteSource(File, Source);
	}
	UE_LOG(LogRUIMigrate, Display,
		   TEXT("pass 2: inserted %d import binding(s); %d cross-module edge(s) left for the owner"), ImportsAdded,
		   CrossModule);

	// ── Gate: compile the whole tree WITH resolution; require ZERO diagnostics (strict day one). ──
	int32 Errors = CrossModule;
	// Cross-file duplicate-export ledger: export-everything can make two files export the same name — a
	// CROSS-FILE UETKX2106 collision that a per-file compile cannot see, so the codemod would green-light
	// a tree that RUICompile -check then rejects (bughunt MIGRATE-1). Enforce the same invariant here.
	TMap<FString, FString> NameToFile;
	for (const FString& File : Files)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *File))
		{
			continue;
		}
		const FString Rel = ProjectRelPathFor(File);
		const FUetkxCompileOutput Out =
			FUetkxCodegen::CompileSource(Source, FPaths::GetBaseFilename(File), Rel, &Resolver);
		for (const FString& Name : Out.ExportedNames)
		{
			if (const FString* Incumbent = NameToFile.Find(Name))
			{
				UE_LOG(LogRUIMigrate, Error,
					   TEXT("%s: UETKX2106: exported binding `%s` is already bound by %s (one exported name, one "
							"file) — rename or keep one private"),
					   *Rel, *Name, *ProjectRelPathFor(*Incumbent));
				++Errors;
			}
			else
			{
				NameToFile.Add(Name, File);
			}
		}
		for (const FUetkxDiag& Diag : Out.Diags)
		{
			if (Diag.Severity == 0)
			{
				UE_LOG(LogRUIMigrate, Error, TEXT("%s: %s: %s"), *Rel, *Diag.Code, *Diag.Message);
				++Errors;
			}
		}
	}
	UE_LOG(LogRUIMigrate, Display, TEXT("RUIMigrateImports: %d file(s), %d error(s) after migration"), Files.Num(),
		   Errors);
	return Errors == 0 ? 0 : 1;
}
