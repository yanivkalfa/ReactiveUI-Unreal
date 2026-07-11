// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxResolve.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UetkxConfig.h"
#include "UetkxLexer.h"

namespace
{
	FString NormAbs(FString P)
	{
		P = FPaths::ConvertRelativePathToFull(P);
		FPaths::NormalizeFilename(P); // backslashes -> forward slashes
		return P;
	}

	FString NormDir(FString P)
	{
		if (P.IsEmpty())
		{
			return P;
		}
		P = FPaths::ConvertRelativePathToFull(P);
		FPaths::NormalizeDirectoryName(P);
		return P;
	}

	const TCHAR* KindWord(EUetkxDeclKind K)
	{
		return K == EUetkxDeclKind::Component ? TEXT("component") : K == EUetkxDeclKind::Hook ? TEXT("hook") : TEXT("module");
	}

	uint32 ExportHashOfScan(const FUetkxPreambleScan& Scan)
	{
		TArray<FString> Lines;
		for (const FUetkxPreambleDecl& D : Scan.Decls)
		{
			Lines.Add(FString::Printf(TEXT("%s|%s|%d"), *D.Name, KindWord(D.Kind), D.bExported ? 1 : 0));
		}
		Lines.Sort();
		uint32 H = 2166136261u;
		for (const FString& L : Lines)
		{
			for (int32 i = 0; i < L.Len(); ++i)
			{
				H = (H ^ static_cast<uint32>(L[i])) * 16777619u;
			}
			H = (H ^ static_cast<uint32>('\n')) * 16777619u;
		}
		return H;
	}

	bool IsIdentStart(int32 C)
	{
		return C == '_' || (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z');
	}

	/** External-reference kinds a verbatim region can carry (setup / hook / module bodies). */
	struct FExtRef
	{
		enum class EKind
		{
			Hook,
			Module
		};
		FString Name;
		int32 At = -1;
		EKind Kind = EKind::Hook;
	};

	/** Scan a verbatim C++ region for bare `Use<Upper>(` calls (user hooks) and leftmost `Ident::`
	 *  scope quals (module references), respecting comments/strings and member/scope prefixes — the
	 *  PrefixHookCalls classification (so hand-header namespaces like `RuiDemo::X` are found here as
	 *  Module refs but filtered out later when no file exports the name). BaseAt = the region's
	 *  absolute offset in the original source. */
	void CollectExternalRefs(const FString& Region, int32 BaseAt, TArray<FExtRef>& Out)
	{
		const TArray<int32> Src = FUetkxLexer::ToCodePoints(Region);
		const int32 N = Src.Num();
		int32 i = 0;
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Src, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			if (!IsIdentStart(Src[i]))
			{
				++i;
				continue;
			}
			const int32 s = i;
			while (i < N && FUetkxLexer::IsIdentCode(Src[i]))
			{
				++i;
			}
			const FString Ident = FUetkxLexer::FromCodePoints(Src, s, i - s);
			// preceding non-ws: member (`.`/`->`) or scope (`:`) blocks a fresh reference.
			bool bMember = false, bScope = false;
			for (int32 k = s - 1; k >= 0; --k)
			{
				const int32 P = Src[k];
				if (P == ' ' || P == '\t')
				{
					continue;
				}
				bMember = (P == '.') || (P == '>' && k > 0 && Src[k - 1] == '-');
				bScope = (P == ':');
				break;
			}
			if (bMember || bScope)
			{
				continue;
			}
			// following non-ws: `::` (module qual) or `(` (call).
			int32 p = i;
			while (p < N && (Src[p] == ' ' || Src[p] == '\t'))
			{
				++p;
			}
			const bool bFollowScope = (p + 1 < N && Src[p] == ':' && Src[p + 1] == ':');
			const bool bFollowParen = (p < N && Src[p] == '(');
			if (bFollowScope)
			{
				Out.Add({Ident, BaseAt + s, FExtRef::EKind::Module});
			}
			else if (bFollowParen && Ident.StartsWith(TEXT("Use")) && Ident.Len() >= 4 && FChar::IsUpper(Ident[3]) &&
					 !FUetkxFileScan::HookNames().Contains(Ident))
			{
				Out.Add({Ident, BaseAt + s, FExtRef::EKind::Hook});
			}
		}
	}
} // namespace

