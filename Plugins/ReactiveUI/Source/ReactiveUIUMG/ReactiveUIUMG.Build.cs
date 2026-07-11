// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// Epic interop, UObject side (Phase 6): URuiHostWidget (a Rui tree INSIDE a UMG/Blueprint
// hierarchy — "our UI inside theirs"), RUI::Umg::UserWidget (a UMG widget INSIDE a Rui tree —
// "theirs inside ours"), URuiWorldSubsystem (per-world mount surface with the teardown
// contract: PIE end / level travel unmounts every root), and UseField over FieldNotify
// ("their data feeding ours" — the engine FieldNotification module, plugin-independent).
public class ReactiveUIUMG : ModuleRules
{
	public ReactiveUIUMG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"Slate",
			"SlateCore",
			"FieldNotification",
			"ReactiveUICore",
			"ReactiveUISlate",
		});
	}
}
