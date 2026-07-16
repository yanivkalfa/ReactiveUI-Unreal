// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiTreeView.h"

#include "RuiElementAdapter.h"
#include "RuiRoot.h"

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SRuiTreeRow — one generated row = one reconciler sub-root (the SRuiListRow pattern).
// ─────────────────────────────────────────────────────────────────────────────────────────────

class SRuiTreeRow : public STableRow<TSharedPtr<FRuiValue>>
{
public:
	SLATE_BEGIN_ARGS(SRuiTreeRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, const TSharedRef<STableViewBase>& OwnerTable,
				   const TWeakPtr<SRuiTreeView>& InOwner, const TSharedPtr<FRuiValue>& InItem)
	{
		Owner = InOwner;
		Item = InItem;

		FRuiNode Node;
		if (const TSharedPtr<SRuiTreeView> Pinned = Owner.Pin())
		{
			Node = Pinned->BuildNodeFor(Item);
		}
		RowRoot = FRuiRoot::Create(MoveTemp(Node));
		RowRoot->FlushSync();

		STableRow<TSharedPtr<FRuiValue>>::Construct(
			STableRow<TSharedPtr<FRuiValue>>::FArguments()[RowRoot->GetWidget()], OwnerTable);
	}

	virtual ~SRuiTreeRow() override
	{
		if (RowRoot.IsValid())
		{
			RowRoot->Unmount();
			RowRoot.Reset();
		}
	}

	void Rebuild()
	{
		if (!RowRoot.IsValid())
		{
			return;
		}
		FRuiNode Node;
		if (const TSharedPtr<SRuiTreeView> Pinned = Owner.Pin())
		{
			Node = Pinned->BuildNodeFor(Item);
		}
		RowRoot->Update(MoveTemp(Node));
		RowRoot->FlushSync();
	}

private:
	TWeakPtr<SRuiTreeView> Owner;
	TSharedPtr<FRuiValue> Item;
	TSharedPtr<FRuiRoot> RowRoot;
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SRuiTreeView
// ─────────────────────────────────────────────────────────────────────────────────────────────

void SRuiTreeView::Construct(const FArguments& InArgs)
{
	if (InArgs._Columns.Num() > 0)
	{
		Header = SNew(SHeaderRow);
		for (const FRuiHeaderColumn& Column : InArgs._Columns)
		{
			Header->AddColumn(SHeaderRow::Column(Column.Id).DefaultLabel(Column.Label).FillWidth(Column.FillWidth));
		}
	}

	STreeView<FItemType>::FArguments TreeArgs;
	TreeArgs.TreeItemsSource(&Items)
		.OnGenerateRow(STreeView<FItemType>::FOnGenerateRow::CreateSP(this, &SRuiTreeView::HandleGenerateRow))
		.OnGetChildren(STreeView<FItemType>::FOnGetChildren::CreateSP(this, &SRuiTreeView::HandleGetChildren))
		.OnSelectionChanged(
			STreeView<FItemType>::FOnSelectionChanged::CreateSP(this, &SRuiTreeView::HandleSelectionChanged))
		.OnExpansionChanged(
			STreeView<FItemType>::FOnExpansionChanged::CreateSP(this, &SRuiTreeView::HandleExpansionChanged))
		.SelectionMode(TAttribute<ESelectionMode::Type>::CreateLambda([this]() { return SelectionModeValue; }));
	if (Header.IsValid())
	{
		TreeArgs.HeaderRow(Header);
	}
	TreeWidget = SNew(STreeView<FItemType>);
	TreeWidget->Construct(TreeArgs);

	ChildSlot[TreeWidget.ToSharedRef()];
}

void SRuiTreeView::SetItems(TArray<FItemType> InItems)
{
	Items = MoveTemp(InItems);
	TreeWidget->RequestTreeRefresh();
}

void SRuiTreeView::SetRenderer(TSharedPtr<FRuiItemRenderer> InRenderer)
{
	Renderer = MoveTemp(InRenderer);
	for (int32 i = LiveRows.Num() - 1; i >= 0; --i)
	{
		if (const TSharedPtr<SRuiTreeRow> Row = LiveRows[i].Pin())
		{
			Row->Rebuild();
		}
		else
		{
			LiveRows.RemoveAtSwap(i);
		}
	}
}

void SRuiTreeView::SetChildAccessor(TSharedPtr<FRuiChildAccessor> InAccessor)
{
	ChildAccessor = MoveTemp(InAccessor);
	TreeWidget->RequestTreeRefresh();
}

void SRuiTreeView::SetExpandedItems(const TArray<FItemType>& InExpanded)
{
	bApplyingExpansion = true;
	for (const FItemType& Item : KnownExpanded) // collapse what left the controlled set
	{
		if (!InExpanded.Contains(Item))
		{
			TreeWidget->SetItemExpansion(Item, false);
		}
	}
	for (const FItemType& Item : InExpanded)
	{
		TreeWidget->SetItemExpansion(Item, true);
	}
	KnownExpanded = InExpanded;
	bApplyingExpansion = false;
}

void SRuiTreeView::SetSelectionMode(ESelectionMode::Type InMode)
{
	SelectionModeValue = InMode;
}

void SRuiTreeView::SetOnSelectionChanged(FRuiCallback InCallback)
{
	OnSelectionChanged = MoveTemp(InCallback);
}

void SRuiTreeView::SetOnExpansionChanged(FRuiCallback InCallback)
{
	OnExpansionChanged = MoveTemp(InCallback);
}

FRuiNode SRuiTreeView::BuildNodeFor(const FItemType& Item) const
{
	if (!Renderer.IsValid() || !(*Renderer) || !Item.IsValid())
	{
		return FRuiNode();
	}
	const int32 Index = Items.IndexOfByKey(Item); // root index; nested rows get INDEX_NONE
	return (*Renderer)(*Item, Index);
}

void SRuiTreeView::TrackRow(const TSharedRef<SRuiTreeRow>& Row)
{
	LiveRows.RemoveAll([](const TWeakPtr<SRuiTreeRow>& W) { return !W.IsValid(); });
	LiveRows.Add(Row);
}

void SRuiTreeView::ForceGenerateRows(FVector2D ViewportSize)
{
	const FGeometry Geometry = FGeometry::MakeRoot(ViewportSize, FSlateLayoutTransform());
	TreeWidget->Tick(Geometry, 0.0, 0.016f);
	TreeWidget->Tick(Geometry, 0.016, 0.016f);
}

int32 SRuiTreeView::NumGeneratedRows() const
{
	int32 Count = 0;
	for (const TWeakPtr<SRuiTreeRow>& Row : LiveRows)
	{
		Count += Row.IsValid() ? 1 : 0;
	}
	return Count;
}

TSharedRef<ITableRow> SRuiTreeView::HandleGenerateRow(FItemType Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SRuiTreeRow> Row = SNew(SRuiTreeRow, OwnerTable, SharedThis(this), Item);
	TrackRow(Row);
	return Row;
}

void SRuiTreeView::HandleGetChildren(FItemType Item, TArray<FItemType>& OutChildren)
{
	if (ChildAccessor.IsValid() && (*ChildAccessor) && Item.IsValid())
	{
		OutChildren = (*ChildAccessor)(*Item);
	}
}

void SRuiTreeView::HandleSelectionChanged(FItemType Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct && OnSelectionChanged.IsBound() && Item.IsValid())
	{
		OnSelectionChanged.Execute(*Item);
	}
}

