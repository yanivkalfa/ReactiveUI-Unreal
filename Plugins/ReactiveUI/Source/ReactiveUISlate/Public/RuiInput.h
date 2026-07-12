// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-004 (input APIs, keyboard-shortcut half): RUI::Slate::UseShortcut registers a key-chord ->
// callback for a component's lifetime via a Slate input pre-processor, cleaned up on unmount. The
// LATEST callback fires (a stable ref box refreshed each render), and the pre-processor re-registers
// only when the CHORD changes. Drag-and-drop (the other half of TD-004) is tracked separately.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class FRuiContext;
struct FKeyEvent;

namespace RUI::Slate
{
	/** A keyboard chord: a key plus required modifier state (all must match exactly). */
	struct REACTIVEUISLATE_API FRuiShortcut
	{
		FKey Key;
		bool bCtrl = false;
		bool bShift = false;
		bool bAlt = false;
		bool bCmd = false;

		/** True when the event's key + modifier state match this chord exactly. */
		bool Matches(const FKeyEvent& Event) const;

		/** A stable hash of the chord — the UseEffect dep so it re-registers only on a chord change. */
		int32 DepKey() const;
	};

	/** Register `OnTrigger` to fire when `Chord` is pressed, for the calling component's lifetime.
	 *  Requires a running Slate application (a no-op headless without one). */
	REACTIVEUISLATE_API void UseShortcut(FRuiContext& Ctx, const FRuiShortcut& Chord, TFunction<void()> OnTrigger);
} // namespace RUI::Slate
