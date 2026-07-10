// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiCoreElements.h"
#include "RuiElementRegistry.h"

namespace RUI
{
	FRuiElementTypeId TextElementType()
	{
		static FRuiElementTypeId Id = InternElementType(FName(TEXT("Text")));
		return Id;
	}

	FRuiNode Text(FText InText, FRuiKey Key)
	{
		TSharedRef<FRuiTextProps> Props = MakeShared<FRuiTextProps>();
		Props->SetText(MoveTemp(InText));
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = TextElementType();
		Node.Props = Props;
		Node.Key = Key;
		return Node;
	}

	FRuiNode Text(const FString& InText, FRuiKey Key)
	{
		return Text(FText::FromString(InText), Key);
	}

	// ── Suspense (family polyfill: fallback until IsReady, poll-per-frame driver) ────────

	FRuiNodeArray SuspenseComponent(FRuiContext& Ctx, const FRuiSuspenseProps& Props, const TArray<FRuiNode>& Children)
	{
		auto [bReady, SetReady] = Ctx.UseState<bool>(false);

		// One driver per readiness source: poll each frame via the host's RequestFrame
		// until ready or torn down (the token dies with the cleanup).
		TFunction<bool()> IsReady = Props.IsReady;
		IRuiHostConfig* Host = &Ctx.GetHost();
		Ctx.UseEffect(
			[bReady = bReady, SetReady, IsReady, Host]() -> FRuiEffectCleanup
			{
				if (bReady || !IsReady)
				{
					return FRuiEffectCleanup();
				}
				if (IsReady()) // already satisfied — become ready synchronously
				{
					SetReady(true);
					return FRuiEffectCleanup();
				}
				TSharedRef<bool> Cancelled = MakeShared<bool>(false);
				// Self-re-arming per-frame poll.
				TSharedRef<TFunction<void()>> Poll = MakeShared<TFunction<void()>>();
				*Poll = [Cancelled, IsReady, SetReady, Host, Poll]()
				{
					if (*Cancelled)
					{
						return;
					}
					if (IsReady())
					{
						SetReady(true);
						return;
					}
					Host->RequestFrame(*Poll);
				};
				Host->RequestFrame(*Poll);
				return FRuiEffectCleanup([Cancelled]() { *Cancelled = true; });
			},
			RUI::Deps(bReady)); // re-arm when readiness flips (and tear down the stale driver)

		if (bReady)
		{
			return FRuiNodeArray(Children);
		}
		if (Props.Fallback.IsValid())
		{
			return FRuiNodeArray{*Props.Fallback};
		}
		return FRuiNodeArray();
	}

	FRuiNode Suspense(TFunction<bool()> IsReady, FRuiNode Fallback, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiSuspenseProps Props;
		Props.IsReady = MoveTemp(IsReady);
		Props.Fallback = MakeShared<FRuiNode>(MoveTemp(Fallback));
		return FC(&SuspenseComponent, MoveTemp(Props), MoveTemp(Children), Key);
	}
} // namespace RUI

// Direct registration (the RUI_COMPONENT macro can't token-paste a qualified name).
static const FName GRuiSuspenseComponentId =
	RUI::RegisterComponentId((void*)&RUI::SuspenseComponent, FName(TEXT("RUI.Suspense")));
