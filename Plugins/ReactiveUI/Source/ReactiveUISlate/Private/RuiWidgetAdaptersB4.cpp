// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Batch 3 wave 3 (WIDGET_COMPLETION_PLAN §3) — the protocol widgets. P5a: SConstraintCanvas's
// anchor slot model (Slot.Offset / Slot.Anchors / Slot.Alignment / Slot.AutoSize / Slot.ZOrder —
// ALL live per-slot setters, mirroring the SCanvas adapter's in-place slot mutation).

#include "RuiElementAdapter.h"
#include "RuiEventProxy.h"
#include "RuiSlateElements.h"
#include "RuiSlotValue.h"

#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SNullWidget.h"

namespace
{
	const FName SlotOffsetKey(TEXT("slot.offset"));
	const FName SlotAnchorsKey(TEXT("slot.anchors"));
	const FName SlotAlignmentKey(TEXT("slot.alignment"));
	const FName SlotAutoSizeKey(TEXT("slot.autosize"));
	const FName SlotZOrderKey(TEXT("slot.zorder"));

	/** FMargin from Float (uniform) | Vector2 (h, v) | "l,t,r,b" string. */
	FMargin MarginOf(const FRuiValue& V)
	{
		switch (V.Kind)
		{
		case FRuiValue::EKind::Int:
			return FMargin(static_cast<float>(V.IntValue));
		case FRuiValue::EKind::Float:
			return FMargin(static_cast<float>(V.FloatValue));
		case FRuiValue::EKind::Vector2:
			return FMargin(static_cast<float>(V.Vector2Value.X), static_cast<float>(V.Vector2Value.Y));
		default:
		{
			const FString S = V.Kind == FRuiValue::EKind::Name ? V.NameValue.ToString() : V.StringValue;
			TArray<FString> Parts;
			S.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 4)
			{
				return FMargin(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]),
							   FCString::Atof(*Parts[3]));
			}
			if (Parts.Num() == 2)
			{
				return FMargin(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
			}
			return FMargin(Parts.Num() == 1 ? FCString::Atof(*Parts[0]) : 0.0f);
		}
		}
	}

	/** FAnchors from Vector2 (uniform min=max) | "minX,minY,maxX,maxY" | "x,y" | uniform num. */
	FAnchors AnchorsOf(const FRuiValue& V)
	{
		switch (V.Kind)
		{
		case FRuiValue::EKind::Int:
			return FAnchors(static_cast<float>(V.IntValue));
		case FRuiValue::EKind::Float:
			return FAnchors(static_cast<float>(V.FloatValue));
		case FRuiValue::EKind::Vector2:
			return FAnchors(static_cast<float>(V.Vector2Value.X), static_cast<float>(V.Vector2Value.Y));
		default:
		{
			const FString S = V.Kind == FRuiValue::EKind::Name ? V.NameValue.ToString() : V.StringValue;
			TArray<FString> Parts;
			S.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 4)
			{
				return FAnchors(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]),
								FCString::Atof(*Parts[3]));
			}
			if (Parts.Num() == 2)
			{
				return FAnchors(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
			}
			return FAnchors(Parts.Num() == 1 ? FCString::Atof(*Parts[0]) : 0.0f);
		}
		}
	}

	/** Apply the anchor-slot keys to a live SConstraintCanvas slot (absent keys -> defaults). */
	void ApplyConstraintSlot(SConstraintCanvas::FSlot& Slot, const FRuiStyleDict* SlotProps)
	{
		const FRuiValue* Offset = SlotProps != nullptr ? SlotProps->Find(SlotOffsetKey) : nullptr;
		const FRuiValue* Anchors = SlotProps != nullptr ? SlotProps->Find(SlotAnchorsKey) : nullptr;
		const FRuiValue* Alignment = SlotProps != nullptr ? SlotProps->Find(SlotAlignmentKey) : nullptr;
		const FRuiValue* AutoSize = SlotProps != nullptr ? SlotProps->Find(SlotAutoSizeKey) : nullptr;
		const FRuiValue* ZOrder = SlotProps != nullptr ? SlotProps->Find(SlotZOrderKey) : nullptr;
		Slot.SetOffset(Offset != nullptr ? MarginOf(*Offset) : FMargin(0.f, 0.f, 1.f, 1.f));
		Slot.SetAnchors(Anchors != nullptr ? AnchorsOf(*Anchors) : FAnchors(0.f));
		Slot.SetAlignment(Alignment != nullptr ? RUI::Slate::SlotValue::AsVector2(*Alignment) : FVector2D(0.5, 0.5));
		Slot.SetAutoSize(AutoSize != nullptr && AutoSize->BoolValue);
		Slot.SetZOrder(ZOrder != nullptr ? RUI::Slate::SlotValue::AsFloat(*ZOrder) : 0.f);
	}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────────────────
