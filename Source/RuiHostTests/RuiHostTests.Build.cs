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
			// Keep deps exactly as narrow as the tests that exist (later phases add the
			// modules they test: ReactiveUISlate, ReactiveUIUMG, ...).
			"CoreUObject",
			"Engine",
			"Projects",         // IPluginManager (the Boot suite)
			"ReactiveUICore",   // the mock-host core suites
			"ReactiveUISlate",  // the Slate host suites + reorder spike
			"RuiDemo",          // the Demos suite mounts the gallery
			"ReactiveUIInterp", // the .uetkx scanner/parser suites + VM/Hmr
			"ReactiveUIToolchain", // the codegen suites
			"ReactiveUIEditor", // the TD-006 .uetkx preview suite (FUetkxPreview)
			"ReactiveUIUMG",    // the Phase-6 interop suites (Umg/Mvvm)
			"ReactiveUICommonUI", // the TD-021 CommonUI activatable suite
			"CommonUI",         // UCommonActivatableWidget + ActivateWidget in the suite
			"CommonInput",      // UCommonInputSubsystem (B12 input-method regression)
			"ReactiveUIMVVMBridge", // the TD-021 MVVM global-collection suite
			"ModelViewViewModel",   // UMVVMViewModelBase + the global collection in the suite
			"UMG",
			"FieldNotification",
			"Json",             // contract-corpus loading
			"SlateCore",
			"Slate",
			"InputCore",        // EKeys / FKeyEvent for the shortcut suite
		});
	}
}
