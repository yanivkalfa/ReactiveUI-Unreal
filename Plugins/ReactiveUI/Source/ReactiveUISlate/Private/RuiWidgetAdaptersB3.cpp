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

#include "Styling/StyleDefaults.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorSpectrum.h"
#include "Widgets/Colors/SColorWheel.h"
#include "Widgets/Colors/SComplexGradient.h"
#include "Widgets/Colors/SSimpleGradient.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/Input/SVirtualKeyboardEntry.h"
#include "Widgets/Input/SVolumeControl.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h" // SEnableBox is an SBox — content goes through SBox::SetContent
#include "Widgets/Layout/SEnableBox.h"
#include "Widgets/Layout/SRadialBox.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/SInvalidationPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/ColorGrading/SColorGradingWheel.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextScroller.h"

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
// SVolumeControl (wave 2) — Volume/Muted are attribute-only (no setters): controlled via the
// reconstruct mask; the two user-edit events flow through the proxy.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiVolumeControlAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiVolumeControlProps::Volume_Bit) | (1ull << FRuiVolumeControlProps::bMuted_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiVolumeControlProps& O = static_cast<const FRuiVolumeControlProps&>(Old);
		const FRuiVolumeControlProps& N = static_cast<const FRuiVolumeControlProps&>(New);
		return RUI_CTOR_CHANGED(Volume) || RUI_CTOR_CHANGED(bMuted);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiVolumeControlProps& P = static_cast<const FRuiVolumeControlProps&>(Props);
		return SNew(SVolumeControl)
			.Volume(P.HasVolume() ? P.Volume : 1.0f)
			.Muted(P.HasbMuted() && P.bMuted)
			.OnVolumeChanged(
				FOnFloatValueChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleFloat,
											   static_cast<int32>(FRuiVolumeControlProps::OnVolumeChanged_Bit)))
			.OnMuteChanged(
				SVolumeControl::FOnMuted::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleBool,
												   static_cast<int32>(FRuiVolumeControlProps::OnMuteChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiVolumeControlProps& N = static_cast<const FRuiVolumeControlProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiVolumeControlProps::OnVolumeChanged_Bit), N.OnVolumeChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiVolumeControlProps::OnMuteChanged_Bit), N.OnMuteChanged);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// STextScroller (wave 2) — construct-only options (masked); Start/Suspend/Reset via P2.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiTextScrollerAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiTextScrollerProps::Speed_Bit) | (1ull << FRuiTextScrollerProps::StartDelay_Bit) |
			   (1ull << FRuiTextScrollerProps::EndDelay_Bit) | (1ull << FRuiTextScrollerProps::ScrollOrientation_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiTextScrollerProps& O = static_cast<const FRuiTextScrollerProps&>(Old);
		const FRuiTextScrollerProps& N = static_cast<const FRuiTextScrollerProps&>(New);
		return RUI_CTOR_CHANGED(Speed) || RUI_CTOR_CHANGED(StartDelay) || RUI_CTOR_CHANGED(EndDelay) ||
			   RUI_CTOR_CHANGED(ScrollOrientation);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiTextScrollerProps& P = static_cast<const FRuiTextScrollerProps&>(Props);
		FTextScrollerOptions Options;
		if (P.HasSpeed())
		{
			Options.Speed = P.Speed;
		}
		if (P.HasStartDelay())
		{
			Options.StartDelay = P.StartDelay;
		}
		if (P.HasEndDelay())
		{
			Options.EndDelay = P.EndDelay;
		}
		return SNew(STextScroller)
			.ScrollOptions(Options)
			.ScrollOrientation(P.HasScrollOrientation() ? OrientOf(P.ScrollOrientation) : Orient_Horizontal);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		// STextScroller exposes no content setter (SLATE_DEFAULT_SLOT only) — a data-free peek
		// subclass re-exports the protected ChildSlot (layout-identical, so the cast is safe).
		struct FScrollerPeek : STextScroller
		{
			using SCompoundWidget::ChildSlot;
		};
		static_cast<FScrollerPeek&>(static_cast<STextScroller&>(Parent))
			.ChildSlot.AttachWidget(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SRadialBox (wave 2) — MultiSlot with BARE slots (no per-child args; arc order = child
// order). PreferredWidth is construct-only; the angle params have live setters — the inline
// ones don't invalidate, so we invalidate layout explicitly (the Canvas precedent).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiRadialBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::MultiSlot; }

	virtual uint64 GetReconstructMask() const override { return 1ull << FRuiRadialBoxProps::PreferredWidth_Bit; }

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiRadialBoxProps& O = static_cast<const FRuiRadialBoxProps&>(Old);
		const FRuiRadialBoxProps& N = static_cast<const FRuiRadialBoxProps&>(New);
		return RUI_CTOR_CHANGED(PreferredWidth);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiRadialBoxProps& P = static_cast<const FRuiRadialBoxProps&>(Props);
		return SNew(SRadialBox)
			.PreferredWidth(P.HasPreferredWidth() ? P.PreferredWidth : 100.f)
			.UseAllottedWidth(P.HasbUseAllottedWidth() && P.bUseAllottedWidth)
			.StartingAngle(P.HasStartingAngle() ? P.StartingAngle : 0.f)
			.bDistributeItemsEvenly(!P.HasbDistributeItemsEvenly() || P.bDistributeItemsEvenly)
			.AngleBetweenItems(P.HasAngleBetweenItems() ? P.AngleBetweenItems : 45.f)
			.SectorCentralAngle(P.HasSectorCentralAngle() ? P.SectorCentralAngle : 360.f);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRadialBox& W = static_cast<SRadialBox&>(Widget);
		const FRuiRadialBoxProps& N = static_cast<const FRuiRadialBoxProps&>(New);
		const FRuiRadialBoxProps* O = static_cast<const FRuiRadialBoxProps*>(Old);
		bool bTouched = false;
		RUI_ROW(bUseAllottedWidth, W.SetUseAllottedWidth(N.bUseAllottedWidth))
		RUI_ROW(StartingAngle, (W.SetStartingAngle(N.StartingAngle), bTouched = true))
		RUI_ROW(bDistributeItemsEvenly, (W.SetDistributeItemsEvenly(N.bDistributeItemsEvenly), bTouched = true))
		RUI_ROW(AngleBetweenItems, (W.SetAngleBetweenItems(N.AngleBetweenItems), bTouched = true))
		RUI_ROW(SectorCentralAngle, (W.SetSectorCentralAngle(N.SectorCentralAngle), bTouched = true))
		if (bTouched)
		{
			W.Invalidate(EInvalidateWidgetReason::Layout); // the inline setters skip invalidation
		}
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32, const FRuiStyleDict*) override
	{
		SRadialBox::FScopedWidgetSlotArguments Slot = static_cast<SRadialBox&>(Parent).AddSlot();
		Slot.AttachWidget(Child);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SRadialBox&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuiStyleDict*(const TSharedRef<SWidget>&)>) override
	{
		SRadialBox& W = static_cast<SRadialBox&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			W.RemoveSlot(Child);
		}
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SRadialBox::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SColorWheel / SColorSpectrum (wave 2) — SelectedColor is attribute-only: controlled via the
// reconstruct mask; drags report through OnValueChanged (+ capture begin/end).
// ─────────────────────────────────────────────────────────────────────────────────────────

template <typename TWidget, typename TProps> class TRuiColorPickerAdapter : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override { return 1ull << TProps::SelectedColor_Bit; }

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const TProps& O = static_cast<const TProps&>(Old);
		const TProps& N = static_cast<const TProps&>(New);
		return N.HasSelectedColor() && (!O.HasSelectedColor() || !(O.SelectedColor == N.SelectedColor));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const TProps& N = static_cast<const TProps&>(New);
		Proxy.SetHandler(static_cast<int32>(TProps::OnValueChanged_Bit), N.OnValueChanged);
		Proxy.SetHandler(static_cast<int32>(TProps::OnMouseCaptureBegin_Bit), N.OnMouseCaptureBegin);
		Proxy.SetHandler(static_cast<int32>(TProps::OnMouseCaptureEnd_Bit), N.OnMouseCaptureEnd);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {}

protected:
	template <typename TArgs> void FillCommon(TArgs& Args, const TProps& P, const TSharedPtr<FRuiEventProxy>& Proxy)
	{
		Args.SelectedColor(P.HasSelectedColor() ? P.SelectedColor : FLinearColor::White)
			.OnValueChanged(FOnLinearColorValueChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleColor,
																 static_cast<int32>(TProps::OnValueChanged_Bit)))
			.OnMouseCaptureBegin(FSimpleDelegate::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleVoid,
														   static_cast<int32>(TProps::OnMouseCaptureBegin_Bit)))
			.OnMouseCaptureEnd(FSimpleDelegate::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleVoid,
														 static_cast<int32>(TProps::OnMouseCaptureEnd_Bit)));
	}
};

