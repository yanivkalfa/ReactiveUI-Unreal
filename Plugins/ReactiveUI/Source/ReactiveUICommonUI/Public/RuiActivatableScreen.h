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
#include "UObject/WeakObjectPtr.h"
#include "RuiActivatableScreen.generated.h"

class FRuiRoot;
class UCommonInputSubsystem;
enum class ECommonInputType : uint8;

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

	/** Has the hosted tree designated a desired focus target (RUI::CommonUI::UseDesiredFocus)? */
	bool HasDesiredFocusTarget() const { return FocusRegistry.IsValid() && FocusRegistry->HasTarget(); }

	URuiActivatableScreen(const FObjectInitializer& ObjectInitializer);

	// UVisual declares this public — keep it so (hosts and tests drive teardown directly).
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	/** TD-029 — CommonUI's focus-restoration contract. The contract wants a UWidget but our tree
	 *  is pure Slate, so when the hosted tree designated a target (UseDesiredFocus) the screen
	 *  returns ITSELF; the focus then arrives at NativeOnFocusReceived, which forwards it to the
	 *  designated widget. No designation → the base behavior. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	FRuiNode BuildTree() const;
	void Rerender();
	void RefreshInputMethod();
	/** Bound to UCommonInputSubsystem::OnInputMethodChangedNative so a live device switch re-renders. */
	void HandleInputMethodChanged(ECommonInputType NewInputType);
	/** Remove the input-method subscription from the subsystem we actually bound to (not the current
	 *  owning player's — they may differ, bughunt CMU-1). */
	void UnbindInputMethod();

	TSharedPtr<FRuiRoot> Root;
	FRuiActivationState State;
	/** TD-029 — owned by the screen, provided into the tree via FocusTargetProvider. */
	TSharedPtr<FRuiFocusTargetRegistry> FocusRegistry;
	FDelegateHandle InputMethodHandle;
	/** The subsystem InputMethodHandle is registered on — tracked so an owning-player change re-points
	 *  the subscription and teardown removes it from the RIGHT subsystem (bughunt CMU-1). */
	TWeakObjectPtr<UCommonInputSubsystem> BoundInputSubsystem;
};
