// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// A headless Slate INTERACTION harness for the automation suites. The unit suites drive widgets
// through their public props/adapters, but a few widgets (numeric entry, combo/suggestion dropdowns)
// only reveal their behaviour through a live window, an arranged geometry, an inner sub-widget, or a
// pushed menu. This header collects the small pieces that make those verifiable headless — all built
// on the same primitives the Focus/Shortcut/ListView suites already use (a real SWindow + SlatePrepass,
// geometry-driven Tick, FSlateApplication input). Test-only; lives in the RuiHostTests private tree.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Geometry.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

namespace RuiTest
{
	/** Depth-first search for the first descendant (inclusive of Root) whose widget type name equals
	 *  `TypeName` (SWidget::GetType()). RTTI is off in engine builds, so type identity — not
	 *  dynamic_cast — is how Slate itself distinguishes widgets. Returns null when none matches. */
	inline SWidget* FindDescendantByType(SWidget& Root, FName TypeName)
	{
		if (Root.GetType() == TypeName)
		{
			return &Root;
		}
		FChildren* Children = Root.GetChildren();
		if (Children != nullptr)
		{
			for (int32 i = 0; i < Children->Num(); ++i)
			{
				if (SWidget* Hit = FindDescendantByType(Children->GetChildAt(i).Get(), TypeName))
				{
					return Hit;
				}
			}
		}
		return nullptr;
	}

	/** Typed convenience: find a descendant of the given Slate type by its class name. `WidgetType`
	 *  must be the concrete S-class; `TypeName` its GetType() name (e.g. "SEditableText"). */
	template <typename WidgetType> WidgetType* FindDescendant(const TSharedRef<SWidget>& Root, FName TypeName)
	{
		return static_cast<WidgetType*>(FindDescendantByType(Root.Get(), TypeName));
	}

	/** Search EVERY visible window (menus/popups are separate windows) for the first descendant of
	 *  the named type — the way to reach a widget that lives in a pushed menu. */
	inline SWidget* FindInAllWindowsByType(FName TypeName)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
		for (const TSharedRef<SWindow>& Window : Windows)
		{
			if (SWidget* Hit = FindDescendantByType(Window.Get(), TypeName))
			{
				return Hit;
			}
		}
		return nullptr;
	}

	/** Diagnostic: collect the type names of every descendant of Root (deduped) into OutTypes. */
	inline void CollectDescendantTypes(SWidget& Root, TSet<FName>& OutTypes)
	{
		OutTypes.Add(Root.GetType());
		if (FChildren* Children = Root.GetChildren())
		{
			for (int32 i = 0; i < Children->Num(); ++i)
			{
				CollectDescendantTypes(Children->GetChildAt(i).Get(), OutTypes);
			}
		}
	}

	/** Diagnostic: a comma-joined list of every widget type visible across ALL windows. */
	inline FString DumpAllWindowTypes()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FString();
		}
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
		TSet<FName> Types;
		for (const TSharedRef<SWindow>& Window : Windows)
		{
			CollectDescendantTypes(Window.Get(), Types);
		}
		TArray<FString> Names;
		for (const FName& T : Types)
		{
			Names.Add(T.ToString());
		}
		Names.Sort();
		return FString::Join(Names, TEXT(","));
	}

	/** Diagnostic: the number of currently visible top-level windows (main + menus/popups). */
	inline int32 NumVisibleWindows()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return 0;
		}
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
		return Windows.Num();
	}

	/**
	 * A live test window: a real SWindow hosting `Content`, added to the Slate application (hidden)
	 * and pre-passed so geometry-dependent widgets arrange. Destroys the window on scope exit. Use it
	 * when a widget needs a genuine window context (focus, menus, editable text). No-op-safe fields
	 * when Slate is not initialised — check IsValid() first.
	 */
	struct FTestWindow
	{
		TSharedPtr<SWindow> Window;

		explicit FTestWindow(const TSharedRef<SWidget>& Content, FVector2D Size = FVector2D(400, 300))
		{
			if (!FSlateApplication::IsInitialized())
			{
				return;
			}
			Window = SNew(SWindow).ClientSize(Size);
			FSlateApplication::Get().AddWindow(Window.ToSharedRef(), /*bShowImmediately*/ false);
			Window->SetContent(Content);
			Window->SlatePrepass(1.0f);
		}

		~FTestWindow()
		{
			if (Window.IsValid() && FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
			}
		}

		bool IsValid() const { return Window.IsValid(); }

		/** Prepass the hosted content so its widgets construct/arrange and attribute-bound values
		 *  (e.g. a numeric field's display text) resolve. Widgets whose content generates lazily
		 *  under a Tick (SListView rows, pushed menus) are ticked directly by the caller. */
		void PumpGeometry() const
		{
			if (Window.IsValid())
			{
				Window->SlatePrepass(1.0f);
			}
		}
	};

	/** Tick a single widget with a synthetic geometry (for widgets driven outside a window). */
	inline void PumpWidget(const TSharedRef<SWidget>& Widget, FVector2D Size = FVector2D(400, 300))
	{
		const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Widget->SlatePrepass(1.0f);
			Widget->Tick(Geometry, 0.0, 0.0f);
		}
	}
} // namespace RuiTest
