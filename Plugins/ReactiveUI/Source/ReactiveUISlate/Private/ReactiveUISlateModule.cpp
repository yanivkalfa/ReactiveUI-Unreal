// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "RuiSlateElements.h"
#include "RuiSlateLog.h"

DEFINE_LOG_CATEGORY(LogRuiSlate);

class FReactiveUISlateModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		RUI::Slate::RegisterBuiltinAdapters();
		UE_LOG(LogRuiSlate, Verbose, TEXT("ReactiveUISlate module started"));
	}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FReactiveUISlateModule, ReactiveUISlate)
