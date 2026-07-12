// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiActivatableScreen.h"

#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Engine/LocalPlayer.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	ERuiInputMethod MapInputType(ECommonInputType InType)
	{
		switch (InType)
		{
		case ECommonInputType::Gamepad:
			return ERuiInputMethod::Gamepad;
		case ECommonInputType::Touch:
			return ERuiInputMethod::Touch;
		default:
			return ERuiInputMethod::MouseAndKeyboard;
		}
	}
} // namespace

FRuiNode URuiActivatableScreen::BuildTree() const
{
	TArray<FRuiNode> Children;
	if (!ComponentName.IsNone() && RUI::HasNamedFactory(ComponentName))
	{
		Children.Add(RUI::Named(ComponentName));
	}
	return RUI::CommonUI::ActivationProvider(State, MoveTemp(Children));
}

TSharedRef<SWidget> URuiActivatableScreen::RebuildWidget()
{
	if (IsDesignTime())
	{
		return SNew(STextBlock)
			.Text(FText::Format(NSLOCTEXT("ReactiveUI", "ActivatableDesignTime", "[ReactiveUI screen: {0}]"),
								FText::FromName(ComponentName.IsNone() ? FName(TEXT("<unset>")) : ComponentName)));
	}
	RefreshInputMethod();
	Root = FRuiRoot::Create(BuildTree());
	Root->FlushSync();
	return Root->GetWidget();
}

void URuiActivatableScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	State.bActive = true;
	RefreshInputMethod();
	Rerender();
}

void URuiActivatableScreen::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	State.bActive = false;
	Rerender();
}

void URuiActivatableScreen::Rerender()
{
	if (Root.IsValid())
	{
		Root->Update(BuildTree());
		Root->FlushSync();
	}
}

void URuiActivatableScreen::RefreshInputMethod()
{
	// Best-effort: read the current input device when a local player is present (a live game); in a
	// player-less context (automation) the state simply stays mouse-and-keyboard.
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer))
		{
			State.InputMethod = MapInputType(Input->GetCurrentInputType());
			// Subscribe so a live mid-session device switch (mouse<->gamepad<->touch) re-renders the tree
			// — sampling only at activation left UseInputMethod consumers stale (bughunt B12). Re-point if
			// the owning player's subsystem CHANGED (split-screen reassignment) so we never keep tracking a
			// stale/dead player's subsystem (bughunt CMU-1).
			if (BoundInputSubsystem.Get() != Input)
			{
				UnbindInputMethod();
				InputMethodHandle = Input->OnInputMethodChangedNative.AddUObject(
					this, &URuiActivatableScreen::HandleInputMethodChanged);
				BoundInputSubsystem = Input;
			}
		}
	}
}

void URuiActivatableScreen::UnbindInputMethod()
{
	if (InputMethodHandle.IsValid())
	{
		if (UCommonInputSubsystem* Old = BoundInputSubsystem.Get())
		{
			Old->OnInputMethodChangedNative.Remove(InputMethodHandle);
		}
		InputMethodHandle.Reset();
	}
	BoundInputSubsystem.Reset();
}

void URuiActivatableScreen::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	const ERuiInputMethod Mapped = MapInputType(NewInputType);
	if (Mapped != State.InputMethod)
	{
		State.InputMethod = Mapped;
		Rerender();
	}
}

void URuiActivatableScreen::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	UnbindInputMethod(); // remove from the subsystem we actually bound to (CMU-1), not the current player's
	if (Root.IsValid())
	{
		Root->Unmount(); // cleanups run before the Slate tree is released (family teardown order)
		Root.Reset();
	}
}

#if WITH_EDITOR
const FText URuiActivatableScreen::GetPaletteCategory()
{
	return NSLOCTEXT("ReactiveUI", "PaletteCategory", "ReactiveUI");
}
#endif
