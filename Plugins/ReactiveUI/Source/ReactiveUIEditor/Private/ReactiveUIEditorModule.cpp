// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiEditor, Log, All);

class FReactiveUIEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogRuiEditor, Verbose, TEXT("ReactiveUIEditor module started"));
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FReactiveUIEditorModule, ReactiveUIEditor)