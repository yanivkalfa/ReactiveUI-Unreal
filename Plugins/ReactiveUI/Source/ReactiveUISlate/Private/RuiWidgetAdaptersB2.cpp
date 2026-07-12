// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Batch 2 (Phase 7 step 8) — the everyday game widget set (WIDGET_INVENTORY.md), on the same
// production-line shape RuiWidgetAdapters.cpp established (props struct → adapter rows → factory →
// codegen tag + interp builder → contract/test). Setters are header-verified RUNTIME setters
// wherever Slate exposes them; SSeparator is the exception — its Orientation/Thickness bake at
// construction, so it declares a reconstruct mask (the first shipped widget to exercise TD-011).

#include "RuiElementAdapter.h"
#include "RuiEventProxy.h"
#include "RuiSlateElements.h"
#include "RuiSlateLog.h"

#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SUniformWrapPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/SRichTextBlock.h"

namespace
{
	// ── shared parsers (batch-2-local; the box-panel set lives in RuiCoreAdapters.cpp) ─────

	EHorizontalAlignment HAlignOf(FName Value)
	{
		if (Value == FName(TEXT("left")))
		{
			return HAlign_Left;
		}
		if (Value == FName(TEXT("center")))
		{
			return HAlign_Center;
		}
		if (Value == FName(TEXT("right")))
		{
			return HAlign_Right;
		}
		return HAlign_Fill;
	}

	EVerticalAlignment VAlignOf(FName Value)
	{
		if (Value == FName(TEXT("top")))
		{
			return VAlign_Top;
		}
		if (Value == FName(TEXT("center")))
		{
			return VAlign_Center;
		}
		if (Value == FName(TEXT("bottom")))
		{
			return VAlign_Bottom;
		}
		return VAlign_Fill;
	}

	EStretch::Type StretchOf(FName V)
	{
		if (V == FName(TEXT("fill")))
		{
			return EStretch::Fill;
		}
		if (V == FName(TEXT("scaleToFit")))
		{
			return EStretch::ScaleToFit;
		}
		if (V == FName(TEXT("scaleToFitX")))
		{
			return EStretch::ScaleToFitX;
		}
		if (V == FName(TEXT("scaleToFitY")))
		{
			return EStretch::ScaleToFitY;
		}
		if (V == FName(TEXT("scaleToFill")))
		{
			return EStretch::ScaleToFill;
		}
		if (V == FName(TEXT("scaleBySafeZone")))
		{
			return EStretch::ScaleBySafeZone;
		}
		return EStretch::None;
	}

	EStretchDirection::Type StretchDirOf(FName V)
	{
		if (V == FName(TEXT("downOnly")))
		{
			return EStretchDirection::DownOnly;
		}
		if (V == FName(TEXT("upOnly")))
		{
			return EStretchDirection::UpOnly;
		}
		return EStretchDirection::Both;
	}

	SThrobber::EAnimation ThrobberAnimOf(FName V)
	{
		if (V == FName(TEXT("none")))
		{
			return SThrobber::None;
		}
		if (V == FName(TEXT("vertical")))
		{
			return SThrobber::Vertical;
		}
		if (V == FName(TEXT("horizontal")))
		{
			return SThrobber::Horizontal;
		}
		if (V == FName(TEXT("opacity")))
		{
			return SThrobber::Opacity;
		}
		if (V == FName(TEXT("verticalAndOpacity")))
		{
			return SThrobber::VerticalAndOpacity;
		}
		return SThrobber::All;
	}

