// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SComboBox<TSharedPtr<FRuiValue>>, the dropdown selector. Same render-prop shape as
// ListView (Options + a RenderOption closure), reused for BOTH the collapsed selected-display and the
// generated dropdown rows — each is its own FRuiRoot sub-root. SelectedIndex is CONTROLLED;
// OnSelectionChanged fires the picked index (only on a user pick, never on the programmatic set).
//
// C++-FIRST (the render closure isn't markup-expressible — like ListView). The dropdown rows generate
// only when the menu is open under geometry, so OpenMenu()/NumGeneratedRows() let the suite drive and
// verify the menu headless via the interaction harness.

#pragma once

#include "CoreMinimal.h"
#include "RuiListView.h" // FRuiItemRenderer + MakeItemRenderer
#include "RuiNode.h"
#include "RuiPropsBase.h"
#include "Widgets/SCompoundWidget.h"

template <typename OptionType> class SComboBox;
class FRuiRoot;
class SBox;

/** SComboBox (Leaf; options are DATA). Options + RenderOption render both the selected display and the
 *  dropdown rows; SelectedIndex is controlled; OnSelectionChanged fires the picked index. */
struct REACTIVEUISLATE_API FRuiComboBoxProps final : public FRuiPropsBase
{
	RUI_PROP(TArray<TSharedPtr<FRuiValue>>, Options, 0)
	RUI_PROP(TSharedPtr<FRuiItemRenderer>, RenderOption, 1)
	RUI_PROP(int32, SelectedIndex, 2)
	RUI_PROP_EVENT(OnSelectionChanged, 3)
	RUI_PROPS_BODY(FRuiComboBoxProps,
				   RUI_EQ(Options) RUI_EQ(RenderOption) RUI_EQ(SelectedIndex) RUI_EQ(OnSelectionChanged))
};

/** Wraps SComboBox<TSharedPtr<FRuiValue>> with the render-prop plumbing + controlled selection. */
class REACTIVEUISLATE_API SRuiComboBox final : public SCompoundWidget
{
public:
	using FItemType = TSharedPtr<FRuiValue>;

	SLATE_BEGIN_ARGS(SRuiComboBox) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetOptions(TArray<FItemType> InOptions);
	void SetRenderer(TSharedPtr<FRuiItemRenderer> InRenderer);
	void SetSelectedIndex(int32 Index);
	void SetOnSelectionChanged(FRuiCallback InCb) { OnSelectionChanged = MoveTemp(InCb); }

	int32 GetSelectedIndex() const { return SelectedIndex; }

	/** Open the dropdown (pushes the menu). Pair with the harness's menu tick to generate rows. */
	void OpenMenu();
	/** How many dropdown-row sub-roots are currently live (after the menu has generated). */
	int32 NumGeneratedRows() const;
	/** The widget currently shown as the collapsed selected display (its sub-root's tree). */
	TSharedPtr<SWidget> GetSelectedContent() const;

private:
	TSharedRef<SWidget> HandleGenerateRow(FItemType Item);
	void HandleSelectionChanged(FItemType Item, ESelectInfo::Type SelectInfo);
	FRuiNode BuildNodeFor(const FItemType& Item) const;
	void RefreshSelectedDisplay();

	TArray<FItemType> Options;
	TSharedPtr<FRuiItemRenderer> Renderer;
	int32 SelectedIndex = INDEX_NONE;
	FRuiCallback OnSelectionChanged;

	TSharedPtr<SComboBox<FItemType>> Combo;
	TSharedPtr<SBox> SelectedHolder;
	TSharedPtr<FRuiRoot> SelectedRoot;
	TArray<TSharedPtr<FRuiRoot>> RowRoots;
};

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId ComboBoxType();

	/** A dropdown selector. Hold Options stably; RenderOption renders the selected + each row. */
	REACTIVEUISLATE_API FRuiNode ComboBox(FRuiComboBoxProps Props = FRuiComboBoxProps(), FRuiKey Key = FRuiKey());

	namespace Detail
	{
		void RegisterComboBoxAdapter();
	}
} // namespace RUI::Slate
