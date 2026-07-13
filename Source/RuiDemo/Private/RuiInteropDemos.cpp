// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Interop demos — the Epic-integration pillars made VISIBLE in the gallery. The .uetkx screens all
// prove pillar 1 (our output IS Slate); these prove the other two the thesis promises but no screen
// showed:
//   MvvmDemo     — FieldNotify/MVVM data FEEDS us: UseField re-renders the component when a viewmodel
//                  field broadcasts a change.
//   CommonUiDemo — CommonUI activation FEEDS us: UseIsActive / UseInputMethod drive the UI the way a
//                  URuiActivatableScreen does when it (de)activates on a CommonUI stack.
// They register as named factories (like the compiled .uetkx screens self-register), so the gallery,
// a URuiHostWidget, or a URuiActivatableScreen can all mount them by name.

#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiDemoShared.h"
#include "RuiNode.h"
#include "RuiSlateElements.h"

#include "RuiActivation.h"		// ReactiveUICommonUI — UseIsActive / UseInputMethod / ActivationProvider
#include "RuiFieldHooks.h"		// ReactiveUIUMG      — UseField
#include "RuiSignalViewModel.h" // ReactiveUIUMG      — URuiSignalViewModel

#include "UObject/StrongObjectPtr.h"

using namespace RuiDemo;

namespace
{
	FRuiNode DemoCard(TArray<FRuiNode> Kids)
	{
		FRuiBorderProps Card;
		Card.SetPadding(FMargin(12.0f));
		Card.SetBorderImage(FName(TEXT("WhiteBrush")));
		Card.SetBorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f));
		return RUI::Slate::Border(MoveTemp(Card), {RUI::Slate::VerticalBox(FRuiVerticalBoxProps(), MoveTemp(Kids))});
	}
} // namespace

// ── MVVM / FieldNotify: their data feeds us ───────────────────────────────────────────────────
static FRuiNodeArray MvvmDemoComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	// A viewmodel owned by the component — the strong ptr GC-roots it; it releases on unmount.
	TSharedRef<TRuiRef<TStrongObjectPtr<URuiSignalViewModel>>> VmRef =
		Ctx.UseRef<TStrongObjectPtr<URuiSignalViewModel>>();
	if (!VmRef->Current.IsValid())
	{
		VmRef->Current.Reset(NewObject<URuiSignalViewModel>());
	}
	URuiSignalViewModel* Vm = VmRef->Current.Get();

	// The bridge: subscribe once, re-render whenever the VM's `Int` field broadcasts a change.
	const int32 N = RUI::Umg::UseField<int32>(Ctx, Vm, FName(TEXT("Int")), 0);

	TArray<FRuiNode> Kids;
	Kids.Add(StyledText(TEXT("MVVM / FieldNotify — their data feeds us"), 18.0f));
	Kids.Add(Gap(6.0f));
	Kids.Add(
		StyledText(TEXT("A URuiSignalViewModel is written by the button; UseField re-renders on the field broadcast. "
						"They own the value; we own the structure."),
				   11.0f, FLinearColor(0.7f, 0.7f, 0.8f)));
	Kids.Add(Gap(12.0f));
	Kids.Add(RUI::TextBlock(RUI::Fmt(TEXT("viewmodel.Int = {}"), N)));
	Kids.Add(Gap(6.0f));
	Kids.Add(LabeledButton(TEXT("viewmodel.SetInt(+1)"),
						   [Vm, N]()
						   {
							   if (Vm)
							   {
								   Vm->SetInt(N + 1);
							   }
						   }));

	return {DemoCard(MoveTemp(Kids))};
}
RUI_COMPONENT(MvvmDemoComp)
static const bool GMvvmDemoReg =
	RUI::RegisterNamedFactory(FName(TEXT("MvvmDemo")), []() { return RUI::FC(&MvvmDemoComp); });

// ── CommonUI: activation state feeds us ───────────────────────────────────────────────────────
// The inner component READS activation (UseIsActive / UseInputMethod). The outer wraps it in an
// ActivationProvider whose state a toggle flips — standing in for what a URuiActivatableScreen pushes
// when it activates/deactivates on a CommonUI stack (a real stack drives the exact same hooks).
static FRuiNodeArray ActivationProbeComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	const bool bActive = RUI::CommonUI::UseIsActive(Ctx);
	const ERuiInputMethod Method = RUI::CommonUI::UseInputMethod(Ctx);
	const TCHAR* MethodStr = Method == ERuiInputMethod::Gamepad ? TEXT("Gamepad")
							 : Method == ERuiInputMethod::Touch ? TEXT("Touch")
																: TEXT("Mouse & Keyboard");
	TArray<FRuiNode> Kids;
	Kids.Add(RUI::TextBlock(RUI::Fmt(TEXT("Screen is {}"), bActive ? TEXT("ACTIVE") : TEXT("INACTIVE"))));
	Kids.Add(RUI::TextBlock(RUI::Fmt(TEXT("Input method: {}"), MethodStr)));
	return {RUI::Slate::VerticalBox(FRuiVerticalBoxProps(), MoveTemp(Kids))};
}
RUI_COMPONENT(ActivationProbeComp)

static FRuiNodeArray CommonUiDemoComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [bActive, SetActive] = Ctx.UseState<bool>(true);
	const bool bActiveNow = bActive;

	FRuiActivationState State;
	State.bActive = bActiveNow;
	State.InputMethod = ERuiInputMethod::MouseAndKeyboard;

	TArray<FRuiNode> Kids;
	Kids.Add(StyledText(TEXT("CommonUI — activation feeds us"), 18.0f));
	Kids.Add(Gap(6.0f));
	Kids.Add(
		StyledText(TEXT("A URuiActivatableScreen drives these hooks when it (de)activates on a CommonUI stack; "
						"here a toggle stands in. CommonUI keeps owning input routing / focus — we just read state."),
				   11.0f, FLinearColor(0.7f, 0.7f, 0.8f)));
	Kids.Add(Gap(12.0f));
	Kids.Add(RUI::CommonUI::ActivationProvider(State, {RUI::FC(&ActivationProbeComp)}));
	Kids.Add(Gap(8.0f));
	Kids.Add(LabeledButton(TEXT("Toggle activation"), [SetActive, bActiveNow]() { SetActive(!bActiveNow); }));

	return {DemoCard(MoveTemp(Kids))};
}
RUI_COMPONENT(CommonUiDemoComp)
static const bool GCommonUiDemoReg =
	RUI::RegisterNamedFactory(FName(TEXT("CommonUiDemo")), []() { return RUI::FC(&CommonUiDemoComp); });
