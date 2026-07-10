// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// The markup lexer + parser (the SINGLE grammar implementation -- Toolchain consumes it) and
// the dev-loop interpreter: AST walk emitting builder calls, the restricted expression VM,
// the interp registry, HMR wiring (Phases 3-4). Runtime-type module EXCLUDED from Shipping
// via the .uplugin module descriptor "TargetConfigurationDenyList": ["Shipping"] (D-27) --
// there is no module Type for this. Linkage rule that creates: always-compiled modules must
// never statically reference Interp symbols; Interp self-registers into Core registries from
// StartupModule, and any direct cross-module use is gated behind a WITH_RUI_INTERP define set
// in the DEPENDENT module's Build.cs from (Target.Configuration != Shipping).
// Deps added with the code, per D-27: ReactiveUICore.
public class ReactiveUIInterp : ModuleRules
{
	public ReactiveUIInterp(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});
	}
}