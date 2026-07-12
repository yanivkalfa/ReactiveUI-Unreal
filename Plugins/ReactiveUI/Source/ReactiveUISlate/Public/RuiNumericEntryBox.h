// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SNumericEntryBox<float>, the typed numeric field. SNumericEntryBox has NO value
// setter (its Value is a bound attribute), so SRuiNumericEntryBox holds the controlled value in a
// member the attribute reads — `Value` is applied skip-when-equal against that live value (D-16),
// exactly the controlled-input contract of the editable-text widgets. OnValueChanged/OnValueCommitted
// forward the float payload. Verifiable headless: the displayed value is read from the inner editable
// text via the interaction harness (the entry's value getter reflects the controlled member).

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "RuiPropsBase.h"
#include "Widgets/SCompoundWidget.h"

template <typename NumericType> class SNumericEntryBox;

/** SNumericEntryBox<float> (Leaf): a controlled numeric field. Value is applied skip-when-equal;
 *  MinValue/MaxValue bound the typed input; OnValueChanged/OnValueCommitted carry the float. */
struct REACTIVEUISLATE_API FRuiNumericEntryBoxProps final : public FRuiPropsBase
{
	RUI_PROP(float, Value, 0)
	RUI_PROP(float, MinValue, 1)
	RUI_PROP(float, MaxValue, 2)
	RUI_PROP_EVENT(OnValueChanged, 3)
	RUI_PROP_EVENT(OnValueCommitted, 4)
	RUI_PROPS_BODY(FRuiNumericEntryBoxProps,
				   RUI_EQ(Value) RUI_EQ(MinValue) RUI_EQ(MaxValue) RUI_EQ(OnValueChanged) RUI_EQ(OnValueCommitted))
};

/** Wraps SNumericEntryBox<float> with the controlled-value member the widget's Value attribute reads. */
class REACTIVEUISLATE_API SRuiNumericEntryBox final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuiNumericEntryBox) {}
	SLATE_ARGUMENT(TOptional<float>, MinValue)
	SLATE_ARGUMENT(TOptional<float>, MaxValue)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetValue(float InValue);
	float GetValue() const { return CurrentValue; }
	void SetMinValue(TOptional<float> InMin) { MinValue = InMin; }
	void SetMaxValue(TOptional<float> InMax) { MaxValue = InMax; }
	void SetOnValueChanged(FRuiCallback InCb) { OnValueChangedCb = MoveTemp(InCb); }
	void SetOnValueCommitted(FRuiCallback InCb) { OnValueCommittedCb = MoveTemp(InCb); }

private:
	TOptional<float> GetOptionalValue() const { return CurrentValue; }
	void HandleValueChanged(float InValue);
	void HandleValueCommitted(float InValue, ETextCommit::Type CommitType);
	/** Clamp to [MinValue, MaxValue] when set. AllowSpin(false) routes typed input through the editable-
	 *  text path, which does NOT clamp against the engine's Min/Max — so the wrapper enforces the bounds
	 *  on every forwarded/set value (bughunt IW-1). */
	float Clamp(float InValue) const;

	float CurrentValue = 0.0f;
	TOptional<float> MinValue;
	TOptional<float> MaxValue;
	FRuiCallback OnValueChangedCb;
	FRuiCallback OnValueCommittedCb;
	TSharedPtr<SNumericEntryBox<float>> Entry;
};

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId NumericEntryBoxType();

	/** A controlled numeric field. Drive `Value` from state; OnValueChanged fires as the user types. */
	REACTIVEUISLATE_API FRuiNode NumericEntryBox(FRuiNumericEntryBoxProps Props = FRuiNumericEntryBoxProps(),
												 FRuiKey Key = FRuiKey());

	namespace Detail
	{
		void RegisterNumericEntryBoxAdapter();
	}
} // namespace RUI::Slate