	/** Apply the common slot.* layout keys to a scoped slot-args object (padding/halign/valign). */
	template <typename TSlotArgs> void ConfigureCommonSlot(TSlotArgs& Slot, const FRuiStyleDict* SlotProps)
	{
		if (SlotProps == nullptr)
		{
			return;
		}
		if (const FRuiValue* Padding = SlotProps->Find(FName(TEXT("slot.padding"))))
		{
			const float Uniform = Padding->Kind == FRuiValue::EKind::Int ? static_cast<float>(Padding->IntValue)
																		 : static_cast<float>(Padding->FloatValue);
			Slot.Padding(FMargin(Uniform));
		}
		if (const FRuiValue* H = SlotProps->Find(FName(TEXT("slot.halign"))))
		{
			Slot.HAlign(HAlignOf(H->Kind == FRuiValue::EKind::Name ? H->NameValue : FName(*H->StringValue)));
		}
		if (const FRuiValue* V = SlotProps->Find(FName(TEXT("slot.valign"))))
		{
			Slot.VAlign(VAlignOf(V->Kind == FRuiValue::EKind::Name ? V->NameValue : FName(*V->StringValue)));
		}
	}

	bool ChildPresent(FChildren* Children, const TSharedRef<SWidget>& Child)
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				return true;
			}
		}
		return false;
	}

	/** Read an integer slot.* key (grid column/row); absent -> Def. */
	int32 SlotIntOf(const FRuiStyleDict* SlotProps, const TCHAR* Key, int32 Def)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuiValue* V = SlotProps->Find(FName(Key)))
			{
				return V->Kind == FRuiValue::EKind::Int ? static_cast<int32>(V->IntValue)
														: static_cast<int32>(V->FloatValue);
			}
		}
		return Def;
	}
} // namespace

