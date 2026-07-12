// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-004 (input APIs, drag-and-drop half) — typed DnD over Slate's FDragDropOperation. Slate DnD is
// driven by widget event OVERRIDES (OnDragDetected / OnDrop / OnDragEnter…), not by props or ref
// capture, so the API is a pair of wrapper ELEMENTS rather than a hook: wrap the draggable content
// in `RUI::Slate::DragSource` (it carries an FRuiValue payload + a type tag) and the drop zone in
// `RUI::Slate::DropTarget` (it filters by accepted type tags and fires OnDrop with the payload).
//
// C++-FIRST (like ListView / MakeDrawFn): the drop handler is a closure and the payload is an
// FRuiValue, neither cleanly markup-expressible (nor is an array-of-accept-types attribute), so
// there is no `.uetkx` tag. The events are plain FRuiCallbacks routed straight to the overrides —
// no event proxy (Slate DnD is not a delegate surface). The concrete widgets + operation are
// exported so tests/tools can drive a synthetic drop (the overrides are directly callable) without
// a live Slate drag loop.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "RuiNode.h"
#include "RuiPropsBase.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"

// ── the operation ─────────────────────────────────────────────────────────────────────────────

/** The drag operation carrying our payload + type tag across a drag. */
class REACTIVEUISLATE_API FRuiDragDropOp final : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FRuiDragDropOp, FDragDropOperation)

	FName DragType;
	FRuiValue Payload;
	TFunction<void(bool)> OnEnded; // fired once when the drag concludes (arg = was the drop handled)

	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override
	{
		if (OnEnded)
		{
			OnEnded(bDropWasHandled);
			OnEnded = nullptr; // fire exactly once
		}
		FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);
	}

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override; // simple type-label (defined in .cpp)

	static TSharedRef<FRuiDragDropOp> New(FName InType, FRuiValue InPayload, TFunction<void(bool)> InOnEnded = nullptr)
	{
		TSharedRef<FRuiDragDropOp> Op = MakeShared<FRuiDragDropOp>();
		Op->DragType = InType;
		Op->Payload = MoveTemp(InPayload);
		Op->OnEnded = MoveTemp(InOnEnded);
		Op->Construct();
		return Op;
	}
};

// ── the draggable source ──────────────────────────────────────────────────────────────────────

class REACTIVEUISLATE_API SRuiDragSource final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuiDragSource) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) { ChildSlot[SNullWidget::NullWidget]; }

	void SetContent(const TSharedPtr<SWidget>& Content)
	{
		ChildSlot[Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget];
	}

	void SetDragType(FName InType) { DragType = InType; }
	void SetPayload(FRuiValue InPayload) { Payload = MoveTemp(InPayload); }
	void SetOnDragStart(FRuiCallback InCb) { OnDragStart = MoveTemp(InCb); }
	void SetOnDragEnd(FRuiCallback InCb) { OnDragEnd = MoveTemp(InCb); }

	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
		}
		return FReply::Unhandled();
	}

	virtual FReply OnDragDetected(const FGeometry&, const FPointerEvent&) override
	{
		const FRuiCallback EndCb = OnDragEnd;
		TSharedRef<FRuiDragDropOp> Op = FRuiDragDropOp::New(DragType, Payload,
															[EndCb](bool bHandled)
															{
																if (EndCb.IsBound())
																{
																	EndCb.Execute(FRuiValue(bHandled));
																}
															});
		if (OnDragStart.IsBound())
		{
			OnDragStart.Execute(Payload);
		}
		return FReply::Handled().BeginDragDrop(Op);
	}

private:
	FName DragType;
	FRuiValue Payload;
	FRuiCallback OnDragStart;
	FRuiCallback OnDragEnd;
};

// ── the drop zone ─────────────────────────────────────────────────────────────────────────────

