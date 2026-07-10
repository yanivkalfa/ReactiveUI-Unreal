// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// PIE mount surface for the demo gallery: press Play on any level and the gallery mounts
// over the viewport (set as the project's default game mode in DefaultEngine.ini).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "RuiDemoGameMode.generated.h"

class FRuiRoot;

UCLASS()
class ARuiDemoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<FRuiRoot> Root;
};
