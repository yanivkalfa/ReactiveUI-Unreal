// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.CommonUI.Activation — TD-021: the activation-context mechanism (headless): a component
// reads UseIsActive and re-renders when ActivationProvider publishes a new state.
// ReactiveUI.CommonUI.Screen — the URuiActivatableScreen UObject end-to-end in a standalone game
// instance: activate -> the hosted Rui tree shows ACTIVE; deactivate -> INACTIVE (real CommonUI
// activation driving a real ReactiveUI re-render).

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "RuiActivatableScreen.h"
#include "RuiActivation.h"
#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "RuiSlateTestHarness.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CommonUITest
{
	// A probe component that renders its activation state as text — the observable for both suites.
	static FRuiNodeArray ActiveProbe(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		const bool bActive = RUI::CommonUI::UseIsActive(Ctx);
		return {RUI::TextBlock(bActive ? FString(TEXT("ACTIVE")) : FString(TEXT("INACTIVE")))};
	}
	RUI_COMPONENT(ActiveProbe)

	static FString ProbeText(const TSharedRef<SWidget>& Root)
	{
		SWidget* Text = RuiTest::FindDescendantByType(Root.Get(), FName(TEXT("STextBlock")));
		return Text != nullptr ? static_cast<STextBlock*>(Text)->GetText().ToString() : FString();
	}
} // namespace CommonUITest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCommonUIActivationTest, "ReactiveUI.CommonUI.Activation",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiCommonUIActivationTest::RunTest(const FString&)
{
	using namespace RUI::CommonUI;
	using namespace CommonUITest;

	// Inactive by default.
	FRuiActivationState Inactive;
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(ActivationProvider(Inactive, {RUI::FC(&ActiveProbe)}));
	Root->FlushSync();
	TestEqual(TEXT("inactive state renders INACTIVE"), ProbeText(Root->GetWidget()), FString(TEXT("INACTIVE")));

	// Publish active -> the consuming component re-renders through the context.
	FRuiActivationState Active;
	Active.bActive = true;
	Root->Update(ActivationProvider(Active, {RUI::FC(&ActiveProbe)}));
	Root->FlushSync();
	TestEqual(TEXT("active state re-renders to ACTIVE"), ProbeText(Root->GetWidget()), FString(TEXT("ACTIVE")));

	// Back to inactive.
	Root->Update(ActivationProvider(Inactive, {RUI::FC(&ActiveProbe)}));
	Root->FlushSync();
	TestEqual(TEXT("re-renders back to INACTIVE"), ProbeText(Root->GetWidget()), FString(TEXT("INACTIVE")));

	Root->Unmount();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCommonUIScreenTest, "ReactiveUI.CommonUI.Screen",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiCommonUIScreenTest::RunTest(const FString&)
{
	using namespace CommonUITest;

	// Name-mount the probe so the screen can host it.
	RUI::RegisterNamedFactory(FName(TEXT("RuiActiveProbe")), []() { return RUI::FC(&ActiveProbe); });

	if (GEngine == nullptr)
	{
		AddInfo(TEXT("[commonui] no GEngine — UObject screen construction skipped."));
		return true;
	}

	// A standalone game instance gives a world + local player so CreateWidget builds a real UUserWidget.
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();

	URuiActivatableScreen* Screen = CreateWidget<URuiActivatableScreen>(GameInstance);
	if (TestNotNull(TEXT("activatable screen created"), Screen))
	{
		Screen->ComponentName = FName(TEXT("RuiActiveProbe"));
		TSharedRef<SWidget> Widget = Screen->TakeWidget();

		TestFalse(TEXT("starts inactive"), Screen->IsScreenActive());
		TestEqual(TEXT("hosted tree starts INACTIVE"), ProbeText(Widget), FString(TEXT("INACTIVE")));

		Screen->ActivateWidget();
		TestTrue(TEXT("ActivateWidget marks the screen active"), Screen->IsScreenActive());
		TestEqual(TEXT("activation re-renders the hosted tree to ACTIVE"), ProbeText(Widget), FString(TEXT("ACTIVE")));

		Screen->DeactivateWidget();
		TestFalse(TEXT("DeactivateWidget clears active"), Screen->IsScreenActive());
		TestEqual(TEXT("deactivation re-renders to INACTIVE"), ProbeText(Widget), FString(TEXT("INACTIVE")));
	}

	GameInstance->Shutdown();
	GameInstance->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
