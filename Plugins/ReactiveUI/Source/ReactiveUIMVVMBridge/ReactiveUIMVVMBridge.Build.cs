// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// Global viewmodel-collection registration ONLY (Phase 6) -- the one piece of MVVM interop
// that needs the ModelViewViewModel plugin (everything FieldNotify is engine-level and lives
// in ReactiveUIUMG, D-26). Deps added with the code, per D-27:
//   CoreUObject, ModelViewViewModel (optional plugin ref), ReactiveUIUMG.
public class ReactiveUIMVVMBridge : ModuleRules
{
	public ReactiveUIMVVMBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});
	}
}