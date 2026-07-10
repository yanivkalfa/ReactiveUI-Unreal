// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiDemoGameMode.h"

#include "RuiDemoScreens.h"
#include "RuiRoot.h"

void ARuiDemoGameMode::BeginPlay()
{
	Super::BeginPlay();
	Root = FRuiRoot::CreateInViewport(RuiDemo::GalleryRoot(), /*ZOrder*/ 10);
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
