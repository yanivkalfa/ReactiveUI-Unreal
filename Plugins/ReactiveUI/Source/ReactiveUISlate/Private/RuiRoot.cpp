// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiRoot.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "RuiSlateElements.h"
#include "RuiSlateLog.h"
#include "Widgets/SWindow.h"

void SRuiRoot::Construct(const FArguments&)
{
	// clang-format off
	ChildSlot
	[
		SAssignNew(RootPanel, SOverlay)
	];
	// clang-format on
}

TSharedRef<FRuiRoot> FRuiRoot::CreateDetachedInternal(FRuiNode RootNode)
{
	TSharedRef<FRuiRoot> Root = MakeShareable(new FRuiRoot());
	Root->Host = MakeUnique<FRuiSlateHost>();
	Root->Widget = SNew(SRuiRoot);
	const FRuiHostHandle RootHandle =
		Root->Host->WrapExternalPanel(Root->Widget->GetRootPanel(), RUI::Slate::OverlayType());
	Root->Reconciler = MakeUnique<FRuiReconciler>(*Root->Host, RootHandle);
	Root->Reconciler->Render(MoveTemp(RootNode));
	return Root;
}

TSharedRef<FRuiRoot> FRuiRoot::Create(FRuiNode RootNode)
{
	return CreateDetachedInternal(MoveTemp(RootNode));
}

TSharedRef<FRuiRoot> FRuiRoot::CreateInViewport(FRuiNode RootNode, int32 ZOrder)
{
	TSharedRef<FRuiRoot> Root = CreateDetachedInternal(MoveTemp(RootNode));
	UGameViewportClient* Viewport = GEngine != nullptr ? GEngine->GameViewport : nullptr;
	if (Viewport == nullptr)
	{
		UE_LOG(LogRuiSlate, Error,
			   TEXT("[ReactiveUI] CreateInViewport: no game viewport (PIE not running?) — root left detached"));
		return Root;
	}
	Viewport->AddViewportWidgetContent(Root->GetWidget(), ZOrder);
	Root->bMountedInViewport = true;
	Root->ViewportZOrder = ZOrder;
	return Root;
}

TSharedRef<FRuiRoot> FRuiRoot::CreateInWindow(const TSharedRef<SWindow>& Window, FRuiNode RootNode)
{
	TSharedRef<FRuiRoot> Root = CreateDetachedInternal(MoveTemp(RootNode));
	Window->SetContent(Root->GetWidget());
	Root->MountedWindow = Window;
	return Root;
}

FRuiRoot::~FRuiRoot()
{
	Unmount();
}

void FRuiRoot::Update(FRuiNode RootNode)
{
	if (Reconciler.IsValid())
	{
		Reconciler->Render(MoveTemp(RootNode));
	}
}

void FRuiRoot::FlushSync()
{
	if (Reconciler.IsValid())
	{
		Reconciler->FlushSync();
	}
}

void FRuiRoot::Unmount()
{
	if (Reconciler.IsValid() && Reconciler->IsMounted())
	{
		Reconciler->Unmount();
	}
	if (bMountedInViewport)
	{
		bMountedInViewport = false;
		if (GEngine != nullptr && GEngine->GameViewport != nullptr && Widget.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
	}
	if (TSharedPtr<SWindow> Window = MountedWindow.Pin())
	{
		MountedWindow.Reset();
		Window->SetContent(SNullWidget::NullWidget);
	}
}