// Convenience for the repetitive row shape (mirrors RuiWidgetAdapters.cpp).
#define RUI_ROW(Prop, ApplyExpr)                                                                                       \
	if (N.Has##Prop() && (O == nullptr || !O->Has##Prop() || !(N.Prop == O->Prop)))                                    \
	{                                                                                                                  \
		ApplyExpr;                                                                                                     \
	}

// ─────────────────────────────────────────────────────────────────────────────────────────
// SWidgetSwitcher (MultiSlot; shows one child by index)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiWidgetSwitcherAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SWidgetSwitcher);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SWidgetSwitcher& W = static_cast<SWidgetSwitcher&>(Widget);
		const FRuiWidgetSwitcherProps& N = static_cast<const FRuiWidgetSwitcherProps&>(New);
		const FRuiWidgetSwitcherProps* O = static_cast<const FRuiWidgetSwitcherProps*>(Old);
		RUI_ROW(WidgetIndex, W.SetActiveWidgetIndex(N.WidgetIndex))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32 Index,
							 const FRuiStyleDict* SlotProps) override
	{
		SWidgetSwitcher& W = static_cast<SWidgetSwitcher&>(Parent);
		SWidgetSwitcher::FScopedWidgetSlotArguments Slot = W.AddSlot(Index);
		Slot.AttachWidget(Child);
		ConfigureCommonSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SWidgetSwitcher&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SWidgetSwitcher& W = static_cast<SWidgetSwitcher&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			W.RemoveSlot(Child);
		}
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SWidgetSwitcher::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
			ConfigureCommonSlot(Slot, SlotPropsOf(Child));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuiStyleDict* SlotProps) override
	{
		// SWidgetSwitcher::GetChildren is protected — RemoveSlot returns the removed index
		// (INDEX_NONE if absent), so re-add at the SAME index to preserve the switcher position.
		SWidgetSwitcher& W = static_cast<SWidgetSwitcher&>(Parent);
		const int32 At = W.RemoveSlot(Child);
		if (At != INDEX_NONE)
		{
			SWidgetSwitcher::FScopedWidgetSlotArguments Slot = W.AddSlot(At);
			Slot.AttachWidget(Child);
			ConfigureCommonSlot(Slot, SlotProps);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SScaleBox (SingleContent)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiScaleBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SScaleBox);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SScaleBox& W = static_cast<SScaleBox&>(Widget);
		const FRuiScaleBoxProps& N = static_cast<const FRuiScaleBoxProps&>(New);
		const FRuiScaleBoxProps* O = static_cast<const FRuiScaleBoxProps*>(Old);
		RUI_ROW(Stretch, W.SetStretch(StretchOf(N.Stretch)))
		RUI_ROW(StretchDirection, W.SetStretchDirection(StretchDirOf(N.StretchDirection)))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SScaleBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SThrobber (Leaf)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiThrobberAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SThrobber);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SThrobber& W = static_cast<SThrobber&>(Widget);
		const FRuiThrobberProps& N = static_cast<const FRuiThrobberProps&>(New);
		const FRuiThrobberProps* O = static_cast<const FRuiThrobberProps*>(Old);
		RUI_ROW(NumPieces, W.SetNumPieces(N.NumPieces))
		RUI_ROW(Animate, W.SetAnimate(ThrobberAnimOf(N.Animate)))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SWrapBox (MultiSlot)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiWrapBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SWrapBox);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SWrapBox& W = static_cast<SWrapBox&>(Widget);
		const FRuiWrapBoxProps& N = static_cast<const FRuiWrapBoxProps&>(New);
		const FRuiWrapBoxProps* O = static_cast<const FRuiWrapBoxProps*>(Old);
		RUI_ROW(Orientation,
				W.SetOrientation(N.Orientation == FName(TEXT("vertical")) ? Orient_Vertical : Orient_Horizontal))
		RUI_ROW(bUseAllottedSize, W.SetUseAllottedSize(N.bUseAllottedSize))
		RUI_ROW(WrapSize, W.SetWrapSize(N.WrapSize))
		RUI_ROW(InnerSlotPadding, W.SetInnerSlotPadding(N.InnerSlotPadding))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuiStyleDict* SlotProps) override
	{
		SWrapBox& W = static_cast<SWrapBox&>(Parent);
		SWrapBox::FScopedWidgetSlotArguments Slot = W.AddSlot();
		Slot.AttachWidget(Child);
		ConfigureCommonSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SWrapBox&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SWrapBox& W = static_cast<SWrapBox&>(Parent);
		W.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SWrapBox::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
			ConfigureCommonSlot(Slot, SlotPropsOf(Child));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuiStyleDict* SlotProps) override
	{
		SWrapBox& W = static_cast<SWrapBox&>(Parent);
		if (ChildPresent(W.GetChildren(), Child))
		{
			W.RemoveSlot(Child);
			SWrapBox::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
			ConfigureCommonSlot(Slot, SlotProps);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SMultiLineEditableTextBox — multi-line controlled input (D-16 caret rule)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiMultiLineEditableTextAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		return SNew(SMultiLineEditableTextBox)
			.OnTextChanged(
				FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleText,
										 static_cast<int32>(FRuiMultiLineEditableTextBoxProps::OnTextChanged_Bit)))
			.OnTextCommitted(
				FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleTextCommit,
										   static_cast<int32>(FRuiMultiLineEditableTextBoxProps::OnTextCommitted_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SMultiLineEditableTextBox& W = static_cast<SMultiLineEditableTextBox&>(Widget);
		const FRuiMultiLineEditableTextBoxProps& N = static_cast<const FRuiMultiLineEditableTextBoxProps&>(New);
		const FRuiMultiLineEditableTextBoxProps* O = static_cast<const FRuiMultiLineEditableTextBoxProps*>(Old);
		// The D-16 caret rule: compare against the WIDGET's live text (survives the typing round-trip).
		if (N.HasText() && !W.GetText().EqualTo(N.Text))
		{
			W.SetText(N.Text);
		}
		if (N.HasHintText() && (O == nullptr || !O->HasHintText() || !N.HintText.EqualTo(O->HintText)))
		{
			W.SetHintText(N.HintText);
		}
		RUI_ROW(bIsReadOnly, W.SetIsReadOnly(N.bIsReadOnly))
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiMultiLineEditableTextBoxProps& N = static_cast<const FRuiMultiLineEditableTextBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiMultiLineEditableTextBoxProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiMultiLineEditableTextBoxProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSearchBox — SEditableTextBox specialization (controlled text; D-16 caret rule)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiSearchBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		return SNew(SSearchBox)
			.OnTextChanged(FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleText,
													static_cast<int32>(FRuiSearchBoxProps::OnTextChanged_Bit)))
			.OnTextCommitted(FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleTextCommit,
														static_cast<int32>(FRuiSearchBoxProps::OnTextCommitted_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SSearchBox& W = static_cast<SSearchBox&>(Widget);
		const FRuiSearchBoxProps& N = static_cast<const FRuiSearchBoxProps&>(New);
		const FRuiSearchBoxProps* O = static_cast<const FRuiSearchBoxProps*>(Old);
		if (N.HasText() && !W.GetText().EqualTo(N.Text))
		{
			W.SetText(N.Text);
		}
		if (N.HasHintText() && (O == nullptr || !O->HasHintText() || !N.HintText.EqualTo(O->HintText)))
		{
			W.SetHintText(N.HintText);
		}
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiSearchBoxProps& N = static_cast<const FRuiSearchBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiSearchBoxProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiSearchBoxProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSafeZone (SingleContent)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiSafeZoneAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SSafeZone);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SSafeZone& W = static_cast<SSafeZone&>(Widget);
		const FRuiSafeZoneProps& N = static_cast<const FRuiSafeZoneProps&>(New);
		const FRuiSafeZoneProps* O = static_cast<const FRuiSafeZoneProps*>(Old);
		RUI_ROW(bIsTitleSafe, W.SetTitleSafe(N.bIsTitleSafe))
		// The four pad-side bools apply together (single Slate setter).
		if ((N.HasbPadLeft() || N.HasbPadRight() || N.HasbPadTop() || N.HasbPadBottom()) &&
			(O == nullptr || !(O->bPadLeft == N.bPadLeft) || !(O->bPadRight == N.bPadRight) ||
			 !(O->bPadTop == N.bPadTop) || !(O->bPadBottom == N.bPadBottom)))
		{
			W.SetSidesToPad(N.HasbPadLeft() ? N.bPadLeft : true, N.HasbPadRight() ? N.bPadRight : true,
							N.HasbPadTop() ? N.bPadTop : true, N.HasbPadBottom() ? N.bPadBottom : true);
		}
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SSafeZone&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SDPIScaler (SingleContent)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiDPIScalerAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SDPIScaler);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SDPIScaler& W = static_cast<SDPIScaler&>(Widget);
		const FRuiDPIScalerProps& N = static_cast<const FRuiDPIScalerProps&>(New);
		const FRuiDPIScalerProps* O = static_cast<const FRuiDPIScalerProps*>(Old);
		RUI_ROW(DPIScale, W.SetDPIScale(N.DPIScale))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SDPIScaler&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSeparator (Leaf) — Orientation + Thickness are CONSTRUCT-ONLY (the first shipped widget to
// exercise the TD-011 reconstruct mask); ColorAndOpacity is a live setter (inherited SBorder).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiSeparatorAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	// Orientation (bit 0) + Thickness (bit 1) bake at construction — a change replaces the widget.
	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiSeparatorProps::Orientation_Bit) | (1ull << FRuiSeparatorProps::Thickness_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiSeparatorProps& O = static_cast<const FRuiSeparatorProps&>(Old);
		const FRuiSeparatorProps& N = static_cast<const FRuiSeparatorProps&>(New);
		return !(O.Orientation == N.Orientation) || !(O.Thickness == N.Thickness);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiSeparatorProps& P = static_cast<const FRuiSeparatorProps&>(Props);
		return SNew(SSeparator)
			.Orientation(P.HasOrientation() && P.Orientation == FName(TEXT("vertical")) ? Orient_Vertical
																						: Orient_Horizontal)
			.Thickness(P.HasThickness() ? P.Thickness : 3.0f);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SSeparator& W = static_cast<SSeparator&>(Widget);
		const FRuiSeparatorProps& N = static_cast<const FRuiSeparatorProps&>(New);
		const FRuiSeparatorProps* O = static_cast<const FRuiSeparatorProps*>(Old);
		RUI_ROW(ColorAndOpacity, W.SetColorAndOpacity(N.ColorAndOpacity))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSpinBox<float> (Leaf) — numeric input (self-notifying skip; D-16)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiSpinBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		return SNew(SSpinBox<float>)
			.OnValueChanged(
				SSpinBox<float>::FOnValueChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
														   static_cast<int32>(FRuiSpinBoxProps::OnValueChanged_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SSpinBox<float>& W = static_cast<SSpinBox<float>&>(Widget);
		const FRuiSpinBoxProps& N = static_cast<const FRuiSpinBoxProps&>(New);
		const FRuiSpinBoxProps* O = static_cast<const FRuiSpinBoxProps*>(Old);
		RUI_ROW(MinValue, W.SetMinValue(N.MinValue))
		RUI_ROW(MaxValue, W.SetMaxValue(N.MaxValue))
		RUI_ROW(Delta, W.SetDelta(N.Delta))
		// Self-notifying skip: the drag/commit round-trip lands on an equal value (D-16).
		if (N.HasValue() && !FMath::IsNearlyEqual(W.GetValue(), N.Value))
		{
			W.SetValue(N.Value);
		}
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiSpinBoxProps& N = static_cast<const FRuiSpinBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiSpinBoxProps::OnValueChanged_Bit), N.OnValueChanged);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SUniformWrapPanel (MultiSlot)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiUniformWrapPanelAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SUniformWrapPanel);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SUniformWrapPanel& W = static_cast<SUniformWrapPanel&>(Widget);
		const FRuiUniformWrapPanelProps& N = static_cast<const FRuiUniformWrapPanelProps&>(New);
		const FRuiUniformWrapPanelProps* O = static_cast<const FRuiUniformWrapPanelProps*>(Old);
		RUI_ROW(SlotPadding, W.SetSlotPadding(FMargin(N.SlotPadding)))
		RUI_ROW(HAlign, W.SetHorizontalAlignment(HAlignOf(N.HAlign)))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32, const FRuiStyleDict*) override
	{
		SUniformWrapPanel& W = static_cast<SUniformWrapPanel&>(Parent);
		SUniformWrapPanel::FScopedWidgetSlotArguments Slot = W.AddSlot();
		Slot.AttachWidget(Child);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		FChildren* Children = static_cast<SUniformWrapPanel&>(Parent).GetChildren();
		// SUniformWrapPanel has no RemoveSlot(widget) — rebuild without the removed child.
		TArray<TSharedRef<SWidget>> Kept;
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			TSharedRef<SWidget> C = Children->GetChildAt(i);
			if (&C.Get() != &Child.Get())
			{
				Kept.Add(C);
			}
		}
		Rebuild(static_cast<SUniformWrapPanel&>(Parent), Kept);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)>) override
	{
		Rebuild(static_cast<SUniformWrapPanel&>(Parent), Ordered);
	}

