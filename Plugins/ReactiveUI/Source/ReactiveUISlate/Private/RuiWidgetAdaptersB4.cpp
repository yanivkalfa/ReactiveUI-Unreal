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
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"
#include "Misc/EngineVersionComparison.h"
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
#include "SSearchableComboBox.h"
#include "Widgets/Text/STextBlock.h"
#endif
#include "RuiSlateHost.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Widgets/Layout/SLinkedBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SWindowTitleBarArea.h"
#include "Widgets/SNullWidget.h"

namespace
{
	const FName SlotOffsetKey(TEXT("slot.offset"));
	const FName SlotAnchorsKey(TEXT("slot.anchors"));
	const FName SlotAlignmentKey(TEXT("slot.alignment"));
	const FName SlotAutoSizeKey(TEXT("slot.autosize"));
	const FName SlotZOrderKeyB4(TEXT("slot.zorder"));

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
		const FRuiValue* ZOrder = SlotProps != nullptr ? SlotProps->Find(SlotZOrderKeyB4) : nullptr;
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

// ─────────────────────────────────────────────────────────────────────────────────────────
// SMenuAnchor (P3) — THE popup primitive: anchor child + slot.role="menu" popup content;
// controlled bIsOpen (D-16 skip vs IsOpen()); user dismissals report via OnMenuOpenChanged.
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	const FName SlotRoleKeyB4(TEXT("slot.role"));

	bool IsMenuRole(const FRuiStyleDict* SlotProps)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuiValue* V = SlotProps->Find(SlotRoleKeyB4))
			{
				return (V->Kind == FRuiValue::EKind::Name ? V->NameValue : FName(*V->StringValue)) ==
					   FName(TEXT("menu"));
			}
		}
		return false;
	}

	EMenuPlacement PlacementOf(FName V)
	{
		return V == FName(TEXT("comboBox"))				 ? MenuPlacement_ComboBox
			   : V == FName(TEXT("belowRightAnchor"))	 ? MenuPlacement_BelowRightAnchor
			   : V == FName(TEXT("aboveAnchor"))		 ? MenuPlacement_AboveAnchor
			   : V == FName(TEXT("centeredAboveAnchor")) ? MenuPlacement_CenteredAboveAnchor
			   : V == FName(TEXT("menuRight"))			 ? MenuPlacement_MenuRight
			   : V == FName(TEXT("menuLeft"))			 ? MenuPlacement_MenuLeft
			   : V == FName(TEXT("center"))				 ? MenuPlacement_Center
			   : V == FName(TEXT("centeredBelowAnchor")) ? MenuPlacement_CenteredBelowAnchor
														 : MenuPlacement_BelowAnchor;
	}
} // namespace

class FRuiMenuAnchorAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiMenuAnchorProps& P = static_cast<const FRuiMenuAnchorProps&>(Props);
		return SNew(SMenuAnchor)
			.Placement(P.HasPlacement() ? PlacementOf(P.Placement) : MenuPlacement_BelowAnchor)
			.FitInWindow(!P.HasbFitInWindow() || P.bFitInWindow)
			.OnMenuOpenChanged(
				FOnIsOpenChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleBool,
										   static_cast<int32>(FRuiMenuAnchorProps::OnMenuOpenChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiMenuAnchorProps& N = static_cast<const FRuiMenuAnchorProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiMenuAnchorProps::OnMenuOpenChanged_Bit), N.OnMenuOpenChanged);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SMenuAnchor& W = static_cast<SMenuAnchor&>(Widget);
		const FRuiMenuAnchorProps& N = static_cast<const FRuiMenuAnchorProps&>(New);
		const FRuiMenuAnchorProps* O = static_cast<const FRuiMenuAnchorProps*>(Old);
		if (N.HasPlacement() && (O == nullptr || !O->HasPlacement() || !(N.Placement == O->Placement)))
		{
			W.SetMenuPlacement(PlacementOf(N.Placement));
		}
		if (N.HasbFitInWindow() && (O == nullptr || !O->HasbFitInWindow() || O->bFitInWindow != N.bFitInWindow))
		{
			W.SetFitInWindow(N.bFitInWindow);
		}
		// Controlled open state (D-16): skip when the widget already agrees.
		if (N.HasbIsOpen() && W.IsOpen() != N.bIsOpen)
		{
			W.SetIsOpen(N.bIsOpen, /*bFocusMenu*/ true);
		}
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuiStyleDict* SlotProps) override
	{
		SMenuAnchor& W = static_cast<SMenuAnchor&>(Parent);
		if (IsMenuRole(SlotProps))
		{
			W.SetMenuContent(Child);
		}
		else
		{
			W.SetContent(Child);
		}
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>&) override
	{
		// Two logical holders; clearing either resets to null content (cheap — reassigned on
		// the next InsertChild).
		SMenuAnchor& W = static_cast<SMenuAnchor&>(Parent);
		W.SetMenuContent(SNullWidget::NullWidget);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SMenuAnchor& W = static_cast<SMenuAnchor&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			if (IsMenuRole(SlotPropsOf(Child)))
			{
				W.SetMenuContent(Child);
			}
			else
			{
				W.SetContent(Child);
			}
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuiStyleDict* SlotProps) override
	{
		InsertChild(Parent, Child, 0, SlotProps);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SWindowTitleBarArea — custom title-bar strip; auto-wired to the game window when present.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiWindowTitleBarAreaAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiWindowTitleBarAreaProps& P = static_cast<const FRuiWindowTitleBarAreaProps&>(Props);
		TSharedRef<SWindowTitleBarArea> W =
			SNew(SWindowTitleBarArea)
				.RequestToggleFullscreen(FSimpleDelegate::CreateSP(
					Proxy.ToSharedRef(), &FRuiEventProxy::HandleVoid,
					static_cast<int32>(FRuiWindowTitleBarAreaProps::RequestToggleFullscreen_Bit)));
		if (GEngine != nullptr && GEngine->GameViewport != nullptr)
		{
			W->SetGameWindow(GEngine->GameViewport->GetWindow());
		}
		return W;
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiWindowTitleBarAreaProps& N = static_cast<const FRuiWindowTitleBarAreaProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiWindowTitleBarAreaProps::RequestToggleFullscreen_Bit),
						 N.RequestToggleFullscreen);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SWindowTitleBarArea& W = static_cast<SWindowTitleBarArea&>(Widget);
		const FRuiWindowTitleBarAreaProps& N = static_cast<const FRuiWindowTitleBarAreaProps&>(New);
		const FRuiWindowTitleBarAreaProps* O = static_cast<const FRuiWindowTitleBarAreaProps*>(Old);
		if (N.HasHAlign() && (O == nullptr || !O->HasHAlign() || !(N.HAlign == O->HAlign)))
		{
			W.SetHAlign(N.HAlign == FName(TEXT("left"))		? HAlign_Left
						: N.HAlign == FName(TEXT("center")) ? HAlign_Center
						: N.HAlign == FName(TEXT("right"))	? HAlign_Right
															: HAlign_Fill);
		}
		if (N.HasVAlign() && (O == nullptr || !O->HasVAlign() || !(N.VAlign == O->VAlign)))
		{
			W.SetVAlign(N.VAlign == FName(TEXT("top"))		? VAlign_Top
						: N.VAlign == FName(TEXT("center")) ? VAlign_Center
						: N.VAlign == FName(TEXT("bottom")) ? VAlign_Bottom
															: VAlign_Fill);
		}
		if (N.HasPadding() && (O == nullptr || !O->HasPadding() || !(N.Padding == O->Padding)))
		{
			W.SetPadding(N.Padding);
		}
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SWindowTitleBarArea&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef()
																			 : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SNumericDropDown<float> — preset dropdown; fully attribute/construct-only → masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiNumericDropDownAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiNumericDropDownProps::Values_Bit) | (1ull << FRuiNumericDropDownProps::Labels_Bit) |
			   (1ull << FRuiNumericDropDownProps::Value_Bit) | (1ull << FRuiNumericDropDownProps::bShowNamedValue_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiNumericDropDownProps& O = static_cast<const FRuiNumericDropDownProps&>(Old);
		const FRuiNumericDropDownProps& N = static_cast<const FRuiNumericDropDownProps&>(New);
		auto Changed = [](bool bNewHas, bool bOldHas, const auto& OldV, const auto& NewV)
		{ return bNewHas && (!bOldHas || !(OldV == NewV)); };
		return Changed(N.HasValues(), O.HasValues(), O.Values, N.Values) ||
			   Changed(N.HasLabels(), O.HasLabels(), O.Labels, N.Labels) ||
			   Changed(N.HasValue(), O.HasValue(), O.Value, N.Value) ||
			   Changed(N.HasbShowNamedValue(), O.HasbShowNamedValue(), O.bShowNamedValue, N.bShowNamedValue);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiNumericDropDownProps& P = static_cast<const FRuiNumericDropDownProps&>(Props);
		TArray<SNumericDropDown<float>::FNamedValue> Named;
		for (int32 i = 0; i < P.Values.Num(); ++i)
		{
			const FText Label =
				P.Labels.IsValidIndex(i) ? FText::FromString(P.Labels[i]) : FText::AsNumber(P.Values[i]);
			Named.Add(SNumericDropDown<float>::FNamedValue(P.Values[i], Label, Label));
		}
		return SNew(SNumericDropDown<float>)
			.DropDownValues(Named)
			.Value(P.HasValue() ? P.Value : 0.0f)
			.bShowNamedValue(P.HasbShowNamedValue() && P.bShowNamedValue)
			.OnValueChanged(SNumericDropDown<float>::FOnValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
				static_cast<int32>(FRuiNumericDropDownProps::OnValueChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiNumericDropDownProps& N = static_cast<const FRuiNumericDropDownProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiNumericDropDownProps::OnValueChanged_Bit), N.OnValueChanged);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {} // all masked
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SBreadcrumbTrail<FString> — declarative Crumbs list converged onto the imperative stack.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiBreadcrumbTrailAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return 1ull << FRuiBreadcrumbTrailProps::bShowLeadingDelimiter_Bit;
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiBreadcrumbTrailProps& O = static_cast<const FRuiBreadcrumbTrailProps&>(Old);
		const FRuiBreadcrumbTrailProps& N = static_cast<const FRuiBreadcrumbTrailProps&>(New);
		return N.HasbShowLeadingDelimiter() &&
			   (!O.HasbShowLeadingDelimiter() || O.bShowLeadingDelimiter != N.bShowLeadingDelimiter);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiBreadcrumbTrailProps& P = static_cast<const FRuiBreadcrumbTrailProps&>(Props);
		TWeakPtr<FRuiEventProxy> WeakProxy = Proxy;
		TSharedRef<SBreadcrumbTrail<FString>> W =
			SNew(SBreadcrumbTrail<FString>)
				.ShowLeadingDelimiter(P.HasbShowLeadingDelimiter() && P.bShowLeadingDelimiter)
				.PersistentBreadcrumbs(true)
				.OnCrumbClicked(SBreadcrumbTrail<FString>::FOnCrumbClicked::CreateLambda(
					[WeakProxy](const FString& Crumb)
					{
						if (TSharedPtr<FRuiEventProxy> Pinned = WeakProxy.Pin())
						{
							Pinned->HandleText(FText::FromString(Crumb),
											   static_cast<int32>(FRuiBreadcrumbTrailProps::OnCrumbClicked_Bit));
						}
					}));
		SyncCrumbs(*W, P);
		return W;
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiBreadcrumbTrailProps& N = static_cast<const FRuiBreadcrumbTrailProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiBreadcrumbTrailProps::OnCrumbClicked_Bit), N.OnCrumbClicked);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		const FRuiBreadcrumbTrailProps& N = static_cast<const FRuiBreadcrumbTrailProps&>(New);
		const FRuiBreadcrumbTrailProps* O = static_cast<const FRuiBreadcrumbTrailProps*>(Old);
		if (N.HasCrumbs() && (O == nullptr || !O->HasCrumbs() || !(N.Crumbs == O->Crumbs)))
		{
			SyncCrumbs(static_cast<SBreadcrumbTrail<FString>&>(Widget), N);
		}
	}

