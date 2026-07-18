// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxResolve.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UetkxConfig.h"
#include "UetkxJsxScan.h"
#include "UetkxLexer.h"
#include "UetkxMarkup.h"
#include "UetkxScopedLocals.h"

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
		switch (K)
		{
		case EUetkxDeclKind::Component:
			return TEXT("component");
		case EUetkxDeclKind::Hook:
			return TEXT("hook");
		case EUetkxDeclKind::Module:
			return TEXT("module");
		case EUetkxDeclKind::Value:
			return TEXT("value");
		case EUetkxDeclKind::Util:
			return TEXT("util");
		}
		return TEXT("module");
	}

	bool IsIdentStart(int32 C)
	{
		return C == '_' || (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z');
	}

	/** External-reference kinds a verbatim region can carry (setup / hook / module bodies).
	 *  ES-modules (M4): Call (any non-Use plain call — a UTIL reference candidate) and Bare (a
	 *  plain identifier — a VALUE reference candidate) join the set; both are only ever policed
	 *  against the export table (a name no .uetkx exports stays ambient, exactly like hooks/
	 *  modules — A4), so ordinary locals/engine names never diagnose. Module refs additionally
	 *  carry the MEMBER after `::` so a namespace-alias member (`X::Cool`) can validate against
	 *  the target's export table. */
	struct FExtRef
	{
		enum class EKind
		{
			Hook,
			Module,
			Call,
			Bare
		};
		FString Name;
		FString Member; // Module refs only: the identifier after `::` ("" when absent)
		int32 At = -1;
		EKind Kind = EKind::Hook;
	};

	void CollectMarkupNodeRefs(const FUetkxNode& Node, int32 Base, int32 FallbackAt, const TArray<FString>& IslandSeed,
							   TArray<FExtRef>& Out);

	/** Scan a verbatim C++ region for bare `Use<Upper>(` calls (user hooks) and leftmost `Ident::`
	 *  scope quals (module references), respecting comments/strings and member/scope prefixes — the
	 *  PrefixHookCalls classification (so hand-header namespaces like `RuiDemo::X` are found here as
	 *  Module refs but filtered out later when no file exports the name). BaseAt = the region's
	 *  absolute offset in the original source; when it is UNKNOWN pass `bExactBase = false` and a
	 *  fallback position — every ref then lands at BaseAt itself (the hook validator's fallback
	 *  semantics). TD-034 #2 (N4): `ParamSeed` seeds the scope tracker — a NAME with a local
	 *  declaration in scope is a body local, never an external reference. TD-034 #3 (N5): the
	 *  region is markup-aware — value-markup ranges are CODE ISLANDS skipped by the C++ scan and
	 *  walked as parsed trees (attr/child expressions and directive code join the reference set;
	 *  text children never do, N-08); `IslandSeed` is the conservative locals union those islands
	 *  see (null = reuse ParamSeed). */
	void CollectExternalRefs(const FString& Region, int32 BaseAt, const TArray<FString>& ParamSeed,
							 TArray<FExtRef>& Out, bool bExactBase = true, const TArray<FString>* IslandSeed = nullptr)
	{
		const TArray<int32> Src = FUetkxLexer::ToCodePoints(Region);
		TArray<FUetkxMarkupRange> Ranges = FUetkxJsxScan::FindMarkupRanges(Src, 0, Src.Num());
		for (FUetkxMarkupRange& R : Ranges)
		{
			if (R.End == -1)
			{
				R.End = Src.Num();
			}
		}
		const FUetkxScopedLocals Locals(Src, ParamSeed, &Ranges);
		const int32 N = Src.Num();
		int32 i = 0;
		while (i < N)
		{
			bool bInRange = false;
			for (const FUetkxMarkupRange& R : Ranges)
			{
				if (i >= R.Start && i < R.End)
				{
					i = R.End;
					bInRange = true;
					break;
				}
			}
			if (bInRange)
			{
				continue;
			}
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
				// Only a real `::` scope qualifier (a SECOND colon precedes) blocks a fresh reference; a
				// lone `:` is a ternary / label / case and must NOT drop the ident after it (bughunt
				// IMPORT-3 — e.g. `cond ? UseA() : UseB()` would lose the UseB reference, then the codemod
				// blesses un-buildable output because the needed import is never added).
				bScope = (P == ':') && k > 0 && Src[k - 1] == ':';
				break;
			}
			if (bMember || bScope)
			{
				continue;
			}
			// TD-034 #2 (N4): a body local (param, `Type Name = …`, structured binding) shadowing an
			// exported name is NOT an external reference — no 2305, no codemod import.
			if (Locals.IsLocal(Ident, s))
			{
				continue;
			}
			// following non-ws: `::` (module qual) or `(` (call); anything else is a bare reference.
			int32 p = i;
			while (p < N && (Src[p] == ' ' || Src[p] == '\t'))
			{
				++p;
			}
			const bool bFollowScope = (p + 1 < N && Src[p] == ':' && Src[p + 1] == ':');
			const bool bFollowParen = (p < N && Src[p] == '(');
			if (bFollowScope)
			{
				// ES-modules (M4): capture the member after `::` for namespace-alias validation.
				int32 m = p + 2;
				while (m < N && (Src[m] == ' ' || Src[m] == '\t'))
				{
					++m;
				}
				const int32 Ms = m;
				while (m < N && FUetkxLexer::IsIdentCode(Src[m]))
				{
					++m;
				}
				Out.Add({Ident, FUetkxLexer::FromCodePoints(Src, Ms, m - Ms), bExactBase ? BaseAt + s : BaseAt,
						 FExtRef::EKind::Module});
			}
			else if (bFollowParen && Ident.StartsWith(TEXT("Use")) && Ident.Len() >= 4 && FChar::IsUpper(Ident[3]) &&
					 !FUetkxFileScan::HookNames().Contains(Ident))
			{
				Out.Add({Ident, FString(), bExactBase ? BaseAt + s : BaseAt, FExtRef::EKind::Hook});
			}
			else if (bFollowParen)
			{
				Out.Add({Ident, FString(), bExactBase ? BaseAt + s : BaseAt, FExtRef::EKind::Call});
			}
			else
			{
				Out.Add({Ident, FString(), bExactBase ? BaseAt + s : BaseAt, FExtRef::EKind::Bare});
			}
		}
		// N5: the skipped markup ranges are parsed and walked as trees (parse errors are the
		// emitter's business — UETKX2508 family; a broken window contributes no references).
		const TArray<FString>& Islands = IslandSeed ? *IslandSeed : ParamSeed;
		for (const FUetkxMarkupRange& R : Ranges)
		{
			FUetkxMarkup Parser;
			FUetkxParseResult Pr = Parser.Parse(Src, R.Start, R.End);
			if (!Pr.IsOk())
			{
				continue;
			}
			for (const TSharedPtr<FUetkxNode>& Nd : Pr.Nodes)
			{
				// Node.At/Vat are REGION-buffer offsets (Parse ran over Src whole) — the frame
				// anchor is the region's own base, exactly like ScanMarkupCodeForHooks.
				if (Nd.IsValid())
				{
					CollectMarkupNodeRefs(*Nd, bExactBase ? BaseAt : -1, BaseAt, Islands, Out);
				}
			}
		}
	}

	/** A directive body — statements + `return ( <markup> )` (family 0.7): collect from the lead
	 *  code (markup-aware), then every root of the return window. Mirrors
	 *  ValidateDirectiveBodyHooks (UetkxFileScan.cpp) exactly. */
	void CollectDirectiveBodyRefs(const FString& BodyMarkup, int32 Base, int32 FallbackAt,
								  const TArray<FString>& IslandSeed, TArray<FExtRef>& Out)
	{
		const TArray<int32> Body = FUetkxLexer::ToCodePoints(BodyMarkup);
		const FUetkxSplitReturn Split = FUetkxFileScan::SplitMarkupReturn(Body, /*bRequireMarkupPeek*/ false);
		if (!Split.bOk)
		{
			CollectExternalRefs(BodyMarkup, Base < 0 ? FallbackAt : Base, IslandSeed, Out, Base >= 0, &IslandSeed);
			return;
		}
		const FString Lead = FUetkxLexer::FromCodePoints(Body, 0, Split.ReturnAt);
		CollectExternalRefs(Lead, Base < 0 ? FallbackAt : Base, IslandSeed, Out, Base >= 0, &IslandSeed);
		// the lead's own declarations are live inside the nested window (audit: a lead local
		// shadowing an exported name must not 2305 from an attr expression)
		const FUetkxScopedLocals LeadLocals(FUetkxLexer::ToCodePoints(Lead), IslandSeed);
		const TArray<FString> NestedSeed = LeadLocals.AllDeclNames();
		FUetkxMarkup Parser;
		FUetkxParseResult Pr = Parser.Parse(Body, Split.MStart, Split.MEnd);
		if (Pr.IsOk())
		{
			for (const TSharedPtr<FUetkxNode>& Nd : Pr.Nodes)
			{
				if (Nd.IsValid())
				{
					CollectMarkupNodeRefs(*Nd, Base < 0 ? -1 : Base, Base < 0 ? FallbackAt : Base, NestedSeed, Out);
				}
			}
		}
	}

	/** Walk one parsed markup node for external references (N5, TD-034 #3): attr/spread
	 *  expressions, expression children, directive headers and bodies — the exact recursion of
	 *  ValidateMarkupNodeHooks (UetkxFileScan.cpp), emitting FExtRefs instead of diags. Tags are
	 *  NOT emitted here (the Tags set polices them); text/comment children are never scanned
	 *  (N-08). `Base` = the absolute anchor of this tree's offset frame (Node.At/Vat/BodyAt
	 *  compose on top), or -1 when detached (refs fall back to FallbackAt). */
	void CollectMarkupNodeRefs(const FUetkxNode& Node, int32 Base, int32 FallbackAt, const TArray<FString>& IslandSeed,
							   TArray<FExtRef>& Out)
	{
		const int32 NodeAt = Base < 0 ? FallbackAt : Base + Node.At;
		switch (Node.Type)
		{
		case EUetkxNodeType::El:
		case EUetkxNodeType::Frag:
			for (const FUetkxAttr& Attr : Node.Attrs)
			{
				if (Attr.Kind != EUetkxAttrKind::Expr && Attr.Kind != EUetkxAttrKind::Spread)
				{
					continue;
				}
				const bool bExact = Base >= 0 && Attr.Vat >= 0;
				CollectExternalRefs(Attr.Value, bExact ? Base + Attr.Vat : NodeAt, IslandSeed, Out, bExact,
									&IslandSeed);
			}
			for (const TSharedPtr<FUetkxNode>& Child : Node.Children)
			{
				if (Child.IsValid())
				{
					CollectMarkupNodeRefs(*Child, Base, NodeAt, IslandSeed, Out);
				}
			}
			break;
		case EUetkxNodeType::Expr:
		{
			const bool bExact = Base >= 0 && Node.Vat >= 0;
			CollectExternalRefs(Node.Code, bExact ? Base + Node.Vat : NodeAt, IslandSeed, Out, bExact, &IslandSeed);
			break;
		}
		case EUetkxNodeType::If:
			for (const FUetkxIfBranch& Branch : Node.Branches)
			{
				CollectExternalRefs(Branch.Cond, NodeAt, IslandSeed, Out, /*bExactBase*/ false, &IslandSeed);
				CollectDirectiveBodyRefs(Branch.BodyMarkup, Base < 0 || Branch.BodyAt < 0 ? -1 : Base + Branch.BodyAt,
										 NodeAt, IslandSeed, Out);
			}
			if (Node.ElseBody.IsSet())
			{
				CollectDirectiveBodyRefs(Node.ElseBody.GetValue(),
										 Base < 0 || Node.ElseBodyAt < 0 ? -1 : Base + Node.ElseBodyAt, NodeAt,
										 IslandSeed, Out);
			}
			break;
		case EUetkxNodeType::For:
		case EUetkxNodeType::While:
		{
			CollectExternalRefs(Node.Header, NodeAt, IslandSeed, Out, /*bExactBase*/ false, &IslandSeed);
			// The header's own declarations (`@for (int32 i = 0; …)`) are live inside the
			// body — append them to the body's island seed (the N5 loop-var pin).
			const FUetkxScopedLocals HeaderLocals(FUetkxLexer::ToCodePoints(Node.Header), IslandSeed);
			const TArray<FString> BodySeed = HeaderLocals.AllDeclNames();
			CollectDirectiveBodyRefs(Node.BodyMarkup, Base < 0 || Node.BodyAt < 0 ? -1 : Base + Node.BodyAt, NodeAt,
									 BodySeed, Out);
			break;
		}
		case EUetkxNodeType::Match:
			CollectExternalRefs(Node.Subject, NodeAt, IslandSeed, Out, /*bExactBase*/ false, &IslandSeed);
			for (const FUetkxMatchCase& Case : Node.Cases)
			{
				CollectExternalRefs(Case.Value, NodeAt, IslandSeed, Out, /*bExactBase*/ false, &IslandSeed);
				CollectDirectiveBodyRefs(Case.BodyMarkup, Base < 0 || Case.BodyAt < 0 ? -1 : Base + Case.BodyAt, NodeAt,
										 IslandSeed, Out);
			}
			if (Node.DefaultBody.IsSet())
			{
				CollectDirectiveBodyRefs(Node.DefaultBody.GetValue(),
										 Base < 0 || Node.DefaultBodyAt < 0 ? -1 : Base + Node.DefaultBodyAt, NodeAt,
										 IslandSeed, Out);
			}
			break;
		default:
			break;
		}
	}
} // namespace

