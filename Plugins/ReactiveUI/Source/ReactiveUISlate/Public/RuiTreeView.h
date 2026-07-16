// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-022 closure (WIDGET_COMPLETION_PLAN wave 4) — the hierarchical item-model view.
// STreeView on the SRuiListView pattern: per-row reconciler sub-roots + a GetChildren
// accessor (the piece the flat FRuiValue item type couldn't carry) + CONTROLLED expansion
// (ExpandedItems diffed onto SetItemExpansion; OnExpansionChanged reports user toggles) +
// the P5c column protocol (a Columns list builds the SHeaderRow; construct-only).
//
// C++-FIRST like ListView (render closures are not markup-expressible — no .uetkx tag).
// Hold Items/children arrays stably (UseMemo/UseRef); re-hand RenderItem each render.

#pragma once

#include "CoreMinimal.h"
#include "RuiListView.h" // FRuiItemRenderer + the item-model conventions
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STreeView.h"

class FRuiRoot;
class SRuiTreeRow;

/** Child accessor: item -> its children (return the SAME TSharedPtrs every call — identity
 *  keys the rows and the expansion map). */
using FRuiChildAccessor = TFunction<TArray<TSharedPtr<FRuiValue>>(const FRuiValue&)>;

/** One header column (P5c): construct-only on the tree (a Columns change rebuilds the header). */
struct FRuiHeaderColumn
{
	FName Id;
	FText Label;
	float FillWidth = 1.0f;

	bool operator==(const FRuiHeaderColumn& O) const
	{
		return Id == O.Id && FillWidth == O.FillWidth &&
			   (Label.IdenticalTo(O.Label) || Label.ToString() == O.Label.ToString());
	}
};

/** TreeView props. Expansion is CONTROLLED: ExpandedItems (by item identity) diffs onto the
 *  widget; user toggles report via OnExpansionChanged (Value = bool; pair with selection to
 *  know which item — or use the P2 handle for imperative reads). */
struct REACTIVEUISLATE_API FRuiTreeViewProps final : public FRuiPropsBase
{
	RUI_PROP(TArray<TSharedPtr<FRuiValue>>, Items, 0) // root items
	RUI_PROP(TSharedPtr<FRuiItemRenderer>, RenderItem, 1)
	RUI_PROP(TSharedPtr<FRuiChildAccessor>, GetChildren, 2)
	RUI_PROP(TArray<TSharedPtr<FRuiValue>>, ExpandedItems, 3)
	RUI_PROP(TArray<FRuiHeaderColumn>, Columns, 4)
	RUI_PROP(FName, SelectionMode, 5)
	RUI_PROP_EVENT(OnSelectionChanged, 6)
	RUI_PROP_EVENT(OnExpansionChanged, 7)
	RUI_PROPS_BODY(FRuiTreeViewProps,
				   RUI_EQ(Items) RUI_EQ(RenderItem) RUI_EQ(GetChildren) RUI_EQ(ExpandedItems) RUI_EQ(Columns)
					   RUI_EQ(SelectionMode) RUI_EQ(OnSelectionChanged) RUI_EQ(OnExpansionChanged))
};

/** The concrete tree widget (adapter-driven via the Set*() surface; headless helpers mirror
 *  SRuiListView's). */
class REACTIVEUISLATE_API SRuiTreeView : public SCompoundWidget
{
public:
	using FItemType = TSharedPtr<FRuiValue>;

	SLATE_BEGIN_ARGS(SRuiTreeView) {}
	SLATE_ARGUMENT(TArray<FRuiHeaderColumn>, Columns)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetItems(TArray<FItemType> InItems);
	void SetRenderer(TSharedPtr<FRuiItemRenderer> InRenderer);
	void SetChildAccessor(TSharedPtr<FRuiChildAccessor> InAccessor);
	/** Controlled expansion: expand exactly this set (collapse everything else known). */
	void SetExpandedItems(const TArray<FItemType>& InExpanded);
	void SetSelectionMode(ESelectionMode::Type InMode);
	void SetOnSelectionChanged(FRuiCallback InCallback);
	void SetOnExpansionChanged(FRuiCallback InCallback);

	FRuiNode BuildNodeFor(const FItemType& Item) const;
	void TrackRow(const TSharedRef<SRuiTreeRow>& Row);
	void ForceGenerateRows(FVector2D ViewportSize);
	int32 NumGeneratedRows() const;

	TSharedPtr<STreeView<FItemType>> GetTreeWidget() const { return TreeWidget; }

private:
	TSharedRef<class ITableRow> HandleGenerateRow(FItemType Item, const TSharedRef<class STableViewBase>& OwnerTable);
	void HandleGetChildren(FItemType Item, TArray<FItemType>& OutChildren);
	void HandleSelectionChanged(FItemType Item, ESelectInfo::Type SelectInfo);
	void HandleExpansionChanged(FItemType Item, bool bExpanded);

	TSharedPtr<STreeView<FItemType>> TreeWidget;
	TSharedPtr<SHeaderRow> Header;
	TArray<FItemType> Items;
	TSharedPtr<FRuiItemRenderer> Renderer;
	TSharedPtr<FRuiChildAccessor> ChildAccessor;
	TArray<FItemType> KnownExpanded; // last controlled set (for collapse diffing)
	FRuiCallback OnSelectionChanged;
	FRuiCallback OnExpansionChanged;
	ESelectionMode::Type SelectionModeValue = ESelectionMode::None;
	bool bApplyingExpansion = false; // suppress OnExpansionChanged for programmatic changes
	TArray<TWeakPtr<SRuiTreeRow>> LiveRows;
};

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId TreeViewType();

	/** A virtualized hierarchical tree (TD-022). C++-first, like ListView. */
	REACTIVEUISLATE_API FRuiNode TreeView(FRuiTreeViewProps Props = FRuiTreeViewProps(), FRuiKey Key = FRuiKey());

	/** Wrap a child accessor ONCE (UseMemo/UseRef it) — identity participates in props equality. */
	REACTIVEUISLATE_API TSharedPtr<FRuiChildAccessor> MakeChildAccessor(FRuiChildAccessor Fn);

	namespace Detail
	{
		void RegisterTreeViewAdapter();
	}
} // namespace RUI::Slate
