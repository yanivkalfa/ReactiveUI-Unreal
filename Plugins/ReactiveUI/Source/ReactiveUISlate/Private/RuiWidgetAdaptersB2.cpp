// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Batch 2 (Phase 7 step 8) — the everyday game widget set (WIDGET_INVENTORY.md), on the same
// production-line shape RuiWidgetAdapters.cpp established (props struct → adapter rows → factory →
// codegen tag + interp builder → contract/test). Each widget maps only header-verified RUNTIME
// setters (no reconstruct masks — every prop here is live-settable).

#include "RuiElementAdapter.h"
#include "RuiEventProxy.h"
#include "RuiSlateElements.h"
#include "RuiSlateLog.h"

#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SNullWidget.h"

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

	namespace Detail
	{
		void RegisterBatch2WidgetAdapters()
		{
			RegisterAdapter(WidgetSwitcherType(), MakeUnique<FRuiWidgetSwitcherAdapter>());
			RegisterAdapter(ScaleBoxType(), MakeUnique<FRuiScaleBoxAdapter>());
			RegisterAdapter(ThrobberType(), MakeUnique<FRuiThrobberAdapter>());
			RegisterAdapter(WrapBoxType(), MakeUnique<FRuiWrapBoxAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate

#undef RUI_ROW
