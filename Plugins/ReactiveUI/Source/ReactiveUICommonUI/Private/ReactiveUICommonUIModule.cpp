// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiCommonUI, Log, All);

class FReactiveUICommonUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogRuiCommonUI, Verbose, TEXT("ReactiveUICommonUI module started"));
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FReactiveUICommonUIModule, ReactiveUICommonUI)