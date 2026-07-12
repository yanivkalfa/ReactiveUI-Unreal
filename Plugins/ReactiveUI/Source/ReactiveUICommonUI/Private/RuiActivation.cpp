// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-021 — the activation context mechanism. ActivationProvider is a plain ReactiveUI component that
// provides FRuiActivationState to its children; the Use* hooks read it. No UObject dependency here —
// this is the fully headless-testable half of the CommonUI seam.

#include "RuiActivation.h"

#include "RuiContext.h"

TRuiContext<FRuiActivationState>& RUI::CommonUI::ActivationContext()
{
	static TRuiContext<FRuiActivationState> Ctx(FRuiActivationState{}, FName(TEXT("RuiActivation")));
	return Ctx;
}

namespace
{
	/** Props for the provider component: the state to publish. Equality drives re-provision — a state
	 *  change re-renders the provider, which re-provides the context and dirties consumers. */
	struct FRuiActivationProviderProps final : public FRuiPropsBase
	{
		FRuiActivationState State;

		virtual bool Equals(const FRuiPropsBase& Other) const override
		{
			const FRuiActivationProviderProps& O = static_cast<const FRuiActivationProviderProps&>(Other);
			return BaseFieldsEqual(Other) && State == O.State;
		}
	};

	FRuiNodeArray ActivationProviderComp(FRuiContext& Ctx, const FRuiActivationProviderProps& Props,
										 const TArray<FRuiNode>& Children)
	{
		Ctx.ProvideContext(RUI::CommonUI::ActivationContext(), Props.State);
		return FRuiNodeArray(Children);
	}
	RUI_COMPONENT(ActivationProviderComp)
} // namespace

FRuiActivationState RUI::CommonUI::UseActivation(FRuiContext& Ctx)
{
	return Ctx.UseContext(ActivationContext());
}

bool RUI::CommonUI::UseIsActive(FRuiContext& Ctx)
{
	return UseActivation(Ctx).bActive;
}

ERuiInputMethod RUI::CommonUI::UseInputMethod(FRuiContext& Ctx)
{
	return UseActivation(Ctx).InputMethod;
}

FRuiNode RUI::CommonUI::ActivationProvider(FRuiActivationState State, TArray<FRuiNode> Children, FRuiKey Key)
{
	FRuiActivationProviderProps Props;
	Props.State = State;
	return RUI::FC(&ActivationProviderComp, MoveTemp(Props), MoveTemp(Children), Key);
}
