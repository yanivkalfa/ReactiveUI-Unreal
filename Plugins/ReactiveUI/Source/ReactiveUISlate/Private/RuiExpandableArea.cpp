// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SExpandableArea wrapper. Two persistent SBox holders back the construct-time
// HeaderContent/BodyContent named slots; the reconciler reparents role-tagged children into them.
// Expansion is controlled (SetExpanded skip-when-equal against the live state; OnExpansionChanged
// forwards the user's toggle).

#include "RuiExpandableArea.h"

#include "RuiElementAdapter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SNullWidget.h"

void SRuiExpandableArea::Construct(const FArguments& InArgs)
{
	SAssignNew(HeaderBox, SBox);
	SAssignNew(BodyBox, SBox);

	// clang-format off
	ChildSlot
	[
		SAssignNew(Area, SExpandableArea)
		.InitiallyCollapsed(!InArgs._InitiallyExpanded)
		.OnAreaExpansionChanged(this, &SRuiExpandableArea::HandleExpansionChanged)
		.HeaderContent()
		[
			HeaderBox.ToSharedRef()
		]
		.BodyContent()
		[
			BodyBox.ToSharedRef()
		]
	];
	// clang-format on
}

void SRuiExpandableArea::SetExpanded(bool bExpanded)
{
	// Self-notifying skip (D-16): the widget's own toggle lands on an equal value.
	if (Area.IsValid() && Area->IsExpanded() != bExpanded)
	{
		Area->SetExpanded(bExpanded);
	}
}

bool SRuiExpandableArea::IsExpanded() const
{
	return Area.IsValid() && Area->IsExpanded();
}

void SRuiExpandableArea::SetRoleContent(FName Role, const TSharedPtr<SWidget>& Content)
{
	const TSharedRef<SWidget> Widget = Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget;
	if (Role == FName(TEXT("header")))
	{
		if (HeaderBox.IsValid())
		{
			HeaderBox->SetContent(Widget);
		}
	}
	else if (BodyBox.IsValid())
	{
		BodyBox->SetContent(Widget);
	}
}

void SRuiExpandableArea::ClearContent(const TSharedRef<SWidget>& Content)
{
	auto BoxHolds = [&Content](const TSharedPtr<SBox>& Box) -> bool
	{
		if (!Box.IsValid())
		{
			return false;
		}
		FChildren* Children = Box->GetChildren();
		return Children != nullptr && Children->Num() > 0 && &Children->GetChildAt(0).Get() == &Content.Get();
	};
	if (BoxHolds(HeaderBox))
	{
		HeaderBox->SetContent(SNullWidget::NullWidget);
	}
	else if (BoxHolds(BodyBox))
	{
		BodyBox->SetContent(SNullWidget::NullWidget);
	}
}

TSharedPtr<SWidget> SRuiExpandableArea::GetRoleContent(FName Role) const
{
	const TSharedPtr<SBox>& Box = (Role == FName(TEXT("header"))) ? HeaderBox : BodyBox;
	if (!Box.IsValid())
	{
		return nullptr;
	}
	FChildren* Children = Box->GetChildren();
	if (Children == nullptr || Children->Num() == 0)
	{
		return nullptr;
	}
	TSharedRef<SWidget> Content = Children->GetChildAt(0);
	return Content == SNullWidget::NullWidget ? nullptr : TSharedPtr<SWidget>(Content);
}

void SRuiExpandableArea::HandleExpansionChanged(bool bExpanded)
{
	if (OnExpansionChanged.IsBound())
	{
		OnExpansionChanged.Execute(FRuiValue(bExpanded));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (two role slots)
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	/** The child's role ("header" | "body"); absent/other -> "body". */
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
} // namespace

class FRuiExpandableAreaAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiExpandableAreaProps& P = static_cast<const FRuiExpandableAreaProps&>(Props);
		return SNew(SRuiExpandableArea).InitiallyExpanded(P.HasbIsExpanded() ? P.bIsExpanded : true);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiExpandableArea& W = static_cast<SRuiExpandableArea&>(Widget);
		const FRuiExpandableAreaProps& N = static_cast<const FRuiExpandableAreaProps&>(New);
		const FRuiExpandableAreaProps* O = static_cast<const FRuiExpandableAreaProps*>(Old);
		if (N.HasbIsExpanded() && (O == nullptr || !O->HasbIsExpanded() || !(N.bIsExpanded == O->bIsExpanded)))
		{
			W.SetExpanded(N.bIsExpanded);
		}
		if (N.HasOnExpansionChanged())
		{
			W.SetOnExpansionChanged(N.OnExpansionChanged);
		}
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuiStyleDict* SlotProps) override
	{
		static_cast<SRuiExpandableArea&>(Parent).SetRoleContent(RoleOf(SlotProps), Child);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SRuiExpandableArea&>(Parent).ClearContent(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SRuiExpandableArea& W = static_cast<SRuiExpandableArea&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			W.SetRoleContent(RoleOf(SlotPropsOf(Child)), Child);
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuiStyleDict* SlotProps) override
	{
		SRuiExpandableArea& W = static_cast<SRuiExpandableArea&>(Parent);
		W.ClearContent(Child); // role may have changed — drop then re-route
		W.SetRoleContent(RoleOf(SlotProps), Child);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Type, factory, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace RUI::Slate
{
	FRuiElementTypeId ExpandableAreaType()
	{
		return RUI::InternElementType(FName(TEXT("ExpandableArea")));
	}

	FRuiNode ExpandableArea(FRuiExpandableAreaProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = ExpandableAreaType();
		Node.Props = MakeShared<FRuiExpandableAreaProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterExpandableAreaAdapter()
		{
			RegisterAdapter(ExpandableAreaType(), MakeUnique<FRuiExpandableAreaAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
