// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// The demo gallery game module (host project; NOT shipped with the plugin). Phase 2 mounts the
// first hooks-driven screen here; Phase 8 grows it into the full gallery.
public class RuiDemo : ModuleRules
{
	public RuiDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ReactiveUICore",
			"ReactiveUISlate",
			"SlateCore",
			"Slate",
		});
	}
}