private:
	static void Rebuild(SUniformWrapPanel& W, const TArray<TSharedRef<SWidget>>& Children)
	{
		W.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Children)
		{
			SUniformWrapPanel::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SRichTextBlock (Leaf)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiRichTextBlockAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SRichTextBlock);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRichTextBlock& W = static_cast<SRichTextBlock&>(Widget);
		const FRuiRichTextBlockProps& N = static_cast<const FRuiRichTextBlockProps&>(New);
		const FRuiRichTextBlockProps* O = static_cast<const FRuiRichTextBlockProps*>(Old);
		if (N.HasText() && (O == nullptr || !O->HasText() || !N.Text.EqualTo(O->Text)))
		{
			W.SetText(N.Text);
		}
		RUI_ROW(bAutoWrapText, W.SetAutoWrapText(N.bAutoWrapText))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SGridPanel / SUniformGridPanel (MultiSlot; slot.column + slot.row place each child)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiGridPanelAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SGridPanel);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuiStyleDict* SlotProps) override
	{
		SGridPanel& W = static_cast<SGridPanel&>(Parent);
		SGridPanel::FScopedWidgetSlotArguments Slot =
			W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
		Slot.AttachWidget(Child);
		ConfigureCommonSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		Rebuild(static_cast<SGridPanel&>(Parent), Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SGridPanel& W = static_cast<SGridPanel&>(Parent);
		W.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			const FRuiStyleDict* SlotProps = SlotPropsOf(Child);
			SGridPanel::FScopedWidgetSlotArguments Slot =
				W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
			Slot.AttachWidget(Child);
			ConfigureCommonSlot(Slot, SlotProps);
		}
	}

