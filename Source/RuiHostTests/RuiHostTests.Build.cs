// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// ALL automation tests live here (the ReactiveUI.* suite hierarchy — MASTER_PLAN §4), in the
// host project rather than the plugin, so the shipped plugin stays lean. Editor-type module:
// suites run via `UnrealEditor-Cmd -ExecCmds="Automation RunTests ReactiveUI; Quit"` headless.
public class RuiHostTests : ModuleRules
{
	public RuiHostTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Phase 1 adds: "ReactiveUICore" (mock-host suites). Later phases add the modules
			// they test (ReactiveUISlate, ReactiveUIUMG, ...) — keep deps exactly as narrow as
			// the tests that exist.
			"CoreUObject",
			"Engine",
		});
	}
}
