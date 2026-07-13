// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "DemoUmgWidget.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

UDemoUmgWidget::UDemoUmgWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UDemoUmgWidget::RebuildWidget()
{
	return SNew(SBorder)
		.Padding(FMargin(10.0f))
		.ColorAndOpacity(FLinearColor(
			0.15f, 0.35f, 0.55f,
			1.0f))[SNew(STextBlock)
					   .Text(NSLOCTEXT("RuiDemo", "UmgInner",
									   "▲ This is a real UMG UserWidget, hosted inside the ReactiveUI tree"))];
}
