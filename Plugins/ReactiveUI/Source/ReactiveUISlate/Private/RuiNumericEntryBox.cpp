// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SNumericEntryBox<float> wrapper. The controlled value lives in a member the widget's
// Value attribute reads; OnValueChanged/OnValueCommitted forward the float payload.

#include "RuiNumericEntryBox.h"

#include "RuiElementAdapter.h"
#include "Widgets/Input/SNumericEntryBox.h"

void SRuiNumericEntryBox::Construct(const FArguments& InArgs)
{
	MinValue = InArgs._MinValue;
	MaxValue = InArgs._MaxValue;
	// clang-format off
	ChildSlot
	[
		SAssignNew(Entry, SNumericEntryBox<float>)
		.AllowSpin(false)
		.MinValue(InArgs._MinValue)
		.MaxValue(InArgs._MaxValue)
		.Value(this, &SRuiNumericEntryBox::GetOptionalValue)
		.OnValueChanged(this, &SRuiNumericEntryBox::HandleValueChanged)
		.OnValueCommitted(this, &SRuiNumericEntryBox::HandleValueCommitted)
	];
	// clang-format on
}

float SRuiNumericEntryBox::Clamp(float InValue) const
{
	float V = InValue;
	if (MinValue.IsSet())
	{
		V = FMath::Max(V, MinValue.GetValue());
	}
	if (MaxValue.IsSet())
	{
		V = FMath::Min(V, MaxValue.GetValue());
	}
	return V;
}

void SRuiNumericEntryBox::SetValue(float InValue)
{
	const float Clamped = Clamp(InValue);
	// Controlled skip-when-equal (D-16): the field's own edit round-trips to an equal value.
	if (!FMath::IsNearlyEqual(CurrentValue, Clamped))
	{
		CurrentValue = Clamped;
	}
}

void SRuiNumericEntryBox::HandleValueChanged(float InValue)
{
	// Enforce the documented Min/Max bounds on typed input before forwarding (bughunt IW-1): with
	// AllowSpin(false) the engine never clamps, so an unclamped value would reach the parent state.
	const float Clamped = Clamp(InValue);
	if (OnValueChangedCb.IsBound())
	{
		OnValueChangedCb.Execute(FRuiValue(Clamped));
	}
}

void SRuiNumericEntryBox::HandleValueCommitted(float InValue, ETextCommit::Type)
{
	const float Clamped = Clamp(InValue);
	CurrentValue = Clamped; // reflect the clamp in the field immediately (the display reads CurrentValue)
	if (OnValueCommittedCb.IsBound())
	{
		OnValueCommittedCb.Execute(FRuiValue(Clamped));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (Leaf)
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuiNumericEntryBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiNumericEntryBoxProps& P = static_cast<const FRuiNumericEntryBoxProps&>(Props);
		return SNew(SRuiNumericEntryBox)
			.MinValue(P.HasMinValue() ? TOptional<float>(P.MinValue) : TOptional<float>())
			.MaxValue(P.HasMaxValue() ? TOptional<float>(P.MaxValue) : TOptional<float>());
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiNumericEntryBox& W = static_cast<SRuiNumericEntryBox&>(Widget);
		const FRuiNumericEntryBoxProps& N = static_cast<const FRuiNumericEntryBoxProps&>(New);
		const FRuiNumericEntryBoxProps* O = static_cast<const FRuiNumericEntryBoxProps*>(Old);
		// Bounds first, so a same-render Value re-application clamps against the new bounds (bughunt IW-1:
		// ApplyDiff previously never re-applied Min/Max, so a runtime bound change was inert).
		if (N.HasMinValue() && (O == nullptr || !O->HasMinValue() || !FMath::IsNearlyEqual(N.MinValue, O->MinValue)))
		{
			W.SetMinValue(TOptional<float>(N.MinValue));
		}
		if (N.HasMaxValue() && (O == nullptr || !O->HasMaxValue() || !FMath::IsNearlyEqual(N.MaxValue, O->MaxValue)))
		{
			W.SetMaxValue(TOptional<float>(N.MaxValue));
		}
		if (N.HasValue() && (O == nullptr || !O->HasValue() || !FMath::IsNearlyEqual(N.Value, O->Value)))
		{
			W.SetValue(N.Value);
		}
		if (N.HasOnValueChanged())
		{
			W.SetOnValueChanged(N.OnValueChanged);
		}
		if (N.HasOnValueCommitted())
		{
			W.SetOnValueCommitted(N.OnValueCommitted);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Type, factory, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace RUI::Slate
{
	FRuiElementTypeId NumericEntryBoxType()
	{
		return RUI::InternElementType(FName(TEXT("NumericEntryBox")));
	}

	FRuiNode NumericEntryBox(FRuiNumericEntryBoxProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = NumericEntryBoxType();
		Node.Props = MakeShared<FRuiNumericEntryBoxProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterNumericEntryBoxAdapter()
		{
			RegisterAdapter(NumericEntryBoxType(), MakeUnique<FRuiNumericEntryBoxAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
