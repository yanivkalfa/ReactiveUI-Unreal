// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The boot check — the gate ladder's never-optional rung (dev-process skill): proves the
// plugin's modules actually LOADED (unit suites don't run StartupModule) and that a root
// mounts end-to-end on the host seam.

#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "RuiMockHost.h"
#include "RuiContext.h"
#include "RuiCoreElements.h"

#if WITH_DEV_AUTOMATION_TESTS

static FRuiNodeArray BootComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [V, SetV] = Ctx.UseState<int32>(42);
	return {RUI::Text(FString::Printf(TEXT("boot %d"), V))};
}
RUI_COMPONENT(BootComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBootTest, "ReactiveUI.Boot",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBootTest::RunTest(const FString&)
{
	AddInfo(TEXT("[boot] 1/3 plugin + modules loaded"));
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ReactiveUI"));
	if (!TestTrue(TEXT("ReactiveUI plugin found + enabled"), Plugin.IsValid() && Plugin->IsEnabled()))
	{
		return false;
	}
	TestTrue(TEXT("ReactiveUICore module loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("ReactiveUICore")));
	TestTrue(TEXT("ReactiveUISlate module loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("ReactiveUISlate")));
	TestTrue(TEXT("ReactiveUIInterp module loaded (non-Shipping)"),
			 FModuleManager::Get().IsModuleLoaded(TEXT("ReactiveUIInterp")));

	AddInfo(TEXT("[boot] 2/3 a root mounts on the host seam"));
	FRuiTestHarness H;
	H.Mount(RUI::FC(&BootComp));
	FMockNode* Node = H.ChildAt(0);
	if (!TestNotNull(TEXT("root mounted a node"), Node))
	{
		return false;
	}
	TestEqual(TEXT("hooks ran"), Node->TextOf(), FString(TEXT("boot 42")));

	AddInfo(TEXT("[boot] 3/3 clean unmount"));
	H.Reconciler->Unmount();
	TestEqual(TEXT("tree emptied"), H.RootNode()->Children.Num(), 0);
	TestEqual(TEXT("zero live fibers after unmount"), H.Reconciler->NumLiveFibers(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