uint32 FUetkxResolve::ExportHash(const FUetkxPreambleScan& Scan)
{
	TArray<FString> Lines;
	for (const FUetkxPreambleDecl& D : Scan.Decls)
	{
		Lines.Add(FString::Printf(TEXT("%s|%s|%d"), *D.Name, KindWord(D.Kind), D.bExported ? 1 : 0));
	}
	// ES-modules (U-08): the default-export marker is part of the export shape — changing or
	// re-targeting `export default` re-stales every importer (their default aliases re-bind).
	if (!Scan.DefaultExportName.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("%s|default|1"), *Scan.DefaultExportName));
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

// ── FUetkxFsResolver ────────────────────────────────────────────────────────────────────────

FUetkxFsResolver::FUetkxFsResolver(const FString& InBaseDir, const TArray<FString>& InSearchRoots, bool bInFixtureMode)
	: BaseDir(NormDir(InBaseDir)), SearchRoots(InSearchRoots), bFixtureMode(bInFixtureMode)
{
}

FString FUetkxFsResolver::ImporterAbs(const FString& ImporterPath) const
{
	return NormAbs(FPaths::Combine(BaseDir, ImporterPath));
}

FString FUetkxFsResolver::ComputeTargetPath(const FString& Spec, const FString& ImporterPath) const
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
	return NormAbs(Target);
}

