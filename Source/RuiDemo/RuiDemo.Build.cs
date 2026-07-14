// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// The demo gallery game module (host project; NOT shipped with the plugin). The screens are
// COMPILED .uetkx (Screens/*.uetkx -> committed .uetkx.inl) built through the stable
// aggregator Private/RuiDemo.Uetkx.gen.cpp — the compiled-file SET stays constant while
// RUICompile rewrites contents (D-19.1). NOTE: a module gaining its FIRST .uetkx needs its
// Build.cs touched once (or project files regenerated) so UBT's cached makefile sees the new
// aggregator; after that it never changes again.
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
			"ReactiveUIUMG",	  // interop demos: UseField + URuiSignalViewModel (MVVM/FieldNotify)
			"ReactiveUICommonUI", // interop demos: UseIsActive / ActivationProvider
			"UMG",				  // interop showcase: subclass UUserWidget (UDemoUmgWidget)
			"CommonUI",			  // audit G2: the REAL activatable stack (UDemoStackHostWidget)
			"FieldNotification",  // UWidget's FieldNotify base (pulled by UMG)
			"SlateCore",
			"Slate",
		});
	}
}
