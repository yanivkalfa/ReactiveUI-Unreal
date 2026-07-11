// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The hand-written pattern adapters (Phase 2 step 3, first batch): STextBlock (the text
// contract), SVerticalBox/SHorizontalBox (MultiSlot panels + slot.* props), SButton
// (SingleContent + the event-proxy pattern), SOverlay (MultiSlot; the SRuiRoot panel).
// Every later widget copies these shapes via templates/widget_wrapper.template.cpp.
//
// slot.* keys (v1; parsed here — the host is key-agnostic):
//   slot.padding  Float (uniform) | Vector2 (h, v) | String "l,t,r,b"
//   slot.halign   Name/String: fill|left|center|right
//   slot.valign   Name/String: fill|top|center|bottom
//   slot.fill     Float (box panels: FillHeight/FillWidth; 0/absent = AutoHeight/AutoWidth)
//   slot.zorder   Int (overlay only; defaults keep declaration order)

#include "RuiElementAdapter.h"
#include "RuiEventProxy.h"
#include "RuiSlateElements.h"

#include "RuiSlateLog.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// ─────────────────────────────────────────────────────────────────────────────────────────
// slot.* parsing
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	FMargin ParsePadding(const FRuiValue& V)
	{
		switch (V.Kind)
		{
		case FRuiValue::EKind::Int:
			return FMargin(static_cast<float>(V.IntValue));
		case FRuiValue::EKind::Float:
			return FMargin(static_cast<float>(V.FloatValue));
		case FRuiValue::EKind::Vector2:
			return FMargin(static_cast<float>(V.Vector2Value.X), static_cast<float>(V.Vector2Value.Y));
		case FRuiValue::EKind::String:
		case FRuiValue::EKind::Name:
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
			if (Parts.Num() == 1)
			{
				return FMargin(FCString::Atof(*Parts[0]));
			}
			break;
		}
		default:
			break;
		}
		UE_LOG(LogRuiSlate, Warning, TEXT("[ReactiveUI] slot.padding: unsupported value shape — using 0"));
		return FMargin(0.0f);
	}

	FString ValueAsLowerString(const FRuiValue& V)
	{
		if (V.Kind == FRuiValue::EKind::Name)
		{
			return V.NameValue.ToString().ToLower();
		}
		if (V.Kind == FRuiValue::EKind::String)
		{
			return V.StringValue.ToLower();
		}
		return FString();
	}

	EHorizontalAlignment ParseHAlign(const FRuiValue& V)
	{
		const FString S = ValueAsLowerString(V);
		if (S == TEXT("left"))
		{
			return HAlign_Left;
		}
		if (S == TEXT("center"))
		{
			return HAlign_Center;
		}
		if (S == TEXT("right"))
		{
			return HAlign_Right;
		}
		return HAlign_Fill;
	}

	EVerticalAlignment ParseVAlign(const FRuiValue& V)
	{
		const FString S = ValueAsLowerString(V);
		if (S == TEXT("top"))
		{
			return VAlign_Top;
		}
		if (S == TEXT("center"))
		{
			return VAlign_Center;
		}
		if (S == TEXT("bottom"))
		{
			return VAlign_Bottom;
		}
		return VAlign_Fill;
	}

	const FName SlotPaddingKey(TEXT("slot.padding"));
	const FName SlotHAlignKey(TEXT("slot.halign"));
	const FName SlotVAlignKey(TEXT("slot.valign"));
	const FName SlotFillKey(TEXT("slot.fill"));
	const FName SlotZOrderKey(TEXT("slot.zorder"));

	float SlotFillOf(const FRuiStyleDict* SlotProps)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuiValue* Fill = SlotProps->Find(SlotFillKey))
			{
				return Fill->Kind == FRuiValue::EKind::Int ? static_cast<float>(Fill->IntValue)
														   : static_cast<float>(Fill->FloatValue);
			}
		}
		return 0.0f; // absent -> Auto size
	}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────────────────
