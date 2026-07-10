// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// The engine-blind core: vnodes, fibers, reconciler, hooks, signals, context, and the
// IRuiHostConfig seam. HARD CONSTRAINT (MASTER_PLAN D-27): no UObject, no CoreUObject, no
// Slate — the reconciler touches engines only through the host-config interface, which is
// what lets the mock-host test suites run the whole core headlessly.
public class ReactiveUICore : ModuleRules
{
	public ReactiveUICore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Projects: IPluginManager, used only for the StartupModule version banner
			// (the packaged-fidelity test greps for it). Non-UObject, Core-family module —
			// does not violate the no-CoreUObject constraint.
			"Projects",
		});
	}
}
