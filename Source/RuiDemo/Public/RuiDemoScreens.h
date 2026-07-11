// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The demo gallery (family pattern: one game, many example screens): a menu shell plus the
// screens ported from the Unity sibling's Samples. Mounted in PIE by ARuiDemoGameMode;
// every entry is also mounted headlessly by ReactiveUI.Demos (the demos_test.gd analogue).

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"

namespace RuiDemo
{
	struct FRuiDemoEntry
	{
		FString Name;
		FRuiNode (*Make)();
	};

	/** Every gallery screen (stable order; the shell and the Demos suite both iterate it). */
	RUIDEMO_API const TArray<FRuiDemoEntry>& GetGalleryEntries();

	/** The compiled component names behind the entries (the Demos suite asserts every one
	 *  registered — RUI::Named falls back to an empty Fragment, which must never pass). */
	RUIDEMO_API const TArray<FName>& GetCompiledScreenNames();

	/** The gallery root vnode (menu + selected screen). */
	RUIDEMO_API FRuiNode GalleryRoot();
} // namespace RuiDemo
