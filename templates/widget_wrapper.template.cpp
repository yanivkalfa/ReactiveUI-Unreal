// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ── component-pipeline station 2 TEMPLATE ────────────────────────────────────────────────
// One wrapped Slate widget = this file, filled from its prop-map entry
// (templates/prop-map/<TAG>.json — station 1's output; the single source).
// Replace every __TOKEN__; delete sections the widget doesn't have; keep the section
// ORDER — diffs stay reviewable when every adapter reads the same way.
// The concrete base types (FRuiPropsBase, IRuiElementAdapter, FRuiEventProxy, RUI_PROP)
// land in Phase 1/2 — this template is normative for their call shape; update it in the
// SAME PR if that shape changes (the pipeline skill points lesser models here verbatim).
//
// Traps (component-pipeline skill has the full list): attributes get CONSTANT values +
// setters, never delegate TAttributes (D-12); construct-only args go on the reconstruct
// mask; slot props live on the SLOT (parent adapter), not here; hot-path style keys must
// NOT be reconstruct-masked (D-13); equal-value setter skip for self-notifying widgets (D-16).

#include "__WIDGET_HEADER__" // e.g. Widgets/Input/SButton.h — the header station 1 read

// ── props struct (one RUI_PROP row per prop-map "props" entry) ──────────────────────────
struct FRui__TAG__Props : public FRuiPropsBase
{
	RUI_PROP(FText, __PROP1__, 0)		  // kind=attribute, setter=Set__PROP1__
	RUI_PROP(bool, __PROP2__, 1)		  // ...
	RUI_PROP(FRuiCallback, __EVENT1__, 2) // event slot — diffed by proxy swap, never compared
	// SetBits semantics: bit set = this render specified the field. Removed PLAIN props are
	// ignored (family semantic); removed style/event/ref bits ARE cleaned up.
};

// ── builder (the public codegen contract, D-02 — markup compiles to exactly these calls) ─
namespace RUI
{
	FRuiNodeBuilder<FRui__TAG__Props> __TAG__();
	// One fluent setter per prop: RUI::__TAG__().__PROP1__(...).__EVENT1__(...)
} // namespace RUI

// ── adapter (registered into the element registry at module startup) ─────────────────────
class FRui__TAG__Adapter final : public IRuiElementAdapter
{
public:
	// childKind from the prop-map: multi-slot | single-content | leaf
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::__CHILD_KIND__; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props) override
	{
		const auto& P = static_cast<const FRui__TAG__Props&>(Props);
		// Construct-only args (the reconstructOn list) are consumed HERE and only here.
		TSharedRef<S__SLATE_CLASS__> W = SNew(S__SLATE_CLASS__)
			/* .__CONSTRUCT_ONLY_ARG__(...) */;
		ApplyFull(W, P);
		return W;
	}

	virtual void ApplyDiff(const TSharedRef<SWidget>& Widget, const FRuiPropsBase& Old,
						   const FRuiPropsBase& New) override
	{
		auto& W = static_cast<S__SLATE_CLASS__&>(Widget.Get());
		const auto& O = static_cast<const FRui__TAG__Props&>(Old);
		const auto& N = static_cast<const FRui__TAG__Props&>(New);
		// One flat compare-and-set row per settable prop (memberwise; no hashing, no reflection):
		// if (N.Has__PROP1__() && N.__PROP1__ != O.__PROP1__) { W.Set__PROP1__(N.__PROP1__); }
		// Self-notifying setters (editable text etc.): skip when equal — D-16.
	}

	// Construct-only props: a diff touching these bits marks the fiber for widget
	// REPLACEMENT (type-change path). Keep this mask honest and minimal — and never put
	// hot-path style keys on it (D-13).
	virtual uint64 GetReconstructMask() const override { return 0 /* | RUI_BIT(__ARG__) */; }

	// Events: bind ONCE at construction to the per-node proxy; prop updates just swap the
	// proxy's inner callback (TFunction has no equality — this is the design answer).
	virtual void BindEvents(const TSharedRef<SWidget>& Widget, const TSharedRef<FRuiEventProxy>& Proxy) override
	{
		// static_cast<S__SLATE_CLASS__&>(Widget.Get())
		//     .Set__EVENT1__(F__DELEGATE__::CreateSP(Proxy, &FRuiEventProxy::Handle__EVENT1__));
	}
};

// Registration (module startup): RUI::RegisterAdapter(TEXT("__TAG__"), MakeShared<FRui__TAG__Adapter>());
