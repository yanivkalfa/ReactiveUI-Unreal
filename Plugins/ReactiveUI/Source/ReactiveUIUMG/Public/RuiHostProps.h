// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-028 — the designer/Blueprint → component channel for the "ours in theirs" door.
// URuiHostWidget publishes its BP-set initial props (a name→string map) and its optional
// FieldNotify viewmodel into the hosted tree through this context; components read them with
// the hooks below. Same mechanism as the CommonUI activation seam (plain ReactiveUI context —
// provider node + Use* hooks), so it is headless-testable without a UMG designer in the loop.
//
//   URuiHostWidget      --provides-->  FRuiHostPropsState (via HostPropsProvider)
//   your component      --reads----->  UseHostProp / UseHostProps / UseHostViewModel
//
// The viewmodel arrives as a plain UObject* — hand it straight to RUI::Umg::UseField:
//   UObject* Vm = RUI::Umg::UseHostViewModel(Ctx);
//   const int32 Health = RUI::Umg::UseField<int32>(Ctx, Vm, "Health", 0);

#pragma once

#include "CoreMinimal.h"
#include "RuiContextHandle.h" // TRuiContext
#include "RuiNode.h"
#include "UObject/WeakObjectPtr.h"

class FRuiContext;

/** What a hosted tree can learn from its URuiHostWidget host. */
struct REACTIVEUIUMG_API FRuiHostPropsState
{
	/** Designer/BP-set initial props (stringly typed — the generic channel; parse in the component). */
	TMap<FName, FString> Props;
	/** The host-provided FieldNotify viewmodel (weak — the host UPROPERTY owns the strong ref). */
	TWeakObjectPtr<UObject> ViewModel;

	bool operator==(const FRuiHostPropsState& Other) const
	{
		return ViewModel == Other.ViewModel && Props.OrderIndependentCompareEqual(Other.Props);
	}
	bool operator!=(const FRuiHostPropsState& Other) const { return !(*this == Other); }
};

namespace RUI::Umg
{
	/** The context handle carrying host props to descendants (default = empty / no viewmodel). */
	REACTIVEUIUMG_API TRuiContext<FRuiHostPropsState>& HostPropsContext();

	/** The full host-props state at this point in the tree. */
	REACTIVEUIUMG_API FRuiHostPropsState UseHostProps(FRuiContext& Ctx);

	/** Sugar: one named host prop, or `Default` when the host didn't set it. */
	REACTIVEUIUMG_API FString UseHostProp(FRuiContext& Ctx, FName Name, FString Default = FString());

	/** Sugar: the host-provided viewmodel (nullptr when unset/collected) — feed it to UseField. */
	REACTIVEUIUMG_API UObject* UseHostViewModel(FRuiContext& Ctx);

	/** Provide `State` to `Children` (what URuiHostWidget wraps the hosted component in). */
	REACTIVEUIUMG_API FRuiNode HostPropsProvider(FRuiHostPropsState State,
												 TArray<FRuiNode> Children = TArray<FRuiNode>(),
												 FRuiKey Key = FRuiKey());
} // namespace RUI::Umg
