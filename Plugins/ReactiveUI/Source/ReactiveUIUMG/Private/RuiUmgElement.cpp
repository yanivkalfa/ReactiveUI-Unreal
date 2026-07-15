// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiUmgElement.h"

#include "Blueprint/UserWidget.h"
#include "RuiElementAdapter.h"
#include "RuiMarshal.h"
#include "RuiSlateHost.h"
#include "Slate/SObjectWidget.h"
#include "UObject/UnrealType.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

int32 RUI::Umg::ApplyPropMap(UUserWidget* Widget, const FRuiStyleDict& WidgetProps)
{
	if (Widget == nullptr || WidgetProps.Num() == 0)
	{
		return 0;
	}
	// One conversion table for every seam: the per-property dispatch lives in RuiMarshal
	// (MarshalToProperty — B13 kind-validation rules preserved there verbatim).
	int32 Applied = 0;
	for (const TPair<FName, FRuiValue>& Pair : WidgetProps)
	{
		if (RUI::Umg::MarshalToProperty(Widget, Pair.Key, Pair.Value))
		{
			++Applied;
		}
	}
	// Push the new values into the widget's Slate representation — only once it has one (a hosted
	// widget after TakeWidget). Skips safely for a bare, un-constructed widget (direct tool/test use).
	if (Applied > 0 && Widget->GetCachedWidget().IsValid())
	{
		Widget->SynchronizeProperties();
	}
	return Applied;
}

namespace
{
	class FRuiUmgAdapter final : public IRuiElementAdapter
	{
	public:
		virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

		virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& PropsBase,
												 const TSharedPtr<FRuiEventProxy>&) override
		{
			const FRuiUmgProps& Props = static_cast<const FRuiUmgProps&>(PropsBase);
			UWorld* World = Props.World.Get();
			UClass* WidgetClass = Props.WidgetClass.Get();
			if (!World || !WidgetClass)
			{
				return SNew(STextBlock).Text(NSLOCTEXT("ReactiveUI", "UmgMissing", "<UMG: class/world unset>"));
			}
			UUserWidget* Widget = ::CreateWidget<UUserWidget>(World, TSubclassOf<UUserWidget>(WidgetClass));
			if (!Widget)
			{
				return SNew(STextBlock).Text(NSLOCTEXT("ReactiveUI", "UmgFailed", "<UMG: CreateWidget failed>"));
			}
			// Apply the initial prop map, then TakeWidget -> SObjectWidget (holds the strong UObject
			// ref: the hosted widget lives exactly as long as its Slate representation, UMG's contract).
			RUI::Umg::ApplyPropMap(Widget, Props.WidgetProps);
			return Widget->TakeWidget();
		}

		virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
		{
			// Re-apply the prop map on change: recover the hosted UUserWidget from its SObjectWidget.
			const FRuiUmgProps& N = static_cast<const FRuiUmgProps&>(New);
			const FRuiUmgProps* O = static_cast<const FRuiUmgProps*>(Old);
			if (!N.HasWidgetProps())
			{
				return;
			}
			if (O != nullptr && O->HasWidgetProps() && N.WidgetProps.OrderIndependentCompareEqual(O->WidgetProps))
			{
				return;
			}
			if (Widget.GetType() == FName(TEXT("SObjectWidget")))
			{
				if (UUserWidget* Hosted = static_cast<SObjectWidget&>(Widget).GetWidgetObject())
				{
					RUI::Umg::ApplyPropMap(Hosted, N.WidgetProps);
				}
			}
		}

		virtual uint64 GetReconstructMask() const override
		{
			return 0b11; // WidgetClass + World are construct-only; WidgetProps applies in place
		}

		virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
		{
			const FRuiUmgProps& O = static_cast<const FRuiUmgProps&>(Old);
			const FRuiUmgProps& N = static_cast<const FRuiUmgProps&>(New);
			return !(O.WidgetClass == N.WidgetClass) || !(O.World == N.World);
		}

		virtual bool IsPoolable() const override { return false; } // carries a live UObject
	};

	FRuiElementTypeId UmgElementType()
	{
		static const FRuiElementTypeId Id = RUI::InternElementType(FName(TEXT("UmgUserWidget")));
		return Id;
	}
} // namespace

namespace RUI::Umg
{
	void RegisterUmgAdapters()
	{
		static bool bOnce = false;
		if (bOnce)
		{
			return;
		}
		bOnce = true;
		RUI::Slate::RegisterAdapter(UmgElementType(), MakeUnique<FRuiUmgAdapter>());
	}

	static FRuiNode MakeUmgNode(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuiStyleDict WidgetProps,
								FRuiKey Key)
	{
		RegisterUmgAdapters();
		FRuiUmgProps Props;
		Props.SetWidgetClass(MoveTemp(WidgetClass));
		Props.SetWorld(World);
		if (WidgetProps.Num() > 0)
		{
			Props.SetWidgetProps(MoveTemp(WidgetProps));
		}
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = UmgElementType();
		Node.Props = MakeShared<FRuiUmgProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	FRuiNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuiKey Key)
	{
		return MakeUmgNode(MoveTemp(WidgetClass), World, FRuiStyleDict(), Key);
	}

	FRuiNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuiStyleDict WidgetProps, FRuiKey Key)
	{
		return MakeUmgNode(MoveTemp(WidgetClass), World, MoveTemp(WidgetProps), Key);
	}
} // namespace RUI::Umg
