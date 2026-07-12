// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-021 — the CommonUI activation seam, Rui side. A ReactiveUI tree hosted inside a CommonUI
// activatable needs to REACT to activation (is this screen the active one on the stack?) and to the
// current input method (mouse/keyboard vs gamepad vs touch — CommonUI's whole reason for being). We
// surface both through a context the host provides and hooks the tree reads:
//
//   URuiActivatableScreen  --provides-->  FRuiActivationState (via ActivationProvider)
//   your component         --reads----->  UseActivation / UseIsActive / UseInputMethod
//
// The mechanism is plain ReactiveUI context (no UObject dependency), so it is fully unit-testable
// headless; the UObject (RuiActivatableScreen.h) is only the CommonUI-side data source that flips
// the state and re-renders.

#pragma once

#include "CoreMinimal.h"
#include "RuiContextHandle.h" // TRuiContext
#include "RuiNode.h"

class FRuiContext;

/** The input device family currently driving the UI (mirrors CommonUI's ECommonInputType). */
enum class ERuiInputMethod : uint8
{
	MouseAndKeyboard,
	Gamepad,
	Touch,
};

/** What a hosted ReactiveUI tree can learn about its CommonUI screen. */
struct REACTIVEUICOMMONUI_API FRuiActivationState
{
	bool bActive = false; // is this the active screen on its activatable stack?
	ERuiInputMethod InputMethod = ERuiInputMethod::MouseAndKeyboard;

	bool operator==(const FRuiActivationState& Other) const
	{
		return bActive == Other.bActive && InputMethod == Other.InputMethod;
	}
	bool operator!=(const FRuiActivationState& Other) const { return !(*this == Other); }
};

namespace RUI::CommonUI
{
	/** The context handle carrying activation state to descendants (default = inactive / M&K). */
	REACTIVEUICOMMONUI_API TRuiContext<FRuiActivationState>& ActivationContext();

	/** Read the full activation state at this point in the tree. */
	REACTIVEUICOMMONUI_API FRuiActivationState UseActivation(FRuiContext& Ctx);

	/** Sugar: is the hosting screen active? */
	REACTIVEUICOMMONUI_API bool UseIsActive(FRuiContext& Ctx);

	/** Sugar: the current input method. */
	REACTIVEUICOMMONUI_API ERuiInputMethod UseInputMethod(FRuiContext& Ctx);

	/** Provide `State` to `Children` (what the activatable host wraps the component in). */
	REACTIVEUICOMMONUI_API FRuiNode ActivationProvider(FRuiActivationState State,
													   TArray<FRuiNode> Children = TArray<FRuiNode>(),
													   FRuiKey Key = FRuiKey());
} // namespace RUI::CommonUI