private:
	static void SyncCrumbs(SBreadcrumbTrail<FString>& W, const FRuiBreadcrumbTrailProps& P)
	{
		W.ClearCrumbs();
		for (const FString& Crumb : P.Crumbs)
		{
			W.PushCrumb(FText::FromString(Crumb), Crumb);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SNotificationList (P4) — toast mount point; pushes ride the P2 command path.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiNotificationListAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SNotificationList);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}
};

namespace RUI::Slate
{
	void PushNotification(const FRuiHostHandle& Handle, const FText& Text, float ExpireDuration)
	{
		if (TSharedPtr<SNotificationList> List = WidgetFromHandle<SNotificationList>(Handle))
		{
			if (List->GetType() == FName(TEXT("SNotificationList"))) // guard the cast (P2 rule)
			{
				FNotificationInfo Info(Text);
				Info.ExpireDuration = ExpireDuration;
				List->AddNotification(Info);
			}
		}
	}
} // namespace RUI::Slate

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSearchableComboBox — sinceUE 5.7 (absent from 5.6): the adapter compiles out on older
// engines; the tag/props/factory stay (mounting on 5.6 warns unknown-adapter).
// ─────────────────────────────────────────────────────────────────────────────────────────

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
/** Owns the TSharedPtr<FString> options storage SSearchableComboBox borrows a pointer to. */
class SRuiSearchableComboBox final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuiSearchableComboBox) {}
	SLATE_ARGUMENT(TArray<FString>, Options)
	SLATE_ARGUMENT(FString, InitiallySelected)
	SLATE_EVENT(SSearchableComboBox::FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		for (const FString& Option : InArgs._Options)
		{
			Options.Add(MakeShared<FString>(Option));
		}
		// clang-format off
		ChildSlot
		[
			SAssignNew(Inner, SSearchableComboBox)
			.OptionsSource(&Options)
			.OnSelectionChanged(InArgs._OnSelectionChanged)
			.OnGenerateWidget(SSearchableComboBox::FOnGenerateWidget::CreateLambda(
				[](TSharedPtr<FString> Item)
				{ return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())); }))
			.Content()
			[
				SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const TSharedPtr<FString> Sel =
							Inner.IsValid() ? StaticCastSharedPtr<FString>(Inner->GetSelectedItem()) : nullptr;
						return FText::FromString(Sel.IsValid() ? *Sel : FString());
					})
			]
		];
		// clang-format on
		SetSelected(InArgs._InitiallySelected);
	}

	void SetOptions(const TArray<FString>& InOptions)
	{
		Options.Reset();
		for (const FString& Option : InOptions)
		{
			Options.Add(MakeShared<FString>(Option));
		}
		Inner->RefreshOptions();
	}

	void SetSelected(const FString& Value)
	{
		for (const TSharedPtr<FString>& Option : Options)
		{
			if (*Option == Value)
			{
				Inner->SetSelectedItem(Option);
				return;
			}
		}
		Inner->ClearSelection();
	}

	FString GetSelected() const
	{
		const TSharedPtr<FString> Sel = StaticCastSharedPtr<FString>(Inner->GetSelectedItem());
		return Sel.IsValid() ? *Sel : FString();
	}