// ── FUetkxFsResolver ────────────────────────────────────────────────────────────────────────

FUetkxFsResolver::FUetkxFsResolver(const FString& InBaseDir, const TArray<FString>& InSearchRoots, bool bInFixtureMode)
	: BaseDir(NormDir(InBaseDir)), SearchRoots(InSearchRoots), bFixtureMode(bInFixtureMode)
{
}

FString FUetkxFsResolver::ImporterAbs(const FString& ImporterPath) const
{
	return NormAbs(FPaths::Combine(BaseDir, ImporterPath));
}

FString FUetkxFsResolver::Resolve(const FString& Spec, const FString& ImporterPath) const
{
	const FString ImporterDir = FPaths::GetPath(ImporterAbs(ImporterPath));
	FString Target;
	if (Spec.StartsWith(TEXT("./")) || Spec.StartsWith(TEXT("../")))
	{
		Target = FPaths::Combine(ImporterDir, Spec);
	}
	else if (Spec.StartsWith(TEXT("~/")))
	{
		const FString Anchor = bFixtureMode ? BaseDir : FUetkxConfig::RootAnchorFor(ImporterAbs(ImporterPath));
		if (Anchor.IsEmpty())
		{
			return FString();
		}
		Target = FPaths::Combine(Anchor, Spec.Mid(2));
	}
	else
	{
		return FString(); // engine-native (res://, Assets/, Source/) + bare specifiers are forbidden
	}
	if (!Target.EndsWith(TEXT(".uetkx")))
	{
		Target += TEXT(".uetkx");
	}
	Target = NormAbs(Target);
	return IFileManager::Get().FileExists(*Target) ? Target : FString();
}

const FUetkxPreambleScan* FUetkxFsResolver::CachedScan(const FString& Key) const
{
	const FDateTime Mtime = IFileManager::Get().GetTimeStamp(*Key);
	if (Mtime == FDateTime::MinValue())
	{
		return nullptr;
	}
	if (FCacheEntry* E = Cache.Find(Key))
	{
		if (E->Mtime == Mtime)
		{
			return &E->Scan;
		}
	}
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *Key))
	{
		return nullptr;
	}
	FCacheEntry Entry;
	Entry.Mtime = Mtime;
	Entry.Scan = FUetkxFileScan::ScanPreamble(Source);
	Entry.Hash = ExportHashOfScan(Entry.Scan);
	FCacheEntry& Ref = Cache.Add(Key, MoveTemp(Entry));
	return &Ref.Scan;
}

bool FUetkxFsResolver::GetDecls(const FString& Key, TMap<FString, FUetkxTargetDecl>& Out) const
{
	const FUetkxPreambleScan* Scan = CachedScan(Key);
	if (!Scan)
	{
		return false;
	}
	for (const FUetkxPreambleDecl& D : Scan->Decls)
	{
		Out.Add(D.Name, FUetkxTargetDecl{D.Kind, D.bExported});
	}
	return true;
}

bool FUetkxFsResolver::CrossesModuleBoundary(const FString& ImporterPath, const FString& Key) const
{
	if (bFixtureMode)
	{
		return false; // a flat fixture tree has no module boundary
	}
	const FString ImpMod = NormDir(FUetkxConfig::ModuleRootFor(ImporterAbs(ImporterPath)));
	const FString KeyMod = NormDir(FUetkxConfig::ModuleRootFor(Key));
	if (ImpMod.IsEmpty() || KeyMod.IsEmpty())
	{
		return false; // can't determine a module for one side — don't false-positive
	}
	return ImpMod != KeyMod;
}

uint32 FUetkxFsResolver::ExportHashOf(const FString& Key) const
{
	if (const FCacheEntry* E = Cache.Find(Key))
	{
		const FDateTime Mtime = IFileManager::Get().GetTimeStamp(*Key);
		if (E->Mtime == Mtime)
		{
			return E->Hash;
		}
	}
	const FUetkxPreambleScan* Scan = CachedScan(Key);
	return Scan ? ExportHashOfScan(*Scan) : 0;
}

