// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Phase 2 step 3, batch 2 — the remaining core widgets on the pattern RuiCoreAdapters.cpp
// established (production-line shape: props struct → adapter rows → factory → contract
// coverage in ReactiveUI.Widgets.*). Traps honored per widget: SLATE_EVENT args bind at
// construction (the proxy arrives in CreateWidget), SScrollBox orientation is construct-only
// (reconstruct mask), SEditableTextBox applies text skip-when-equal against the WIDGET
// (D-16 caret rule), SRuiCanvas swaps its draw fn by identity (D-12).

#include "RuiElementAdapter.h"
#include "RuiEventProxy.h"
#include "RuiSlateElements.h"
#include "RuiSlateLog.h"
#include "SRuiCanvas.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SNullWidget.h"

namespace
{
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

	/** One compare-and-set row: apply when set this render AND (fresh widget OR changed). */
	template <typename TProps, typename TValue, typename TApply>
	void Row(const TProps* O, bool bNewHas, bool bOldHas, const TValue& NewValue, const TValue& OldValue, TApply Apply)
	{
		if (bNewHas && (O == nullptr || !bOldHas || !(NewValue == OldValue)))
		{
			Apply(NewValue);
		}
	}
} // namespace

// Convenience for the repetitive row shape below.
#define RUI_ROW(Prop, ApplyExpr)                                                                                       \
	if (N.Has##Prop() && (O == nullptr || !O->Has##Prop() || !(N.Prop == O->Prop)))                                    \
	{                                                                                                                  \
		ApplyExpr;                                                                                                     \
	}

// ─────────────────────────────────────────────────────────────────────────────────────────
// SBorder
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiBorderAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SBorder);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SBorder& W = static_cast<SBorder&>(Widget);
		const FRuiBorderProps& N = static_cast<const FRuiBorderProps&>(New);
		const FRuiBorderProps* O = static_cast<const FRuiBorderProps*>(Old);
		RUI_ROW(Padding, W.SetPadding(N.Padding))
		RUI_ROW(BorderBackgroundColor, W.SetBorderBackgroundColor(FSlateColor(N.BorderBackgroundColor)))
		RUI_ROW(HAlign, W.SetHAlign(HAlignOf(N.HAlign)))
		RUI_ROW(VAlign, W.SetVAlign(VAlignOf(N.VAlign)))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SBorder&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SBox
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SBox);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SBox& W = static_cast<SBox&>(Widget);
		const FRuiBoxProps& N = static_cast<const FRuiBoxProps&>(New);
		const FRuiBoxProps* O = static_cast<const FRuiBoxProps*>(Old);
		RUI_ROW(WidthOverride, W.SetWidthOverride(FOptionalSize(N.WidthOverride)))
		RUI_ROW(HeightOverride, W.SetHeightOverride(FOptionalSize(N.HeightOverride)))
		RUI_ROW(MinDesiredWidth, W.SetMinDesiredWidth(FOptionalSize(N.MinDesiredWidth)))
		RUI_ROW(MinDesiredHeight, W.SetMinDesiredHeight(FOptionalSize(N.MinDesiredHeight)))
		RUI_ROW(MaxDesiredWidth, W.SetMaxDesiredWidth(FOptionalSize(N.MaxDesiredWidth)))
		RUI_ROW(MaxDesiredHeight, W.SetMaxDesiredHeight(FOptionalSize(N.MaxDesiredHeight)))
		RUI_ROW(HAlign, W.SetHAlign(HAlignOf(N.HAlign)))
		RUI_ROW(VAlign, W.SetVAlign(VAlignOf(N.VAlign)))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SImage (v1: tint + desired size — brush content is the D-17 asset/GC work)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiImageAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SImage);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SImage& W = static_cast<SImage&>(Widget);
		const FRuiImageProps& N = static_cast<const FRuiImageProps&>(New);
		const FRuiImageProps* O = static_cast<const FRuiImageProps*>(Old);
		RUI_ROW(ColorAndOpacity, W.SetColorAndOpacity(FSlateColor(N.ColorAndOpacity)))
		RUI_ROW(DesiredSizeOverride, W.SetDesiredSizeOverride(N.DesiredSizeOverride))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SScrollBox (Orientation = construct-only; reorder rebuilds — scroll lists pair with
// reuse_by_slot, which never reorders)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiScrollBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiScrollBoxProps& P = static_cast<const FRuiScrollBoxProps&>(Props);
		const EOrientation Orientation =
			(P.HasOrientation() && P.Orientation == FName(TEXT("horizontal"))) ? Orient_Horizontal : Orient_Vertical;
		return SNew(SScrollBox).Orientation(Orientation);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		// Header-sweep correction: SetOrientation IS a runtime setter — no reconstruct mask.
		SScrollBox& W = static_cast<SScrollBox&>(Widget);
		const FRuiScrollBoxProps& N = static_cast<const FRuiScrollBoxProps&>(New);
		const FRuiScrollBoxProps* O = static_cast<const FRuiScrollBoxProps*>(Old);
		RUI_ROW(Orientation,
				W.SetOrientation(N.Orientation == FName(TEXT("horizontal")) ? Orient_Horizontal : Orient_Vertical))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuiStyleDict* SlotProps) override
	{
		SScrollBox& Box = static_cast<SScrollBox&>(Parent);
		SScrollBox::FScopedWidgetSlotArguments Slot = Box.AddSlot();
		Slot.AttachWidget(Child);
		ConfigureSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SScrollBox&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SScrollBox& Box = static_cast<SScrollBox&>(Parent);
		Box.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SScrollBox::FScopedWidgetSlotArguments Slot = Box.AddSlot();
			Slot.AttachWidget(Child);
			ConfigureSlot(Slot, SlotPropsOf(Child));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuiStyleDict* SlotProps) override
	{
		// Padding-only slots: remove + re-add at the end (scroll slots are order-stable for
		// this path; reuse_by_slot lists never hit it). Presence-check first — RemoveSlot
		// returns void, and re-adding a widget that was never a child would grow the list.
		SScrollBox& Box = static_cast<SScrollBox&>(Parent);
		FChildren* Children = Box.GetChildren();
		bool bPresent = false;
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				bPresent = true;
				break;
			}
		}
		if (bPresent)
		{
			Box.RemoveSlot(Child);
			SScrollBox::FScopedWidgetSlotArguments Slot = Box.AddSlot();
			Slot.AttachWidget(Child);
			ConfigureSlot(Slot, SlotProps);
		}
	}