private:
	TArray<TSharedPtr<FString>> Options;
	TSharedPtr<SSearchableComboBox> Inner;
};

class FRuiSearchableComboBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiSearchableComboBoxProps& P = static_cast<const FRuiSearchableComboBoxProps&>(Props);
		TWeakPtr<FRuiEventProxy> WeakProxy = Proxy;
		return SNew(SRuiSearchableComboBox)
			.Options(P.Options)
			.InitiallySelected(P.SelectedItem.ToString())
			.OnSelectionChanged(SSearchableComboBox::FOnSelectionChanged::CreateLambda(
				[WeakProxy](TSharedPtr<FString> Item, ESelectInfo::Type)
				{
					if (TSharedPtr<FRuiEventProxy> Pinned = WeakProxy.Pin())
					{
						Pinned->HandleText(FText::FromString(Item.IsValid() ? *Item : FString()),
										   static_cast<int32>(FRuiSearchableComboBoxProps::OnSelectionChanged_Bit));
					}
				}));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiSearchableComboBoxProps& N = static_cast<const FRuiSearchableComboBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiSearchableComboBoxProps::OnSelectionChanged_Bit), N.OnSelectionChanged);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiSearchableComboBox& W = static_cast<SRuiSearchableComboBox&>(Widget);
		const FRuiSearchableComboBoxProps& N = static_cast<const FRuiSearchableComboBoxProps&>(New);
		const FRuiSearchableComboBoxProps* O = static_cast<const FRuiSearchableComboBoxProps*>(Old);
		if (N.HasOptions() && (O == nullptr || !O->HasOptions() || !(N.Options == O->Options)))
		{
			W.SetOptions(N.Options);
		}
		// Controlled selection (D-16): skip when the widget already agrees.
		if (N.HasSelectedItem() && W.GetSelected() != N.SelectedItem.ToString())
		{
			W.SetSelected(N.SelectedItem.ToString());
		}
	}
};
#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

