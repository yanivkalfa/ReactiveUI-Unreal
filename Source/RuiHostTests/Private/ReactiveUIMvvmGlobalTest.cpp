// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Mvvm.GlobalCollection — TD-021: the MVVM-plugin global viewmodel-collection registration.
// A URuiMvvmViewModel (a UMVVMViewModelBase) is registered in the game instance's global collection and
// resolved back by context name; Rui writes route to the right field via Set(FRuiValue), and a
// FieldNotify listener confirms the broadcast — the "globally bindable, ours feeding theirs" path.

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "INotifyFieldValueChanged.h" // UE 5.7 deprecated the FieldNotification/IFieldValueChanged.h path
#include "RuiMvvmViewModel.h"
#include "RuiTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiMvvmGlobalTest, "ReactiveUI.Mvvm.GlobalCollection",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiMvvmGlobalTest::RunTest(const FString&)
{
	if (GEngine == nullptr)
	{
		AddInfo(TEXT("[mvvm] no GEngine — global-collection test skipped."));
		return true;
	}

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();

	URuiMvvmViewModel* Vm = NewObject<URuiMvvmViewModel>(GameInstance);

	// ── register in the global collection + resolve back by context name ─────────────────────────
	const bool bRegistered = RUI::Mvvm::RegisterGlobalViewModel(GameInstance, FName(TEXT("PlayerStats")), Vm);
	TestTrue(TEXT("viewmodel registered in the global collection"), bRegistered);

	UMVVMViewModelBase* Found =
		RUI::Mvvm::FindGlobalViewModel(GameInstance, FName(TEXT("PlayerStats")), URuiMvvmViewModel::StaticClass());
	TestTrue(TEXT("resolves back the SAME instance by context name"), Found == Vm);

	TestNull(
		TEXT("an unregistered context name resolves to null"),
		RUI::Mvvm::FindGlobalViewModel(GameInstance, FName(TEXT("NoSuchContext")), URuiMvvmViewModel::StaticClass()));

	// ── Rui writes route by kind + broadcast a FieldNotify change ────────────────────────────────
	int32 Broadcasts = 0;
	Vm->AddFieldValueChangedDelegate(URuiMvvmViewModel::FFieldNotificationClassDescriptor::IntValue,
									 INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
										 [&Broadcasts](UObject*, UE::FieldNotification::FFieldId) { ++Broadcasts; }));

	Vm->Set(FRuiValue(42));
	TestEqual(TEXT("Set(int) routed to IntValue"), Vm->IntValue, 42);
	TestEqual(TEXT("IntValue change broadcast once"), Broadcasts, 1);

	Vm->Set(FRuiValue(42)); // equal -> skip, no broadcast
	TestEqual(TEXT("equal set does not re-broadcast"), Broadcasts, 1);

	Vm->Set(FRuiValue(FString(TEXT("hello"))));
	TestEqual(TEXT("Set(string) routed to TextValue"), Vm->TextValue.ToString(), FString(TEXT("hello")));

	Vm->Set(FRuiValue(true));
	TestTrue(TEXT("Set(bool) routed to BoolValue"), Vm->BoolValue);

	GameInstance->Shutdown();
	GameInstance->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
