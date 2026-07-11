// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// Editor-only surface (Phases 3-4, 8): the compile-on-save watcher (three triggers + busy
// guard/deadman + MessageLog "ReactiveUI" de-dup), RUICompile/RUIExportSchema/RUIContractDump
// commandlets, .uetkx asset actions (browser visibility, New Component, open-external), and
// later the Inspector tab. Deps added with the code, per D-27:
//   CoreUObject, Engine, UnrealEd, DirectoryWatcher, Projects, ReactiveUICore,
//   ReactiveUIToolchain, ReactiveUIInterp.
public class ReactiveUIEditor : ModuleRules
{
	public ReactiveUIEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",		 // commandlet UCLASSes
			"Engine",			 // UCommandlet base
			"Projects",			 // sweep roots
			"ReactiveUIInterp",	 // diagnostics types (via the toolchain's public surface)
			"ReactiveUIToolchain", // FUetkxDriver / FUetkxCodegen
		});
	}
}