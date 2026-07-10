// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// IRuiHostConfig — THE seam (D-11). The reconciler touches engines only through this
// interface over opaque FRuiHostHandle values. ReactiveUISlate implements it against real
// widgets; FRuiMockHost implements it for the headless core suites — which is what makes
// the entire reconciler testable without an RHI, exactly like react-dom vs react-test-renderer.
//
// Shape follows React's mutation-mode HostConfig where it earns its keep and the family
// host where Slate reality differs (child adapters, apply-diff-in-commit — React 19 style,
// no prepareUpdate).

#pragma once

#include "CoreMinimal.h"
#include "RuiTypes.h"
#include "RuiPropsBase.h"

class REACTIVEUICORE_API IRuiHostConfig
{
public:
	virtual ~IRuiHostConfig() = default;

	// --- instances ---------------------------------------------------------------------

	/** Create a host node for the interned element type with its full initial props applied
	 *  (construct-only args consumed here; then a full apply — the adapters' CreateWidget). */
	virtual FRuiHostHandle CreateInstance(FRuiElementTypeId Type, const FRuiPropsBase& Props) = 0;

	/** Diff-apply in commit (React-19 commitUpdate — no prepareUpdate payloads). OldProps may
	 *  be null on pool reuse with no stashed props. */
	virtual void CommitUpdate(const FRuiHostHandle& Node, FRuiElementTypeId Type, const FRuiPropsBase* OldProps,
							  const FRuiPropsBase& NewProps) = 0;

	/** Destroy or pool the node (host's choice — GO-05 pooling lives host-side; the
	 *  reconciler only promises the node is detached and done). bWasChildless lets the host
	 *  apply the family's pool-only-childless-leaves rule without re-walking. */
	virtual void ReleaseInstance(const FRuiHostHandle& Node, FRuiElementTypeId Type, const FRuiPropsBase* LastProps,
								 bool bWasChildless) = 0;

	// --- tree mutation -----------------------------------------------------------------

	/** Append/insert Child under Parent (Parent null = the root container). Index < 0 =
	 *  APPEND — the reconciler's placement path always appends and relies on
	 *  ReorderChildren on structural frames for exact order (the family model); precise
	 *  indices arrive only from hosts' own internal uses. Single-content containers
	 *  enforce/warn capacity themselves (warn_capacity). */
	virtual void InsertChild(const FRuiHostHandle& Parent, const FRuiHostHandle& Child, int32 Index) = 0;

	/** Remove Child from Parent (Parent null = root container). Never destroys — Release does. */
	virtual void RemoveChild(const FRuiHostHandle& Parent, const FRuiHostHandle& Child) = 0;

	/** Re-assert the full child order under Parent (the enforce-order fallback; hosts that
	 *  can move precisely may optimize). Called only on structural frames (family rule). */
	virtual void ReorderChildren(const FRuiHostHandle& Parent, const TArray<FRuiHostHandle>& Ordered) = 0;

	/** Portal support: insert/remove against an out-of-tree target handle. Defaults to the
	 *  normal child ops (targets ARE host handles); hosts may specialize. */
	virtual void InsertPortalChild(const FRuiPortalHandle& Target, const FRuiHostHandle& Child, int32 Index)
	{
		InsertChild(Target, Child, Index);
	}
	virtual void RemovePortalChild(const FRuiPortalHandle& Target, const FRuiHostHandle& Child)
	{
		RemoveChild(Target, Child);
	}

	// --- text (family: raw string children auto-wrap to text elements) -------------------

	/** The host's text element type (STextBlock / mock text). Invalid id = host has none. */
	virtual FRuiElementTypeId GetTextElementType() const = 0;

	// --- scheduling ----------------------------------------------------------------------

	/** Schedule Callback once before the next frame's paint (OnPreTick in Slate; manual pump
	 *  in the mock). The reconciler coalesces all updates into one such callback. */
	virtual void RequestFrame(TFunction<void()> Callback) = 0;

	/** Host-supplied platform data for UseSafeArea (mock returns a test value; Slate host
	 *  reads FDisplayMetrics; CommonUI overrides in Phase 6 — the D-27 stub-owner chain). */
	virtual void GetSafeArea(float& OutLeft, float& OutTop, float& OutRight, float& OutBottom) const
	{
		OutLeft = OutTop = OutRight = OutBottom = 0.0f;
	}
};
