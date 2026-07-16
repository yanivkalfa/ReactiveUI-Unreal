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

	/** The quoted or angle-bracket header path inside a `#include "X.h"` line, or empty if
	 *  malformed. Duplicates UetkxFileScan.cpp's private ExtractIncludeHeader (that one is file-
	 *  local to the Interp module; small enough not to warrant exporting across module boundaries
	 *  for one caller). */
	FString HeaderOfIncludeLine(const FString& Line)
	{
		int32 Open = INDEX_NONE;
		TCHAR CloseChar = TEXT('"');
		if (Line.FindChar(TEXT('"'), Open))
		{
			CloseChar = TEXT('"');
		}
		else if (Line.FindChar(TEXT('<'), Open))
		{
			CloseChar = TEXT('>');
		}
		else
		{
			return FString();
		}
		int32 Close = INDEX_NONE;
		if (!Line.Mid(Open + 1).FindChar(CloseChar, Close))
		{
			return FString();
		}
		return Line.Mid(Open + 1, Close);
	}

	/** The [line-start, line-end-INCLUSIVE-of-its-newline) span containing a code-point offset —
	 *  deleting this whole span removes a line with no blank-line residue. */
	void LineBounds(const TArray<int32>& Cp, int32 Offset, int32& OutStart, int32& OutEndInclusive)
	{
		int32 S = Offset;
		while (S > 0 && Cp[S - 1] != TEXT('\n'))
		{
			--S;
		}
		int32 E = Offset;
		while (E < Cp.Num() && Cp[E] != TEXT('\n'))
		{
			++E;
		}
		if (E < Cp.Num())
		{
			++E; // include the newline itself
		}
		OutStart = S;
		OutEndInclusive = E;
	}

	/** One surgical edit: replace `Cp[Start, EndExclusive)` with `Text` (code points). Deletions
	 *  pass an empty Text. Kept separate from `InsertCp` (zero-length replace) because -tidy edits
	 *  both insert AND remove text at the SAME described span. */
	struct FTidyEdit
	{
		int32 Start = 0;
		int32 EndExclusive = 0;
		FString Text;
	};

	/** -tidy (§C): the edit list for one file — auto-included headers (FUetkxFileScan::
	 *  AutoIncludedHeaders) are DELETED whether spelled as a raw `#include` or `import "@X.h"`; a
	 *  surviving raw `#include "X.h"` converts to `import "@X.h"` IN PLACE. Named imports are
	 *  never touched. SURGICAL by construction: only edits the exact span of a recognized
	 *  construct that is ALONE on its physical line — a construct sharing a line with anything
	 *  else (e.g. a trailing `// comment`) is left completely untouched (logged), so a comment or
	 *  other user text anywhere in the preamble can never be lost. Returns edits in ASCENDING
	 *  Start order (the caller applies them descending so earlier edits don't shift later ones). */
	TArray<FTidyEdit> CollectTidyEdits(const TArray<int32>& Cp, const FUetkxFileScanResult& Scan,
									   const FString& RelPathForLog, int32& OutRemoved, int32& OutConverted,
									   int32& OutSkippedShared)
	{
		const TArray<FString>& AutoHeaders = FUetkxFileScan::AutoIncludedHeaders();
		TArray<FTidyEdit> Edits;

		auto TryEdit = [&](int32 ConstructAt, const FString& ConstructText, const FString& Replacement, bool bDrop)
		{
			int32 LineStart, LineEndInclusive;
			LineBounds(Cp, ConstructAt, LineStart, LineEndInclusive);
			const FString Line =
				FUetkxLexer::FromCodePoints(Cp, LineStart, LineEndInclusive - LineStart).TrimStartAndEnd();
			if (Line != ConstructText)
			{
				// Something else shares this line (a trailing comment, unusual spacing the trim
				// didn't normalize) — leave it untouched rather than risk losing that content.
				UE_LOG(LogRUIMigrate, Warning, TEXT("%s: -tidy left `%s` untouched — its line has other content"),
					   *RelPathForLog, *ConstructText);
				++OutSkippedShared;
				return;
			}
			if (bDrop)
			{
				Edits.Add({LineStart, LineEndInclusive, FString()});
				++OutRemoved;
			}
			else
			{
				Edits.Add({LineStart, LineEndInclusive, Replacement + TEXT("\n")});
				++OutConverted;
			}
		};

		for (int32 i = 0; i < Scan.PreambleIncludes.Num(); ++i)
		{
			const FString& Include = Scan.PreambleIncludes[i];
			const FString Header = HeaderOfIncludeLine(Include);
			if (Header.IsEmpty())
			{
				continue; // unrecognized #include shape — leave it exactly as authored
			}
			const bool bAuto = AutoHeaders.Contains(Header);
			TryEdit(Scan.PreambleIncludeAts[i], Include, FString::Printf(TEXT("import \"@%s\""), *Header), bAuto);
		}
		for (const FUetkxImportDecl& Imp : Scan.Imports)
		{
			if (!Imp.bHostInclude || !AutoHeaders.Contains(Imp.Specifier))
			{
				continue; // named imports, and @-imports not on the auto-included list: untouched
			}
			TryEdit(Imp.At, FString::Printf(TEXT("import \"@%s\""), *Imp.Specifier), FString(), /*bDrop*/ true);
		}

		Edits.Sort([](const FTidyEdit& A, const FTidyEdit& B) { return A.Start < B.Start; });
		return Edits;
	}
} // namespace