FString FUetkxFsResolver::Resolve(const FString& Spec, const FString& ImporterPath) const
{
	const FString Target = ComputeTargetPath(Spec, ImporterPath);
	return (!Target.IsEmpty() && IFileManager::Get().FileExists(*Target)) ? Target : FString();
}

FString FUetkxFsResolver::WouldBeLabel(const FString& Spec, const FString& ImporterPath) const
{
	const FString Target = ComputeTargetPath(Spec, ImporterPath);
	return Target.IsEmpty() ? FString() : LabelForKey(Target);
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
	Entry.Hash = FUetkxResolve::ExportHash(Entry.Scan);
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

FString FUetkxFsResolver::DefaultExportOf(const FString& Key) const
{
	const FUetkxPreambleScan* Scan = CachedScan(Key);
	return Scan ? Scan->DefaultExportName : FString();
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
	return Scan ? FUetkxResolve::ExportHash(*Scan) : 0;
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
		// Deterministic "first exporter wins": FindFilesRecursive returns an OS-dependent order, so a
		// duplicate-export tie-break would differ across machines/CI without a stable sort (bughunt
		// RESOLVE-1). The codemod's chosen specifier must be reproducible.
		Files.Sort();
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
						  const FString& ImporterPath, const IUetkxImportResolver& Resolver, TArray<FUetkxDiag>& Diags,
						  TMap<FString, uint32>& DepHashes)
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
	for (const FUetkxComponentDecl& D : Scan.Components)
	{
		SameFile.Add(D.Name);
	}
	for (const FUetkxHookDecl& D : Scan.Hooks)
	{
		SameFile.Add(D.Name);
	}
	for (const FUetkxModuleDecl& D : Scan.Modules)
	{
		SameFile.Add(D.Name);
	}
	for (const FUetkxValueDecl& D : Scan.Values)
	{
		SameFile.Add(D.Name);
	}
	for (const FUetkxUtilDecl& D : Scan.Utils)
	{
		SameFile.Add(D.Name);
	}

	// 1. Resolve imports → the imported-binding table + per-name diagnostics. ImportedNames holds
	// LOCAL bindings (what tags/code are spelled with — a renamed import binds its alias, `* as`
	// binds the namespace alias, default binds the default alias); the 2301/2302 validation loop
	// checks TARGET names (what the target file must export) — the ES-modules split (G-05/M4).
	TSet<FString> ImportedNames;
	// ES-modules (G-05): `* as X` targets — alias -> the target's decl table + label, so `X::M`
	// member references validate against the real export surface.
	TMap<FString, TMap<FString, FUetkxTargetDecl>> NamespaceTargets;
	TMap<FString, FString> NamespaceLabels;
	for (const FUetkxImportDecl& Imp : Scan.Imports)
	{
		// Host includes (`import "@Header.h"`, INCLUDE_RETIREMENT_PLAN.md §B) resolve to no
		// .uetkx file — Specifier is a header path, not a peer-file specifier. Skip entirely;
		// their empty Names array already no-ops every OTHER pass in this file.
		if (Imp.bHostInclude)
		{
			continue;
		}
		const FString Key = Resolver.Resolve(Imp.Specifier, ImporterPath);
		if (Key.IsEmpty())
		{
			Add(TEXT("UETKX2300"), 0,
				FString::Printf(TEXT("unknown import specifier `%s` — no file at %s(.uetkx)"), *Imp.Specifier,
								*Imp.Specifier),
				Imp.SpecifierAt, FMath::Max(1, Imp.Specifier.Len() + 2));
			// Record the unresolved dep under the WOULD-BE label (the project-relative path the specifier
			// maps to) — the SAME key space resolved deps use — so DepsChanged can reconstruct it and a
			// future file appearing there flips staleness (M8 / bughunt DRV-2 / IMPORT-1). The raw
			// specifier was unreconstructable (DepsChanged combined it against the project root, missing
			// both the importer-relative anchor and the `.uetkx` suffix). Fall back to the specifier only
			// for a forbidden/anchorless form, which can never resolve anyway.
			const FString WouldBe = Resolver.WouldBeLabel(Imp.Specifier, ImporterPath);
			DepHashes.Add(WouldBe.IsEmpty() ? Imp.Specifier : WouldBe, 0);
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
			// These bindings ARE imported (2308 is the resolution to fix), so mark them imported —
			// otherwise the strict-usage pass ALSO emits a contradictory 2305 "not imported, add this
			// import" for the exact binding that is already imported (bughunt IMPORT-2).
			for (int32 n = 0; n < Imp.Names.Num(); ++n)
			{
				ImportedNames.Add(Imp.LocalNames.IsValidIndex(n) ? Imp.LocalNames[n] : Imp.Names[n]);
			}
			if (Imp.bNamespace)
			{
				ImportedNames.Add(Imp.NamespaceAlias);
			}
			if (Imp.bDefault)
			{
				ImportedNames.Add(Imp.DefaultAlias);
			}
			continue;
		}
		TMap<FString, FUetkxTargetDecl> Decls;
		Resolver.GetDecls(Key, Decls);
		if (Imp.bNamespace)
		{
			ImportedNames.Add(Imp.NamespaceAlias);
			NamespaceLabels.Add(Imp.NamespaceAlias, Label);
			NamespaceTargets.Add(Imp.NamespaceAlias, MoveTemp(Decls));
			continue;
		}
		if (Imp.bDefault)
		{
			ImportedNames.Add(Imp.DefaultAlias);
			if (Resolver.DefaultExportOf(Key).IsEmpty())
			{
				Add(TEXT("UETKX2326"), 0,
					FString::Printf(TEXT("%s has no default export — use a named import: import { ... } from \"%s\""),
									*Label, *Imp.Specifier),
					Imp.DefaultAliasAt, Imp.DefaultAlias.Len());
			}
			continue;
		}
		for (int32 n = 0; n < Imp.Names.Num(); ++n)
		{
			const FString& Name = Imp.Names[n];
			const int32 NameAt = Imp.NameAts[n];
			ImportedNames.Add(Imp.LocalNames.IsValidIndex(n) ? Imp.LocalNames[n] : Name);
			const FUetkxTargetDecl* T = Decls.Find(Name);
			if (!T)
			{
				Add(TEXT("UETKX2302"), 0, FString::Printf(TEXT("`%s` is not declared in %s"), *Name, *Label), NameAt,
					Name.Len());
			}
			else if (!T->bExported)
			{
				Add(TEXT("UETKX2301"), 0,
					FString::Printf(TEXT("`%s` is not exported by %s — add `export` to its declaration"), *Name,
									*Label),
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
	// User hook calls + module quals + plain calls (utils) + bare identifiers (values) police
	// EXPORT-TABLE names only (A4): a reference whose name is exported by a .uetkx file (with the
	// MATCHING kind) but not imported here is 2305; a name exported by NO file is ambient (a
	// hand-written header hook / engine namespace / a body local) and never diagnoses — imports
	// address .uetkx targets only. ES-modules (M4): value initializers + util bodies join the
	// scanned regions; `X::M` refs through a namespace alias validate the MEMBER against the
	// target's export table (2301/2302, qualified spelling).
	TArray<FExtRef> Refs;
	for (const FUetkxComponentDecl& D : Scan.Components)
	{
		TArray<FString> Seed;
		for (const FUetkxParam& P : D.Params)
		{
			Seed.Add(P.Name);
		}
		// N5 (TD-034 #3): scan the WHOLE body, markup-aware — return windows and value-markup
		// ranges are parsed and their attr/child expressions + directive code join the reference
		// set. Islands see the conservative locals union (setup locals are live inside markup).
		const TArray<int32> BodyCp = FUetkxLexer::ToCodePoints(D.Body);
		TArray<FUetkxMarkupRange> BodyRanges = FUetkxJsxScan::FindMarkupRanges(BodyCp, 0, BodyCp.Num());
		const FUetkxScopedLocals BodyLocals(BodyCp, Seed, &BodyRanges);
		const TArray<FString> IslandSeed = BodyLocals.AllDeclNames();
		CollectExternalRefs(D.Body, D.BodyAt, Seed, Refs, /*bExactBase*/ true, &IslandSeed);
	}
	for (const FUetkxHookDecl& D : Scan.Hooks)
	{
		CollectExternalRefs(D.Body, D.BodyAt, FUetkxScopedLocals::ParamNamesOf(D.Params), Refs);
	}
	for (const FUetkxModuleDecl& D : Scan.Modules)
	{
		CollectExternalRefs(D.Body, D.BodyAt, {}, Refs);
	}
	for (const FUetkxValueDecl& D : Scan.Values)
	{
		CollectExternalRefs(D.Init, D.InitAt, {}, Refs);
	}
	for (const FUetkxUtilDecl& D : Scan.Utils)
	{
		CollectExternalRefs(D.Body, D.BodyAt, FUetkxScopedLocals::ParamNamesOf(D.Params), Refs);
	}
	for (const FExtRef& R : Refs)
	{
		// A referenced name is USED (for 2304) regardless of whether it resolves — an imported
		// name that appears here is not unused even if it turns out private/ambient.
		Referenced.Add(R.Name);
		// ES-modules (G-05): `X::M` through a namespace alias — validate the member against the
		// target's export table with the qualified spelling in the message.
		if (R.Kind == FExtRef::EKind::Module)
		{
			if (const TMap<FString, FUetkxTargetDecl>* Targets = NamespaceTargets.Find(R.Name))
			{
				const FString& Label = NamespaceLabels[R.Name];
				const FString Qualified = R.Name + TEXT("::") + R.Member;
				const FUetkxTargetDecl* T = R.Member.IsEmpty() ? nullptr : Targets->Find(R.Member);
				if (!T)
				{
					Add(TEXT("UETKX2302"), 0, FString::Printf(TEXT("`%s` is not declared in %s"), *Qualified, *Label),
						R.At, Qualified.Len());
				}
				else if (!T->bExported)
				{
					Add(TEXT("UETKX2301"), 0,
						FString::Printf(TEXT("`%s` is not exported by %s — add `export` to its declaration"),
										*Qualified, *Label),
						R.At, Qualified.Len());
				}
				continue;
			}
		}
		EUetkxDeclKind Kind;
		const FString Owner = Resolver.FindExporter(R.Name, Kind);
		if (Owner.IsEmpty())
		{
			continue; // exported by no file → ambient (hand-written), not policed
		}
		EUetkxDeclKind Want;
		switch (R.Kind)
		{
		case FExtRef::EKind::Hook:
			Want = EUetkxDeclKind::Hook;
			break;
		case FExtRef::EKind::Module:
			Want = EUetkxDeclKind::Module;
			break;
		case FExtRef::EKind::Call:
			Want = EUetkxDeclKind::Util;
			break;
		default:
			Want = EUetkxDeclKind::Value;
			break;
		}
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

	// 3. Unused imports (2304, warn): an imported LOCAL binding never referenced (ES-modules M4:
	// usage counts the local alias — what the author actually spells in tags/code).
	for (const FUetkxImportDecl& Imp : Scan.Imports)
	{
		if (Imp.bHostInclude)
		{
			continue;
		}
		if (Imp.bNamespace)
		{
			if (!Referenced.Contains(Imp.NamespaceAlias))
			{
				Add(TEXT("UETKX2304"), 1, FString::Printf(TEXT("unused import `%s`"), *Imp.NamespaceAlias),
					Imp.NamespaceAliasAt, Imp.NamespaceAlias.Len());
			}
			continue;
		}
		if (Imp.bDefault)
		{
			if (!Referenced.Contains(Imp.DefaultAlias))
			{
				Add(TEXT("UETKX2304"), 1, FString::Printf(TEXT("unused import `%s`"), *Imp.DefaultAlias),
					Imp.DefaultAliasAt, Imp.DefaultAlias.Len());
			}
			continue;
		}
		for (int32 n = 0; n < Imp.Names.Num(); ++n)
		{
			const FString& Local = Imp.LocalNames.IsValidIndex(n) ? Imp.LocalNames[n] : Imp.Names[n];
			if (!Referenced.Contains(Local))
			{
				Add(TEXT("UETKX2304"), 1, FString::Printf(TEXT("unused import `%s`"), *Local), Imp.NameAts[n],
					Local.Len());
			}
		}
	}
}

void FUetkxResolve::MissingImports(const FUetkxFileScanResult& Scan, const TSet<FString>& Tags,
								   const FString& ImporterPath, const IUetkxImportResolver& Resolver,
								   TMap<FString, TArray<FString>>& OutBySpecifier, TArray<FString>& OutCrossModule)
{
	TSet<FString> SameFile;
	for (const FUetkxComponentDecl& D : Scan.Components)
	{
		SameFile.Add(D.Name);
	}
	for (const FUetkxHookDecl& D : Scan.Hooks)
	{
		SameFile.Add(D.Name);
	}
	for (const FUetkxModuleDecl& D : Scan.Modules)
	{
		SameFile.Add(D.Name);
	}
	for (const FUetkxValueDecl& D : Scan.Values)
	{
		SameFile.Add(D.Name);
	}
	for (const FUetkxUtilDecl& D : Scan.Utils)
	{
		SameFile.Add(D.Name);
	}
	// Local bindings (ES-modules M4): a renamed import binds its alias; `* as`/default bind their
	// aliases — all suppress "missing import" for references spelled with them.
	TSet<FString> AlreadyImported;
	for (const FUetkxImportDecl& Imp : Scan.Imports)
	{
		for (int32 n = 0; n < Imp.Names.Num(); ++n)
		{
			AlreadyImported.Add(Imp.LocalNames.IsValidIndex(n) ? Imp.LocalNames[n] : Imp.Names[n]);
		}
		if (Imp.bNamespace)
		{
			AlreadyImported.Add(Imp.NamespaceAlias);
		}
		if (Imp.bDefault)
		{
			AlreadyImported.Add(Imp.DefaultAlias);
		}
	}
	TSet<FString> Handled;
	// RequiredKind: the export kind this reference shape can target (Component for tags via
	// INDEX_NONE sentinel — any kind was the historical tag behavior, kept).
	auto Consider = [&](const FString& Name, TOptional<EUetkxDeclKind> RequiredKind)
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
		if (RequiredKind.IsSet() && Kind != RequiredKind.GetValue())
		{
			return; // shape mismatch (e.g. a hand-written C++ namespace qual) — ambient (A4)
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
		Consider(Tag, TOptional<EUetkxDeclKind>());
	}
	TArray<FExtRef> Refs;
	for (const FUetkxComponentDecl& D : Scan.Components)
	{
		TArray<FString> Seed;
		for (const FUetkxParam& P : D.Params)
		{
			Seed.Add(P.Name);
		}
		// N5 (TD-034 #3): scan the WHOLE body, markup-aware — return windows and value-markup
		// ranges are parsed and their attr/child expressions + directive code join the reference
		// set. Islands see the conservative locals union (setup locals are live inside markup).
		const TArray<int32> BodyCp = FUetkxLexer::ToCodePoints(D.Body);
		TArray<FUetkxMarkupRange> BodyRanges = FUetkxJsxScan::FindMarkupRanges(BodyCp, 0, BodyCp.Num());
		const FUetkxScopedLocals BodyLocals(BodyCp, Seed, &BodyRanges);
		const TArray<FString> IslandSeed = BodyLocals.AllDeclNames();
		CollectExternalRefs(D.Body, D.BodyAt, Seed, Refs, /*bExactBase*/ true, &IslandSeed);
	}
	for (const FUetkxHookDecl& D : Scan.Hooks)
	{
		CollectExternalRefs(D.Body, D.BodyAt, FUetkxScopedLocals::ParamNamesOf(D.Params), Refs);
	}
	for (const FUetkxModuleDecl& D : Scan.Modules)
	{
		CollectExternalRefs(D.Body, D.BodyAt, {}, Refs);
	}
	for (const FUetkxValueDecl& D : Scan.Values)
	{
		CollectExternalRefs(D.Init, D.InitAt, {}, Refs);
	}
	for (const FUetkxUtilDecl& D : Scan.Utils)
	{
		CollectExternalRefs(D.Body, D.BodyAt, FUetkxScopedLocals::ParamNamesOf(D.Params), Refs);
	}
	for (const FExtRef& R : Refs)
	{
		switch (R.Kind)
		{
		case FExtRef::EKind::Hook:
			Consider(R.Name, EUetkxDeclKind::Hook);
			break;
		case FExtRef::EKind::Module:
			Consider(R.Name, EUetkxDeclKind::Module);
			break;
		case FExtRef::EKind::Call:
			Consider(R.Name, EUetkxDeclKind::Util);
			break;
		case FExtRef::EKind::Bare:
			Consider(R.Name, EUetkxDeclKind::Value);
			break;
		}
	}
	for (TPair<FString, TArray<FString>>& Pair : OutBySpecifier)
	{
		Pair.Value.Sort();
	}
}
