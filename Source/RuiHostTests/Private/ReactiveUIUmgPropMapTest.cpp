// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Umg.PropMap — TD-021: the per-class UMG prop-map bridge. A declarative name->FRuiValue
// map is applied to a hosted UUserWidget's UPROPERTYs by reflection (type-matched), so a hosted widget
// receives Rui-driven data without a hand-written binding. Verifies the reflection application
// directly (int/float/bool/text/string, unknown skipped) and end-to-end through the UserWidget element.

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "RuiRoot.h"
#include "RuiTypes.h"
#include "RuiUmgElement.h"
#include "RuiTestViewModel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUmgPropMapTest, "ReactiveUI.Umg.PropMap",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUmgPropMapTest::RunTest(const FString&)
{
	// ── the reflection application, direct (no world needed) ─────────────────────────────────────
	URuiTestUserWidget* Widget = NewObject<URuiTestUserWidget>(GetTransientPackage());
	Widget->AddToRoot();

	FRuiStyleDict Props;
	Props.Add(FName(TEXT("IntValue")), FRuiValue(42));
	Props.Add(FName(TEXT("FloatValue")), FRuiValue(3.5f));
	Props.Add(FName(TEXT("BoolValue")), FRuiValue(true));
	Props.Add(FName(TEXT("TextValue")), FRuiValue(FText::FromString(TEXT("hello"))));
	Props.Add(FName(TEXT("StringValue")), FRuiValue(FString(TEXT("world"))));

	const int32 Applied = RUI::Umg::ApplyPropMap(Widget, Props);
	TestEqual(TEXT("all five typed properties applied"), Applied, 5);
	TestEqual(TEXT("int reflected"), Widget->IntValue, 42);
	TestTrue(TEXT("float reflected"), FMath::IsNearlyEqual(Widget->FloatValue, 3.5f));
	TestTrue(TEXT("bool reflected"), Widget->BoolValue);
	TestEqual(TEXT("text reflected"), Widget->TextValue.ToString(), FString(TEXT("hello")));
	TestEqual(TEXT("string reflected"), Widget->StringValue, FString(TEXT("world")));

	// unknown property names are skipped, not errors
	FRuiStyleDict Unknown;
	Unknown.Add(FName(TEXT("NoSuchProperty")), FRuiValue(1));
	TestEqual(TEXT("unknown property applies nothing"), RUI::Umg::ApplyPropMap(Widget, Unknown), 0);

	// a String value coerces into an FText property
	FRuiStyleDict Coerce;
	Coerce.Add(FName(TEXT("TextValue")), FRuiValue(FString(TEXT("coerced"))));
	RUI::Umg::ApplyPropMap(Widget, Coerce);
	TestEqual(TEXT("string coerces into FText property"), Widget->TextValue.ToString(), FString(TEXT("coerced")));

	Widget->RemoveFromRoot();

	// ── end-to-end through the UserWidget element (mounted in a standalone game instance) ────────
	if (GEngine != nullptr)
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
		GameInstance->AddToRoot();
		GameInstance->InitializeStandalone();
		UWorld* World = GameInstance->GetWorld();

		if (TestNotNull(TEXT("standalone world"), World))
		{
			FRuiStyleDict Mounted;
			Mounted.Add(FName(TEXT("IntValue")), FRuiValue(7));
			TSharedRef<FRuiRoot> Root =
				FRuiRoot::Create(RUI::Umg::UserWidget(URuiTestUserWidget::StaticClass(), World, Mounted));
			Root->FlushSync();
			TestTrue(TEXT("hosted widget produced a Slate child"),
					 Root->GetWidget()->GetRootPanel()->GetChildren()->Num() > 0);
			Root->Unmount();
		}

		GameInstance->Shutdown();
		GameInstance->RemoveFromRoot();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
