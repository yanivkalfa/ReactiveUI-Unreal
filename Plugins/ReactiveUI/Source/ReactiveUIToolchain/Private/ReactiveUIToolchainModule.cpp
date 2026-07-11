// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "UetkxToolchainLog.h"

DEFINE_LOG_CATEGORY(LogRuiToolchain);

class FReactiveUIToolchainModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogRuiToolchain, Verbose, TEXT("ReactiveUIToolchain module started"));
	}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FReactiveUIToolchainModule, ReactiveUIToolchain)