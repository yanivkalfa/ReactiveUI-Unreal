// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiMVVMBridge, Log, All);

class FReactiveUIMVVMBridgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogRuiMVVMBridge, Verbose, TEXT("ReactiveUIMVVMBridge module started"));
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FReactiveUIMVVMBridgeModule, ReactiveUIMVVMBridge)