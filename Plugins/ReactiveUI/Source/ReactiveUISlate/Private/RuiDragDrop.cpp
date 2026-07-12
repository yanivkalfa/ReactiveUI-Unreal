// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-004 drag-and-drop half — adapters/factories over the wrapper widgets + operation declared in
// RuiDragDrop.h. The events are plain FRuiCallbacks invoked from the widget overrides (Slate DnD is
// an override surface, not a delegate one — so no event proxy).

#include "RuiDragDrop.h"

#include "RuiElementAdapter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

TSharedPtr<SWidget> FRuiDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)[SNew(STextBlock).Text(FText::FromName(DragType))];
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapters
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuiDragSourceAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SRuiDragSource);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiDragSource& W = static_cast<SRuiDragSource&>(Widget);
		const FRuiDragSourceProps& N = static_cast<const FRuiDragSourceProps&>(New);
		const FRuiDragSourceProps* O = static_cast<const FRuiDragSourceProps*>(Old);
		if (N.HasDragType() && (O == nullptr || !O->HasDragType() || !(N.DragType == O->DragType)))
		{
			W.SetDragType(N.DragType);
		}
		if (N.HasPayload() && (O == nullptr || !O->HasPayload() || !(N.Payload == O->Payload)))
		{
			W.SetPayload(N.Payload);
		}
		if (N.HasOnDragStart())
		{
			W.SetOnDragStart(N.OnDragStart);
		}
		if (N.HasOnDragEnd())
		{
			W.SetOnDragEnd(N.OnDragEnd);
		}
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SRuiDragSource&>(Parent).SetContent(Child);
	}
};

class FRuiDropTargetAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::SingleContent; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase&, const TSharedPtr<FRuiEventProxy>&) override
	{
		return SNew(SRuiDropTarget);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiDropTarget& W = static_cast<SRuiDropTarget&>(Widget);
		const FRuiDropTargetProps& N = static_cast<const FRuiDropTargetProps&>(New);
		const FRuiDropTargetProps* O = static_cast<const FRuiDropTargetProps*>(Old);
		if (N.HasAcceptTypes() && (O == nullptr || !O->HasAcceptTypes() || !(N.AcceptTypes == O->AcceptTypes)))
		{
			W.SetAcceptTypes(N.AcceptTypes);
		}
		if (N.HasOnDrop())
		{
			W.SetOnDrop(N.OnDrop);
		}
		if (N.HasOnDragEnter())
		{
			W.SetOnDragEnter(N.OnDragEnter);
		}
		if (N.HasOnDragLeave())
		{
			W.SetOnDragLeave(N.OnDragLeave);
		}
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SRuiDropTarget&>(Parent).SetContent(Child);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Types, factories, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace RUI::Slate
{
	FRuiElementTypeId DragSourceType()
	{
		return RUI::InternElementType(FName(TEXT("DragSource")));
	}
	FRuiElementTypeId DropTargetType()
	{
		return RUI::InternElementType(FName(TEXT("DropTarget")));
	}

	namespace
	{
		template <typename TProps>
		FRuiNode MakeDnDNode(FRuiElementTypeId Type, TProps Props, TArray<FRuiNode> Children, FRuiKey Key)
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

	FRuiNode DragSource(FRuiDragSourceProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeDnDNode(DragSourceType(), MoveTemp(Props), MoveTemp(Children), Key);
	}

	FRuiNode DropTarget(FRuiDropTargetProps Props, TArray<FRuiNode> Children, FRuiKey Key)
	{
		return MakeDnDNode(DropTargetType(), MoveTemp(Props), MoveTemp(Children), Key);
	}

	namespace Detail
	{
		void RegisterDragDropAdapters()
		{
			RegisterAdapter(DragSourceType(), MakeUnique<FRuiDragSourceAdapter>());
			RegisterAdapter(DropTargetType(), MakeUnique<FRuiDropTargetAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate
