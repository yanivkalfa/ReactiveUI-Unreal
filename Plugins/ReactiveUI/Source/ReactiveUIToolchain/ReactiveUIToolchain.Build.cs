// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// UncookedOnly codegen authority (Phase 3): .uetkx -> committed .uetkx.inl + the stable
// <Module>.Uetkx.gen.cpp aggregators, the formatter, diagnostics sidecars (UETKX####, shared
// numbers with the family; UE-only codes from UETKX3000+), the compiler fingerprint, and all
// staleness machinery (D-18/D-19). Consumes the parser from ReactiveUIInterp -- never
// reimplements it (single grammar implementation).
// Deps added with the code, per D-27: ReactiveUICore, ReactiveUIInterp.
public class ReactiveUIToolchain : ModuleRules
{
	public ReactiveUIToolchain(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"ReactiveUICore",   // RUI:: registries referenced by generated-code shapes
			"ReactiveUIInterp", // the single grammar implementation (D-27)
		});
	}
}