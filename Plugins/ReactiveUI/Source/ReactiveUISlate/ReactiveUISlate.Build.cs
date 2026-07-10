// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// The Slate host (Phase 2): FRuiSlateHost + the per-widget adapter registry, SRuiRoot, the
// widget pool, SRuiCanvas (draw_fn), and style v1. The ONLY module that talks to concrete
// Slate APIs on behalf of the reconciler (MASTER_PLAN D-11..D-13).
// Deps added WITH the code that uses them (IWYU-clean skeletons), per D-27:
//   SlateCore, Slate, InputCore, ReactiveUICore.
public class ReactiveUISlate : ModuleRules
{
	public ReactiveUISlate(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"ReactiveUICore",  // the host implements the core's IRuiHostConfig seam
			"SlateCore",       // SWidget, FReply, slot types (public headers reference them)
			"Slate",           // concrete widgets (STextBlock, SButton, panels), FSlateApplication
			"InputCore",       // key/pointer types reaching through Slate delegates
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",     // Engine types below pull it in
			"ApplicationCore", // FDisplayMetrics (UseSafeArea)
			"Engine",          // the game-viewport mount surface (UGameViewportClient)
		});
	}
}