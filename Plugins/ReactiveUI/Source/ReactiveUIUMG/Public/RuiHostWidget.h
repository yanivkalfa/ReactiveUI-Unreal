// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// "Our UI inside theirs": a UMG widget hosting a ReactiveUI tree — drop it into any UMG
// hierarchy (Blueprint designer, Editor Utility Widget, CommonUI screen) and point it at a
// registered component name. Design time shows a placeholder (the designer must never run
// live component code); ReleaseSlateResources unmounts (cleanups run before the Slate tree
// is released — the family teardown contract).

#pragma once

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "RuiHostWidget.generated.h"

class FRuiRoot;

UCLASS(meta = (DisplayName = "ReactiveUI Host"))
class REACTIVEUIUMG_API URuiHostWidget : public UWidget
{
	GENERATED_BODY()

public:
	/** The registered component to mount (a compiled .uetkx component's name, or anything
	 *  self-registered via RUI::RegisterNamedFactory). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactiveUI")
	FName ComponentName;

	/** Re-mount (e.g. after changing ComponentName at runtime). */
	UFUNCTION(BlueprintCallable, Category = "ReactiveUI")
	void Remount();

	// UWidget
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	/** Build the current content (placeholder / unknown-name text / a live mounted root). */
	TSharedRef<SWidget> BuildContent();

	TSharedPtr<FRuiRoot> Root;
	/** Stable wrapper returned to UMG once; Remount swaps its content in place (UMG's
	 *  TakeWidget caches the outer widget, so returning a NEW one from a re-Rebuild never
	 *  reaches an already-slotted host — audit 2026-07-14, Remount was a no-op without this). */
	TSharedPtr<class SBox> Container;
};
