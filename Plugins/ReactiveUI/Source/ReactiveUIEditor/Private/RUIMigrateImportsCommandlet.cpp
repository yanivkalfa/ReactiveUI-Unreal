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

	/** Replace `[Start, EndExclusive)` (code points) with Text and return the new source. */
	FString ReplaceCp(const FString& Source, int32 Start, int32 EndExclusive, const FString& Text)
	{
		TArray<int32> Cp = FUetkxLexer::ToCodePoints(Source);
		Start = FMath::Clamp(Start, 0, Cp.Num());
		EndExclusive = FMath::Clamp(EndExclusive, Start, Cp.Num());
		Cp.RemoveAt(Start, EndExclusive - Start);
		Cp.Insert(FUetkxLexer::ToCodePoints(Text), Start);
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

	// ── shared passes (both codemods compose from these) ─────────────────────────────────────

	/** Pass 0 (INCLUDE_RETIREMENT_PLAN.md §C): the preamble tidy — see CollectTidyEdits. */
	void RunTidyPass(const TArray<FString>& Files)
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

	/** Pass 1 (A3): export EVERYTHING. By-name C++/UMG consumers keep resolving; privacy is
	 *  opt-in going forward. Inserting `export ` before each un-exported decl keyword, descending,
	 *  so earlier inserts don't shift later offsets. Idempotent: exported decls are skipped. */
	void RunExportEverythingPass(const TArray<FString>& Files)
	{
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
	}

	/** Pass 2 (A3): insert the imports each file needs (fresh reference scan). */
	void RunInsertImportsPass(const TArray<FString>& Files, const FUetkxFsResolver& Resolver, int32& OutCrossModule)
	{
		int32 ImportsAdded = 0;
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
				++OutCrossModule;
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
				Block += FString::Printf(TEXT("import { %s } from \"%s\"\n"), *FString::Join(BySpec[Spec], TEXT(", ")),
										 *Spec);
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
			   OutCrossModule);
	}

	/** The final gate: compile the whole tree WITH resolution; ZERO error diagnostics + the
	 *  cross-file 2106 ledger. When bRequireZero2320 is set (the ES-modules codemod), a UETKX2320
	 *  deprecation warning in any file NOT in SkippedFiles is ALSO an error — the migrated tree
	 *  must be wrapper-free outside the explicitly-reported skips. Counted from compile results,
	 *  never the Message Log (the watcher's log path only reports severity 0). */
	int32 RunZeroDiagGate(const TArray<FString>& Files, const FUetkxFsResolver& Resolver, int32 SeedErrors,
						  bool bRequireZero2320, const TSet<FString>& SkippedFiles, const TCHAR* Label)
	{
		int32 Errors = SeedErrors;
		// Cross-file duplicate-export ledger: export-everything can make two files export the same
		// name — a CROSS-FILE UETKX2106 collision that a per-file compile cannot see, so the codemod
		// would green-light a tree that RUICompile -check then rejects (bughunt MIGRATE-1).
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
				else if (bRequireZero2320 && Diag.Code == TEXT("UETKX2320") && !SkippedFiles.Contains(File))
				{
					UE_LOG(LogRUIMigrate, Error,
						   TEXT("%s: UETKX2320 survived the migration (wrapper syntax remains) — %s"), *Rel,
						   *Diag.Message);
					++Errors;
				}
			}
		}
		UE_LOG(LogRUIMigrate, Display, TEXT("%s: %d file(s), %d error(s) after migration"), Label, Files.Num(), Errors);
		return Errors;
	}

	// ── ES-modules wrapper rewriting (M7 pass 3) ─────────────────────────────────────────────

	/** One hoisted module: its exported members become top-level value/util declarations; other
	 *  files' `{ M }` imports flip to `* as M` (their `M::x` refs keep compiling, U-03). */
	struct FHoistedModule
	{
		FString File; // normalized absolute path of the file that held the module
		FString Name;
		TArray<FString> MemberNames;
	};

	/** Parse a module body into simple members. Returns false (OutText untouched) when ANY member
	 *  is not a plain `inline const T N = init;` / `inline const T N{init};` variable or an
	 *  `inline R F(args) { body }` function — such modules are left wrapped (the caller reports).
	 *  OutText = the hoisted top-level declarations (each `export `-prefixed when bExported). */
	bool HoistModuleMembers(const FString& Body, bool bExported, FString& OutText, TArray<FString>& OutMemberNames)
	{
		const TArray<int32> Cp = FUetkxLexer::ToCodePoints(Body);
		const int32 N = Cp.Num();
		const FString Prefix = bExported ? TEXT("export ") : TEXT("");
		FString Out;
		int32 i = 0;
		auto SkipWsComments = [&](int32 p)
		{
			for (;;)
			{
				while (p < N && (Cp[p] == ' ' || Cp[p] == '\t' || Cp[p] == '\n' || Cp[p] == '\r'))
				{
					++p;
				}
				const int32 nc = FUetkxLexer::SkipNoncode(Cp, p);
				if (nc == p)
				{
					return p;
				}
				p = nc;
			}
		};
		auto ReadIdent = [&](int32 p, FString& OutIdent) -> int32
		{
			const int32 s = p;
			while (p < N && FUetkxLexer::IsIdentCode(Cp[p]))
			{
				++p;
			}
			OutIdent = FUetkxLexer::FromCodePoints(Cp, s, p - s);
			return p;
		};
		while (i < N)
		{
			i = SkipWsComments(i);
			if (i >= N)
			{
				break;
			}
			// Modifiers: `inline` / `const` / `constexpr` are consumed (top-level values are
			// const-by-construction; utils re-gain `inline` at emit). Anything structural fails.
			int32 p = i;
			bool bSawModifier = true;
			while (bSawModifier)
			{
				p = SkipWsComments(p);
				FString Word;
				const int32 e = ReadIdent(p, Word);
				if (Word == TEXT("inline") || Word == TEXT("const") || Word == TEXT("constexpr"))
				{
					p = e;
					continue;
				}
				if (Word == TEXT("static") || Word == TEXT("struct") || Word == TEXT("class") ||
					Word == TEXT("using") || Word == TEXT("typedef") || Word == TEXT("template") ||
					Word == TEXT("enum") || Word == TEXT("namespace"))
				{
					return false; // not a simple member — leave the module wrapped
				}
				bSawModifier = false;
			}
			// Head: idents/quals up to a top-level `=`, `{` (brace-init) or `(` (function).
			int32 HeadStart = SkipWsComments(p);
			int32 q = HeadStart;
			int32 Depth = 0;
			int32 IdentStart = -1, IdentEnd = -1;
			TCHAR Trigger = 0;
			int32 TriggerAt = -1;
			while (q < N)
			{
				const int32 nc = FUetkxLexer::SkipNoncode(Cp, q);
				if (nc != q)
				{
					q = nc;
					continue;
				}
				const int32 C = Cp[q];
				if (Depth == 0 && FUetkxLexer::IsIdentCode(C) && !(C >= '0' && C <= '9'))
				{
					IdentStart = q;
					while (q < N && FUetkxLexer::IsIdentCode(Cp[q]))
					{
						++q;
					}
					IdentEnd = q;
					continue;
				}
				if (C == '<' || C == '[')
				{
					++Depth;
					++q;
					continue;
				}
				if (C == '>' || C == ']')
				{
					--Depth;
					++q;
					continue;
				}
				if (Depth == 0 && (C == '=' || C == '{' || C == '('))
				{
					Trigger = static_cast<TCHAR>(C);
					TriggerAt = q;
					break;
				}
				if (C == ';' && Depth == 0)
				{
					return false; // a bare `T N;` (no initializer) — not hoistable as a value
				}
				++q;
			}
			if (Trigger == 0 || IdentStart < 0 || IdentEnd > TriggerAt)
			{
				return false;
			}
			const FString Name = FUetkxLexer::FromCodePoints(Cp, IdentStart, IdentEnd - IdentStart);
			const FString Type = FUetkxLexer::FromCodePoints(Cp, HeadStart, IdentStart - HeadStart).TrimStartAndEnd();
			if (Type.IsEmpty())
			{
				return false;
			}
			if (Trigger == '=')
			{
				// `= init ;` — find the top-level `;` (only ()/[]/{} nest in expression position).
				int32 r = TriggerAt + 1;
				int32 D2 = 0;
				int32 SemiAt = -1;
				while (r < N)
				{
					const int32 nc = FUetkxLexer::SkipNoncode(Cp, r);
					if (nc != r)
					{
						r = nc;
						continue;
					}
					const int32 C = Cp[r];
					if (C == '(' || C == '[' || C == '{')
					{
						++D2;
					}
					else if (C == ')' || C == ']' || C == '}')
					{
						--D2;
					}
					else if (C == ';' && D2 == 0)
					{
						SemiAt = r;
						break;
					}
					++r;
				}
				if (SemiAt < 0)
				{
					return false;
				}
				const FString Init =
					FUetkxLexer::FromCodePoints(Cp, TriggerAt + 1, SemiAt - TriggerAt - 1).TrimStartAndEnd();
				Out += FString::Printf(TEXT("%s%s %s = %s;\n"), *Prefix, *Type, *Name, *Init);
				OutMemberNames.Add(Name);
				i = SemiAt + 1;
				continue;
			}
			if (Trigger == '{')
			{
				// `T N{init};` — brace-init normalizes to `= { init }` (U-01 values require `=`).
				const int32 Close = FUetkxLexer::FindMatching(Cp, TriggerAt);
				if (Close == -1)
				{
					return false;
				}
				const FString Init =
					FUetkxLexer::FromCodePoints(Cp, TriggerAt + 1, Close - TriggerAt - 1).TrimStartAndEnd();
				Out += FString::Printf(TEXT("%s%s %s = { %s };\n"), *Prefix, *Type, *Name, *Init);
				OutMemberNames.Add(Name);
				int32 r = SkipWsComments(Close + 1);
				i = (r < N && Cp[r] == ';') ? r + 1 : Close + 1;
				continue;
			}
			// Trigger == '(' — a member function → a top-level util.
			const int32 ParenClose = FUetkxLexer::FindMatching(Cp, TriggerAt);
			if (ParenClose == -1)
			{
				return false;
			}
			int32 BodyOpen = SkipWsComments(ParenClose + 1);
			if (BodyOpen >= N || Cp[BodyOpen] != '{')
			{
				return false;
			}
			const int32 BodyClose = FUetkxLexer::FindMatching(Cp, BodyOpen);
			if (BodyClose == -1)
			{
				return false;
			}
			const FString Params =
				FUetkxLexer::FromCodePoints(Cp, TriggerAt + 1, ParenClose - TriggerAt - 1).TrimStartAndEnd();
			const FString FnBody =
				FUetkxLexer::FromCodePoints(Cp, BodyOpen + 1, BodyClose - BodyOpen - 1).TrimStartAndEnd();
			Out += FString::Printf(TEXT("%s%s %s(%s) {\n\t%s\n}\n"), *Prefix, *Type, *Name, *Params, *FnBody);
			OutMemberNames.Add(Name);
			i = BodyClose + 1;
		}
		if (Out.IsEmpty())
		{
			return false; // an empty module hoists to NOTHING — leave it (its author decides)
		}
		OutText = Out;
		return true;
	}

	/** Strip same-file `M::member` quals after M was hoisted (its namespace no longer exists).
	 *  Word-boundary, skip-noncode, and ONLY when the member after `::` is a hoisted member —
	 *  ambient namespaces are never touched. */
	FString StripModuleQuals(const FString& Source, const FString& ModuleName, const TSet<FString>& Members)
	{
		const TArray<int32> Cp = FUetkxLexer::ToCodePoints(Source);
		const int32 N = Cp.Num();
		FString Out;
		int32 i = 0;
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Cp, i);
			if (j != i)
			{
				Out += FUetkxLexer::FromCodePoints(Cp, i, j - i);
				i = j;
				continue;
			}
			const bool bWordStart = (i == 0) || !FUetkxLexer::IsIdentCode(Cp[i - 1]);
			if (bWordStart && FUetkxLexer::KeywordAt(Cp, i, *ModuleName))
			{
				// preceding `::`/member access blocks (an ambient qual of the same name); a LONE `:`
				// is a ternary/label/case and must not block (the IMPORT-3 rule)
				bool bBlocked = false;
				for (int32 k = i - 1; k >= 0; --k)
				{
					const int32 P = Cp[k];
					if (P == ' ' || P == '\t')
					{
						continue;
					}
					bBlocked = ((P == ':') && k > 0 && Cp[k - 1] == ':') || (P == '.') ||
							   (P == '>' && k > 0 && Cp[k - 1] == '-');
					break;
				}
				int32 p = i + ModuleName.Len();
				while (p < N && (Cp[p] == ' ' || Cp[p] == '\t'))
				{
					++p;
				}
				if (!bBlocked && p + 1 < N && Cp[p] == ':' && Cp[p + 1] == ':')
				{
					int32 m = p + 2;
					while (m < N && (Cp[m] == ' ' || Cp[m] == '\t'))
					{
						++m;
					}
					FString Member;
					int32 e = m;
					while (e < N && FUetkxLexer::IsIdentCode(Cp[e]))
					{
						++e;
					}
					Member = FUetkxLexer::FromCodePoints(Cp, m, e - m);
					if (Members.Contains(Member))
					{
						i = m; // drop `M ::` — the member ident flows through bare
						continue;
					}
				}
			}
			Out += FUetkxLexer::FromCodePoints(Cp, i, 1);
			++i;
		}
		return Out;
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

	if (bTidy)
	{
		RunTidyPass(Files);
	}
	RunExportEverythingPass(Files);

	// The resolver over the freshly-exported tree: base = project dir (importer paths are
	// project-relative), search roots = the sweep roots (the export index universe).
	const FUetkxFsResolver Resolver(FPaths::ProjectDir(), Roots, /*bFixtureMode*/ false);
	int32 CrossModule = 0;
	RunInsertImportsPass(Files, Resolver, CrossModule);
	const int32 Errors = RunZeroDiagGate(Files, Resolver, CrossModule, /*bRequireZero2320*/ false, TSet<FString>(),
										 TEXT("RUIMigrateImports"));
	return Errors == 0 ? 0 : 1;
}

