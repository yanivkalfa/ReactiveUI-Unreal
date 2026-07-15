// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-028 — the host-props context mechanism (see RuiHostProps.h). Mirrors the CommonUI
// activation seam: a provider component publishes the state; equality gates re-provision.

#include "RuiHostProps.h"

#include "RuiContext.h"

TRuiContext<FRuiHostPropsState>& RUI::Umg::HostPropsContext()
{
	static TRuiContext<FRuiHostPropsState> Ctx(FRuiHostPropsState{}, FName(TEXT("RuiHostProps")));
	return Ctx;
}

namespace
{
	struct FRuiHostPropsProviderProps final : public FRuiPropsBase
	{
		FRuiHostPropsState State;

		virtual bool Equals(const FRuiPropsBase& Other) const override
		{
			const FRuiHostPropsProviderProps& O = static_cast<const FRuiHostPropsProviderProps&>(Other);
			return BaseFieldsEqual(Other) && State == O.State;
		}
	};

	FRuiNodeArray HostPropsProviderComp(FRuiContext& Ctx, const FRuiHostPropsProviderProps& Props,
										const TArray<FRuiNode>& Children)
	{
		Ctx.ProvideContext(RUI::Umg::HostPropsContext(), Props.State);
		return FRuiNodeArray(Children);
	}
	RUI_COMPONENT(HostPropsProviderComp)
} // namespace

FRuiHostPropsState RUI::Umg::UseHostProps(FRuiContext& Ctx)
{
	return Ctx.UseContext(HostPropsContext());
}

FString RUI::Umg::UseHostProp(FRuiContext& Ctx, FName Name, FString Default)
{
	const FRuiHostPropsState State = UseHostProps(Ctx);
	if (const FString* Found = State.Props.Find(Name))
	{
		return *Found;
	}
	return Default;
}

UObject* RUI::Umg::UseHostViewModel(FRuiContext& Ctx)
{
	return UseHostProps(Ctx).ViewModel.Get();
}

FRuiNode RUI::Umg::HostPropsProvider(FRuiHostPropsState State, TArray<FRuiNode> Children, FRuiKey Key)
{
	FRuiHostPropsProviderProps Props;
	Props.State = MoveTemp(State);
	return RUI::FC(&HostPropsProviderComp, MoveTemp(Props), MoveTemp(Children), Key);
}