// Text (STextBlock) — renders core FRuiTextBlockProps (the GetTextElementType contract)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiTextAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(STextBlock);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		STextBlock& W = static_cast<STextBlock&>(Widget);
		const FRuiTextBlockProps& N = static_cast<const FRuiTextBlockProps&>(New);
		const FRuiTextBlockProps* O = static_cast<const FRuiTextBlockProps*>(Old);
		if (N.HasText())
		{
			const bool bSame = O != nullptr && O->HasText() &&
							   (N.Text.IdenticalTo(O->Text) || N.Text.ToString() == O->Text.ToString());
			if (!bSame)
			{
				W.SetText(N.Text);
			}
		}
	}

	// Widget-specific style keys (D-13): color + fontSize map to setters; null = reset.
	virtual bool ApplyStyleKey(SWidget& Widget, FName Key, const FRuiValue* Value) override
	{
		STextBlock& W = static_cast<STextBlock&>(Widget);
		if (Key == FName(TEXT("ColorAndOpacity")))
		{
			W.SetColorAndOpacity(Value != nullptr ? FSlateColor(Value->ColorValue) : FSlateColor::UseForeground());
			return true;
		}
		if (Key == FName(TEXT("Font.Size")))
		{
			const int32 Size = Value != nullptr
								   ? (Value->Kind == FRuiValue::EKind::Int ? static_cast<int32>(Value->IntValue)
																		   : static_cast<int32>(Value->FloatValue))
								   : 9; // reset: the default core-style size
			W.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
			return true;
		}
		if (Key == FName(TEXT("Justification")))
		{
			ETextJustify::Type Justify = ETextJustify::Left;
			if (Value != nullptr)
			{
				const FName Name =
					Value->Kind == FRuiValue::EKind::Name ? Value->NameValue : FName(*Value->StringValue);
				Justify = Name == FName(TEXT("center"))	 ? ETextJustify::Center
						  : Name == FName(TEXT("right")) ? ETextJustify::Right
														 : ETextJustify::Left;
			}
			W.SetJustification(Justify);
			return true;
		}
		if (Key == FName(TEXT("AutoWrapText")))
		{
			W.SetAutoWrapText(Value != nullptr && Value->BoolValue);
			return true;
		}
		return false;
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Box panels (SVerticalBox / SHorizontalBox) — MultiSlot
// ─────────────────────────────────────────────────────────────────────────────────────────

/** Shared mechanics for the two box panels (their slot APIs are twins). CreateWidget lives
 *  on the concrete subclasses: SNew stringizes its type token for the widget's debug type
 *  name, so `SNew(TBox)` would label every panel "TBox" in the Widget Reflector. */
template <typename TBox> class TRuiBoxPanelAdapter : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override
	{
		// Panels have no own props in v1 — layout is all slot.* on the children.
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32 Index,
							 const FRuiStyleDict* SlotProps) override
	{
		TBox& Box = static_cast<TBox&>(Parent);
		typename TBox::FScopedWidgetSlotArguments Slot = Box.InsertSlot(Index);
		Slot.AttachWidget(Child);
		ConfigureSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<TBox&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		// Minimal-move enforce-order (the family's move_child walk): only out-of-place
		// widgets detach + reinsert. The Bench.SlateReorder spike pits this against a full
		// rebuild — decision recorded in plans/TECH_DEBT.md.
		TBox& Box = static_cast<TBox&>(Parent);
		FChildren* Children = Box.GetChildren();
		for (int32 i = 0; i < Ordered.Num(); ++i)
		{
			if (i < Children->Num() && &Children->GetChildAt(i).Get() == &Ordered[i].Get())
			{
				continue;
			}
			Box.RemoveSlot(Ordered[i]);
			typename TBox::FScopedWidgetSlotArguments Slot = Box.InsertSlot(i);
			Slot.AttachWidget(Ordered[i]);
			ConfigureSlot(Slot, SlotPropsOf(Ordered[i]));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuiStyleDict* SlotProps) override
	{
		// TD-010(a): mutate the LIVE FSlot in place — no detach/reinsert churn on hot slot-prop
		// animation paths (e.g. an animated Slot.Padding). GetSlotAt returns a const ref; the slot
		// is engine-mutable, so we cast to the concrete box FSlot and drive its runtime setters.
		TBox& Box = static_cast<TBox&>(Parent);
		FChildren* Children = Box.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				typename TBox::FSlot& Slot =
					const_cast<typename TBox::FSlot&>(static_cast<const typename TBox::FSlot&>(Children->GetSlotAt(i)));
				ConfigureSlotLive(Slot, SlotProps);
				Box.Invalidate(EInvalidateWidgetReason::Layout);
				return;
			}
		}
	}

