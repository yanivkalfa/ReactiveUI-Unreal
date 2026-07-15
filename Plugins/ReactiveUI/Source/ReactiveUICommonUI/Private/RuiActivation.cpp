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

// ── TD-029: desired focus ─────────────────────────────────────────────────────────────────

TRuiContext<TSharedPtr<FRuiFocusTargetRegistry>>& RUI::CommonUI::FocusTargetContext()
{
	static TRuiContext<TSharedPtr<FRuiFocusTargetRegistry>> Ctx(TSharedPtr<FRuiFocusTargetRegistry>(),
																FName(TEXT("RuiFocusTarget")));
	return Ctx;
}

namespace
{
	struct FRuiFocusTargetProviderProps final : public FRuiPropsBase
	{
		TSharedPtr<FRuiFocusTargetRegistry> Registry;

		virtual bool Equals(const FRuiPropsBase& Other) const override
		{
			const FRuiFocusTargetProviderProps& O = static_cast<const FRuiFocusTargetProviderProps&>(Other);
			return BaseFieldsEqual(Other) && Registry == O.Registry;
		}
	};

	FRuiNodeArray FocusTargetProviderComp(FRuiContext& Ctx, const FRuiFocusTargetProviderProps& Props,
										  const TArray<FRuiNode>& Children)
	{
		Ctx.ProvideContext(RUI::CommonUI::FocusTargetContext(), Props.Registry);
		return FRuiNodeArray(Children);
	}
	RUI_COMPONENT(FocusTargetProviderComp)
} // namespace

void RUI::CommonUI::UseDesiredFocus(FRuiContext& Ctx, TFunction<void()> FocusAction)
{
	const TSharedPtr<FRuiFocusTargetRegistry> Registry = Ctx.UseContext(FocusTargetContext());
	// One effect slot, re-run every commit: the render phase stays pure, the LATEST action wins
	// (it may capture fresh state), and the cleanup clears the designation on unmount. The weak
	// capture never extends the screen-owned registry's lifetime.
	TWeakPtr<FRuiFocusTargetRegistry> Weak = Registry;
	Ctx.UseEffect(
		[Weak, Action = MoveTemp(FocusAction)]() -> TFunction<void()>
		{
			if (const TSharedPtr<FRuiFocusTargetRegistry> Pinned = Weak.Pin())
			{
				Pinned->FocusDesired = Action;
			}
			return [Weak]()
			{
				if (const TSharedPtr<FRuiFocusTargetRegistry> Pinned = Weak.Pin())
				{
					Pinned->FocusDesired = nullptr;
				}
			};
		},
		RUI::EveryCommit());
}

FRuiNode RUI::CommonUI::FocusTargetProvider(TSharedPtr<FRuiFocusTargetRegistry> Registry, TArray<FRuiNode> Children,
											FRuiKey Key)
{
	FRuiFocusTargetProviderProps Props;
	Props.Registry = MoveTemp(Registry);
	return RUI::FC(&FocusTargetProviderComp, MoveTemp(Props), MoveTemp(Children), Key);
}
