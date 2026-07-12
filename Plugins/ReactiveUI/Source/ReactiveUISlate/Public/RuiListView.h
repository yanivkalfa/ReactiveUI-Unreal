// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-022 — item-model views (SListView / STileView): the VIRTUALIZED list surface. Unlike a
// ScrollBox-of-children (every item is a live widget), these generate widgets only for the rows
// currently in view, so a 100k-item list stays cheap. The React shape is a RENDER PROP: you hand
// the view a stable item array + a `RenderItem(value, index) -> FRuiNode` closure, and each
// generated row mounts its OWN reconciler sub-root (a detached FRuiRoot) that renders that node.
//
// This is a C++-FIRST API (like MakeDrawFn / MakeAssetBrush): the render closure is a C++ lambda,
// not markup-expressible, so there is no `.uetkx` tag — a virtualized list is the escape hatch you
// drop to C++ for. In markup you write the non-virtualized `<VerticalBox>{Items.map(...)}` form.
//
// Reactivity: rows are keyed by the item TSharedPtr identity (SListView's native contract — hold
// the array stably in state/UseMemo for row reuse). When the parent re-renders and hands a NEW
// RenderItem closure (the common case — it captures fresh state), every LIVE row re-runs the
// closure and updates its sub-root in place; no row churn. When the item set changes (add / remove
// / reorder / new pointers), the list regenerates only the affected rows.

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiPropsBase.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FRuiRoot;
class SRuiListRow;

/** Per-item render callback: item value + row index -> a vnode subtree (its own sub-root). */
using FRuiItemRenderer = TFunction<FRuiNode(const FRuiValue&, int32)>;

/** Which concrete Slate view backs the widget (construct-only — a change is a different element). */
enum class ERuiItemViewKind : uint8
{
	List, // SListView<T>
	Tile, // STileView<T>
};

/** SListView (Leaf-to-the-reconciler; children are DATA, not vnodes). Items are compared by the
 *  TSharedPtr identity per element; RenderItem by shared-inner identity (wrap once — MakeItemRenderer
 *  — and re-hand a fresh closure each render to push new state into the rows). */
struct REACTIVEUISLATE_API FRuiListViewProps final : public FRuiPropsBase
{
	RUI_PROP(TArray<TSharedPtr<FRuiValue>>, Items, 0)
	RUI_PROP(TSharedPtr<FRuiItemRenderer>, RenderItem, 1)
	RUI_PROP(FName, SelectionMode, 2) // none (default) | single | singleToggle | multi
	RUI_PROP_EVENT(OnSelectionChanged, 3)
	RUI_PROPS_BODY(FRuiListViewProps, RUI_EQ(Items) RUI_EQ(RenderItem) RUI_EQ(SelectionMode) RUI_EQ(OnSelectionChanged))
};

/** STileView — the same item model laid out as a uniform grid of tiles. ItemWidth / ItemHeight are
 *  the tile cell size (construct-time; STileView bakes the panel around them). */
struct REACTIVEUISLATE_API FRuiTileViewProps final : public FRuiPropsBase
{
	RUI_PROP(TArray<TSharedPtr<FRuiValue>>, Items, 0)
	RUI_PROP(TSharedPtr<FRuiItemRenderer>, RenderItem, 1)
	RUI_PROP(float, ItemWidth, 2)
	RUI_PROP(float, ItemHeight, 3)
	RUI_PROP(FName, SelectionMode, 4)
	RUI_PROP_EVENT(OnSelectionChanged, 5)
	RUI_PROPS_BODY(FRuiTileViewProps, RUI_EQ(Items) RUI_EQ(RenderItem) RUI_EQ(ItemWidth) RUI_EQ(ItemHeight)
										  RUI_EQ(SelectionMode) RUI_EQ(OnSelectionChanged))
};