private:
	static void ConfigureSlot(SScrollBox::FScopedWidgetSlotArguments& Slot, const FRuiStyleDict* SlotProps)
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
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSpacer
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiSpacerAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SSpacer);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SSpacer& W = static_cast<SSpacer&>(Widget);
		const FRuiSpacerProps& N = static_cast<const FRuiSpacerProps&>(New);
		const FRuiSpacerProps* O = static_cast<const FRuiSpacerProps*>(Old);
		RUI_ROW(Size, W.SetSize(N.Size))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SEditableTextBox — controlled input (D-16)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiEditableTextAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		return SNew(SEditableTextBox)
			.OnTextChanged(FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleText,
													static_cast<int32>(FRuiEditableTextBoxProps::OnTextChanged_Bit)))
			.OnTextCommitted(
				FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleTextCommit,
										   static_cast<int32>(FRuiEditableTextBoxProps::OnTextCommitted_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SEditableTextBox& W = static_cast<SEditableTextBox&>(Widget);
		const FRuiEditableTextBoxProps& N = static_cast<const FRuiEditableTextBoxProps&>(New);
		const FRuiEditableTextBoxProps* O = static_cast<const FRuiEditableTextBoxProps*>(Old);
		// THE caret rule: compare against the WIDGET's live text, not old props — after the
		// typing round-trip (OnTextChanged -> setState -> re-render) the values match and the
		// skip preserves caret/selection. A programmatic state change still applies.
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
		const FRuiEditableTextBoxProps& N = static_cast<const FRuiEditableTextBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiEditableTextBoxProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiEditableTextBoxProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SCheckBox
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiCheckBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		return SNew(SCheckBox).OnCheckStateChanged(
			FOnCheckStateChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleChecked,
										   static_cast<int32>(FRuiCheckBoxProps::OnCheckStateChanged_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SCheckBox& W = static_cast<SCheckBox&>(Widget);
		const FRuiCheckBoxProps& N = static_cast<const FRuiCheckBoxProps&>(New);
		// Self-notifying family: skip when the widget already agrees (D-16).
		if (N.HasbIsChecked())
		{
			const ECheckBoxState Want = N.bIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			if (W.GetCheckedState() != Want)
			{
				W.SetIsChecked(Want);
			}
		}
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiCheckBoxProps& N = static_cast<const FRuiCheckBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiCheckBoxProps::OnCheckStateChanged_Bit), N.OnCheckStateChanged);
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SCheckBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSlider
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiSliderAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		return SNew(SSlider).OnValueChanged(
			FOnFloatValueChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
										   static_cast<int32>(FRuiSliderProps::OnValueChanged_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SSlider& W = static_cast<SSlider&>(Widget);
		const FRuiSliderProps& N = static_cast<const FRuiSliderProps&>(New);
		const FRuiSliderProps* O = static_cast<const FRuiSliderProps*>(Old);
		if ((N.HasMinValue() || N.HasMaxValue()) &&
			(O == nullptr || !(O->MinValue == N.MinValue) || !(O->MaxValue == N.MaxValue)))
		{
			W.SetMinAndMaxValues(N.HasMinValue() ? N.MinValue : 0.0f, N.HasMaxValue() ? N.MaxValue : 1.0f);
		}
		RUI_ROW(StepSize, W.SetStepSize(N.StepSize))
		// Self-notifying skip (D-16): the drag round-trip lands on an equal value.
		if (N.HasValue() && !FMath::IsNearlyEqual(W.GetValue(), N.Value))
		{
			W.SetValue(N.Value);
		}
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiSliderProps& N = static_cast<const FRuiSliderProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiSliderProps::OnValueChanged_Bit), N.OnValueChanged);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SProgressBar
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiProgressBarAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SProgressBar);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SProgressBar& W = static_cast<SProgressBar&>(Widget);
		const FRuiProgressBarProps& N = static_cast<const FRuiProgressBarProps&>(New);
		const FRuiProgressBarProps* O = static_cast<const FRuiProgressBarProps*>(Old);
		RUI_ROW(Percent, W.SetPercent(N.Percent))
	}

	virtual bool ApplyStyleKey(SWidget& Widget, FName Key, const FRuiValue* Value) override
	{
		if (Key == FName(TEXT("FillColorAndOpacity")))
		{
			static_cast<SProgressBar&>(Widget).SetFillColorAndOpacity(
				Value != nullptr ? FSlateColor(Value->ColorValue) : FSlateColor(FLinearColor::White));
			return true;
		}
		return false;
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SRuiCanvas
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiCanvasAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SRuiCanvas);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiCanvas& W = static_cast<SRuiCanvas&>(Widget);
		const FRuiCanvasProps& N = static_cast<const FRuiCanvasProps&>(New);
		const FRuiCanvasProps* O = static_cast<const FRuiCanvasProps*>(Old);
		if (N.HasDrawFn())
		{
			W.SetDrawFn(N.DrawFn); // identity-guarded inside the widget
		}
		RUI_ROW(RedrawKey, W.SetRedrawKey(N.RedrawKey))
		RUI_ROW(CanvasSize, W.SetCanvasDesiredSize(N.CanvasSize))
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
		FRuiNode MakeHostNode2(FRuiElementTypeId Type, TProps Props, TArray<FRuiNode> Children, FRuiKey Key)
		{
			FRuiNode Node;
			Node.Kind = ERuiNodeKind::Host;
			Node.ElementType = Type;
			Node.Props = MakeShared<TProps>(MoveTemp(Props));
			Node.Children = RUI::MakeChildren(MoveTemp(Children));
			Node.Key = Key;
			return Node;
		}

		FRuiElementTypeId BorderType()
		{
			return RUI::InternElementType(FName(TEXT("Border")));
		}
		FRuiElementTypeId BoxType()
		{
			return RUI::InternElementType(FName(TEXT("Box")));
		}
		FRuiElementTypeId ImageType()
		{
			return RUI::InternElementType(FName(TEXT("Image")));
		}
		FRuiElementTypeId ScrollBoxType()
		{
			return RUI::InternElementType(FName(TEXT("ScrollBox")));
		}
		FRuiElementTypeId SpacerType()
		{
			return RUI::InternElementType(FName(TEXT("Spacer")));
		}
		FRuiElementTypeId EditableTextBoxType()
		{
			return RUI::InternElementType(FName(TEXT("EditableTextBox")));
		}
		FRuiElementTypeId CheckBoxType()
		{
			return RUI::InternElementType(FName(TEXT("CheckBox")));
		}
		FRuiElementTypeId SliderType()
		{
			return RUI::InternElementType(FName(TEXT("Slider")));
		}
		FRuiElementTypeId ProgressBarType()
		{
			return RUI::InternElementType(FName(TEXT("ProgressBar")));
		}
		FRuiElementTypeId RuiCanvasType()
		{
			return RUI::InternElementType(FName(TEXT("RuiCanvas")));
		}
	} // namespace

	FRuiNode Border(FRuiBorderProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNode2(BorderType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode Box(FRuiBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNode2(BoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode Image(FRuiImageProps Props, FRuiKey Key)
	{
		return MakeHostNode2(ImageType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode ScrollBox(FRuiScrollBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNode2(ScrollBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode Spacer(FRuiSpacerProps Props, FRuiKey Key)
	{
		return MakeHostNode2(SpacerType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode EditableTextBox(FRuiEditableTextBoxProps Props, FRuiKey Key)
	{
		return MakeHostNode2(EditableTextBoxType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode CheckBox(FRuiCheckBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNode2(CheckBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode Slider(FRuiSliderProps Props, FRuiKey Key)
	{
		return MakeHostNode2(SliderType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode ProgressBar(FRuiProgressBarProps Props, FRuiKey Key)
	{
		return MakeHostNode2(ProgressBarType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode RuiCanvas(FRuiCanvasProps Props, FRuiKey Key)
	{
		return MakeHostNode2(RuiCanvasType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}

	TSharedPtr<FRuiDrawFn> MakeDrawFn(FRuiDrawFn Fn)
	{
		return MakeShared<FRuiDrawFn>(MoveTemp(Fn));
	}

	namespace Detail
	{
		void RegisterBatch2Adapters()
		{
			RegisterAdapter(BorderType(), MakeUnique<FRuiBorderAdapter>());
			RegisterAdapter(BoxType(), MakeUnique<FRuiBoxAdapter>());
			RegisterAdapter(ImageType(), MakeUnique<FRuiImageAdapter>());
			RegisterAdapter(ScrollBoxType(), MakeUnique<FRuiScrollBoxAdapter>());
			RegisterAdapter(SpacerType(), MakeUnique<FRuiSpacerAdapter>());
			RegisterAdapter(EditableTextBoxType(), MakeUnique<FRuiEditableTextAdapter>());
			RegisterAdapter(CheckBoxType(), MakeUnique<FRuiCheckBoxAdapter>());
			RegisterAdapter(SliderType(), MakeUnique<FRuiSliderAdapter>());
			RegisterAdapter(ProgressBarType(), MakeUnique<FRuiProgressBarAdapter>());
			RegisterAdapter(RuiCanvasType(), MakeUnique<FRuiCanvasAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate

#undef RUI_ROW
