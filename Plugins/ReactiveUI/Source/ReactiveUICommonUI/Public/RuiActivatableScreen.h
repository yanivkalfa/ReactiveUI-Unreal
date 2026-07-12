// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-021 — "our UI as a CommonUI screen". URuiActivatableScreen is a UCommonActivatableWidget that
// hosts a ReactiveUI component and publishes its CommonUI activation state (active? input method?)
// into the tree via ActivationProvider, so the component reacts with UseActivation/UseInputMethod.
// Push it onto a UCommonActivatableWidgetStack like any other screen; it re-renders on activation and
// input-method changes, and unmounts (running cleanups) when its Slate resources release.

#pragma once

#include "CommonActivatableWidget.h"
#include "CoreMinimal.h"
#include "RuiActivation.h"
#include "RuiActivatableScreen.generated.h"

class FRuiRoot;

UCLASS(meta = (DisplayName = "ReactiveUI Activatable Screen"))
class REACTIVEUICOMMONUI_API URuiActivatableScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** The registered component to host (a compiled .uetkx component or a RUI::RegisterNamedFactory). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactiveUI")
	FName ComponentName;

	/** Is this screen currently active on its stack? (mirrors the state pushed into the tree). */
	UFUNCTION(BlueprintCallable, Category = "ReactiveUI")
	bool IsScreenActive() const { return State.bActive; }

	/** The activation state currently published to the hosted tree. */
	const FRuiActivationState& GetActivationState() const { return State; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	FRuiNode BuildTree() const;
	void Rerender();
	void RefreshInputMethod();

	TSharedPtr<FRuiRoot> Root;
	FRuiActivationState State;
};