FString FUetkxFsResolver::LabelForKey(const FString& Key) const
{
	FString Label = Key;
	FPaths::MakePathRelativeTo(Label, bFixtureMode ? *(BaseDir + TEXT("/")) : *FPaths::ProjectDir());
	Label.ReplaceInline(TEXT("\\"), TEXT("/"));
	return Label;
}

void FUetkxFsResolver::EnsureIndex() const
{
	if (bIndexBuilt)
	{
		return;
	}
	bIndexBuilt = true;
	for (const FString& Root : SearchRoots)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.uetkx"), true, false);
		for (const FString& File : Files)
		{
			const FString Norm = NormAbs(File);
			// The driver's export universe excludes the D-22 contract fixtures (harness-only).
			if (!bFixtureMode && Norm.Contains(TEXT("/ContractFixtures/")))
			{
				continue;
			}
			const FUetkxPreambleScan* Scan = CachedScan(Norm);
			if (!Scan)
			{
				continue;
			}
			for (const FUetkxPreambleDecl& D : Scan->Decls)
			{
				if (D.bExported && !ExporterOf.Contains(D.Name))
				{
					ExportIndex.Add(D.Name, D.Kind);
					ExporterOf.Add(D.Name, Norm);
				}
			}
		}
	}
}

FString FUetkxFsResolver::FindExporter(const FString& Name, EUetkxDeclKind& OutKind) const
{
	EnsureIndex();
	if (const FString* Key = ExporterOf.Find(Name))
	{
		OutKind = ExportIndex[Name];
		return *Key;
	}
	return FString();
}

FString FUetkxFsResolver::SuggestSpecifier(const FString& ImporterPath, const FString& Key) const
{
	const FString ImpDir = FPaths::GetPath(ImporterAbs(ImporterPath));
	FString Rel = Key;
	FPaths::MakePathRelativeTo(Rel, *(ImpDir + TEXT("/")));
	Rel.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (Rel.EndsWith(TEXT(".uetkx")))
	{
		Rel = Rel.LeftChop(6);
	}
	if (!Rel.StartsWith(TEXT("./")) && !Rel.StartsWith(TEXT("../")))
	{
		Rel = TEXT("./") + Rel;
	}
	return Rel;
}

// ── FUetkxResolve::Apply ────────────────────────────────────────────────────────────────────

