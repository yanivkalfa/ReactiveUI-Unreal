// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The promise-completing interop widgets (audit Phase 4 — plans/AUDIT_2026-07-14.md §3):
//
//   UDemoHostUserWidget  — G1, "ours INSIDE theirs": a hand-built UMG UserWidget whose widget
//                          tree contains a URuiHostWidget pointing at a registered component.
//                          Embedded in the gallery it demonstrates the full sandwich:
//                          ReactiveUI → UMG → ReactiveUI.
//   UDemoVmBoundWidget   — G3, reverse MVVM: a plain UMG widget that subscribes (FieldNotify)
//                          to OUR URuiSignalViewModel and mirrors its Int field — their view,
//                          our value, no MVVM-plugin dependency.
//   UDemoActivatableScreen / UDemoStackHostWidget — G2, the real CommonUI stack: a
//                          UCommonActivatableWidgetStack that pushes/pops a URuiActivatableScreen
//                          hosting the ActivationProbe component — REAL (de)activation, not the
//                          toggle stand-in.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "FieldNotificationId.h"
#include "RuiActivatableScreen.h"
#include "DemoInteropWidgets.generated.h"

class UButton;
class UCommonActivatableWidgetStack;
class UTextBlock;

/** G1 — a UMG UserWidget hosting a ReactiveUI component through URuiHostWidget. */
UCLASS()
class UDemoHostUserWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};

/** G3 — a plain UMG widget bound (FieldNotify) to the shared URuiSignalViewModel. */
UCLASS()
class UDemoVmBoundWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void OnVmFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);
	void RefreshText();

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;

	FDelegateHandle VmHandle;
};

/** G2 — a URuiActivatableScreen preconfigured to host the ActivationProbe component. */
UCLASS()
class UDemoActivatableScreen : public URuiActivatableScreen
{
	GENERATED_BODY()

public:
	UDemoActivatableScreen(const FObjectInitializer& ObjectInitializer);
};

/** G2 — a real UCommonActivatableWidgetStack; a button pushes/pops the screen above. */
UCLASS()
class UDemoStackHostWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void OnToggleClicked();

	UPROPERTY()
	TObjectPtr<UCommonActivatableWidgetStack> Stack;

	UPROPERTY()
	TObjectPtr<UDemoActivatableScreen> ActiveScreen;

	UPROPERTY()
	TObjectPtr<UTextBlock> ToggleLabel;
};