int32 URUIMigrateImportsCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);
	const bool bTidy =
		Switches.ContainsByPredicate([](const FString& S) { return S.Equals(TEXT("tidy"), ESearchCase::IgnoreCase); });

	const TArray<FString> Roots = URUICompileCommandlet::DefaultRoots();
	TArray<FString> Files;
	for (const FString& Root : Roots)
	{
		Files.Append(FUetkxDriver::FindAll(Root)); // ContractFixtures auto-excluded (hand-edited)
	}
	Files.Sort();

	// ── Pass 0 (-tidy only, INCLUDE_RETIREMENT_PLAN.md §C): rewrite each file's preamble so it
	// holds ONLY import lines — auto-included headers dropped, surviving raw #includes converted
	// to `import "@X.h"`. SURGICAL (CollectTidyEdits): only a construct alone on its own physical
	// line is touched, so a comment anywhere in the preamble is never at risk. Runs before export/
	// import migration so pass 2's insertion point (recomputed fresh per file) sees the tidied text.
	if (bTidy)
	{
		int32 FilesTidied = 0, LinesRemoved = 0, LinesConverted = 0, ConstructsSkipped = 0;
		for (const FString& File : Files)
		{
			FString Source;
			if (!FFileHelper::LoadFileToString(Source, *File))
			{
				continue;
			}
			const FString Rel = ProjectRelPathFor(File);
			const FUetkxFileScanResult Scan = FUetkxFileScan::Scan(Source, FPaths::GetBaseFilename(File));
			const TArray<int32> Cp = FUetkxLexer::ToCodePoints(Source);
			int32 Removed = 0, Converted = 0, Skipped = 0;
			TArray<FTidyEdit> Edits = CollectTidyEdits(Cp, Scan, Rel, Removed, Converted, Skipped);
			ConstructsSkipped += Skipped;
			if (Edits.IsEmpty())
			{
				continue; // nothing to do — already imports-only, or nothing redundant (idempotent)
			}
			TArray<int32> NewCp = Cp;
			for (int32 idx = Edits.Num() - 1; idx >= 0; --idx) // descending so earlier edits keep their offsets valid
			{
				const FTidyEdit& Edit = Edits[idx];
				NewCp.RemoveAt(Edit.Start, Edit.EndExclusive - Edit.Start);
				NewCp.Insert(FUetkxLexer::ToCodePoints(Edit.Text), Edit.Start);
			}
			// A deleted line at the very top of the file leaves the ORIGINAL blank separator line
			// (that used to sit between the includes and the rest) as an orphaned leading blank
			// line — strip only LEADING blank lines (never interior spacing, which may be
			// intentional) so a fully-emptied preamble doesn't leave the file starting on a blank.
			int32 LeadingBlank = 0;
			while (LeadingBlank < NewCp.Num() && NewCp[LeadingBlank] == TEXT('\n'))
			{
				++LeadingBlank;
			}
			if (LeadingBlank > 0)
			{
				NewCp.RemoveAt(0, LeadingBlank);
			}
			if (WriteSource(File, FUetkxLexer::FromCodePoints(NewCp, 0, NewCp.Num())))
			{
				++FilesTidied;
				LinesRemoved += Removed;
				LinesConverted += Converted;
			}
		}
		UE_LOG(LogRUIMigrate, Display,
			   TEXT("pass 0 (-tidy): %d file(s) rewritten (%d redundant line(s) removed, %d #include(s) converted "
					"to `import \"@...\"`); %d construct(s) left untouched (shared a line with other content)"),
			   FilesTidied, LinesRemoved, LinesConverted, ConstructsSkipped);
	}

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
