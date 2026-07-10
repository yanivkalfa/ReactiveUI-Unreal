// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// Epic interop, UObject side (Phases 6-7): URuiHostWidget, URuiSubsystem (per-world roots +
// teardown-before-GC contract, D-17), RUI::Umg, UseField/UseViewModel over FieldNotify,
// URuiSignalViewModel (reverse bridge), the brush FGCObject root, and UseSfx world glue.
// Deps added with the code, per D-27:
//   CoreUObject, Engine, UMG, FieldNotification, ReactiveUICore, ReactiveUISlate.
public class ReactiveUIUMG : ModuleRules
{
	public ReactiveUIUMG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});
	}
}