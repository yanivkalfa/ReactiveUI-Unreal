// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiUmgElement.h"

#include "Blueprint/UserWidget.h"
#include "RuiElementAdapter.h"
#include "RuiSlateHost.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

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
			// TakeWidget -> SObjectWidget, which holds the strong UObject reference: the hosted
			// widget lives exactly as long as its Slate representation (UMG's own contract).
			return Widget->TakeWidget();
		}

		virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override
		{
			// The hosted widget manages its own state; class/world changes reconstruct instead.
		}

		virtual uint64 GetReconstructMask() const override
		{
			return 0b11; // WidgetClass + World are construct-only
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

	FRuiNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuiKey Key)
	{
		RegisterUmgAdapters();
		FRuiUmgProps Props;
		Props.SetWidgetClass(MoveTemp(WidgetClass));
		Props.SetWorld(World);
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = UmgElementType();
		Node.Props = MakeShared<FRuiUmgProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}
} // namespace RUI::Umg
