// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-004 (input APIs, keyboard-shortcut half): RUI::Slate::UseShortcut registers a key-chord ->
// callback for a component's lifetime via a Slate input pre-processor, cleaned up on unmount. The
// LATEST callback fires (a stable ref box refreshed each render), and the pre-processor re-registers
// only when the CHORD changes. Drag-and-drop (the other half of TD-004) is tracked separately.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "RuiTypes.h" // FRuiHostHandle

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

	// ── TD-022 (focus extensions): programmatic focus over a widget ref ─────────────────────

	/**
	 * A focus handle: attach `Ref` to the target element's `ref=` prop, then call `Focus()` to
	 * move keyboard focus there and `IsFocused()` to query it. The React ref lifecycle keeps the
	 * captured widget in sync (attached on mount, cleared on unmount). Headless-safe (no-ops
	 * without a running Slate application).
	 */
	struct REACTIVEUISLATE_API FRuiFocusHandle
	{
		TFunction<void(const FRuiHostHandle&)> Ref;
		TFunction<void()> Focus;
		TFunction<bool()> IsFocused;
	};

	/** Stable focus handle for the calling component (a UseRef under the hood). */
	REACTIVEUISLATE_API FRuiFocusHandle UseFocus(FRuiContext& Ctx);

	/** Imperative focus/blur on a mounted host handle (e.g. from an effect or event). */
	REACTIVEUISLATE_API void FocusWidget(const FRuiHostHandle& Handle);
	REACTIVEUISLATE_API void ClearFocus();
} // namespace RUI::Slate