int32 URUIMigrateEsModulesCommandlet::Main(const FString& Params)
{
	const TArray<FString> Roots = URUICompileCommandlet::DefaultRoots();
	TArray<FString> Files;
	for (const FString& Root : Roots)
	{
		Files.Append(FUetkxDriver::FindAll(Root)); // ContractFixtures auto-excluded (hand-edited, M8)
	}
	Files.Sort();

	// G-10 canonical order: 1. tidy → 2. export-normalize → 3. wrappers→plain → 4. imports → 5. gate.
	RunTidyPass(Files);
	RunExportEverythingPass(Files);

	// ── Pass 3: rewrite wrapper declarations to plain typed declarations (scanned-record-driven).
	int32 ComponentsRewritten = 0, HooksRewritten = 0, ModulesHoisted = 0, ModulesSkipped = 0;
	TArray<FHoistedModule> Hoisted;
	TSet<FString> SkippedFiles; // files keeping a wrapper (their 2320 is reported, not fatal)
	for (const FString& File : Files)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *File))
		{
			continue;
		}
		const FString Rel = ProjectRelPathFor(File);
		const FUetkxFileScanResult Scan = FUetkxFileScan::Scan(Source, FPaths::GetBaseFilename(File));
		if (Scan.HasError())
		{
			UE_LOG(LogRUIMigrate, Warning, TEXT("%s: parse error — left unmigrated (the gate reports it)"), *Rel);
			SkippedFiles.Add(File);
			continue;
		}

		// Edits descend by source position so earlier spans stay valid.
		struct FRewrite
		{
			int32 Start = 0, End = 0;
			FString Text;
		};
		TArray<FRewrite> Rewrites;
		TArray<TPair<FString, TSet<FString>>> HoistedHere; // module name -> member set (same-file qual strip)
		for (const TPair<EUetkxDeclKind, int32>& Ord : Scan.Order)
		{
			if (Ord.Key == EUetkxDeclKind::Component)
			{
				const FUetkxComponentDecl& D = Scan.Components[Ord.Value];
				if (!D.bLegacySyntax)
				{
					continue;
				}
				TArray<FString> Parts;
				for (const FUetkxParam& P : D.Params)
				{
					FString Part = P.Type.IsEmpty() ? P.Name : P.Type + TEXT(" ") + P.Name;
					if (!P.Default.IsEmpty())
					{
						Part += TEXT(" = ") + P.Default;
					}
					Parts.Add(MoveTemp(Part));
				}
				// Header span: [`component`, the body `{`) — BodyAt is one past the `{`.
				Rewrites.Add({D.At, D.BodyAt - 1,
							  FString::Printf(TEXT("FRuiNode %s(%s) "), *D.Name, *FString::Join(Parts, TEXT(", ")))});
				++ComponentsRewritten;
			}
			else if (Ord.Key == EUetkxDeclKind::Hook)
			{
				const FUetkxHookDecl& D = Scan.Hooks[Ord.Value];
				if (!D.bLegacySyntax)
				{
					continue;
				}
				const FString Ret = D.Ret.TrimStartAndEnd();
				Rewrites.Add({D.At, D.BodyAt - 1,
							  FString::Printf(TEXT("%s %s(%s) "), Ret.IsEmpty() ? TEXT("void") : *Ret, *D.Name,
											  *D.Params.TrimStartAndEnd())});
				++HooksRewritten;
			}
			else if (Ord.Key == EUetkxDeclKind::Module)
			{
				const FUetkxModuleDecl& D = Scan.Modules[Ord.Value];
				FString HoistText;
				TArray<FString> MemberNames;
				if (!HoistModuleMembers(D.Body, D.bExported, HoistText, MemberNames))
				{
					UE_LOG(LogRUIMigrate, Warning,
						   TEXT("%s: module `%s` holds non-simple members (struct/using/static/...) — LEFT WRAPPED; "
								"migrate it by hand or move the members to a C++ header"),
						   *Rel, *D.Name);
					SkippedFiles.Add(File);
					++ModulesSkipped;
					continue;
				}
				// The whole decl span [export?, Next) is replaced by the hoisted members.
				const int32 Start = (D.bExported && D.ExportAt >= 0) ? D.ExportAt : D.At;
				Rewrites.Add({Start, D.Next, HoistText});
				FHoistedModule H;
				H.File = File;
				H.Name = D.Name;
				H.MemberNames = MemberNames;
				Hoisted.Add(MoveTemp(H));
				HoistedHere.Emplace(D.Name, TSet<FString>(MemberNames));
				++ModulesHoisted;
			}
		}
		if (Rewrites.IsEmpty())
		{
			continue;
		}
		Rewrites.Sort([](const FRewrite& A, const FRewrite& B) { return A.Start < B.Start; });
		for (int32 idx = Rewrites.Num() - 1; idx >= 0; --idx)
		{
			Source = ReplaceCp(Source, Rewrites[idx].Start, Rewrites[idx].End, Rewrites[idx].Text);
		}
		// Same-file `M::member` quals lose their namespace with the hoist — strip them.
		for (const TPair<FString, TSet<FString>>& H : HoistedHere)
		{
			Source = StripModuleQuals(Source, H.Key, H.Value);
		}
		WriteSource(File, Source);
	}
	UE_LOG(LogRUIMigrate, Display,
		   TEXT("pass 3: %d component(s) + %d hook(s) rewritten to plain declarations; %d module(s) hoisted, %d left "
				"wrapped (reported above)"),
		   ComponentsRewritten, HooksRewritten, ModulesHoisted, ModulesSkipped);

	// ── Pass 3.5: other files' `{ M }` imports of a hoisted module flip to `import * as M from …`
	// (their `M::x` references keep compiling through the U-03 strip plane — zero body edits).
	const FUetkxFsResolver Resolver(FPaths::ProjectDir(), Roots, /*bFixtureMode*/ false);
	int32 ImportsConverted = 0;
	if (Hoisted.Num() > 0)
	{
		auto NormAbs = [](FString P)
		{
			P = FPaths::ConvertRelativePathToFull(P);
			FPaths::NormalizeFilename(P);
			return P;
		};
		TMap<FString, TSet<FString>> HoistedByFile; // normalized path -> hoisted module names
		for (const FHoistedModule& H : Hoisted)
		{
			HoistedByFile.FindOrAdd(NormAbs(H.File)).Add(H.Name);
		}
		for (const FString& File : Files)
		{
			FString Source;
			if (!FFileHelper::LoadFileToString(Source, *File))
			{
				continue;
			}
			const FString Rel = ProjectRelPathFor(File);
			const FUetkxFileScanResult Scan = FUetkxFileScan::Scan(Source, FPaths::GetBaseFilename(File));
			// Descending so earlier import spans stay valid.
			for (int32 idx = Scan.Imports.Num() - 1; idx >= 0; --idx)
			{
				const FUetkxImportDecl& Imp = Scan.Imports[idx];
				if (Imp.bHostInclude || Imp.bNamespace || Imp.bDefault || Imp.Names.IsEmpty())
				{
					continue;
				}
				const FString Key = Resolver.Resolve(Imp.Specifier, Rel);
				const TSet<FString>* HoistedNames = Key.IsEmpty() ? nullptr : HoistedByFile.Find(NormAbs(Key));
				if (!HoistedNames)
				{
					continue;
				}
				TArray<FString> Kept, Converted;
				for (int32 n = 0; n < Imp.Names.Num(); ++n)
				{
					const FString Spelled = (Imp.LocalNames.IsValidIndex(n) && Imp.LocalNames[n] != Imp.Names[n])
												? Imp.Names[n] + TEXT(" as ") + Imp.LocalNames[n]
												: Imp.Names[n];
					(HoistedNames->Contains(Imp.Names[n]) ? Converted : Kept).Add(Spelled);
				}
				if (Converted.IsEmpty())
				{
					continue;
				}
				FString NewText;
				for (const FString& M : Converted)
				{
					NewText += FString::Printf(TEXT("import * as %s from \"%s\"\n"), *M, *Imp.Specifier);
					++ImportsConverted;
				}
				if (!Kept.IsEmpty())
				{
					NewText += FString::Printf(TEXT("import { %s } from \"%s\"\n"), *FString::Join(Kept, TEXT(", ")),
											   *Imp.Specifier);
				}
				NewText = NewText.LeftChop(1); // the span we replace does not include the newline
				const int32 End = Imp.SpecifierAt + 1 + Imp.Specifier.Len() + 1; // past the closing quote
				Source = ReplaceCp(Source, Imp.At, End, NewText);
			}
			WriteSource(File, Source);
		}
		UE_LOG(LogRUIMigrate, Display, TEXT("pass 3.5: %d module import(s) converted to `import * as`"),
			   ImportsConverted);
	}

	// ── Pass 4: insert any still-missing imports (fresh reference scan over the migrated tree).
	int32 CrossModule = 0;
	RunInsertImportsPass(Files, Resolver, CrossModule);

	// ── Pass 5: the zero-diagnostics gate — zero errors AND zero 2320 outside the reported skips.
	const int32 Errors = RunZeroDiagGate(Files, Resolver, CrossModule, /*bRequireZero2320*/ true, SkippedFiles,
										 TEXT("RUIMigrateEsModules"));
	return Errors == 0 ? 0 : 1;
}
