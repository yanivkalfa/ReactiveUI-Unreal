// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Batch 3 wave 1 (WIDGET_COMPLETION_PLAN §3) — the mechanical leaves, on the B2 production
// pattern. The wave's defining trait: FOUR of these widgets expose NO runtime setters
// (SColorBlock / the two gradients / SHyperlink bake every arg at Construct), so they are the
// first shipped widgets whose ENTIRE prop surface rides the TD-011 reconstruct mask — a prop
// change replaces the widget in place (ReplaceWidget; cheap leaves, no state to lose).
// SBackgroundBlur / SInvalidationPanel are ordinary setter-based single-content wraps;
// SEnableBox / SScissorRectBox are content-only.

#include "RuiElementAdapter.h"
#include "RuiEventProxy.h"
#include "RuiSlateElements.h"

#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SComplexGradient.h"
#include "Widgets/Colors/SSimpleGradient.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h" // SEnableBox is an SBox — content goes through SBox::SetContent
#include "Widgets/Layout/SEnableBox.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/SInvalidationPanel.h"
#include "Widgets/SNullWidget.h"

namespace
{
	// One compare-and-set row (the B2 convention).
#define RUI_ROW(Prop, ApplyExpr)                                                                                       \
	if (N.Has##Prop() && (O == nullptr || !O->Has##Prop() || !(N.Prop == O->Prop)))                                    \
	{                                                                                                                  \
		ApplyExpr;                                                                                                     \
	}

	// One construct-only change gate (the Separator convention, bughunt SEP-REBUILD-1: gate on
	// the Has-bits so REMOVING a prop never forces a spurious rebuild).
#define RUI_CTOR_CHANGED(Prop) (N.Has##Prop() && (!O.Has##Prop() || !(O.Prop == N.Prop)))

	EOrientation OrientOf(FName V)
	{
		return V == FName(TEXT("horizontal")) ? Orient_Horizontal : Orient_Vertical;
	}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────────────────
// SColorBlock — fully construct-only (no engine setters); every prop is masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiColorBlockAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiColorBlockProps::Color_Bit) | (1ull << FRuiColorBlockProps::Size_Bit) |
			   (1ull << FRuiColorBlockProps::bUseSRGB_Bit) |
			   (1ull << FRuiColorBlockProps::bShowBackgroundForAlpha_Bit) |
			   (1ull << FRuiColorBlockProps::bColorIsHSV_Bit) | (1ull << FRuiColorBlockProps::AlphaDisplayMode_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiColorBlockProps& O = static_cast<const FRuiColorBlockProps&>(Old);
		const FRuiColorBlockProps& N = static_cast<const FRuiColorBlockProps&>(New);
		return RUI_CTOR_CHANGED(Color) || RUI_CTOR_CHANGED(Size) || RUI_CTOR_CHANGED(bUseSRGB) ||
			   RUI_CTOR_CHANGED(bShowBackgroundForAlpha) || RUI_CTOR_CHANGED(bColorIsHSV) ||
			   RUI_CTOR_CHANGED(AlphaDisplayMode);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiColorBlockProps& P = static_cast<const FRuiColorBlockProps&>(Props);
		const EColorBlockAlphaDisplayMode Alpha =
			P.AlphaDisplayMode == FName(TEXT("separate")) ? EColorBlockAlphaDisplayMode::Separate
			: P.AlphaDisplayMode == FName(TEXT("ignore")) ? EColorBlockAlphaDisplayMode::Ignore
														  : EColorBlockAlphaDisplayMode::Combined;
		return SNew(SColorBlock)
			.Color(P.HasColor() ? P.Color : FLinearColor::White)
			.Size(P.HasSize() ? P.Size : FVector2D(16.0, 16.0))
			.UseSRGB(P.HasbUseSRGB() ? P.bUseSRGB : true)
			.ShowBackgroundForAlpha(P.HasbShowBackgroundForAlpha() && P.bShowBackgroundForAlpha)
			.ColorIsHSV(P.HasbColorIsHSV() && P.bColorIsHSV)
			.AlphaDisplayMode(Alpha);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {} // all masked
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSimpleGradient / SComplexGradient — construct-only paint leaves; fully masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiSimpleGradientAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiSimpleGradientProps::StartColor_Bit) | (1ull << FRuiSimpleGradientProps::EndColor_Bit) |
			   (1ull << FRuiSimpleGradientProps::Orientation_Bit) |
			   (1ull << FRuiSimpleGradientProps::bHasAlphaBackground_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiSimpleGradientProps& O = static_cast<const FRuiSimpleGradientProps&>(Old);
		const FRuiSimpleGradientProps& N = static_cast<const FRuiSimpleGradientProps&>(New);
		return RUI_CTOR_CHANGED(StartColor) || RUI_CTOR_CHANGED(EndColor) || RUI_CTOR_CHANGED(Orientation) ||
			   RUI_CTOR_CHANGED(bHasAlphaBackground);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiSimpleGradientProps& P = static_cast<const FRuiSimpleGradientProps&>(Props);
		return SNew(SSimpleGradient)
			.StartColor(P.HasStartColor() ? P.StartColor : FLinearColor::Black)
			.EndColor(P.HasEndColor() ? P.EndColor : FLinearColor::White)
			.Orientation(P.HasOrientation() ? OrientOf(P.Orientation) : Orient_Vertical)
			.HasAlphaBackground(P.HasbHasAlphaBackground() && P.bHasAlphaBackground);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}
};

class FRuiComplexGradientAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiComplexGradientProps::GradientColors_Bit) |
			   (1ull << FRuiComplexGradientProps::Orientation_Bit) |
			   (1ull << FRuiComplexGradientProps::bHasAlphaBackground_Bit) |
			   (1ull << FRuiComplexGradientProps::DesiredSizeOverride_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiComplexGradientProps& O = static_cast<const FRuiComplexGradientProps&>(Old);
		const FRuiComplexGradientProps& N = static_cast<const FRuiComplexGradientProps&>(New);
		return RUI_CTOR_CHANGED(GradientColors) || RUI_CTOR_CHANGED(Orientation) ||
			   RUI_CTOR_CHANGED(bHasAlphaBackground) || RUI_CTOR_CHANGED(DesiredSizeOverride);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiComplexGradientProps& P = static_cast<const FRuiComplexGradientProps&>(Props);
		return SNew(SComplexGradient)
			.GradientColors(P.GradientColors)
			.Orientation(P.HasOrientation() ? OrientOf(P.Orientation) : Orient_Vertical)
			.HasAlphaBackground(P.HasbHasAlphaBackground() && P.bHasAlphaBackground)
			.DesiredSizeOverride(P.HasDesiredSizeOverride() ? TOptional<FVector2D>(P.DesiredSizeOverride)
															: TOptional<FVector2D>());
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SHyperlink — construct-only text/padding (masked) + OnNavigate through the event proxy.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiHyperlinkAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiHyperlinkProps::Text_Bit) | (1ull << FRuiHyperlinkProps::Padding_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiHyperlinkProps& O = static_cast<const FRuiHyperlinkProps&>(Old);
		const FRuiHyperlinkProps& N = static_cast<const FRuiHyperlinkProps&>(New);
		const bool bText = N.HasText() && (!O.HasText() || !O.Text.IdenticalTo(N.Text));
		return bText || RUI_CTOR_CHANGED(Padding);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiHyperlinkProps& P = static_cast<const FRuiHyperlinkProps&>(Props);
		return SNew(SHyperlink)
			.Text(P.Text)
			.Padding(P.HasPadding() ? P.Padding : FMargin(0.0f))
			.OnNavigate(FSimpleDelegate::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleVoid,
												  static_cast<int32>(FRuiHyperlinkProps::OnNavigate_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiHyperlinkProps& N = static_cast<const FRuiHyperlinkProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiHyperlinkProps::OnNavigate_Bit), N.OnNavigate);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SEnableBox / SScissorRectBox — content-only containers.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiEnableBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SEnableBox);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		// SEnableBox has no SetContent — content is its (single) ChildSlot via SBox.
		static_cast<SBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

class FRuiScissorRectBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SScissorRectBox);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SScissorRectBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef()
																		 : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SBackgroundBlur — setter-based single content.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiBackgroundBlurAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SBackgroundBlur);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SBackgroundBlur& W = static_cast<SBackgroundBlur&>(Widget);
		const FRuiBackgroundBlurProps& N = static_cast<const FRuiBackgroundBlurProps&>(New);
		const FRuiBackgroundBlurProps* O = static_cast<const FRuiBackgroundBlurProps*>(Old);
		RUI_ROW(BlurStrength, W.SetBlurStrength(N.BlurStrength))
		RUI_ROW(BlurRadius, W.SetBlurRadius(TOptional<int32>(N.BlurRadius)))
		RUI_ROW(bApplyAlphaToBlur, W.SetApplyAlphaToBlur(N.bApplyAlphaToBlur))
		RUI_ROW(Padding, W.SetPadding(N.Padding))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SBackgroundBlur&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef()
																		 : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SInvalidationPanel — setter-based single content (opt-in paint cache).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiInvalidationPanelAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SInvalidationPanel);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SInvalidationPanel& W = static_cast<SInvalidationPanel&>(Widget);
		const FRuiInvalidationPanelProps& N = static_cast<const FRuiInvalidationPanelProps&>(New);
		const FRuiInvalidationPanelProps* O = static_cast<const FRuiInvalidationPanelProps*>(Old);
		RUI_ROW(bCanCache, W.SetCanCache(N.bCanCache))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SInvalidationPanel&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef()
																			: SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Type ids + factories + registration
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace RUI::Slate
{
	namespace
	{
		template <typename TProps>
		FRuiNode MakeHostNodeB3(FRuiElementTypeId Type, TProps Props, TArray<FRuiNode> Children, FRuiKey Key)
		{
			FRuiNode Node;
			Node.Kind = ERuiNodeKind::Host;
			Node.ElementType = Type;
			Node.Props = MakeShared<TProps>(MoveTemp(Props));
			Node.Children = RUI::MakeChildren(MoveTemp(Children));
			Node.Key = Key;
			return Node;
		}

		FRuiElementTypeId ColorBlockType()
		{
			return RUI::InternElementType(FName(TEXT("ColorBlock")));
		}
		FRuiElementTypeId SimpleGradientType()
		{
			return RUI::InternElementType(FName(TEXT("SimpleGradient")));
		}
		FRuiElementTypeId ComplexGradientType()
		{
			return RUI::InternElementType(FName(TEXT("ComplexGradient")));
		}
		FRuiElementTypeId HyperlinkType()
		{
			return RUI::InternElementType(FName(TEXT("Hyperlink")));
		}
		FRuiElementTypeId EnableBoxType()
		{
			return RUI::InternElementType(FName(TEXT("EnableBox")));
		}
		FRuiElementTypeId ScissorRectBoxType()
		{
			return RUI::InternElementType(FName(TEXT("ScissorRectBox")));
		}
		FRuiElementTypeId BackgroundBlurType()
		{
			return RUI::InternElementType(FName(TEXT("BackgroundBlur")));
		}
		FRuiElementTypeId InvalidationPanelType()
		{
			return RUI::InternElementType(FName(TEXT("InvalidationPanel")));
		}
	} // namespace

	FRuiNode ColorBlock(FRuiColorBlockProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(ColorBlockType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode SimpleGradient(FRuiSimpleGradientProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(SimpleGradientType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode ComplexGradient(FRuiComplexGradientProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(ComplexGradientType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode Hyperlink(FRuiHyperlinkProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(HyperlinkType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode EnableBox(FRuiEnableBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB3(EnableBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode ScissorRectBox(FRuiScissorRectBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB3(ScissorRectBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode BackgroundBlur(FRuiBackgroundBlurProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB3(BackgroundBlurType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode InvalidationPanel(FRuiInvalidationPanelProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB3(InvalidationPanelType(), MoveTemp(Props), MoveTemp(Children), Key);
	}

	namespace Detail
	{
		void RegisterBatch3WidgetAdapters()
		{
			RegisterAdapter(ColorBlockType(), MakeUnique<FRuiColorBlockAdapter>());
			RegisterAdapter(SimpleGradientType(), MakeUnique<FRuiSimpleGradientAdapter>());
			RegisterAdapter(ComplexGradientType(), MakeUnique<FRuiComplexGradientAdapter>());
			RegisterAdapter(HyperlinkType(), MakeUnique<FRuiHyperlinkAdapter>());
			RegisterAdapter(EnableBoxType(), MakeUnique<FRuiEnableBoxAdapter>());
			RegisterAdapter(ScissorRectBoxType(), MakeUnique<FRuiScissorRectBoxAdapter>());
			RegisterAdapter(BackgroundBlurType(), MakeUnique<FRuiBackgroundBlurAdapter>());
			RegisterAdapter(InvalidationPanelType(), MakeUnique<FRuiInvalidationPanelAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate

#undef RUI_ROW
#undef RUI_CTOR_CHANGED
