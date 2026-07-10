// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ReactiveUIUnrealDemoEditorTarget : TargetRules
{
	public ReactiveUIUnrealDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RuiDemo");
		ExtraModuleNames.Add("RuiHostTests");
	}
}
