// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiUmg, Log, All);

class FReactiveUIUMGModule : public IModuleInterface
{
public:
	virtual void StartupModule() override { UE_LOG(LogRuiUmg, Verbose, TEXT("ReactiveUIUMG module started")); }

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FReactiveUIUMGModule, ReactiveUIUMG)