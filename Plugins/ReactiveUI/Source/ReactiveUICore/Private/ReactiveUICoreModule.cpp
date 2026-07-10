// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiCore, Log, All);

// Phase 1 (MASTER_PLAN §3) fills this module with the reconciler. The startup banner below is
// load-bearing already: the ReactiveUI.Boot suite and the packaged-fidelity test (fresh project,
// enable plugin, expect the banner in the log) both key off it — the exact analogue of the Godot
// repo's "a silent Output means the plugin is NOT running" rule.
class FReactiveUICoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FString VersionName = TEXT("unknown");
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ReactiveUI")))
		{
			VersionName = Plugin->GetDescriptor().VersionName;
		}
		UE_LOG(LogRuiCore, Display, TEXT("ReactiveUI %s loaded (ReactiveUICore)"), *VersionName);
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FReactiveUICoreModule, ReactiveUICore)
