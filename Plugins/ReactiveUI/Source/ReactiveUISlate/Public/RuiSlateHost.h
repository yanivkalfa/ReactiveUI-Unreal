// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuiSlateHost — IRuiHostConfig over real Slate widgets (D-11): the ONLY runtime seam
// between the reconciler and concrete Slate APIs. Generic by construction: everything
// widget-specific routes through the adapter registry. FRuiSlateNode is what an
// FRuiHostHandle points at on this host.
//
// Scheduling: RequestFrame callbacks queue and drain once per Slate OnPreTick (registered
// lazily, unregistered on destruction). NEVER executed synchronously — a hook setter must
// not re-enter the reconciler from its own call stack (the mid-render restart contract).

#pragma once

#include "CoreMinimal.h"
#include "RuiElementAdapter.h"
#include "RuiEventProxy.h"
#include "RuiHostConfig.h"
#include "Widgets/SWidget.h"

/** One mounted widget: the concrete payload behind FRuiHostHandle on the Slate host. */
struct REACTIVEUISLATE_API FRuiSlateNode
{
	TSharedPtr<SWidget> Widget;
	TSharedPtr<FRuiEventProxy> Proxy; // minted only when the adapter binds events
	IRuiElementAdapter* Adapter = nullptr;
	FRuiElementTypeId Type;

	/** Latest committed slot.* props (owned copy — parents re-apply on reorder/updates). */
	TSharedPtr<FRuiStyleDict> SlotProps;

	/** The EFFECTIVE style dict last applied (classes merged + inline) — the diff base. */
	TSharedPtr<FRuiStyleDict> AppliedStyle;

	/** The parent NODE while attached (child ops + slot-prop updates go via its adapter). */
	TWeakPtr<FRuiSlateNode> ParentNode;

	/** SingleContent bookkeeping: the current content child (warn_capacity semantics). */
	TWeakPtr<SWidget> ContentChild;
	bool bWarnedCapacity = false;
};

class REACTIVEUISLATE_API FRuiSlateHost final : public IRuiHostConfig
{
public:
	FRuiSlateHost();
	virtual ~FRuiSlateHost() override;

	FRuiSlateHost(const FRuiSlateHost&) = delete;
	FRuiSlateHost& operator=(const FRuiSlateHost&) = delete;

	/** Wrap an existing panel widget (the mount surface's root) as a host node so the
	 *  reconciler can parent top-level children into it. */
	FRuiHostHandle WrapExternalPanel(const TSharedRef<SWidget>& Panel, FRuiElementTypeId PanelType);

	static FRuiSlateNode* Resolve(const FRuiHostHandle& Handle) { return static_cast<FRuiSlateNode*>(Handle.Get()); }

	// ── IRuiHostConfig ────────────────────────────────────────────────────────────────────
	virtual FRuiHostHandle CreateInstance(FRuiElementTypeId Type, const FRuiPropsBase& Props) override;
	virtual void CommitUpdate(const FRuiHostHandle& Node, FRuiElementTypeId Type, const FRuiPropsBase* OldProps,
							  const FRuiPropsBase& NewProps) override;
	virtual void ReleaseInstance(const FRuiHostHandle& Node, FRuiElementTypeId Type,
								 const TSharedPtr<const FRuiPropsBase>& LastProps, bool bWasChildless) override;
	virtual void OnBeforeCommit() override;
	virtual void OnAfterCommit() override;
	virtual void InsertChild(const FRuiHostHandle& Parent, const FRuiHostHandle& Child, int32 Index) override;
	virtual void RemoveChild(const FRuiHostHandle& Parent, const FRuiHostHandle& Child) override;
	virtual void ReorderChildren(const FRuiHostHandle& Parent, const TArray<FRuiHostHandle>& Ordered) override;
	virtual FRuiElementTypeId GetTextElementType() const override;
	virtual void RequestFrame(TFunction<void()> Callback) override;
	virtual void GetSafeArea(float& OutLeft, float& OutTop, float& OutRight, float& OutBottom) const override;

	/** Drain the frame queue now (tests / FlushSync surfaces — same batch rule as PreTick:
	 *  callbacks queued while draining run on the NEXT drain). */
	void PumpFrameQueue();

	int32 NumPooled(FRuiElementTypeId Type) const;

private:
	void OnSlatePreTick(float DeltaTime);
	void EnsurePreTickRegistered();
	static void RemoveChildFromParent(FRuiSlateNode& Parent, const TSharedRef<SWidget>& ChildWidget);

	TArray<TFunction<void()>> FrameQueue;
	FDelegateHandle PreTickHandle;
	bool bPreTickRegistered = false;
	bool bWarnedNoSlateApp = false;

	/** GO-05 node pool: detached event-less leaves, stashed with their last props + applied
	 *  style, diffed on reuse. Per-type cap; drained with the host. */
	struct FPoolEntry
	{
		TSharedPtr<SWidget> Widget;
		TSharedPtr<const FRuiPropsBase> LastProps;
		TSharedPtr<FRuiStyleDict> AppliedStyle;
	};
	TMap<FRuiElementTypeId, TArray<FPoolEntry>> Pool;
	static constexpr int32 PoolCapPerType = 256;

	/** Focus capture between the commit fences (per-commit; cleared after restore). */
	TWeakPtr<SWidget> FocusedBeforeCommit;
};
