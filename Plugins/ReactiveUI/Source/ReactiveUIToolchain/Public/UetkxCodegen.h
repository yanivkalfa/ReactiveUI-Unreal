// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxCodegen — the single codegen authority (D-18): compiles .uetkx source into the
// committed sibling `Foo.uetkx.inl` — reflection-free C++ (no UCLASS/UPROPERTY, D-19.2):
// a typed props struct, the component function (setup spliced VERBATIM with hooks
// auto-prefixed to Ctx.*), the markup lowered to the D-33 builder vocabulary, the baked
// __RUI_HOOK_SIG constant (state-reset detection, Phase 4), RUI::RegisterComponentId, and
// an inline wrapper `FRuiNode <Name>(Props, Children, Key)` for cross-component references.
// FText string literals emit as NSLOCTEXT (self-namespaced per file — D-32).

#pragma once

#include "CoreMinimal.h"
#include "UetkxFileScan.h"

class IUetkxImportResolver; // UetkxResolve.h (M4) — resolution runs inside CompileSource when set

struct REACTIVEUITOOLCHAIN_API FUetkxCompileOutput
{
	bool bOk = false;
	FString Inl; // the generated .uetkx.inl text ("" on failure)
	TArray<FUetkxDiag> Diags;
	TArray<FString> ComponentNames;	 // ALL decl bindings (component/hook/module names) -> this file; refs
	TArray<FString> ExportedNames;	 // EXPORTED decl names only — the UETKX2106 global-uniqueness ledger (A5e)
	TArray<FString> Uses;			 // component names REFERENCED by markup (aggregator topo order)
	bool bSupportFile = false;		 // no markup (only hooks/modules) — HMR rebuild, not interp swap
	uint32 HookSig = 0;				 // first component's hook signature (interp swap key)
	uint32 ExportHash = 0;			 // FNV over this file's exported decl shapes — reverse staleness (M8)
	TMap<FString, uint32> DepHashes; // resolved import label -> its export_hash (staleness graph, M8)
	FString DefaultExportName;		 // ES-modules (U-08): `export default <Name>` target ("" = none)
};

class REACTIVEUITOOLCHAIN_API FUetkxCodegen
{
public:
	/** Compile one .uetkx source into its sibling .inl. Basename = file stem (binding + NSLOCTEXT
	 *  namespace). ProjectRelPath = the source path relative to the project (forward slashes) for
	 *  `#line` mapping (M7); "" = the fixtures/tests default (Basename + ".uetkx"). Resolver, when
	 *  non-null (driver/fixture harness), runs import resolution + strict diagnostics AFTER scan
	 *  and BEFORE emit (M4); null = resolution skipped (syntax-only). Declarations lower in SOURCE
	 *  order (mixed-decl v1): hook -> inline free function, module -> namespace, component ->
	 *  struct + impl + wrapper. */
	static FUetkxCompileOutput CompileSource(const FString& Source, const FString& Basename,
											 const FString& ProjectRelPath = FString(),
											 const IUetkxImportResolver* Resolver = nullptr,
											 TOptional<bool> bSellerRepoOverride = TOptional<bool>());

	/** D-32(a) context-aware generated-code header. TRUE only inside ReactiveUI's OWN repo (the seller's
	 *  monorepo, marked by a `.rui-seller-repo` sentinel at the project root that never ships in the Fab
	 *  package) — there generated code carries the seller copyright. In a CUSTOMER project the sentinel
	 *  is absent, so generated code carries the neutral "belongs to your project" banner. */
	static bool IsSellerRepo();

	/** The generated-file copyright/attribution header line for `Basename` (seller vs neutral per
	 *  IsSellerRepo / the explicit override). Includes the trailing newline. */
	static FString GeneratedCopyrightLine(const FString& Basename,
										  TOptional<bool> bSellerRepoOverride = TOptional<bool>());

	/** The markup vocabulary as JSON — elements/attrs (typed), style keys, slot keys, hooks.
	 *  RUIExportSchema writes this to Saved/ReactiveUI/schema.json for the LSP (Phase 5). */
	static FString ExportSchemaJson();
};