private:
	static void Rebuild(SGridPanel& W, const TSharedRef<SWidget>& Remove)
	{
		// SGridPanel::RemoveSlot exists but takes a widget; a bare remove drops cell placement,
		// so rebuild keeping every OTHER child at its current column/row is out of reach without
		// stored cells. v1: just remove the slot (the reconciler re-inserts survivors on reorder).
		W.RemoveSlot(Remove);
	}
};

class FRuiUniformGridPanelAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SUniformGridPanel);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SUniformGridPanel& W = static_cast<SUniformGridPanel&>(Widget);
		const FRuiUniformGridPanelProps& N = static_cast<const FRuiUniformGridPanelProps&>(New);
		const FRuiUniformGridPanelProps* O = static_cast<const FRuiUniformGridPanelProps*>(Old);
		RUI_ROW(SlotPadding, W.SetSlotPadding(FMargin(N.SlotPadding)))
		RUI_ROW(MinDesiredSlotWidth, W.SetMinDesiredSlotWidth(N.MinDesiredSlotWidth))
		RUI_ROW(MinDesiredSlotHeight, W.SetMinDesiredSlotHeight(N.MinDesiredSlotHeight))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuiStyleDict* SlotProps) override
	{
		SUniformGridPanel& W = static_cast<SUniformGridPanel&>(Parent);
		SUniformGridPanel::FScopedWidgetSlotArguments Slot =
			W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
		Slot.AttachWidget(Child);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SUniformGridPanel&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SUniformGridPanel& W = static_cast<SUniformGridPanel&>(Parent);
		W.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			const FRuiStyleDict* SlotProps = SlotPropsOf(Child);
			SUniformGridPanel::FScopedWidgetSlotArguments Slot =
				W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
			Slot.AttachWidget(Child);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Types, factories, registration
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace RUI::Slate
{
	namespace
	{
		template <typename TProps>
		FRuiNode MakeHostNodeB2(FRuiElementTypeId Type, TProps Props, TArray<FRuiNode> Children, FRuiKey Key)
		{
			FRuiNode Node;
			Node.Kind = ERuiNodeKind::Host;
			Node.ElementType = Type;
			Node.Props = MakeShared<TProps>(MoveTemp(Props));
			Node.Children = RUI::MakeChildren(MoveTemp(Children));
			Node.Key = Key;
			return Node;
		}

		FRuiElementTypeId WidgetSwitcherType()
		{
			return RUI::InternElementType(FName(TEXT("WidgetSwitcher")));
		}
		FRuiElementTypeId ScaleBoxType()
		{
			return RUI::InternElementType(FName(TEXT("ScaleBox")));
		}
		FRuiElementTypeId ThrobberType()
		{
			return RUI::InternElementType(FName(TEXT("Throbber")));
		}
		FRuiElementTypeId WrapBoxType()
		{
			return RUI::InternElementType(FName(TEXT("WrapBox")));
		}
		FRuiElementTypeId MultiLineEditableTextBoxType()
		{
			return RUI::InternElementType(FName(TEXT("MultiLineEditableTextBox")));
		}
		FRuiElementTypeId SearchBoxType()
		{
			return RUI::InternElementType(FName(TEXT("SearchBox")));
		}
		FRuiElementTypeId SafeZoneType()
		{
			return RUI::InternElementType(FName(TEXT("SafeZone")));
		}
		FRuiElementTypeId DPIScalerType()
		{
			return RUI::InternElementType(FName(TEXT("DPIScaler")));
		}
		FRuiElementTypeId SeparatorType()
		{
			return RUI::InternElementType(FName(TEXT("Separator")));
		}
		FRuiElementTypeId SpinBoxType()
		{
			return RUI::InternElementType(FName(TEXT("SpinBox")));
		}
		FRuiElementTypeId UniformWrapPanelType()
		{
			return RUI::InternElementType(FName(TEXT("UniformWrapPanel")));
		}
		FRuiElementTypeId RichTextBlockType()
		{
			return RUI::InternElementType(FName(TEXT("RichTextBlock")));
		}
		FRuiElementTypeId GridPanelType()
		{
			return RUI::InternElementType(FName(TEXT("GridPanel")));
		}
		FRuiElementTypeId UniformGridPanelType()
		{
			return RUI::InternElementType(FName(TEXT("UniformGridPanel")));
		}
	} // namespace

	FRuiNode WidgetSwitcher(FRuiWidgetSwitcherProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB2(WidgetSwitcherType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode ScaleBox(FRuiScaleBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB2(ScaleBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode Throbber(FRuiThrobberProps Props, FRuiKey Key)
	{
		return MakeHostNodeB2(ThrobberType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode WrapBox(FRuiWrapBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB2(WrapBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode MultiLineEditableTextBox(FRuiMultiLineEditableTextBoxProps Props, FRuiKey Key)
	{
		return MakeHostNodeB2(MultiLineEditableTextBoxType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode SearchBox(FRuiSearchBoxProps Props, FRuiKey Key)
	{
		return MakeHostNodeB2(SearchBoxType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode SafeZone(FRuiSafeZoneProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB2(SafeZoneType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode DPIScaler(FRuiDPIScalerProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB2(DPIScalerType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode Separator(FRuiSeparatorProps Props, FRuiKey Key)
	{
		return MakeHostNodeB2(SeparatorType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode SpinBox(FRuiSpinBoxProps Props, FRuiKey Key)
	{
		return MakeHostNodeB2(SpinBoxType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode UniformWrapPanel(FRuiUniformWrapPanelProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB2(UniformWrapPanelType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode RichTextBlock(FRuiRichTextBlockProps Props, FRuiKey Key)
	{
		return MakeHostNodeB2(RichTextBlockType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode GridPanel(FRuiGridPanelProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB2(GridPanelType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode UniformGridPanel(FRuiUniformGridPanelProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB2(UniformGridPanelType(), MoveTemp(Props), MoveTemp(Children), Key);
	}

	namespace Detail
	{
		void RegisterBatch2WidgetAdapters()
		{
			RegisterAdapter(WidgetSwitcherType(), MakeUnique<FRuiWidgetSwitcherAdapter>());
			RegisterAdapter(ScaleBoxType(), MakeUnique<FRuiScaleBoxAdapter>());
			RegisterAdapter(ThrobberType(), MakeUnique<FRuiThrobberAdapter>());
			RegisterAdapter(WrapBoxType(), MakeUnique<FRuiWrapBoxAdapter>());
			RegisterAdapter(MultiLineEditableTextBoxType(), MakeUnique<FRuiMultiLineEditableTextAdapter>());
			RegisterAdapter(SearchBoxType(), MakeUnique<FRuiSearchBoxAdapter>());
			RegisterAdapter(SafeZoneType(), MakeUnique<FRuiSafeZoneAdapter>());
			RegisterAdapter(DPIScalerType(), MakeUnique<FRuiDPIScalerAdapter>());
			RegisterAdapter(SeparatorType(), MakeUnique<FRuiSeparatorAdapter>());
			RegisterAdapter(SpinBoxType(), MakeUnique<FRuiSpinBoxAdapter>());
			RegisterAdapter(UniformWrapPanelType(), MakeUnique<FRuiUniformWrapPanelAdapter>());
			RegisterAdapter(RichTextBlockType(), MakeUnique<FRuiRichTextBlockAdapter>());
			RegisterAdapter(GridPanelType(), MakeUnique<FRuiGridPanelAdapter>());
			RegisterAdapter(UniformGridPanelType(), MakeUnique<FRuiUniformGridPanelAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate

#undef RUI_ROW
