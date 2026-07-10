// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ReactiveUIUnrealDemoTarget : TargetRules
{
	public ReactiveUIUnrealDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Pinned (not Latest): the repo supports a 5.6–5.8 engine matrix; floating settings
		// versions would change behavior per engine (MASTER_PLAN D-28).
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RuiDemo");
	}
}