/**
 * The concrete virtualized-view widget. Wraps SListView / STileView<TSharedPtr<FRuiValue>> and owns
 * the per-row sub-root plumbing. The adapter drives it entirely through the Set*() surface; the
 * ForceGenerateRows / NumGeneratedRows pair exists for deterministic headless row generation
 * (rows only generate under a real arranged geometry — a test/tool ticks the list directly).
 */
class REACTIVEUISLATE_API SRuiListView : public SCompoundWidget
{
public:
	using FItemType = TSharedPtr<FRuiValue>;

	SLATE_BEGIN_ARGS(SRuiListView) : _ViewKind(ERuiItemViewKind::List), _ItemWidth(128.0f), _ItemHeight(128.0f) {}
	SLATE_ARGUMENT(ERuiItemViewKind, ViewKind)
	SLATE_ARGUMENT(float, ItemWidth)
	SLATE_ARGUMENT(float, ItemHeight)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Replace the backing item array + refresh (regenerates only the rows that actually change). */
	void SetItems(TArray<FItemType> InItems);

	/** Swap the render closure and re-run it against every LIVE row's sub-root (the reactive path). */
	void SetRenderer(TSharedPtr<FRuiItemRenderer> InRenderer);

	void SetSelectionMode(ESelectionMode::Type InMode);
	void SetOnSelectionChanged(FRuiCallback InCallback);

	/** Build the vnode for one item using the CURRENT renderer (index resolved from the item set). */
	FRuiNode BuildNodeFor(const FItemType& Item) const;

	/** Register a freshly generated row so a later renderer swap can rebuild it; compacts dead refs. */
	void TrackRow(const TSharedRef<SRuiListRow>& Row);

	/** Force row generation for a synthetic viewport size (headless tests/tools). Ticks the inner
	 *  list twice — measure, then generate — so rows exist without a live Slate paint loop. */
	void ForceGenerateRows(FVector2D ViewportSize);

	/** How many row sub-roots are currently live (dead weak refs excluded). */
	int32 NumGeneratedRows() const;

	TSharedPtr<SListView<FItemType>> GetListWidget() const { return ListWidget; }

private:
	TSharedRef<class ITableRow> HandleGenerateRow(FItemType Item, const TSharedRef<class STableViewBase>& OwnerTable);
	void HandleSelectionChanged(FItemType Item, ESelectInfo::Type SelectInfo);

	ERuiItemViewKind ViewKind = ERuiItemViewKind::List;
	TSharedPtr<SListView<FItemType>> ListWidget;
	TArray<FItemType> Items;
	TSharedPtr<FRuiItemRenderer> Renderer;
	FRuiCallback OnSelectionChanged;
	ESelectionMode::Type SelectionModeValue = ESelectionMode::None;
	TArray<TWeakPtr<SRuiListRow>> LiveRows;
};

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId ListViewType();
	REACTIVEUISLATE_API FRuiElementTypeId TileViewType();

	/** A virtualized list. Hold `Items` stably (UseMemo/UseRef) for row reuse; re-hand `RenderItem`
	 *  each render to push fresh state into the visible rows. */
	REACTIVEUISLATE_API FRuiNode ListView(FRuiListViewProps Props = FRuiListViewProps(), FRuiKey Key = FRuiKey());

	/** A virtualized tile grid (same item model as ListView). */
	REACTIVEUISLATE_API FRuiNode TileView(FRuiTileViewProps Props = FRuiTileViewProps(), FRuiKey Key = FRuiKey());

	/** Wrap a render closure ONCE (UseMemo/UseRef it). Re-handing a fresh closure re-renders rows. */
	REACTIVEUISLATE_API TSharedPtr<FRuiItemRenderer> MakeItemRenderer(FRuiItemRenderer Fn);

	/** Register the ListView/TileView adapters (called from RegisterBuiltinAdapters; idempotent). */
	namespace Detail
	{
		void RegisterItemViewAdapters();
	}
} // namespace RUI::Slate