class REACTIVEUISLATE_API SRuiDropTarget final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuiDropTarget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) { ChildSlot[SNullWidget::NullWidget]; }

	void SetContent(const TSharedPtr<SWidget>& Content)
	{
		ChildSlot[Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget];
	}

	void SetAcceptTypes(TArray<FName> InTypes) { AcceptTypes = MoveTemp(InTypes); }
	void SetOnDrop(FRuiCallback InCb) { OnDropCb = MoveTemp(InCb); }
	void SetOnDragEnter(FRuiCallback InCb) { OnEnterCb = MoveTemp(InCb); }
	void SetOnDragLeave(FRuiCallback InCb) { OnLeaveCb = MoveTemp(InCb); }

	bool IsOver() const { return bIsOver; }

	virtual void OnDragEnter(const FGeometry&, const FDragDropEvent& Event) override
	{
		const TSharedPtr<FRuiDragDropOp> Op = Event.GetOperationAs<FRuiDragDropOp>();
		if (Accepts(Op))
		{
			bIsOver = true;
			if (OnEnterCb.IsBound())
			{
				OnEnterCb.Execute(Op->Payload);
			}
		}
	}

	virtual void OnDragLeave(const FDragDropEvent&) override
	{
		if (bIsOver)
		{
			bIsOver = false;
			if (OnLeaveCb.IsBound())
			{
				OnLeaveCb.Execute(FRuiValue());
			}
		}
	}

	virtual FReply OnDragOver(const FGeometry&, const FDragDropEvent& Event) override
	{
		return Accepts(Event.GetOperationAs<FRuiDragDropOp>()) ? FReply::Handled() : FReply::Unhandled();
	}

	virtual FReply OnDrop(const FGeometry&, const FDragDropEvent& Event) override
	{
		const TSharedPtr<FRuiDragDropOp> Op = Event.GetOperationAs<FRuiDragDropOp>();
		if (Accepts(Op))
		{
			bIsOver = false;
			if (OnDropCb.IsBound())
			{
				OnDropCb.Execute(Op->Payload);
			}
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

private:
	bool Accepts(const TSharedPtr<FRuiDragDropOp>& Op) const
	{
		return Op.IsValid() && (AcceptTypes.Num() == 0 || AcceptTypes.Contains(Op->DragType));
	}

	TArray<FName> AcceptTypes;
	FRuiCallback OnDropCb;
	FRuiCallback OnEnterCb;
	FRuiCallback OnLeaveCb;
	bool bIsOver = false;
};

// ── typed props + factories ─────────────────────────────────────────────────────────────────────

/** A draggable source (SingleContent): its child becomes grabbable; a left-drag begins an operation
 *  carrying `Payload` tagged `DragType`. OnDragStart fires with the payload when a drag begins;
 *  OnDragEnd fires with a bool (was the drop handled?) when it ends. */
struct REACTIVEUISLATE_API FRuiDragSourceProps final : public FRuiPropsBase
{
	RUI_PROP(FName, DragType, 0)
	RUI_PROP(FRuiValue, Payload, 1)
	RUI_PROP_EVENT(OnDragStart, 2)
	RUI_PROP_EVENT(OnDragEnd, 3)
	RUI_PROPS_BODY(FRuiDragSourceProps, RUI_EQ(DragType) RUI_EQ(Payload) RUI_EQ(OnDragStart) RUI_EQ(OnDragEnd))
};

/** A drop zone (SingleContent): accepts operations whose DragType is in `AcceptTypes` (empty = accept
 *  ANY FRuiDragDropOp). OnDrop fires with the dropped payload; OnDragEnter/OnDragLeave fire with the
 *  hovering payload (for hover styling) as an accepted op enters/leaves. */
struct REACTIVEUISLATE_API FRuiDropTargetProps final : public FRuiPropsBase
{
	RUI_PROP(TArray<FName>, AcceptTypes, 0)
	RUI_PROP_EVENT(OnDrop, 1)
	RUI_PROP_EVENT(OnDragEnter, 2)
	RUI_PROP_EVENT(OnDragLeave, 3)
	RUI_PROPS_BODY(FRuiDropTargetProps, RUI_EQ(AcceptTypes) RUI_EQ(OnDrop) RUI_EQ(OnDragEnter) RUI_EQ(OnDragLeave))
};

namespace RUI::Slate
{
	REACTIVEUISLATE_API FRuiElementTypeId DragSourceType();
	REACTIVEUISLATE_API FRuiElementTypeId DropTargetType();

	/** Wrap draggable content. The child is grabbed on a left-drag; the operation carries `Payload`. */
	REACTIVEUISLATE_API FRuiNode DragSource(FRuiDragSourceProps Props = FRuiDragSourceProps(),
											TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());

	/** Wrap a drop zone. Accepted operations dropped here fire OnDrop with their payload. */
	REACTIVEUISLATE_API FRuiNode DropTarget(FRuiDropTargetProps Props = FRuiDropTargetProps(),
											TArray<FRuiNode> Children = TArray<FRuiNode>(), FRuiKey Key = FRuiKey());

	/** Register the DragSource/DropTarget adapters (called from RegisterBuiltinAdapters; idempotent). */
	namespace Detail
	{
		void RegisterDragDropAdapters();
	}
} // namespace RUI::Slate
