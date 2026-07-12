// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SComboBox wrapper. The selected display and each dropdown row are FRuiRoot sub-roots
// built from the shared RenderOption closure. Selection is controlled (SetSelectedItem never re-fires
// OnSelectionChanged — that only fires on a user pick, filtered by ESelectInfo::Direct).

#include "RuiComboBox.h"

#include "RuiElementAdapter.h"
#include "RuiRoot.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

void SRuiComboBox::Construct(const FArguments&)
{
	SAssignNew(SelectedHolder, SBox);
	// clang-format off
	ChildSlot
	[
		SAssignNew(Combo, SComboBox<FItemType>)
		.OptionsSource(&Options)
		.OnGenerateWidget(this, &SRuiComboBox::HandleGenerateRow)
		.OnComboBoxOpening(this, &SRuiComboBox::HandleMenuOpening)
		.OnSelectionChanged(this, &SRuiComboBox::HandleSelectionChanged)
		[
			SelectedHolder.ToSharedRef()
		]
	];
	// clang-format on
}

SRuiComboBox::~SRuiComboBox()
{
	// Tear down every reconciler sub-root this widget owns (rows + the selected display) so their
	// hooks/effects clean up rather than leaking with the widget (bughunt IW-2).
	UnmountRowRoots();
	if (SelectedRoot.IsValid())
	{
		SelectedRoot->Unmount();
		SelectedRoot.Reset();
	}
}

void SRuiComboBox::SetOptions(TArray<FItemType> InOptions)
{
	Options = MoveTemp(InOptions);
	if (Combo.IsValid())
	{
		// RefreshOptions can prune a now-absent selection and fire OnSelectionChanged(nullptr, Direct);
		// that is an engine-originated prune, not a user pick, so suppress our callback around it
		// (bughunt B6-1 / IW-3 — Direct alone is not a programmatic-vs-user signal).
		bApplyingSelection = true;
		Combo->RefreshOptions();
		bApplyingSelection = false;
	}
	RefreshSelectedDisplay();
}

void SRuiComboBox::SetRenderer(TSharedPtr<FRuiItemRenderer> InRenderer)
{
	Renderer = MoveTemp(InRenderer);
	RefreshSelectedDisplay();
}

void SRuiComboBox::SetSelectedIndex(int32 Index)
{
	if (Index == SelectedIndex)
	{
		return;
	}
	SelectedIndex = Index;
	if (Combo.IsValid() && Options.IsValidIndex(Index))
	{
		bApplyingSelection = true; // suppress our own callback (bughunt B6)
		Combo->SetSelectedItem(Options[Index]);
		bApplyingSelection = false;
	}
	RefreshSelectedDisplay();
}

FRuiNode SRuiComboBox::BuildNodeFor(const FItemType& Item) const
{
	if (!Item.IsValid() || !Renderer.IsValid() || !(*Renderer))
	{
		return FRuiNode();
	}
	return (*Renderer)(*Item, Options.IndexOfByKey(Item));
}

void SRuiComboBox::RefreshSelectedDisplay()
{
	if (!SelectedHolder.IsValid())
	{
		return;
	}
	const FItemType Selected = Options.IsValidIndex(SelectedIndex) ? Options[SelectedIndex] : nullptr;
	FRuiNode Node = BuildNodeFor(Selected);
	if (SelectedRoot.IsValid())
	{
		SelectedRoot->Update(MoveTemp(Node));
		SelectedRoot->FlushSync();
	}
	else
	{
		SelectedRoot = FRuiRoot::Create(MoveTemp(Node));
		SelectedRoot->FlushSync();
	}
	SelectedHolder->SetContent(SelectedRoot->GetWidget());
}

TSharedRef<SWidget> SRuiComboBox::HandleGenerateRow(FItemType Item)
{
	TSharedRef<FRuiRoot> Row = FRuiRoot::Create(BuildNodeFor(Item));
	Row->FlushSync();
	RowRoots.Add(Row);
	return Row->GetWidget();
}

void SRuiComboBox::HandleSelectionChanged(FItemType Item, ESelectInfo::Type /*SelectInfo*/)
{
	const int32 Index = Item.IsValid() ? Options.IndexOfByKey(Item) : INDEX_NONE;
	SelectedIndex = Index;
	RefreshSelectedDisplay();
	// Fire only for a genuine USER pick — suppressed via the reentrancy flag while WE drive the set,
	// NOT via ESelectInfo (SComboBox emits Direct for user keyboard-close / gamepad-accept too, B6).
	// A user pick always resolves to a valid option; an INDEX_NONE here is an engine-originated prune
	// (the selected option was removed from Options), never a user action — so never report it as one
	// (bughunt IW-3 / B6-1).
	if (!bApplyingSelection && Index != INDEX_NONE && OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Execute(FRuiValue(Index));
	}
}

void SRuiComboBox::HandleMenuOpening()
{
	UnmountRowRoots(); // each open regenerates fresh rows; retire the prior open's sub-roots
}

void SRuiComboBox::UnmountRowRoots()
{
	for (const TSharedPtr<FRuiRoot>& Row : RowRoots)
	{
		if (Row.IsValid())
		{
			Row->Unmount();
		}
	}
	RowRoots.Reset();
}

void SRuiComboBox::OpenMenu()
{
	UnmountRowRoots(); // a fresh open regenerates the visible rows (retire the previous ones)
	if (Combo.IsValid())
	{
		Combo->SetIsOpen(true);
	}
}

int32 SRuiComboBox::NumGeneratedRows() const
{
	return RowRoots.Num();
}

TSharedPtr<SWidget> SRuiComboBox::GetSelectedContent() const
{
	return SelectedRoot.IsValid() ? TSharedPtr<SWidget>(SelectedRoot->GetWidget()) : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (Leaf; options are data)
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuiComboBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SRuiComboBox);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiComboBox& W = static_cast<SRuiComboBox&>(Widget);
		const FRuiComboBoxProps& N = static_cast<const FRuiComboBoxProps&>(New);
		const FRuiComboBoxProps* O = static_cast<const FRuiComboBoxProps*>(Old);
		if (N.HasOptions() && (O == nullptr || !O->HasOptions() || !(N.Options == O->Options)))
		{
			W.SetOptions(N.Options);
		}
		if (N.HasRenderOption() && (O == nullptr || !O->HasRenderOption() || !(N.RenderOption == O->RenderOption)))
		{
			W.SetRenderer(N.RenderOption);
		}
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
	FRuiElementTypeId ComboBoxType()
	{
		return RUI::InternElementType(FName(TEXT("ComboBox")));
	}

	FRuiNode ComboBox(FRuiComboBoxProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = ComboBoxType();
		Node.Props = MakeShared<FRuiComboBoxProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterComboBoxAdapter()
		{
			RegisterAdapter(ComboBoxType(), MakeUnique<FRuiComboBoxAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
