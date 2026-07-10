// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiDemoGameMode.h"

#include "GameFramework/PlayerController.h"
#include "RuiDemoScreens.h"
#include "RuiRoot.h"

void ARuiDemoGameMode::BeginPlay()
{
	Super::BeginPlay();
	Root = FRuiRoot::CreateInViewport(RuiDemo::GalleryRoot(), /*ZOrder*/ 10);

	// UI-friendly input: keep the cursor visible and never lock it to the viewport (PIE's
	// default game input mode captures the mouse on click — the Shift+F1 annoyance).
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}
}

void ARuiDemoGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Root.IsValid())
	{
		Root->Unmount(); // before the world dies: cleanups + widget detach (D-17 order)
		Root.Reset();
	}
	Super::EndPlay(EndPlayReason);
}
