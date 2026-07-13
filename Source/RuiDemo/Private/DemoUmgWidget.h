// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// A tiny hand-built UMG UserWidget for the interop showcase — proof of "their widget inside our tree":
// RUI::Umg::UserWidget mounts one of these as a Slate child of a ReactiveUI component. It builds its
// content in C++ (RebuildWidget) so it renders something visible without a Blueprint asset.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "DemoUmgWidget.generated.h"

UCLASS()
class UDemoUmgWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDemoUmgWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