class FRuiColorWheelAdapter final : public TRuiColorPickerAdapter<SColorWheel, FRuiColorWheelProps>
{
public:
	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiColorWheelProps& P = static_cast<const FRuiColorWheelProps&>(Props);
		SColorWheel::FArguments Args;
		FillCommon(Args, P, Proxy);
		TSharedRef<SColorWheel> W = SNew(SColorWheel);
		W->Construct(Args);
		return W;
	}
};

class FRuiColorSpectrumAdapter final : public TRuiColorPickerAdapter<SColorSpectrum, FRuiColorSpectrumProps>
{
public:
	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiColorSpectrumProps& P = static_cast<const FRuiColorSpectrumProps&>(Props);
		SColorSpectrum::FArguments Args;
		FillCommon(Args, P, Proxy);
		TSharedRef<SColorSpectrum> W = SNew(SColorSpectrum);
		W->Construct(Args);
		return W;
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SLayeredImage (wave 2) — SImage + live overlay layers (brush identity, B11 reset rule).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiLayeredImageAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SLayeredImage);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SLayeredImage& W = static_cast<SLayeredImage&>(Widget);
		const FRuiLayeredImageProps& N = static_cast<const FRuiLayeredImageProps&>(New);
		const FRuiLayeredImageProps* O = static_cast<const FRuiLayeredImageProps*>(Old);
		RUI_ROW(ColorAndOpacity, W.SetColorAndOpacity(FSlateColor(N.ColorAndOpacity)))
		RUI_ROW(DesiredSizeOverride, W.SetDesiredSizeOverride(N.DesiredSizeOverride))
		// Base brush: pointer-backed — reset on removal (B11), else the widget dangles.
		if (O != nullptr && O->HasImage() && !N.HasImage())
		{
			W.SetImage(FStyleDefaults::GetNoBrush());
		}
		else
		{
			RUI_ROW(Image, W.SetImage(N.Image.Get()))
		}
		// Layers: identity-diffed as a list; any change rebuilds the layer stack (cheap).
		const bool bLayersRemoved = O != nullptr && O->HasLayers() && !N.HasLayers();
		const bool bLayersChanged = N.HasLayers() && (O == nullptr || !O->HasLayers() || !(N.Layers == O->Layers));
		if (bLayersRemoved || bLayersChanged)
		{
			W.RemoveAllLayers();
			if (N.HasLayers())
			{
				for (const TSharedPtr<FSlateBrush>& Layer : N.Layers)
				{
					W.AddLayer(Layer.Get());
				}
			}
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SInputKeySelector (wave 2) — live SelectedKey; capture-behavior args construct-only.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiInputKeySelectorAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiInputKeySelectorProps::KeySelectionText_Bit) |
			   (1ull << FRuiInputKeySelectorProps::NoKeySpecifiedText_Bit) |
			   (1ull << FRuiInputKeySelectorProps::bAllowModifierKeys_Bit) |
			   (1ull << FRuiInputKeySelectorProps::bAllowGamepadKeys_Bit) |
			   (1ull << FRuiInputKeySelectorProps::bEscapeCancelsSelection_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiInputKeySelectorProps& O = static_cast<const FRuiInputKeySelectorProps&>(Old);
		const FRuiInputKeySelectorProps& N = static_cast<const FRuiInputKeySelectorProps&>(New);
		auto TextChanged = [](bool bNewHas, bool bOldHas, const FText& A, const FText& B)
		{ return bNewHas && (!bOldHas || !(B.IdenticalTo(A) || B.ToString() == A.ToString())); };
		return TextChanged(N.HasKeySelectionText(), O.HasKeySelectionText(), O.KeySelectionText, N.KeySelectionText) ||
			   TextChanged(N.HasNoKeySpecifiedText(), O.HasNoKeySpecifiedText(), O.NoKeySpecifiedText,
						   N.NoKeySpecifiedText) ||
			   RUI_CTOR_CHANGED(bAllowModifierKeys) || RUI_CTOR_CHANGED(bAllowGamepadKeys) ||
			   RUI_CTOR_CHANGED(bEscapeCancelsSelection);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiInputKeySelectorProps& P = static_cast<const FRuiInputKeySelectorProps&>(Props);
		TWeakPtr<FRuiEventProxy> WeakProxy = Proxy;
		return SNew(SInputKeySelector)
			.SelectedKey(P.HasSelectedKey() ? FInputChord(FKey(P.SelectedKey)) : FInputChord())
			.KeySelectionText(P.KeySelectionText)
			.NoKeySpecifiedText(P.NoKeySpecifiedText)
			.AllowModifierKeys(!P.HasbAllowModifierKeys() || P.bAllowModifierKeys)
			.AllowGamepadKeys(P.HasbAllowGamepadKeys() && P.bAllowGamepadKeys)
			.EscapeCancelsSelection(!P.HasbEscapeCancelsSelection() || P.bEscapeCancelsSelection)
			.OnKeySelected(SInputKeySelector::FOnKeySelected::CreateLambda(
				[WeakProxy](const FInputChord& Chord)
				{
					if (TSharedPtr<FRuiEventProxy> Pinned = WeakProxy.Pin())
					{
						// Key-only payload (TD-016: modifiers are the multi-field trigger).
						Pinned->HandleName(Chord.Key.GetFName(),
										   static_cast<int32>(FRuiInputKeySelectorProps::OnKeySelected_Bit));
					}
				}))
			.OnIsSelectingKeyChanged(SInputKeySelector::FOnIsSelectingKeyChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleVoid,
				static_cast<int32>(FRuiInputKeySelectorProps::OnIsSelectingKeyChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiInputKeySelectorProps& N = static_cast<const FRuiInputKeySelectorProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiInputKeySelectorProps::OnKeySelected_Bit), N.OnKeySelected);
		Proxy.SetHandler(static_cast<int32>(FRuiInputKeySelectorProps::OnIsSelectingKeyChanged_Bit),
						 N.OnIsSelectingKeyChanged);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SInputKeySelector& W = static_cast<SInputKeySelector&>(Widget);
		const FRuiInputKeySelectorProps& N = static_cast<const FRuiInputKeySelectorProps&>(New);
		const FRuiInputKeySelectorProps* O = static_cast<const FRuiInputKeySelectorProps*>(Old);
		RUI_ROW(SelectedKey, W.SetSelectedKey(FInputChord(FKey(N.SelectedKey))))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SEditableText (wave 2) — the raw single-line edit; full live setters; D-16 caret rule.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiRawEditableTextAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }
	// Stateful (caret/selection): IsPoolable() already excludes event-bearing leaves.

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		return SNew(SEditableText)
			.OnTextChanged(FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleText,
													static_cast<int32>(FRuiEditableTextProps::OnTextChanged_Bit)))
			.OnTextCommitted(
				FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleTextCommit,
										   static_cast<int32>(FRuiEditableTextProps::OnTextCommitted_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiEditableTextProps& N = static_cast<const FRuiEditableTextProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiEditableTextProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiEditableTextProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SEditableText& W = static_cast<SEditableText&>(Widget);
		const FRuiEditableTextProps& N = static_cast<const FRuiEditableTextProps&>(New);
		const FRuiEditableTextProps* O = static_cast<const FRuiEditableTextProps*>(Old);
		// D-16: skip-when-equal against the WIDGET so the caret survives the typing round-trip.
		if (N.HasText() && W.GetText().ToString() != N.Text.ToString())
		{
			W.SetText(N.Text);
		}
		if (N.HasHintText() &&
			(O == nullptr || !O->HasHintText() ||
			 !(N.HintText.IdenticalTo(O->HintText) || N.HintText.ToString() == O->HintText.ToString())))
		{
			W.SetHintText(N.HintText);
		}
		RUI_ROW(bIsReadOnly, W.SetIsReadOnly(N.bIsReadOnly))
		RUI_ROW(bIsPassword, W.SetIsPassword(N.bIsPassword))
		RUI_ROW(MinDesiredWidth, W.SetMinDesiredWidth(N.MinDesiredWidth))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SInlineEditableTextBlock (wave 2) — click-to-edit label; bMultiLine construct-only.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiInlineEditableTextBlockAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return 1ull << FRuiInlineEditableTextBlockProps::bMultiLine_Bit;
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiInlineEditableTextBlockProps& O = static_cast<const FRuiInlineEditableTextBlockProps&>(Old);
		const FRuiInlineEditableTextBlockProps& N = static_cast<const FRuiInlineEditableTextBlockProps&>(New);
		return RUI_CTOR_CHANGED(bMultiLine);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiInlineEditableTextBlockProps& P = static_cast<const FRuiInlineEditableTextBlockProps&>(Props);
		return SNew(SInlineEditableTextBlock)
			.MultiLine(P.HasbMultiLine() && P.bMultiLine)
			.OnTextCommitted(
				FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleTextCommit,
										   static_cast<int32>(FRuiInlineEditableTextBlockProps::OnTextCommitted_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiInlineEditableTextBlockProps& N = static_cast<const FRuiInlineEditableTextBlockProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiInlineEditableTextBlockProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SInlineEditableTextBlock& W = static_cast<SInlineEditableTextBlock&>(Widget);
		const FRuiInlineEditableTextBlockProps& N = static_cast<const FRuiInlineEditableTextBlockProps&>(New);
		const FRuiInlineEditableTextBlockProps* O = static_cast<const FRuiInlineEditableTextBlockProps*>(Old);
		// No widget-side text getter — diff against the previous PROPS (commit-to-commit).
		if (N.HasText() && (O == nullptr || !O->HasText() ||
							!(N.Text.IdenticalTo(O->Text) || N.Text.ToString() == O->Text.ToString())))
		{
			W.SetText(N.Text);
		}
		if (N.HasHintText() &&
			(O == nullptr || !O->HasHintText() ||
			 !(N.HintText.IdenticalTo(O->HintText) || N.HintText.ToString() == O->HintText.ToString())))
		{
			W.SetHintText(N.HintText);
		}
		RUI_ROW(bIsReadOnly, W.SetReadOnly(N.bIsReadOnly))
		RUI_ROW(WrapTextAt, W.SetWrapTextAt(N.WrapTextAt))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SVirtualKeyboardEntry (wave 2) — mobile OS-keyboard field; Text live, the rest masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiVirtualKeyboardEntryAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiVirtualKeyboardEntryProps::HintText_Bit) |
			   (1ull << FRuiVirtualKeyboardEntryProps::bIsReadOnly_Bit) |
			   (1ull << FRuiVirtualKeyboardEntryProps::KeyboardType_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiVirtualKeyboardEntryProps& O = static_cast<const FRuiVirtualKeyboardEntryProps&>(Old);
		const FRuiVirtualKeyboardEntryProps& N = static_cast<const FRuiVirtualKeyboardEntryProps&>(New);
		const bool bHint = N.HasHintText() && (!O.HasHintText() || !(N.HintText.IdenticalTo(O.HintText) ||
																	 N.HintText.ToString() == O.HintText.ToString()));
		return bHint || RUI_CTOR_CHANGED(bIsReadOnly) || RUI_CTOR_CHANGED(KeyboardType);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiVirtualKeyboardEntryProps& P = static_cast<const FRuiVirtualKeyboardEntryProps&>(Props);
		const EKeyboardType Keyboard = P.KeyboardType == FName(TEXT("number"))	   ? Keyboard_Number
									   : P.KeyboardType == FName(TEXT("web"))	   ? Keyboard_Web
									   : P.KeyboardType == FName(TEXT("email"))	   ? Keyboard_Email
									   : P.KeyboardType == FName(TEXT("password")) ? Keyboard_Password
																				   : Keyboard_Default;
		return SNew(SVirtualKeyboardEntry)
			.Text(P.Text)
			.HintText(P.HintText)
			.IsReadOnly(P.HasbIsReadOnly() && P.bIsReadOnly)
			.KeyboardType(Keyboard)
			.OnTextChanged(
				FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleText,
										 static_cast<int32>(FRuiVirtualKeyboardEntryProps::OnTextChanged_Bit)))
			.OnTextCommitted(
				FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuiEventProxy::HandleTextCommit,
										   static_cast<int32>(FRuiVirtualKeyboardEntryProps::OnTextCommitted_Bit)));
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiVirtualKeyboardEntryProps& N = static_cast<const FRuiVirtualKeyboardEntryProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiVirtualKeyboardEntryProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiVirtualKeyboardEntryProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SVirtualKeyboardEntry& W = static_cast<SVirtualKeyboardEntry&>(Widget);
		const FRuiVirtualKeyboardEntryProps& N = static_cast<const FRuiVirtualKeyboardEntryProps&>(New);
		if (N.HasText() && W.GetText().ToString() != N.Text.ToString()) // D-16
		{
			W.SetText(N.Text);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SColorGradingWheel (wave 2; AdvancedWidgets module) — live attribute setters throughout.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuiColorGradingWheelAdapter final : public IRuiElementAdapter
{
	using SWheel = UE::ColorGrading::SColorGradingWheel;

public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	// The attribute setters are PROTECTED in the engine class - construct-only from outside.
	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuiColorGradingWheelProps::SelectedColor_Bit) |
			   (1ull << FRuiColorGradingWheelProps::DesiredWheelSize_Bit) |
			   (1ull << FRuiColorGradingWheelProps::ExponentDisplacement_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuiPropsBase& Old, const FRuiPropsBase& New) const override
	{
		const FRuiColorGradingWheelProps& O = static_cast<const FRuiColorGradingWheelProps&>(Old);
		const FRuiColorGradingWheelProps& N = static_cast<const FRuiColorGradingWheelProps&>(New);
		return RUI_CTOR_CHANGED(SelectedColor) || RUI_CTOR_CHANGED(DesiredWheelSize) ||
			   RUI_CTOR_CHANGED(ExponentDisplacement);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props,
											 const TSharedPtr<FRuiEventProxy>& Proxy) override
	{
		const FRuiColorGradingWheelProps& P = static_cast<const FRuiColorGradingWheelProps&>(Props);
		SWheel::FArguments Args;
		Args.SelectedColor(P.HasSelectedColor() ? P.SelectedColor : FLinearColor::White)
			.OnValueChanged(SWheel::FOnColorGradingWheelValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleColorRef,
				static_cast<int32>(FRuiColorGradingWheelProps::OnValueChanged_Bit)))
			.OnMouseCaptureBegin(SWheel::FOnColorGradingWheelMouseCapture::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleColorRef,
				static_cast<int32>(FRuiColorGradingWheelProps::OnMouseCaptureBegin_Bit)))
			.OnMouseCaptureEnd(SWheel::FOnColorGradingWheelMouseCapture::CreateSP(
				Proxy.ToSharedRef(), &FRuiEventProxy::HandleColorRef,
				static_cast<int32>(FRuiColorGradingWheelProps::OnMouseCaptureEnd_Bit)));
		if (P.HasDesiredWheelSize())
		{
			Args.DesiredWheelSize(P.DesiredWheelSize);
		}
		if (P.HasExponentDisplacement())
		{
			Args.ExponentDisplacement(P.ExponentDisplacement);
		}
		TSharedRef<SWheel> W = SNew(SWheel);
		W->Construct(Args);
		return W;
	}

	virtual void SyncEventHandlers(FRuiEventProxy& Proxy, const FRuiPropsBase& New) override
	{
		const FRuiColorGradingWheelProps& N = static_cast<const FRuiColorGradingWheelProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuiColorGradingWheelProps::OnValueChanged_Bit), N.OnValueChanged);
		Proxy.SetHandler(static_cast<int32>(FRuiColorGradingWheelProps::OnMouseCaptureBegin_Bit),
						 N.OnMouseCaptureBegin);
		Proxy.SetHandler(static_cast<int32>(FRuiColorGradingWheelProps::OnMouseCaptureEnd_Bit), N.OnMouseCaptureEnd);
	}

	virtual void ApplyDiff(SWidget&, const FRuiPropsBase*, const FRuiPropsBase&) override {} // all masked
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
		FRuiElementTypeId VolumeControlType()
		{
			return RUI::InternElementType(FName(TEXT("VolumeControl")));
		}
		FRuiElementTypeId TextScrollerType()
		{
			return RUI::InternElementType(FName(TEXT("TextScroller")));
		}
		FRuiElementTypeId RadialBoxType()
		{
			return RUI::InternElementType(FName(TEXT("RadialBox")));
		}
		FRuiElementTypeId ColorWheelType()
		{
			return RUI::InternElementType(FName(TEXT("ColorWheel")));
		}
		FRuiElementTypeId ColorSpectrumType()
		{
			return RUI::InternElementType(FName(TEXT("ColorSpectrum")));
		}
		FRuiElementTypeId LayeredImageType()
		{
			return RUI::InternElementType(FName(TEXT("LayeredImage")));
		}
		FRuiElementTypeId InputKeySelectorType()
		{
			return RUI::InternElementType(FName(TEXT("InputKeySelector")));
		}
		FRuiElementTypeId EditableTextType()
		{
			return RUI::InternElementType(FName(TEXT("EditableText")));
		}
		FRuiElementTypeId InlineEditableTextBlockType()
		{
			return RUI::InternElementType(FName(TEXT("InlineEditableTextBlock")));
		}
		FRuiElementTypeId VirtualKeyboardEntryType()
		{
			return RUI::InternElementType(FName(TEXT("VirtualKeyboardEntry")));
		}
		FRuiElementTypeId ColorGradingWheelType()
		{
			return RUI::InternElementType(FName(TEXT("ColorGradingWheel")));
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
	FRuiNode VolumeControl(FRuiVolumeControlProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(VolumeControlType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode TextScroller(FRuiTextScrollerProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB3(TextScrollerType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode RadialBox(FRuiRadialBoxProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeHostNodeB3(RadialBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuiNode ColorWheel(FRuiColorWheelProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(ColorWheelType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode ColorSpectrum(FRuiColorSpectrumProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(ColorSpectrumType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode LayeredImage(FRuiLayeredImageProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(LayeredImageType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode InputKeySelector(FRuiInputKeySelectorProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(InputKeySelectorType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode EditableText(FRuiEditableTextProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(EditableTextType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode InlineEditableTextBlock(FRuiInlineEditableTextBlockProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(InlineEditableTextBlockType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode VirtualKeyboardEntry(FRuiVirtualKeyboardEntryProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(VirtualKeyboardEntryType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
	}
	FRuiNode ColorGradingWheel(FRuiColorGradingWheelProps Props, FRuiKey Key)
	{
		return MakeHostNodeB3(ColorGradingWheelType(), MoveTemp(Props), TArray<FRuiNode>(), Key);
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
			RegisterAdapter(VolumeControlType(), MakeUnique<FRuiVolumeControlAdapter>());
			RegisterAdapter(TextScrollerType(), MakeUnique<FRuiTextScrollerAdapter>());
			RegisterAdapter(RadialBoxType(), MakeUnique<FRuiRadialBoxAdapter>());
			RegisterAdapter(ColorWheelType(), MakeUnique<FRuiColorWheelAdapter>());
			RegisterAdapter(ColorSpectrumType(), MakeUnique<FRuiColorSpectrumAdapter>());
			RegisterAdapter(LayeredImageType(), MakeUnique<FRuiLayeredImageAdapter>());
			RegisterAdapter(InputKeySelectorType(), MakeUnique<FRuiInputKeySelectorAdapter>());
			RegisterAdapter(EditableTextType(), MakeUnique<FRuiRawEditableTextAdapter>());
			RegisterAdapter(InlineEditableTextBlockType(), MakeUnique<FRuiInlineEditableTextBlockAdapter>());
			RegisterAdapter(VirtualKeyboardEntryType(), MakeUnique<FRuiVirtualKeyboardEntryAdapter>());
			RegisterAdapter(ColorGradingWheelType(), MakeUnique<FRuiColorGradingWheelAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate

#undef RUI_ROW
#undef RUI_CTOR_CHANGED
