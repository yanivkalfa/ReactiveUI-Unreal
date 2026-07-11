// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Logging/LogMacros.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "UetkxWatcher.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiEditor, Log, All);

class FReactiveUIEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
		FMessageLogInitializationOptions Options;
		Options.bShowFilters = true;
		Options.bShowPages = false;
		MessageLogModule.RegisterLogListing(TEXT("ReactiveUI"),
											NSLOCTEXT("ReactiveUI", "MessageLogLabel", "ReactiveUI"), Options);
		Watcher = MakeUnique<FUetkxWatcher>();
		Watcher->Start();
		UE_LOG(LogRuiEditor, Display, TEXT("ReactiveUIEditor started — .uetkx watcher armed"));
	}

	virtual void ShutdownModule() override
	{
		if (Watcher.IsValid())
		{
			Watcher->Stop();
			Watcher.Reset();
		}
		if (FModuleManager::Get().IsModuleLoaded(TEXT("MessageLog")))
		{
			FModuleManager::GetModuleChecked<FMessageLogModule>(TEXT("MessageLog"))
				.UnregisterLogListing(TEXT("ReactiveUI"));
		}
	}

private:
	TUniquePtr<FUetkxWatcher> Watcher;
};

IMPLEMENT_MODULE(FReactiveUIEditorModule, ReactiveUIEditor)