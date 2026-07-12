// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-022 — item-model views. The virtualization comes from Slate's SListView / STileView; the
// reactive half is SRuiListRow, a table row that owns a per-row FRuiRoot sub-root. Each row is an
// independent little reconciler: it renders RenderItem(item, index) into its own widget tree, and
// when the parent hands a fresh RenderItem closure the row re-renders that sub-root in place (no
// widget churn — the SListView row is reused, only its content is re-reconciled).

#include "RuiListView.h"

#include "RuiElementAdapter.h"
#include "RuiRoot.h"
#include "RuiSlateLog.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STileView.h"

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SRuiListRow — one generated row; owns a detached FRuiRoot rendering the item's subtree.
// ─────────────────────────────────────────────────────────────────────────────────────────────

class SRuiListRow : public STableRow<TSharedPtr<FRuiValue>>
{
public:
	SLATE_BEGIN_ARGS(SRuiListRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, const TSharedRef<STableViewBase>& OwnerTable,
				   const TWeakPtr<SRuiListView>& InOwner, const TSharedPtr<FRuiValue>& InItem)
	{
		Owner = InOwner;
		Item = InItem;

		FRuiNode Node;
		if (const TSharedPtr<SRuiListView> Pinned = Owner.Pin())
		{
			Node = Pinned->BuildNodeFor(Item);
		}
		RowRoot = FRuiRoot::Create(MoveTemp(Node));
		RowRoot->FlushSync();

		STableRow<TSharedPtr<FRuiValue>>::Construct(
			STableRow<TSharedPtr<FRuiValue>>::FArguments()[RowRoot->GetWidget()], OwnerTable);
	}

	virtual ~SRuiListRow() override
	{
		// Explicit teardown so the sub-root's effect cleanups run deterministically when the list
		// recycles/drops the row (before the shared ref would otherwise unwind).
		if (RowRoot.IsValid())
		{
			RowRoot->Unmount();
			RowRoot.Reset();
		}
	}

	/** Re-run the CURRENT renderer against this row's item and re-reconcile the sub-root in place. */
	void Rebuild()
	{
		if (!RowRoot.IsValid())
		{
			return;
		}
		FRuiNode Node;
		if (const TSharedPtr<SRuiListView> Pinned = Owner.Pin())
		{
			Node = Pinned->BuildNodeFor(Item);
		}
		RowRoot->Update(MoveTemp(Node));
		RowRoot->FlushSync();
	}

private:
	TWeakPtr<SRuiListView> Owner;
	TSharedPtr<FRuiValue> Item;
	TSharedPtr<FRuiRoot> RowRoot;
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SRuiListView — wraps SListView/STileView and owns the row bookkeeping.
// ─────────────────────────────────────────────────────────────────────────────────────────────

void SRuiListView::Construct(const FArguments& InArgs)
{
	ViewKind = InArgs._ViewKind;

	if (ViewKind == ERuiItemViewKind::Tile)
	{
		ListWidget =
			SNew(STileView<FItemType>)
				.ItemWidth(InArgs._ItemWidth)
				.ItemHeight(InArgs._ItemHeight)
				.ListItemsSource(&Items)
				.OnGenerateTile(this, &SRuiListView::HandleGenerateRow)
				.SelectionMode(TAttribute<ESelectionMode::Type>::CreateLambda([this]() { return SelectionModeValue; }))
				.OnSelectionChanged(this, &SRuiListView::HandleSelectionChanged);
	}
	else
	{
		ListWidget =
			SNew(SListView<FItemType>)
				.ListItemsSource(&Items)
				.OnGenerateRow(this, &SRuiListView::HandleGenerateRow)
				.SelectionMode(TAttribute<ESelectionMode::Type>::CreateLambda([this]() { return SelectionModeValue; }))
				.OnSelectionChanged(this, &SRuiListView::HandleSelectionChanged);
	}

	ChildSlot[ListWidget.ToSharedRef()];
}

void SRuiListView::SetItems(TArray<FItemType> InItems)
{
	Items = MoveTemp(InItems);
	if (ListWidget.IsValid())
	{
		ListWidget->RequestListRefresh();
	}
}

void SRuiListView::SetRenderer(TSharedPtr<FRuiItemRenderer> InRenderer)
{
	Renderer = MoveTemp(InRenderer);
	// The reactive path: every live row re-runs the new closure against its own sub-root.
	int32 Live = 0;
	for (int32 i = LiveRows.Num() - 1; i >= 0; --i)
	{
		if (const TSharedPtr<SRuiListRow> Row = LiveRows[i].Pin())
		{
			Row->Rebuild();
			++Live;
		}
		else
		{
			LiveRows.RemoveAtSwap(i);
		}
	}
}

void SRuiListView::SetSelectionMode(ESelectionMode::Type InMode)
{
	SelectionModeValue = InMode;
}

void SRuiListView::SetOnSelectionChanged(FRuiCallback InCallback)
{
	OnSelectionChanged = MoveTemp(InCallback);
}

FRuiNode SRuiListView::BuildNodeFor(const FItemType& Item) const
{
	if (!Item.IsValid() || !Renderer.IsValid() || !(*Renderer))
	{
		return FRuiNode(); // empty fragment — renders nothing
	}
	const int32 Index = Items.IndexOfByKey(Item);
	return (*Renderer)(*Item, Index);
}

void SRuiListView::TrackRow(const TSharedRef<SRuiListRow>& Row)
{
	for (int32 i = LiveRows.Num() - 1; i >= 0; --i)
	{
		if (!LiveRows[i].IsValid())
		{
			LiveRows.RemoveAtSwap(i);
		}
	}
	LiveRows.Add(Row);
}

void SRuiListView::ForceGenerateRows(FVector2D ViewportSize)
{
	if (!ListWidget.IsValid())
	{
		return;
	}
	const FGeometry Geometry = FGeometry::MakeRoot(ViewportSize, FSlateLayoutTransform());
	ListWidget->RequestListRefresh();
	// Two ticks: the first arranges/measures the panel, the second regenerates rows to fill it.
	ListWidget->SlatePrepass(1.0f);
	ListWidget->Tick(Geometry, 0.0, 0.0f);
	ListWidget->SlatePrepass(1.0f);
	ListWidget->Tick(Geometry, 0.0, 0.0f);
}

int32 SRuiListView::NumGeneratedRows() const
{
	int32 Live = 0;
	for (const TWeakPtr<SRuiListRow>& Row : LiveRows)
	{
		if (Row.IsValid())
		{
			++Live;
		}
	}
	return Live;
}

TSharedRef<ITableRow> SRuiListView::HandleGenerateRow(FItemType Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SRuiListRow> Row = SNew(SRuiListRow, OwnerTable, TWeakPtr<SRuiListView>(SharedThis(this)), Item);
	TrackRow(Row);
	return Row;
}

void SRuiListView::HandleSelectionChanged(FItemType Item, ESelectInfo::Type SelectInfo)
{
	if (!OnSelectionChanged.IsBound())
	{
		return;
	}
	const int32 Index = Item.IsValid() ? Items.IndexOfByKey(Item) : INDEX_NONE;
	OnSelectionChanged.Execute(FRuiValue(Index));
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Shared setter application (both views share Items/RenderItem/SelectionMode/OnSelectionChanged).
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	ESelectionMode::Type SelectionModeOf(FName V)
	{
		if (V == FName(TEXT("single")))
		{
			return ESelectionMode::Single;
		}
		if (V == FName(TEXT("singleToggle")))
		{
			return ESelectionMode::SingleToggle;
		}
		if (V == FName(TEXT("multi")))
		{
			return ESelectionMode::Multi;
		}
		return ESelectionMode::None;
	}

	/** Apply the four shared item-view props; Old==nullptr applies everything set. */
	template <typename TProps> void ApplyItemViewShared(SRuiListView& W, const TProps* O, const TProps& N)
	{
		if (N.HasItems() && (O == nullptr || !O->HasItems() || !(N.Items == O->Items)))
		{
			W.SetItems(N.Items);
		}
		if (N.HasRenderItem() && (O == nullptr || !O->HasRenderItem() || !(N.RenderItem == O->RenderItem)))
		{
			W.SetRenderer(N.RenderItem);
		}
		if (N.HasSelectionMode() && (O == nullptr || !O->HasSelectionMode() || !(N.SelectionMode == O->SelectionMode)))
		{
			W.SetSelectionMode(SelectionModeOf(N.SelectionMode));
		}
		// Event: identity-swap every commit (cheap; the widget forwards to the current callback).
		if (N.HasOnSelectionChanged())
		{
			W.SetOnSelectionChanged(N.OnSelectionChanged);
		}
	}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapters
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuiListViewAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	// Live rows + sub-roots are per-instance state — never pool this widget.
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SRuiListView).ViewKind(ERuiItemViewKind::List);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiListView& W = static_cast<SRuiListView&>(Widget);
		ApplyItemViewShared(W, static_cast<const FRuiListViewProps*>(Old), static_cast<const FRuiListViewProps&>(New));
	}
};

class FRuiTileViewAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	// Tile cell size is construct-only (STileView bakes its panel around it) — a change rebuilds.
	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiTileViewProps::ItemWidth_Bit) | (1ull << FRuiTileViewProps::ItemHeight_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiTileViewProps& O = static_cast<const FRuiTileViewProps&>(Old);
		const FRuiTileViewProps& N = static_cast<const FRuiTileViewProps&>(New);
		// Has-bit gated (SEP-REBUILD-1 class): removing a cell-size prop is not a construct-only change.
		const bool bW = N.HasItemWidth() && (!O.HasItemWidth() || !(O.ItemWidth == N.ItemWidth));
		const bool bH = N.HasItemHeight() && (!O.HasItemHeight() || !(O.ItemHeight == N.ItemHeight));
		return bW || bH;
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiTileViewProps& P = static_cast<const FRuiTileViewProps&>(Props);
		return SNew(SRuiListView)
			.ViewKind(ERuiItemViewKind::Tile)
			.ItemWidth(P.HasItemWidth() ? P.ItemWidth : 128.0f)
			.ItemHeight(P.HasItemHeight() ? P.ItemHeight : 128.0f);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiListView& W = static_cast<SRuiListView&>(Widget);
		ApplyItemViewShared(W, static_cast<const FRuiTileViewProps*>(Old), static_cast<const FRuiTileViewProps&>(New));
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Types, factories, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace RUI::Slate
{
	FRuiElementTypeId ListViewType()
	{
		return RUI::InternElementType(FName(TEXT("ListView")));
	}
	FRuiElementTypeId TileViewType()
	{
		return RUI::InternElementType(FName(TEXT("TileView")));
	}

	FRuiNode ListView(FRuiListViewProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = ListViewType();
		Node.Props = MakeShared<FRuiListViewProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	FRuiNode TileView(FRuiTileViewProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = TileViewType();
		Node.Props = MakeShared<FRuiTileViewProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	TSharedPtr<FRuiItemRenderer> MakeItemRenderer(FRuiItemRenderer Fn)
	{
		return MakeShared<FRuiItemRenderer>(MoveTemp(Fn));
	}

	namespace Detail
	{
		void RegisterItemViewAdapters()
		{
			RegisterAdapter(ListViewType(), MakeUnique<FRuiListViewAdapter>());
			RegisterAdapter(TileViewType(), MakeUnique<FRuiTileViewAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
