// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SSegmentedControl, the labelled tab-bar / radio-group selector. `Labels` bakes the
// segments (SSegmentedControl has no clear-children API, so a label-set change is CONSTRUCT-only —
// the reconstruct mask replaces the widget); `SelectedIndex` is a CONTROLLED runtime prop applied
// skip-when-equal against the widget's live value (D-16). OnSelectionChanged fires the segment index
// when the user picks one. Headless-safe: SetSelectedIndex/GetSelectedIndex are directly exercisable.

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiPropsBase.h"
#include "Widgets/SCompoundWidget.h"

template <typename OptionType> class SSegmentedControl;

/** SSegmentedControl<int32> (Leaf): one text segment per `Labels` entry (value = index).
 *  `SelectedIndex` is the controlled selection; OnSelectionChanged fires the picked index. */
struct REACTIVEUISLATE_API FRuiSegmentedControlProps final : public FRuiPropsBase
{
	RUI_PROP(TArray<FString>, Labels, 0) // construct-only (segments bake)
	RUI_PROP(int32, SelectedIndex, 1)	 // controlled runtime
	RUI_PROP_EVENT(OnSelectionChanged, 2)
	RUI_PROPS_BODY(FRuiSegmentedControlProps, RUI_EQ(Labels) RUI_EQ(SelectedIndex) RUI_EQ(OnSelectionChanged))
};

/** Wraps SSegmentedControl<int32> with a stable callback holder + the controlled-selection surface. */
class REACTIVEUISLATE_API SRuiSegmentedControl final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuiSegmentedControl) : _InitialIndex(0) {}
	SLATE_ARGUMENT(TArray<FString>, Labels)
	SLATE_ARGUMENT(int32, InitialIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetSelectedIndex(int32 Index);
	int32 GetSelectedIndex() const;
	int32 NumSegments() const;
	void SetOnSelectionChanged(FRuiCallback InCb) { OnSelectionChanged = MoveTemp(InCb); }

private:
	void HandleValueChanged(int32 Value);

	TSharedPtr<SSegmentedControl<int32>> Control;
	FRuiCallback OnSelectionChanged;
};

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId SegmentedControlType();

	/** A labelled segmented selector (tab bar). Labels bake the segments; SelectedIndex is controlled. */
	REACTIVEUISLATE_API FRuiNode SegmentedControl(FRuiSegmentedControlProps Props = FRuiSegmentedControlProps(),
												  FRuiKey Key = FRuiKey());

	namespace Detail
	{
		void RegisterSegmentedControlAdapter();
	}
} // namespace RUI::Slate
