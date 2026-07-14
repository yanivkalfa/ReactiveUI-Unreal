// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiHostWidget.h"

#include "RuiNode.h"
#include "RuiRoot.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> URuiHostWidget::BuildContent()
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

TSharedRef<SWidget> URuiHostWidget::RebuildWidget()
{
	// The stable wrapper: UMG caches THIS widget; Remount swaps its content in place.
	Container = SNew(SBox)[BuildContent()];
	return Container.ToSharedRef();
}

void URuiHostWidget::Remount()
{
	if (Root.IsValid())
	{
		Root->Unmount();
		Root.Reset();
	}
	if (Container.IsValid())
	{
		Container->SetContent(BuildContent()); // in place — the parent slot keeps the wrapper
	}
	InvalidateLayoutAndVolatility();
}

void URuiHostWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (Root.IsValid())
	{
		Root->Unmount(); // cleanups run BEFORE the Slate tree is released (family order)
		Root.Reset();
	}
	Container.Reset();
}

#if WITH_EDITOR
const FText URuiHostWidget::GetPaletteCategory()
{
	return NSLOCTEXT("ReactiveUI", "PaletteCategory", "ReactiveUI");
}
#endif