void FUetkxResolve::Apply(const FUetkxFileScanResult& Scan, const TMap<FString, int32>& UseAts,
						  const FString& ImporterPath, const IUetkxImportResolver& Resolver,
						  TArray<FUetkxDiag>& Diags, TMap<FString, uint32>& DepHashes)
{
	auto Add = [&Diags](const TCHAR* Code, int32 Sev, const FString& Msg, int32 At, int32 Len = 1)
	{
		FUetkxDiag D;
		D.Code = Code;
		D.Severity = Sev;
		D.Message = Msg;
		D.Offset = At;
		D.Length = Len;
		Diags.Add(MoveTemp(D));
	};

	TSet<FString> SameFile;
	for (const FUetkxComponentDecl& D : Scan.Components) { SameFile.Add(D.Name); }
	for (const FUetkxHookDecl& D : Scan.Hooks) { SameFile.Add(D.Name); }
	for (const FUetkxModuleDecl& D : Scan.Modules) { SameFile.Add(D.Name); }

	// 1. Resolve imports → the imported-name table + per-name diagnostics.
	TSet<FString> ImportedNames;
	for (const FUetkxImportDecl& Imp : Scan.Imports)
	{
		const FString Key = Resolver.Resolve(Imp.Specifier, ImporterPath);
		if (Key.IsEmpty())
		{
			Add(TEXT("UETKX2300"), 0,
				FString::Printf(TEXT("unknown import specifier `%s` — no file at %s(.uetkx)"), *Imp.Specifier,
								*Imp.Specifier),
				Imp.SpecifierAt, FMath::Max(1, Imp.Specifier.Len() + 2));
			// Record the unresolved specifier so a future file appearing there flips staleness (M8).
			DepHashes.Add(Imp.Specifier, 0);
			continue;
		}
		const FString Label = Resolver.LabelForKey(Key);
		DepHashes.Add(Label, Resolver.ExportHashOf(Key));
		if (Resolver.CrossesModuleBoundary(ImporterPath, Key))
		{
			Add(TEXT("UETKX2308"), 0,
				FString::Printf(
					TEXT("import crosses a module/root boundary (%s -> %s) — imports are module-scoped in v1"),
					*ImporterPath, *Label),
				Imp.At);
			continue;
		}
		TMap<FString, FUetkxTargetDecl> Decls;
		Resolver.GetDecls(Key, Decls);
		for (int32 n = 0; n < Imp.Names.Num(); ++n)
		{
			const FString& Name = Imp.Names[n];
			const int32 NameAt = Imp.NameAts[n];
			ImportedNames.Add(Name);
			const FUetkxTargetDecl* T = Decls.Find(Name);
			if (!T)
			{
				Add(TEXT("UETKX2302"), 0, FString::Printf(TEXT("`%s` is not declared in %s"), *Name, *Label), NameAt,
					Name.Len());
			}
			else if (!T->bExported)
			{
				Add(TEXT("UETKX2301"), 0,
					FString::Printf(TEXT("`%s` is not exported by %s — add `export` to its declaration"), *Name, *Label),
					NameAt, Name.Len());
			}
		}
	}

	// 2. Strict usage (2305/2307): every referenced markup tag / user hook call / module qual must
	// be imported or same-file. Reference set (with offsets) = component tags (from the emitter),
	// bare Use<Upper>( calls, and `Ident::` module quals.
	TSet<FString> Referenced;
	auto Emit2305 = [&](const FString& Name, const FString& Owner, int32 At)
	{
		Add(TEXT("UETKX2305"), 0,
			FString::Printf(TEXT("`%s` is defined in %s but not imported — add: import { %s } from \"%s\""), *Name,
							*Resolver.LabelForKey(Owner), *Name, *Resolver.SuggestSpecifier(ImporterPath, Owner)),
			At, Name.Len());
	};
	// Component TAGS are markup-owned: a tag that is not a host element, not same-file, and not
	// imported MUST resolve to an exported component (2305) or it is broken (2307) — a markup tag
	// has no ambient/hand-written escape hatch.
	for (const TPair<FString, int32>& Tag : UseAts)
	{
		Referenced.Add(Tag.Key);
		if (SameFile.Contains(Tag.Key) || ImportedNames.Contains(Tag.Key))
		{
			continue;
		}
		EUetkxDeclKind Kind;
		const FString Owner = Resolver.FindExporter(Tag.Key, Kind);
		if (!Owner.IsEmpty())
		{
			Emit2305(Tag.Key, Owner, Tag.Value);
		}
		else
		{
			Add(TEXT("UETKX2307"), 0,
				FString::Printf(TEXT("`%s` is used like a uetkx component/hook but no file exports it"), *Tag.Key),
				Tag.Value, Tag.Key.Len());
		}
	}
	// User hook calls + module quals police EXPORT-TABLE names only (A4): a `Use<Upper>(` call or
	// `Ident::` qual whose name is exported by a .uetkx file but not imported here is 2305; a name
	// exported by NO file is ambient (a hand-written header hook / engine namespace) and never
	// diagnoses — imports address .uetkx targets only.
	TArray<FExtRef> Refs;
	for (const FUetkxComponentDecl& D : Scan.Components) { CollectExternalRefs(D.Setup, D.SetupAt, Refs); }
	for (const FUetkxHookDecl& D : Scan.Hooks) { CollectExternalRefs(D.Body, D.BodyAt, Refs); }
	for (const FUetkxModuleDecl& D : Scan.Modules) { CollectExternalRefs(D.Body, D.BodyAt, Refs); }
	for (const FExtRef& R : Refs)
	{
		// A referenced name is USED (for 2304) regardless of whether it resolves — an imported
		// name that appears here is not unused even if it turns out private/ambient.
		Referenced.Add(R.Name);
		EUetkxDeclKind Kind;
		const FString Owner = Resolver.FindExporter(R.Name, Kind);
		if (Owner.IsEmpty())
		{
			continue; // exported by no file → ambient (hand-written), not policed
		}
		const EUetkxDeclKind Want = R.Kind == FExtRef::EKind::Hook ? EUetkxDeclKind::Hook : EUetkxDeclKind::Module;
		if (Kind != Want)
		{
			continue; // shape mismatch (e.g. Ident:: that names a component) → not this reference
		}
		if (SameFile.Contains(R.Name) || ImportedNames.Contains(R.Name))
		{
			continue;
		}
		Emit2305(R.Name, Owner, R.At);
	}

	// 3. Unused imports (2304, warn): an imported name never referenced.
	for (const FUetkxImportDecl& Imp : Scan.Imports)
	{
		for (int32 n = 0; n < Imp.Names.Num(); ++n)
		{
			if (!Referenced.Contains(Imp.Names[n]))
			{
				Add(TEXT("UETKX2304"), 1, FString::Printf(TEXT("unused import `%s`"), *Imp.Names[n]), Imp.NameAts[n],
					Imp.Names[n].Len());
			}
		}
	}
}

