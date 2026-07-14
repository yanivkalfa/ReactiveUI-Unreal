// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ReactiveUIUnrealDemoTarget : TargetRules
{
	public ReactiveUIUnrealDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Latest, NOT a pinned version — the previous V5 pin had it backwards for installed
		// engines: UBT enforces the engine's shared build environment, so a pin that differs
		// from an engine's own defaults is a hard error there (5.7 rejected V5). Latest = each
		// engine's canonical settings, the only choice compatible across the 5.6–5.8 matrix
		// (MASTER_PLAN D-28).
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RuiDemo");
	}
}