// ─────────────────────────────────────────────────────────────────────────────────────────
// SLinkedBox — uniform-size sibling groups via a shared, adapter-owned FLinkedBoxManager.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiLinkedBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual uint64 GetReconstructMask() const override { return 1ull << FRuiLinkedBoxProps::GroupKey_Bit; }

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiLinkedBoxProps& O = static_cast<const FRuiLinkedBoxProps&>(Old);
		const FRuiLinkedBoxProps& N = static_cast<const FRuiLinkedBoxProps&>(New);
		return N.HasGroupKey() && (!O.HasGroupKey() || !(O.GroupKey == N.GroupKey));
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiLinkedBoxProps& P = static_cast<const FRuiLinkedBoxProps&>(Props);
		const FName Group = P.HasGroupKey() ? P.GroupKey : NAME_None;
		TSharedPtr<FLinkedBoxManager> Manager = Managers.FindRef(Group).Pin();
		if (!Manager.IsValid())
		{
			Manager = MakeShared<FLinkedBoxManager>();
			Managers.Add(Group, Manager);
			// Opportunistic prune (groups are few; keeps dead weaks from accumulating).
			for (auto It = Managers.CreateIterator(); It; ++It)
			{
				if (!It->Value.IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
		TSharedRef<SLinkedBox> W = SNew(SLinkedBox, Manager.ToSharedRef());
		LiveManagers.Add(&W.Get(), Manager); // keep the manager alive as long as any member widget
		return W;
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}

private:
	TMap<FName, TWeakPtr<FLinkedBoxManager>> Managers;
	TMap<const SWidget*, TSharedPtr<FLinkedBoxManager>> LiveManagers; // strong refs per live widget
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SVirtualJoystick — touch overlay mount; configured imperatively via P2 (FControlInfo).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiVirtualJoystickAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SVirtualJoystick);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SVectorInputBox / SRotatorInputBox (wave 4) — templated float rows; fully masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiVectorInputBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiVectorInputBoxProps::X_Bit) | (1ull << FRuiVectorInputBoxProps::Y_Bit) |
			   (1ull << FRuiVectorInputBoxProps::Z_Bit) | (1ull << FRuiVectorInputBoxProps::bColorAxisLabels_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiVectorInputBoxProps& O = static_cast<const FRuiVectorInputBoxProps&>(Old);
		const FRuiVectorInputBoxProps& N = static_cast<const FRuiVectorInputBoxProps&>(New);
		auto Changed = [](bool bNewHas, bool bOldHas, const auto& OldV, const auto& NewV)
		{ return bNewHas && (!bOldHas || !(OldV == NewV)); };
		return Changed(N.HasX(), O.HasX(), O.X, N.X) || Changed(N.HasY(), O.HasY(), O.Y, N.Y) ||
			   Changed(N.HasZ(), O.HasZ(), O.Z, N.Z) ||
			   Changed(N.HasbColorAxisLabels(), O.HasbColorAxisLabels(), O.bColorAxisLabels, N.bColorAxisLabels);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiVectorInputBoxProps& P = static_cast<const FRuiVectorInputBoxProps&>(Props);
		return SNew(SVectorInputBox)
			.X(P.HasX() ? TOptional<float>(P.X) : TOptional<float>())
			.Y(P.HasY() ? TOptional<float>(P.Y) : TOptional<float>())
			.Z(P.HasZ() ? TOptional<float>(P.Z) : TOptional<float>())
			.bColorAxisLabels(P.HasbColorAxisLabels() && P.bColorAxisLabels)
			.OnXChanged(SVectorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
				static_cast<int32>(FRuiVectorInputBoxProps::OnXChanged_Bit)))
			.OnYChanged(SVectorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
				static_cast<int32>(FRuiVectorInputBoxProps::OnYChanged_Bit)))
			.OnZChanged(SVectorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
				static_cast<int32>(FRuiVectorInputBoxProps::OnZChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiVectorInputBoxProps& N = static_cast<const FRuiVectorInputBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiVectorInputBoxProps::OnXChanged_Bit), N.OnXChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiVectorInputBoxProps::OnYChanged_Bit), N.OnYChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiVectorInputBoxProps::OnZChanged_Bit), N.OnZChanged);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {} // all masked
};

class FRuiRotatorInputBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiRotatorInputBoxProps::Roll_Bit) | (1ull << FRuiRotatorInputBoxProps::Pitch_Bit) |
			   (1ull << FRuiRotatorInputBoxProps::Yaw_Bit) | (1ull << FRuiRotatorInputBoxProps::bColorAxisLabels_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiRotatorInputBoxProps& O = static_cast<const FRuiRotatorInputBoxProps&>(Old);
		const FRuiRotatorInputBoxProps& N = static_cast<const FRuiRotatorInputBoxProps&>(New);
		auto Changed = [](bool bNewHas, bool bOldHas, const auto& OldV, const auto& NewV)
		{ return bNewHas && (!bOldHas || !(OldV == NewV)); };
		return Changed(N.HasRoll(), O.HasRoll(), O.Roll, N.Roll) ||
			   Changed(N.HasPitch(), O.HasPitch(), O.Pitch, N.Pitch) || Changed(N.HasYaw(), O.HasYaw(), O.Yaw, N.Yaw) ||
			   Changed(N.HasbColorAxisLabels(), O.HasbColorAxisLabels(), O.bColorAxisLabels, N.bColorAxisLabels);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiRotatorInputBoxProps& P = static_cast<const FRuiRotatorInputBoxProps&>(Props);
		return SNew(SRotatorInputBox)
			.Roll(P.HasRoll() ? TOptional<float>(P.Roll) : TOptional<float>())
			.Pitch(P.HasPitch() ? TOptional<float>(P.Pitch) : TOptional<float>())
			.Yaw(P.HasYaw() ? TOptional<float>(P.Yaw) : TOptional<float>())
			.bColorAxisLabels(P.HasbColorAxisLabels() && P.bColorAxisLabels)
			.OnRollChanged(SRotatorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
				static_cast<int32>(FRuiRotatorInputBoxProps::OnRollChanged_Bit)))
			.OnPitchChanged(SRotatorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
				static_cast<int32>(FRuiRotatorInputBoxProps::OnPitchChanged_Bit)))
			.OnYawChanged(SRotatorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
				static_cast<int32>(FRuiRotatorInputBoxProps::OnYawChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiRotatorInputBoxProps& N = static_cast<const FRuiRotatorInputBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiRotatorInputBoxProps::OnRollChanged_Bit), N.OnRollChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiRotatorInputBoxProps::OnPitchChanged_Bit), N.OnPitchChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiRotatorInputBoxProps::OnYawChanged_Bit), N.OnYawChanged);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {} // all masked
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
		FRuiElementTypeId MenuAnchorType()
		{
			return RUI::InternElementType(FName(TEXT("MenuAnchor")));
		}
		FRuiElementTypeId WindowTitleBarAreaType()
		{
			return RUI::InternElementType(FName(TEXT("WindowTitleBarArea")));
		}
		FRuiElementTypeId NumericDropDownType()
		{
			return RUI::InternElementType(FName(TEXT("NumericDropDown")));
		}
		FRuiElementTypeId BreadcrumbTrailType()
		{
			return RUI::InternElementType(FName(TEXT("BreadcrumbTrail")));
		}
		FRuiElementTypeId NotificationListType()
		{
			return RUI::InternElementType(FName(TEXT("NotificationList")));
		}
		FRuiElementTypeId SearchableComboBoxType()
		{
			return RUI::InternElementType(FName(TEXT("SearchableComboBox")));
		}
		FRuiElementTypeId LinkedBoxType()
		{
			return RUI::InternElementType(FName(TEXT("LinkedBox")));
		}
		FRuiElementTypeId VirtualJoystickType()
		{
			return RUI::InternElementType(FName(TEXT("VirtualJoystick")));
		}
		FRuiElementTypeId VectorInputBoxType()
		{
			return RUI::InternElementType(FName(TEXT("VectorInputBox")));
		}
		FRuiElementTypeId RotatorInputBoxType()
		{
			return RUI::InternElementType(FName(TEXT("RotatorInputBox")));
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

	FRuiNode MenuAnchor(FRuiMenuAnchorProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = MenuAnchorType();
		Node.Props = MakeShared<FRuiMenuAnchorProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuiNode WindowTitleBarArea(FRuiWindowTitleBarAreaProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = WindowTitleBarAreaType();
		Node.Props = MakeShared<FRuiWindowTitleBarAreaProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuiNode NumericDropDown(FRuiNumericDropDownProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = NumericDropDownType();
		Node.Props = MakeShared<FRuiNumericDropDownProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(TArray<FRuiNode>());
		Node.Key = Key;
		return Node;
	}

	FRuiNode BreadcrumbTrail(FRuiBreadcrumbTrailProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = BreadcrumbTrailType();
		Node.Props = MakeShared<FRuiBreadcrumbTrailProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(TArray<FRuiNode>());
		Node.Key = Key;
		return Node;
	}

	FRuiNode NotificationList(FRuiNotificationListProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = NotificationListType();
		Node.Props = MakeShared<FRuiNotificationListProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(TArray<FRuiNode>());
		Node.Key = Key;
		return Node;
	}

	FRuiNode SearchableComboBox(FRuiSearchableComboBoxProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = SearchableComboBoxType();
		Node.Props = MakeShared<FRuiSearchableComboBoxProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(TArray<FRuiNode>());
		Node.Key = Key;
		return Node;
	}

	FRuiNode LinkedBox(FRuiLinkedBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = LinkedBoxType();
		Node.Props = MakeShared<FRuiLinkedBoxProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuiNode VirtualJoystick(FRuiVirtualJoystickProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = VirtualJoystickType();
		Node.Props = MakeShared<FRuiVirtualJoystickProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(TArray<FRuiNode>());
		Node.Key = Key;
		return Node;
	}

	FRuiNode VectorInputBox(FRuiVectorInputBoxProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = VectorInputBoxType();
		Node.Props = MakeShared<FRuiVectorInputBoxProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(TArray<FRuiNode>());
		Node.Key = Key;
		return Node;
	}

	FRuiNode RotatorInputBox(FRuiRotatorInputBoxProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = RotatorInputBoxType();
		Node.Props = MakeShared<FRuiRotatorInputBoxProps>(MoveTemp(Props));
		Node.Children = RUI::MakeChildren(TArray<FRuiNode>());
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterBatch3Wave3Adapters()
		{
			RegisterAdapter(ConstraintCanvasType(), MakeUnique<FRuiConstraintCanvasAdapter>());
			RegisterAdapter(SplitterType(), MakeUnique<FRuiSplitterAdapter>());
			RegisterAdapter(MenuAnchorType(), MakeUnique<FRuiMenuAnchorAdapter>());
			RegisterAdapter(WindowTitleBarAreaType(), MakeUnique<FRuiWindowTitleBarAreaAdapter>());
			RegisterAdapter(NumericDropDownType(), MakeUnique<FRuiNumericDropDownAdapter>());
			RegisterAdapter(BreadcrumbTrailType(), MakeUnique<FRuiBreadcrumbTrailAdapter>());
			RegisterAdapter(NotificationListType(), MakeUnique<FRuiNotificationListAdapter>());
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
			RegisterAdapter(SearchableComboBoxType(), MakeUnique<FRuiSearchableComboBoxAdapter>());
#endif
			RegisterAdapter(LinkedBoxType(), MakeUnique<FRuiLinkedBoxAdapter>());
			RegisterAdapter(VirtualJoystickType(), MakeUnique<FRuiVirtualJoystickAdapter>());
			RegisterAdapter(VectorInputBoxType(), MakeUnique<FRuiVectorInputBoxAdapter>());
			RegisterAdapter(RotatorInputBoxType(), MakeUnique<FRuiRotatorInputBoxAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
