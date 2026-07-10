// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiSlate, Log, All);

class FReactiveUISlateModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogRuiSlate, Verbose, TEXT("ReactiveUISlate module started"));
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FReactiveUISlateModule, ReactiveUISlate)