void SRuiTreeView::HandleExpansionChanged(FItemType Item, bool bExpanded)
{
	if (!bApplyingExpansion && OnExpansionChanged.IsBound())
	{
		OnExpansionChanged.Execute(FRuiValue(bExpanded));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter + factory
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	ESelectionMode::Type TreeSelectionModeOf(FName V)
	{
		return V == FName(TEXT("single"))		  ? ESelectionMode::Single
			   : V == FName(TEXT("singleToggle")) ? ESelectionMode::SingleToggle
			   : V == FName(TEXT("multi"))		  ? ESelectionMode::Multi
												  : ESelectionMode::None;
	}

	class FRuiTreeViewAdapter final : public IRuiElementAdapter
	{
	public:
		virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
		virtual bool HasEvents() const override { return true; } // callbacks flow via setters

		virtual uint64 GetReconstructMask() const override { return 1ull << FRuiTreeViewProps::Columns_Bit; }

		virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
		{
			const FRuiTreeViewProps& O = static_cast<const FRuiTreeViewProps&>(Old);
			const FRuiTreeViewProps& N = static_cast<const FRuiTreeViewProps&>(New);
			return N.HasColumns() && (!O.HasColumns() || !(O.Columns == N.Columns));
		}

		virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
		{
			const FRuiTreeViewProps& P = static_cast<const FRuiTreeViewProps&>(Props);
			return SNew(SRuiTreeView).Columns(P.Columns);
		}

		virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
		{
			SRuiTreeView& W = static_cast<SRuiTreeView&>(Widget);
			const FRuiTreeViewProps& N = static_cast<const FRuiTreeViewProps&>(New);
			const FRuiTreeViewProps* O = static_cast<const FRuiTreeViewProps*>(Old);
			if (N.HasSelectionMode() &&
				(O == nullptr || !O->HasSelectionMode() || !(N.SelectionMode == O->SelectionMode)))
			{
				W.SetSelectionMode(TreeSelectionModeOf(N.SelectionMode));
			}
			if (N.HasOnSelectionChanged())
			{
				W.SetOnSelectionChanged(N.OnSelectionChanged);
			}
			if (N.HasOnExpansionChanged())
			{
				W.SetOnExpansionChanged(N.OnExpansionChanged);
			}
			if (N.HasGetChildren() && (O == nullptr || !O->HasGetChildren() || !(N.GetChildren == O->GetChildren)))
			{
				W.SetChildAccessor(N.GetChildren);
			}
			if (N.HasRenderItem() && (O == nullptr || !O->HasRenderItem() || !(N.RenderItem == O->RenderItem)))
			{
				W.SetRenderer(N.RenderItem);
			}
			if (N.HasItems() && (O == nullptr || !O->HasItems() || !(N.Items == O->Items)))
			{
				W.SetItems(N.Items);
			}
			if (N.HasExpandedItems() &&
				(O == nullptr || !O->HasExpandedItems() || !(N.ExpandedItems == O->ExpandedItems)))
			{
				W.SetExpandedItems(N.ExpandedItems);
			}
		}
	};
} // namespace

namespace RUI::Slate
{
	FRuiElementTypeId TreeViewType()
	{
		return RUI::InternElementType(FName(TEXT("TreeView")));
	}

	FRuiNode TreeView(FRuiTreeViewProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = TreeViewType();
		Node.Props = MakeShared<FRuiTreeViewProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(TArray<FRuiNode>());
		Node.Key = Key;
		return Node;
	}

	TSharedPtr<FRuiChildAccessor> MakeChildAccessor(FRuiChildAccessor Fn)
	{
		return MakeShared<FRuiChildAccessor>(MoveTemp(Fn));
	}

	namespace Detail
	{
		void RegisterTreeViewAdapter()
		{
			RegisterAdapter(TreeViewType(), MakeUnique<FRuiTreeViewAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
