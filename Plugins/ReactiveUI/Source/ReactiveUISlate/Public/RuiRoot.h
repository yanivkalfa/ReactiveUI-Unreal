// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// SRuiRoot + FRuiRoot — the mount surfaces (family: ReactiveRoot.create; hold the returned
// root for the UI's lifetime, Unmount() runs every cleanup). SRuiRoot is a plain compound
// widget whose inner overlay hosts the reconciler's top-level children; FRuiRoot owns the
// host + reconciler + widget and wires the three mount surfaces: detached (tests/tools),
// game viewport, and an SWindow. (The editor-tab surface ships with Phase 8's Inspector.)

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiReconciler.h"
#include "RuiSlateHost.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"

class SWindow;

class REACTIVEUISLATE_API SRuiRoot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuiRoot) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedRef<SOverlay> GetRootPanel() const { return RootPanel.ToSharedRef(); }

private:
	TSharedPtr<SOverlay> RootPanel;
};

/**
 * A mounted ReactiveUI root. Create*() renders synchronously (no empty first frame).
 * Keep the returned shared ref alive for the UI's lifetime; Unmount() (or destruction)
 * tears down: effect cleanups, refs nulled, widgets detached, fibers freed.
 */
class REACTIVEUISLATE_API FRuiRoot
{
public:
	/** Detached: the SRuiRoot widget exists but is not parented anywhere (tests, tools,
	 *  callers that place GetWidget() themselves). */
	static TSharedRef<FRuiRoot> Create(FRuiNode RootNode);

	/** Game viewport overlay (AddViewportWidgetContent). Requires a live GameViewport —
	 *  logs an error and returns a detached root otherwise. */
	static TSharedRef<FRuiRoot> CreateInViewport(FRuiNode RootNode, int32 ZOrder = 10);

	/** Fill an existing SWindow's content slot. */
	static TSharedRef<FRuiRoot> CreateInWindow(const TSharedRef<SWindow>& Window, FRuiNode RootNode);

	~FRuiRoot();
	FRuiRoot(const FRuiRoot&) = delete;
	FRuiRoot& operator=(const FRuiRoot&) = delete;

	/** Replace the root vnode (top-level re-render; synchronous). */
	void Update(FRuiNode RootNode);

	/** Run any pending coalesced work NOW (tests, HMR, teardown fences). */
	void FlushSync();

	/** Full teardown. Idempotent; also detaches from the viewport/window surface. */
	void Unmount();

	bool IsMounted() const { return Reconciler.IsValid() && Reconciler->IsMounted(); }

	TSharedRef<SRuiRoot> GetWidget() const { return Widget.ToSharedRef(); }
	FRuiReconciler& GetReconciler() { return *Reconciler; }
	FRuiSlateHost& GetHost() { return *Host; }

private:
	FRuiRoot() = default;
	static TSharedRef<FRuiRoot> CreateDetachedInternal(FRuiNode RootNode);

	// Host must outlive the reconciler (the reconciler holds IRuiHostConfig&); members
	// destroy in reverse declaration order, so keep this order.
	TUniquePtr<FRuiSlateHost> Host;
	TUniquePtr<FRuiReconciler> Reconciler;
	TSharedPtr<SRuiRoot> Widget;

	/** Which surface we attached to (for Unmount detach). */
	TWeakPtr<SWindow> MountedWindow;
	bool bMountedInViewport = false;
	int32 ViewportZOrder = 0;
};
