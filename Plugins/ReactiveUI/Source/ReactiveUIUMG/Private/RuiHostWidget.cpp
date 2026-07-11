// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiHostWidget.h"

#include "RuiNode.h"
#include "RuiRoot.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> URuiHostWidget::RebuildWidget()
{
	if (IsDesignTime())
	{
		// The designer must never run live component code — placeholder only.
		return SNew(STextBlock)
			.Text(FText::Format(NSLOCTEXT("ReactiveUI", "HostDesignTime", "[ReactiveUI: {0}]"),
								FText::FromName(ComponentName.IsNone() ? FName(TEXT("<unset>")) : ComponentName)));
	}
	if (ComponentName.IsNone() || !RUI::HasNamedFactory(ComponentName))
	{
		return SNew(STextBlock)
			.Text(FText::Format(
				NSLOCTEXT("ReactiveUI", "HostUnknown", "[ReactiveUI: '{0}' is not a registered component]"),
				FText::FromName(ComponentName)));
	}
	Root = FRuiRoot::Create(RUI::Named(ComponentName));
	Root->FlushSync();
	return Root->GetWidget();
}

void URuiHostWidget::Remount()
{
	if (Root.IsValid())
	{
		Root->Unmount();
		Root.Reset();
	}
	InvalidateLayoutAndVolatility();
	// UMG rebuilds the Slate widget lazily via TakeWidget on next access; forcing it here
	// keeps runtime callers deterministic.
	TakeWidget();
}

void URuiHostWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (Root.IsValid())
	{
		Root->Unmount(); // cleanups run BEFORE the Slate tree is released (family order)
		Root.Reset();
	}
}

#if WITH_EDITOR
const FText URuiHostWidget::GetPaletteCategory()
{
	return NSLOCTEXT("ReactiveUI", "PaletteCategory", "ReactiveUI");
}
#endif