// SConstraintCanvas (P5a) — anchor-based absolute panel; live in-place slot mutation.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiConstraintCanvasAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SConstraintCanvas);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuiStyleDict* SlotProps) override
	{
		SConstraintCanvas& W = static_cast<SConstraintCanvas&>(Parent);
		SConstraintCanvas::FScopedWidgetSlotArguments Slot = W.AddSlot();
		Slot.AttachWidget(Child);
		if (SConstraintCanvas::FSlot* Live = Slot.GetSlot())
		{
			ApplyConstraintSlot(*Live, SlotProps);
		}
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SConstraintCanvas&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SConstraintCanvas& W = static_cast<SConstraintCanvas&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			W.RemoveSlot(Child);
		}
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SConstraintCanvas::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
			if (SConstraintCanvas::FSlot* Live = Slot.GetSlot())
			{
				ApplyConstraintSlot(*Live, SlotPropsOf(Child));
			}
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuiStyleDict* SlotProps) override
	{
		SConstraintCanvas& W = static_cast<SConstraintCanvas&>(Parent);
		FChildren* Children = W.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				ApplyConstraintSlot(const_cast<SConstraintCanvas::FSlot&>(
										static_cast<const SConstraintCanvas::FSlot&>(Children->GetSlotAt(i))),
									SlotProps);
				W.Invalidate(EInvalidateWidgetReason::Layout);
				return;
			}
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSplitter (P5b) — resizable panes; live per-slot fraction/rule/min/resizable setters.
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	const FName SlotSizeRuleKey(TEXT("slot.sizerule"));
	const FName SlotSizeValueKey(TEXT("slot.sizevalue"));
	const FName SlotMinSizeKey(TEXT("slot.minsize"));
	const FName SlotResizableKey(TEXT("slot.resizable"));

	void ApplySplitterSlot(SSplitter::FSlot& Slot, const FRuiStyleDict* SlotProps)
	{
		const FRuiValue* Rule = SlotProps != nullptr ? SlotProps->Find(SlotSizeRuleKey) : nullptr;
		const FRuiValue* Value = SlotProps != nullptr ? SlotProps->Find(SlotSizeValueKey) : nullptr;
		const FRuiValue* MinSize = SlotProps != nullptr ? SlotProps->Find(SlotMinSizeKey) : nullptr;
		const FRuiValue* Resizable = SlotProps != nullptr ? SlotProps->Find(SlotResizableKey) : nullptr;
		const bool bSizeToContent =
			Rule != nullptr && (Rule->Kind == FRuiValue::EKind::Name ? Rule->NameValue : FName(*Rule->StringValue)) ==
								   FName(TEXT("sizeToContent"));
		Slot.SetSizingRule(bSizeToContent ? SSplitter::SizeToContent : SSplitter::FractionOfParent);
		Slot.SetSizeValue(Value != nullptr ? RUI::Slate::SlotValue::AsFloat(*Value) : 1.0f);
		Slot.SetMinSize(MinSize != nullptr ? RUI::Slate::SlotValue::AsFloat(*MinSize) : 20.0f);
		Slot.SetResizable(Resizable == nullptr || Resizable->BoolValue);
	}
} // namespace

class FRuiSplitterAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return 1ull << FRuiSplitterProps::PhysicalSplitterHandleSize_Bit;
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiSplitterProps& O = static_cast<const FRuiSplitterProps&>(Old);
		const FRuiSplitterProps& N = static_cast<const FRuiSplitterProps&>(New);
		return N.HasPhysicalSplitterHandleSize() &&
			   (!O.HasPhysicalSplitterHandleSize() || !(O.PhysicalSplitterHandleSize == N.PhysicalSplitterHandleSize));
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiSplitterProps& P = static_cast<const FRuiSplitterProps&>(Props);
		return SNew(SSplitter)
			.Orientation(P.HasOrientation() && P.Orientation == FName(TEXT("vertical")) ? Orient_Vertical
																						: Orient_Horizontal)
			.PhysicalSplitterHandleSize(P.HasPhysicalSplitterHandleSize() ? P.PhysicalSplitterHandleSize : 5.0f)
			.OnSplitterFinishedResizing(
				FSimpleDelegate::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleVoid,
										  static_cast<int32>(FRuiSplitterProps::OnSplitterFinishedResizing_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiSplitterProps& N = static_cast<const FRuiSplitterProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiSplitterProps::OnSplitterFinishedResizing_Bit),
						 N.OnSplitterFinishedResizing);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SSplitter& W = static_cast<SSplitter&>(Widget);
		const FRuiSplitterProps& N = static_cast<const FRuiSplitterProps&>(New);
		const FRuiSplitterProps* O = static_cast<const FRuiSplitterProps*>(Old);
		if (N.HasOrientation() && (O == nullptr || !O->HasOrientation() || !(N.Orientation == O->Orientation)))
		{
			W.SetOrientation(N.Orientation == FName(TEXT("vertical")) ? Orient_Vertical : Orient_Horizontal);
		}
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuiStyleDict* SlotProps) override
	{
		SSplitter& W = static_cast<SSplitter&>(Parent);
		SSplitter::FScopedWidgetSlotArguments Slot = W.AddSlot();
		Slot.AttachWidget(Child);
		if (SSplitter::FSlot* Live = Slot.GetSlot())
		{
			ApplySplitterSlot(*Live, SlotProps);
		}
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		SSplitter& W = static_cast<SSplitter&>(Parent);
		FChildren* Children = W.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				W.RemoveAt(i);
				return;
			}
		}
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SSplitter& W = static_cast<SSplitter&>(Parent);
		while (W.GetChildren()->Num() > 0)
		{
			W.RemoveAt(0);
		}
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SSplitter::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
			if (SSplitter::FSlot* Live = Slot.GetSlot())
			{
				ApplySplitterSlot(*Live, SlotPropsOf(Child));
			}
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuiStyleDict* SlotProps) override
	{
		SSplitter& W = static_cast<SSplitter&>(Parent);
		FChildren* Children = W.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				ApplySplitterSlot(
					const_cast<SSplitter::FSlot&>(static_cast<const SSplitter::FSlot&>(Children->GetSlotAt(i))),
					SlotProps);
				W.Invalidate(EInvalidateWidgetReason::Layout);
				return;
			}
		}
	}
};

namespace RUI::Slate
{
	namespace
	{
		FRuiElementTypeId ConstraintCanvasType()
		{
			return RUI::InternElementType(FName(TEXT("ConstraintCanvas")));
		}
		FRuiElementTypeId SplitterType()
		{
			return RUI::InternElementType(FName(TEXT("Splitter")));
		}
	} // namespace

	FRuiNode ConstraintCanvas(FRuiConstraintCanvasProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = ConstraintCanvasType();
		Node.Props = MakeShared<FRuiConstraintCanvasProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuiNode Splitter(FRuiSplitterProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = SplitterType();
		Node.Props = MakeShared<FRuiSplitterProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterBatch3Wave3Adapters()
		{
			RegisterAdapter(ConstraintCanvasType(), MakeUnique<FRuiConstraintCanvasAdapter>());
			RegisterAdapter(SplitterType(), MakeUnique<FRuiSplitterAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