void FUetkxResolve::MissingImports(const FUetkxFileScanResult& Scan, const TSet<FString>& Tags,
								   const FString& ImporterPath, const IUetkxImportResolver& Resolver,
								   TMap<FString, TArray<FString>>& OutBySpecifier, TArray<FString>& OutCrossModule)
{
	TSet<FString> SameFile;
	for (const FUetkxComponentDecl& D : Scan.Components) { SameFile.Add(D.Name); }
	for (const FUetkxHookDecl& D : Scan.Hooks) { SameFile.Add(D.Name); }
	for (const FUetkxModuleDecl& D : Scan.Modules) { SameFile.Add(D.Name); }
	TSet<FString> AlreadyImported;
	for (const FUetkxImportDecl& Imp : Scan.Imports)
	{
		for (const FString& Name : Imp.Names)
		{
			AlreadyImported.Add(Name);
		}
	}
	TSet<FString> Handled;
	auto Consider = [&](const FString& Name, bool bModuleQual)
	{
		if (SameFile.Contains(Name) || AlreadyImported.Contains(Name) || Handled.Contains(Name))
		{
			return;
		}
		EUetkxDeclKind Kind;
		const FString Owner = Resolver.FindExporter(Name, Kind);
		if (Owner.IsEmpty())
		{
			return; // exported by no file — the codemod cannot fix it (2307 stays for the author)
		}
		if (bModuleQual && Kind != EUetkxDeclKind::Module)
		{
			return; // a hand-written C++ namespace qual, not a module — ambient (A4)
		}
		Handled.Add(Name);
		if (Resolver.CrossesModuleBoundary(ImporterPath, Owner))
		{
			OutCrossModule.Add(FString::Printf(TEXT("%s -> %s"), *Name, *Resolver.LabelForKey(Owner)));
			return; // never auto-written (2308) — surfaces the real ownership decision
		}
		OutBySpecifier.FindOrAdd(Resolver.SuggestSpecifier(ImporterPath, Owner)).AddUnique(Name);
	};
	for (const FString& Tag : Tags)
	{
		Consider(Tag, false);
	}
	TArray<FExtRef> Refs;
	for (const FUetkxComponentDecl& D : Scan.Components) { CollectExternalRefs(D.Setup, D.SetupAt, Refs); }
	for (const FUetkxHookDecl& D : Scan.Hooks) { CollectExternalRefs(D.Body, D.BodyAt, Refs); }
	for (const FUetkxModuleDecl& D : Scan.Modules) { CollectExternalRefs(D.Body, D.BodyAt, Refs); }
	for (const FExtRef& R : Refs)
	{
		Consider(R.Name, R.Kind == FExtRef::EKind::Module);
	}
	for (TPair<FString, TArray<FString>>& Pair : OutBySpecifier)
	{
		Pair.Value.Sort();
	}
}
