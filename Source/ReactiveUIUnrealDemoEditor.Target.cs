// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ReactiveUIUnrealDemoEditorTarget : TargetRules
{
	public ReactiveUIUnrealDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// Latest, NOT a pinned version: launcher (installed) engines enforce their SHARED build
		// environment — an editor target must match each engine's own canonical settings, and a
		// pin that differs is a hard UBT error (5.7 rejected the previous V5 pin:
		// "modifies the values of properties: UndefinedIdentifierWarningLevel"). Latest resolves
		// to each engine's defaults, which is exactly what the 5.6–5.8 matrix (D-28) needs.
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RuiDemo");
		ExtraModuleNames.Add("RuiHostTests");
	}
}
