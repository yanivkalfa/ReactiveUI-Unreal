// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiInput.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "RuiContext.h"
#include "RuiHooksInternal.h"
#include "RuiSlateHost.h"

bool RUI::Slate::FRuiShortcut::Matches(const FKeyEvent& Event) const
{
	if (Event.GetKey() != Key)
	{
		return false;
	}
	const FModifierKeysState M = Event.GetModifierKeys();
	return M.IsControlDown() == bCtrl && M.IsShiftDown() == bShift && M.IsAltDown() == bAlt &&
		   M.IsCommandDown() == bCmd;
}

int32 RUI::Slate::FRuiShortcut::DepKey() const
{
	const int32 Mods = (bCtrl ? 1 : 0) | (bShift ? 2 : 0) | (bAlt ? 4 : 0) | (bCmd ? 8 : 0);
	return static_cast<int32>(HashCombine(GetTypeHash(Key), ::GetTypeHash(Mods)));
}

namespace
{
	/** Fires the box's CURRENT callback (latest closure) when the chord matches; consumes the key. */
	class FRuiShortcutProcessor : public IInputProcessor
	{
	public:
		FRuiShortcutProcessor(const RUI::Slate::FRuiShortcut& InChord, TSharedRef<TRuiRef<TFunction<void()>>> InBox)
			: Chord(InChord), Box(MoveTemp(InBox))
		{
		}

		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			if (Chord.Matches(Event) && Box->Current)
			{
				Box->Current();
				return true;
			}
			return false;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("RuiShortcut"); }

	private:
		RUI::Slate::FRuiShortcut Chord;
		TSharedRef<TRuiRef<TFunction<void()>>> Box;
	};
} // namespace

void RUI::Slate::UseShortcut(FRuiContext& Ctx, const FRuiShortcut& Chord, TFunction<void()> OnTrigger)
{
	// A stable box holding the LATEST callback, refreshed each render — the pre-processor always
	// fires the current closure, so the effect re-registers only when the CHORD itself changes.
	TSharedRef<TRuiRef<TFunction<void()>>> Box = Ctx.UseRef<TFunction<void()>>(TFunction<void()>());
	Box->Current = MoveTemp(OnTrigger);

	Ctx.UseEffect(
		[Chord, Box]() -> FRuiEffectCleanup
		{
			if (!FSlateApplication::IsInitialized())
			{
				return FRuiEffectCleanup();
			}
			TSharedRef<FRuiShortcutProcessor> Proc = MakeShared<FRuiShortcutProcessor>(Chord, Box);
			FSlateApplication::Get().RegisterInputPreProcessor(Proc);
			return [Proc]()
			{
				if (FSlateApplication::IsInitialized())
				{
					FSlateApplication::Get().UnregisterInputPreProcessor(Proc);
				}
			};
		},
		RUI::Deps(Chord.DepKey()));
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// TD-022 — focus extensions
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	/** The SWidget behind a host handle (null-safe). */
	TSharedPtr<SWidget> WidgetOf(const FRuiHostHandle& Handle)
	{
		if (FRuiSlateNode* Node = FRuiSlateHost::Resolve(Handle))
		{
			return Node->Widget;
		}
		return nullptr;
	}
} // namespace

void RUI::Slate::FocusWidget(const FRuiHostHandle& Handle)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	if (TSharedPtr<SWidget> Widget = WidgetOf(Handle))
	{
		FSlateApplication::Get().SetUserFocus(FSlateApplication::CursorUserIndex, Widget, EFocusCause::SetDirectly);
	}
}

void RUI::Slate::ClearFocus()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ClearUserFocus(FSlateApplication::CursorUserIndex, EFocusCause::SetDirectly);
	}
}

RUI::Slate::FRuiFocusHandle RUI::Slate::UseFocus(FRuiContext& Ctx)
{
	// A stable weak box the ref keeps in sync (attach -> widget, detach -> null); Focus/IsFocused
	// read it. Ref/Focus/IsFocused capture the same box, so they stay valid for the component's life.
	TSharedRef<TRuiRef<TWeakPtr<SWidget>>> Box = Ctx.UseRef<TWeakPtr<SWidget>>();

	FRuiFocusHandle Handle;
	Handle.Ref = [Box](const FRuiHostHandle& H) { Box->Current = WidgetOf(H); };
	Handle.Focus = [Box]()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		if (TSharedPtr<SWidget> Widget = Box->Current.Pin())
		{
			FSlateApplication::Get().SetUserFocus(FSlateApplication::CursorUserIndex, Widget, EFocusCause::SetDirectly);
		}
	};
	Handle.IsFocused = [Box]() -> bool
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}
		const TSharedPtr<SWidget> Widget = Box->Current.Pin();
		return Widget.IsValid() &&
			   FSlateApplication::Get().GetUserFocusedWidget(FSlateApplication::CursorUserIndex) == Widget;
	};
	return Handle;
}