private:
	static void ConfigureSlot(typename TBox::FScopedWidgetSlotArguments& Slot, const FRuiStyleDict* SlotProps)
	{
		// Slate box slots DEFAULT TO FILL (why engine code writes .AutoHeight() everywhere).
		// The family default is content-hugging AUTO — fill only when slot.fill asks. Without
		// this, every row squeezes proportionally: clipped titles, crushed inputs, invisible
		// button labels (the owner's first-playtest report).
		const float Fill = SlotFillOf(SlotProps);
		if (Fill > 0.0f)
		{
			SetFill(Slot, Fill);
		}
		else
		{
			SetAuto(Slot);
		}
		if (SlotProps == nullptr)
		{
			return;
		}
		if (const FRuiValue* Padding = SlotProps->Find(SlotPaddingKey))
		{
			Slot.Padding(ParsePadding(*Padding));
		}
		if (const FRuiValue* H = SlotProps->Find(SlotHAlignKey))
		{
			Slot.HAlign(ParseHAlign(*H));
		}
		if (const FRuiValue* V = SlotProps->Find(SlotVAlignKey))
		{
			Slot.VAlign(ParseVAlign(*V));
		}
	}

	static void SetFill(SVerticalBox::FScopedWidgetSlotArguments& Slot, float Fill) { Slot.FillHeight(Fill); }
	static void SetFill(SHorizontalBox::FScopedWidgetSlotArguments& Slot, float Fill) { Slot.FillWidth(Fill); }
	static void SetAuto(SVerticalBox::FScopedWidgetSlotArguments& Slot) { Slot.AutoHeight(); }
	static void SetAuto(SHorizontalBox::FScopedWidgetSlotArguments& Slot) { Slot.AutoWidth(); }

	// Live-FSlot counterparts (TD-010(a) in-place update).
	static void SetFillLive(SVerticalBox::FSlot& Slot, float Fill) { Slot.SetFillHeight(Fill); }
	static void SetFillLive(SHorizontalBox::FSlot& Slot, float Fill) { Slot.SetFillWidth(Fill); }
	static void SetAutoLive(SVerticalBox::FSlot& Slot) { Slot.SetAutoHeight(); }
	static void SetAutoLive(SHorizontalBox::FSlot& Slot) { Slot.SetAutoWidth(); }

	/** Apply the full slot config to a LIVE FSlot. Absent keys RESET to the box slot defaults
	 *  (padding 0, HAlign/VAlign Fill) so an update mirrors the fresh-reinsert result exactly. */
	static void ConfigureSlotLive(typename TBox::FSlot& Slot, const FRuiStyleDict* SlotProps)
	{
		const float Fill = SlotFillOf(SlotProps);
		if (Fill > 0.0f)
		{
			SetFillLive(Slot, Fill);
		}
		else
		{
			SetAutoLive(Slot);
		}
		const FRuiValue* Padding = SlotProps ? SlotProps->Find(SlotPaddingKey) : nullptr;
		Slot.SetPadding(Padding ? ParsePadding(*Padding) : FMargin(0.0f));
		const FRuiValue* H = SlotProps ? SlotProps->Find(SlotHAlignKey) : nullptr;
		Slot.SetHorizontalAlignment(H ? ParseHAlign(*H) : HAlign_Fill);
		const FRuiValue* V = SlotProps ? SlotProps->Find(SlotVAlignKey) : nullptr;
		Slot.SetVerticalAlignment(V ? ParseVAlign(*V) : VAlign_Fill);
	}
};

class FRuiVerticalBoxAdapter final : public TRuiBoxPanelAdapter<SVerticalBox>
{
public:
	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SVerticalBox);
	}
};

class FRuiHorizontalBoxAdapter final : public TRuiBoxPanelAdapter<SHorizontalBox>
{
public:
	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SHorizontalBox);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Button (SButton) — SingleContent + the event-proxy pattern
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiButtonAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		TSharedRef<SButton> W = SNew(SButton);
		W->SetOnClicked(FOnClicked::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleReply,
											 static_cast<int32>(FRuiButtonProps::OnClicked_Bit)));
		return W;
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SButton& W = static_cast<SButton&>(Widget);
		const FRuiButtonProps& N = static_cast<const FRuiButtonProps&>(New);
		const FRuiButtonProps* O = static_cast<const FRuiButtonProps*>(Old);
		if (N.HasbEnabled() && (O == nullptr || !O->HasbEnabled() || O->bEnabled != N.bEnabled))
		{
			W.SetEnabled(N.bEnabled);
		}
		if (N.HasContentPadding() &&
			(O == nullptr || !O->HasContentPadding() || !(N.ContentPadding == O->ContentPadding)))
		{
			W.SetContentPadding(N.ContentPadding);
		}
	}

	virtual bool HasEvents() const override { return true; }

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiButtonProps& N = static_cast<const FRuiButtonProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiButtonProps::OnClicked_Bit), N.OnClicked);
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SButton&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Overlay (SOverlay) — MultiSlot (also the SRuiRoot inner panel)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiOverlayAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SOverlay);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32 Index,
							 const FRuiStyleDict* SlotProps) override
	{
		SOverlay& Overlay = static_cast<SOverlay&>(Parent);
		(void)Index; // overlay stacking order == slot order; reorder enforces it
		SOverlay::FScopedWidgetSlotArguments Slot = Overlay.AddSlot(ZOrderOf(SlotProps));
		Slot.AttachWidget(Child);
		ConfigureSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SOverlay&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		// AddSlot's parameter is a Z-ORDER, not an index — a minimal-move walk can't target
		// positions. Overlays stack a handful of layers, so rebuild: declaration order wins,
		// explicit slot.zorder re-applies on the way back in.
		SOverlay& Overlay = static_cast<SOverlay&>(Parent);
		Overlay.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			const FRuiStyleDict* SlotProps = SlotPropsOf(Child);
			SOverlay::FScopedWidgetSlotArguments Slot = Overlay.AddSlot(ZOrderOf(SlotProps));
			Slot.AttachWidget(Child);
			ConfigureSlot(Slot, SlotProps);
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuiStyleDict* SlotProps) override
	{
		SOverlay& Overlay = static_cast<SOverlay&>(Parent);
		if (Overlay.RemoveSlot(Child))
		{
			SOverlay::FScopedWidgetSlotArguments Slot = Overlay.AddSlot(ZOrderOf(SlotProps));
			Slot.AttachWidget(Child);
			ConfigureSlot(Slot, SlotProps);
		}
	}

