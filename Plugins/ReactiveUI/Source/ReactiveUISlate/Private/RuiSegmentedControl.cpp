// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SSegmentedControl wrapper. Segments are baked from Labels at construction (value =
// index); SelectedIndex drives SetValue skip-when-equal; OnValueChanged forwards the picked index.

#include "RuiSegmentedControl.h"

#include "RuiElementAdapter.h"
#include "Widgets/Input/SSegmentedControl.h"

void SRuiSegmentedControl::Construct(const FArguments& InArgs)
{
	SAssignNew(Control, SSegmentedControl<int32>).OnValueChanged(this, &SRuiSegmentedControl::HandleValueChanged);
	for (int32 i = 0; i < InArgs._Labels.Num(); ++i)
	{
		Control->AddSlot(i, /*bRebuildChildren*/ false).Text(FText::FromString(InArgs._Labels[i]));
	}
	Control->RebuildChildren();
	if (InArgs._Labels.Num() > 0)
	{
		const int32 Initial = FMath::Clamp(InArgs._InitialIndex, 0, InArgs._Labels.Num() - 1);
		// bUpdateChildren=TRUE so the segment highlight follows the controlled value (bughunt B8): the
		// value attribute is unbound, so the check states are static snapshots — without the refresh they
		// stay Unchecked and no controlled/initial selection ever visually highlights (only user clicks do).
		Control->SetValue(Initial, /*bUpdateChildren*/ true);
	}
	ChildSlot[Control.ToSharedRef()];
}

void SRuiSegmentedControl::SetSelectedIndex(int32 Index)
{
	// Controlled skip-when-equal (D-16): the widget's own click lands on an equal value.
	if (Control.IsValid() && Index >= 0 && Index < Control->NumSlots() && Control->GetValue() != Index)
	{
		Control->SetValue(Index, /*bUpdateChildren*/ true); // refresh the highlight (bughunt B8)
	}
}

int32 SRuiSegmentedControl::GetSelectedIndex() const
{
	return Control.IsValid() ? Control->GetValue() : INDEX_NONE;
}

int32 SRuiSegmentedControl::NumSegments() const
{
	return Control.IsValid() ? Control->NumSlots() : 0;
}

void SRuiSegmentedControl::HandleValueChanged(int32 Value)
{
	if (OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Execute(FRuiValue(Value));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (Leaf; Labels construct-only)
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuiSegmentedControlAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	// Segments bake from Labels at construction (no clear API) — a label-set change replaces the widget.
	virtual uint64 GetReconstructMask() const override { return (1ull << FRuiSegmentedControlProps::Labels_Bit); }

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiSegmentedControlProps& O = static_cast<const FRuiSegmentedControlProps&>(Old);
		const FRuiSegmentedControlProps& N = static_cast<const FRuiSegmentedControlProps&>(New);
		// Has-bit gated (SEP-REBUILD-1 class): removing Labels is not a construct-only change.
		return N.HasLabels() && (!O.HasLabels() || !(O.Labels == N.Labels));
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiSegmentedControlProps& P = static_cast<const FRuiSegmentedControlProps&>(Props);
		return SNew(SRuiSegmentedControl).Labels(P.Labels).InitialIndex(P.HasSelectedIndex() ? P.SelectedIndex : 0);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiSegmentedControl& W = static_cast<SRuiSegmentedControl&>(Widget);
		const FRuiSegmentedControlProps& N = static_cast<const FRuiSegmentedControlProps&>(New);
		const FRuiSegmentedControlProps* O = static_cast<const FRuiSegmentedControlProps*>(Old);
		if (N.HasSelectedIndex() && (O == nullptr || !O->HasSelectedIndex() || !(N.SelectedIndex == O->SelectedIndex)))
		{
			W.SetSelectedIndex(N.SelectedIndex);
		}
		if (N.HasOnSelectionChanged())
		{
			W.SetOnSelectionChanged(N.OnSelectionChanged);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Type, factory, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace RUI::Slate
{
	FRuiElementTypeId SegmentedControlType()
	{
		return RUI::InternElementType(FName(TEXT("SegmentedControl")));
	}

	FRuiNode SegmentedControl(FRuiSegmentedControlProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = SegmentedControlType();
		Node.Props = MakeShared<FRuiSegmentedControlProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterSegmentedControlAdapter()
		{
			RegisterAdapter(SegmentedControlType(), MakeUnique<FRuiSegmentedControlAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
