// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiExpandableButton.h"

#include "RuiEventProxy.h"
#include "Widgets/SNullWidget.h"

void SRuiExpandableButton::Construct(const FArguments& InArgs)
{
	SAssignNew(CollapsedBox, SBox);
	SAssignNew(ExpandedBox, SBox);
	SAssignNew(BodyBox, SBox);

	// clang-format off
	ChildSlot
	[
		SAssignNew(Button, SExpandableButton)
		.CollapsedText(InArgs._CollapsedText)
		.ExpandedText(InArgs._ExpandedText)
		.IsExpanded(InArgs._IsExpanded)
		.OnExpansionClicked(InArgs._OnExpansionClicked)
		.OnCloseClicked(InArgs._OnCloseClicked)
		.CollapsedButtonContent()
		[
			CollapsedBox.ToSharedRef()
		]
		.ExpandedButtonContent()
		[
			ExpandedBox.ToSharedRef()
		]
		.ExpandedChildContent()
		[
			BodyBox.ToSharedRef()
		]
	];
	// clang-format on
}

void SRuiExpandableButton::SetRoleContent(FName Role, const TSharedPtr<SWidget>& Content)
{
	const TSharedRef<SWidget> W = Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget;
	if (Role == FName(TEXT("collapsed")))
	{
		CollapsedBox->SetContent(W);
	}
	else if (Role == FName(TEXT("expanded")))
	{
		ExpandedBox->SetContent(W);
	}
	else
	{
		BodyBox->SetContent(W);
	}
}

void SRuiExpandableButton::ClearContent(const TSharedRef<SWidget>& Content)
{
	for (const TSharedPtr<SBox>& Holder : {CollapsedBox, ExpandedBox, BodyBox})
	{
		if (Holder->GetChildren()->Num() > 0 && &Holder->GetChildren()->GetChildAt(0).Get() == &Content.Get())
		{
			Holder->SetContent(SNullWidget::NullWidget);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	FName RoleOf(const FRuiStyleDict* SlotProps)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuiValue* V = SlotProps->Find(FName(TEXT("slot.role"))))
			{
				return V->Kind == FRuiValue::EKind::Name ? V->NameValue : FName(*V->StringValue);
			}
		}
		return FName(TEXT("body"));
	}

	class FRuiExpandableButtonAdapter final : public IRuiElementAdapter
	{
	public:
		virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }
		virtual bool HasEvents() const override { return true; }

		virtual uint64 GetReconstructMask() const override
		{
			return (1ull << FRuiExpandableButtonProps::CollapsedText_Bit) |
				   (1ull << FRuiExpandableButtonProps::ExpandedText_Bit) |
				   (1ull << FRuiExpandableButtonProps::bIsExpanded_Bit);
		}

		virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
		{
			const FRuiExpandableButtonProps& O = static_cast<const FRuiExpandableButtonProps&>(Old);
			const FRuiExpandableButtonProps& N = static_cast<const FRuiExpandableButtonProps&>(New);
			auto TextChanged = [](bool bNewHas, bool bOldHas, const FText& OldV, const FText& NewV)
			{ return bNewHas && (!bOldHas || !(NewV.IdenticalTo(OldV) || NewV.ToString() == OldV.ToString())); };
			return TextChanged(N.HasCollapsedText(), O.HasCollapsedText(), O.CollapsedText, N.CollapsedText) ||
				   TextChanged(N.HasExpandedText(), O.HasExpandedText(), O.ExpandedText, N.ExpandedText) ||
				   (N.HasbIsExpanded() && (!O.HasbIsExpanded() || O.bIsExpanded != N.bIsExpanded));
		}

		virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
												 const TSharedPtr<FRuiEventProxy>& Proxy) override
		{
			const FRuiExpandableButtonProps& P = static_cast<const FRuiExpandableButtonProps&>(Props);
			return SNew(SRuiExpandableButton)
				.CollapsedText(P.CollapsedText)
				.ExpandedText(P.ExpandedText)
				.IsExpanded(!P.HasbIsExpanded() || P.bIsExpanded)
				.OnExpansionClicked(
					FOnClicked::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleReply,
										 static_cast<int32>(FRuiExpandableButtonProps::OnExpansionClicked_Bit)))
				.OnCloseClicked(
					FOnClicked::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleReply,
										 static_cast<int32>(FRuiExpandableButtonProps::OnCloseClicked_Bit)));
		}

		virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
		{
			const FRuiExpandableButtonProps& N = static_cast<const FRuiExpandableButtonProps&>(New);
			Proxy.SetHandler(static_cast<int32>(FRuiExpandableButtonProps::OnExpansionClicked_Bit),
							 N.OnExpansionClicked);
			Proxy.SetHandler(static_cast<int32>(FRuiExpandableButtonProps::OnCloseClicked_Bit), N.OnCloseClicked);
		}

		virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {} // all masked

		virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
								 const FRuiStyleDict* SlotProps) override
		{
			static_cast<SRuiExpandableButton&>(Parent).SetRoleContent(RoleOf(SlotProps), Child);
		}

		virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
		{
			static_cast<SRuiExpandableButton&>(Parent).ClearContent(Child);
		}

		virtual void
		ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
						TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
		{
			SRuiExpandableButton& W = static_cast<SRuiExpandableButton&>(Parent);
			for (const TSharedRef<SWidget>& Child : Ordered)
			{
				W.SetRoleContent(RoleOf(SlotPropsOf(Child)), Child);
			}
		}

		virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
										  const FRuiStyleDict* SlotProps) override
		{
			static_cast<SRuiExpandableButton&>(Parent).SetRoleContent(RoleOf(SlotProps), Child);
		}
	};
} // namespace

namespace RUI::Slate
{
	FRuiElementTypeId ExpandableButtonType()
	{
		return RUI::InternElementType(FName(TEXT("ExpandableButton")));
	}

	FRuiNode ExpandableButton(FRuiExpandableButtonProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = ExpandableButtonType();
		Node.Props = MakeShared<FRuiExpandableButtonProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterExpandableButtonAdapter()
		{
			RegisterAdapter(ExpandableButtonType(), MakeUnique<FRuiExpandableButtonAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