private:
	static int32 ZOrderOf(const FRuiStyleDict* SlotProps)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuiValue* Z = SlotProps->Find(SlotZOrderKey))
			{
				return static_cast<int32>(Z->IntValue);
			}
		}
		return INDEX_NONE;
	}

	static void ConfigureSlot(SOverlay::FScopedWidgetSlotArguments& Slot, const FRuiStyleDict* SlotProps)
	{
		if (SlotProps == nullptr)
		{
			return;
		}
		if (const FRuiValue* Padding = SlotProps->Find(SlotPaddingKey))
		{
			Slot.Padding(ParsePadding(*Padding));
		}
		if (const FRuiValue* H = SlotProps->Find(SlotHAlignKey))
		{
			Slot.HAlign(ParseHAlign(*H));
		}
		if (const FRuiValue* V = SlotProps->Find(SlotVAlignKey))
		{
			Slot.VAlign(ParseVAlign(*V));
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Element factories + registration
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace RUI::Slate
{
	FRuiElementTypeId VerticalBoxType()
	{
		static FRuiElementTypeId Id = RUI::InternElementType(FName(TEXT("VerticalBox")));
		return Id;
	}
	FRuiElementTypeId HorizontalBoxType()
	{
		static FRuiElementTypeId Id = RUI::InternElementType(FName(TEXT("HorizontalBox")));
		return Id;
	}
	FRuiElementTypeId ButtonType()
	{
		static FRuiElementTypeId Id = RUI::InternElementType(FName(TEXT("Button")));
		return Id;
	}
	FRuiElementTypeId OverlayType()
	{
		static FRuiElementTypeId Id = RUI::InternElementType(FName(TEXT("Overlay")));
		return Id;
	}

	namespace
	{
		template <typename TProps>
		FRuiNode MakeHostNode(FRuiElementTypeId Type, TProps Props, TArray<FRuiNode> Children, FRuiKey Key)
		{
			FRuiNode Node;
			Node.Kind = ERuiNodeKind::Host;
			Node.ElementType = Type;
			Node.Props = MakeShared<TProps>(MoveTemp(Props));
			Node.Children = RUI::MakeChildren(MoveTemp(Children));
			Node.Key = Key;
			return Node;
		}
	} // namespace

	FRuiNode VerticalBox(FRuiVerticalBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNode(VerticalBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode HorizontalBox(FRuiHorizontalBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNode(HorizontalBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode Button(FRuiButtonProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNode(ButtonType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode Overlay(FRuiOverlayProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNode(OverlayType(), MoveTemp(Props), MoveTemp(Children), Key);
	}

	namespace Detail
	{
		void RegisterBatch2Adapters(); // RuiWidgetAdapters.cpp
	}

	void RegisterBuiltinAdapters()
	{
		RegisterAdapter(RUI::TextBlockElementType(), MakeUnique<FRuiTextAdapter>());
		RegisterAdapter(VerticalBoxType(), MakeUnique<FRuiVerticalBoxAdapter>());
		RegisterAdapter(HorizontalBoxType(), MakeUnique<FRuiHorizontalBoxAdapter>());
		RegisterAdapter(ButtonType(), MakeUnique<FRuiButtonAdapter>());
		RegisterAdapter(OverlayType(), MakeUnique<FRuiOverlayAdapter>());
		Detail::RegisterBatch2Adapters();
	}
} // namespace RUI::Slate
