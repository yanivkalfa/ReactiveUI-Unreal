// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The mock host: IRuiHostConfig over plain test nodes — what makes the ENTIRE reconciler
// testable headlessly (the react-test-renderer seam, D-11). Also the test harness and the
// test elements the ported core suites render.

#pragma once

#include "CoreMinimal.h"
#include "RuiHostConfig.h"
#include "RuiElementRegistry.h"
#include "RuiReconciler.h"
#include "RuiCoreElements.h"

/** A mock host node: enough structure to assert identity, order, props, and lifecycle. */
struct FMockNode
{
	FName Tag;
	/** Raw view of the latest committed props — the fiber owns them (kept alive while
	 *  committed); updated on every CommitUpdate. Test-read only. */
	const FRuiPropsBase* Props = nullptr;
	TArray<TSharedPtr<FMockNode>> Children;
	FMockNode* Parent = nullptr;
	int32 UpdateCount = 0;
	bool bReleased = false;

	template <typename T> const T* PropsAs() const { return static_cast<const T*>(Props); }

	FString TextOf() const
	{
		const FRuiTextBlockProps* T = PropsAs<FRuiTextBlockProps>();
		return T ? T->Text.ToString() : FString();
	}
};

class FRuiMockHost final : public IRuiHostConfig
{
public:
	TSharedRef<FMockNode> Root = MakeShared<FMockNode>();
	int32 CreatedCount = 0;
	int32 ReleasedCount = 0;
	FRuiSafeArea SafeArea{1, 2, 3, 4};

	static FMockNode* Cast(const FRuiHostHandle& H) { return static_cast<FMockNode*>(H.Get()); }

	virtual FRuiHostHandle CreateInstance(FRuiElementTypeId Type, const FRuiPropsBase& Props) override
	{
		TSharedRef<FMockNode> Node = MakeShared<FMockNode>();
		Node->Tag = RUI::GetElementTypeName(Type);
		Node->Props = &Props;
		++CreatedCount;
		return Node;
	}

	virtual void CommitUpdate(const FRuiHostHandle& Node, FRuiElementTypeId, const FRuiPropsBase*,
							  const FRuiPropsBase& NewProps) override
	{
		FMockNode* N = Cast(Node);
		N->Props = &NewProps;
		++N->UpdateCount;
	}

	virtual void ReleaseInstance(const FRuiHostHandle& Node, FRuiElementTypeId, const TSharedPtr<const FRuiPropsBase>&,
								 bool) override
	{
		FMockNode* N = Cast(Node);
		N->bReleased = true;
		++ReleasedCount;
		if (N->Parent != nullptr)
		{
			RemoveFrom(N->Parent, N);
		}
	}

	virtual void InsertChild(const FRuiHostHandle& Parent, const FRuiHostHandle& Child, int32 Index) override
	{
		FMockNode* P = Parent.IsValid() ? Cast(Parent) : &Root.Get();
		TSharedPtr<FMockNode> C = StaticCastSharedPtr<FMockNode>(Child);
		if (C->Parent != nullptr)
		{
			RemoveFrom(C->Parent, C.Get());
		}
		C->Parent = P;
		if (Index < 0 || Index >= P->Children.Num())
		{
			P->Children.Add(C);
		}
		else
		{
			P->Children.Insert(C, Index);
		}
	}

	virtual void RemoveChild(const FRuiHostHandle& Parent, const FRuiHostHandle& Child) override
	{
		FMockNode* P = Parent.IsValid() ? Cast(Parent) : &Root.Get();
		RemoveFrom(P, Cast(Child));
	}

	virtual void ReorderChildren(const FRuiHostHandle& Parent, const TArray<FRuiHostHandle>& Ordered) override
	{
		FMockNode* P = Parent.IsValid() ? Cast(Parent) : &Root.Get();
		TArray<TSharedPtr<FMockNode>> NewOrder;
		NewOrder.Reserve(Ordered.Num());
		for (const FRuiHostHandle& H : Ordered)
		{
			FMockNode* Want = Cast(H);
			for (const TSharedPtr<FMockNode>& Existing : P->Children)
			{
				if (Existing.Get() == Want)
				{
					NewOrder.Add(Existing);
					break;
				}
			}
		}
		// Keep any host children the reconciler doesn't know (none in practice) at the end.
		for (const TSharedPtr<FMockNode>& Existing : P->Children)
		{
			if (!NewOrder.Contains(Existing))
			{
				NewOrder.Add(Existing);
			}
		}
		P->Children = MoveTemp(NewOrder);
	}

	virtual FRuiElementTypeId GetTextElementType() const override { return RUI::TextBlockElementType(); }

	virtual void RequestFrame(TFunction<void()> Callback) override { FrameQueue.Add(MoveTemp(Callback)); }

	virtual void GetSafeArea(float& L, float& T, float& R, float& B) const override
	{
		L = SafeArea.Left;
		T = SafeArea.Top;
		R = SafeArea.Right;
		B = SafeArea.Bottom;
	}

	/** Run one "frame": drain the callbacks queued so far (new ones queue for the next). */
	void PumpFrame()
	{
		TArray<TFunction<void()>> Batch = MoveTemp(FrameQueue);
		FrameQueue.Reset();
		for (TFunction<void()>& Fn : Batch)
		{
			Fn();
		}
	}

	void Pump(int32 Frames = 2)
	{
		for (int32 i = 0; i < Frames; ++i)
		{
			PumpFrame();
		}
	}

	/** Pump until nothing is queued (bounded — a runaway loop fails the assert). */
	bool PumpUntilIdle(int32 MaxFrames = 64)
	{
		int32 n = 0;
		while (!FrameQueue.IsEmpty())
		{
			if (++n > MaxFrames)
			{
				return false;
			}
			PumpFrame();
		}
		return true;
	}

private:
	static void RemoveFrom(FMockNode* Parent, FMockNode* Child)
	{
		Parent->Children.RemoveAll([Child](const TSharedPtr<FMockNode>& C) { return C.Get() == Child; });
		if (Child->Parent == Parent)
		{
			Child->Parent = nullptr;
		}
	}

	TArray<TFunction<void()>> FrameQueue;
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Test elements ("Box": a generic container/leaf with a label + value + event)
// ─────────────────────────────────────────────────────────────────────────────────────────

struct FTestBoxProps final : public FRuiPropsBase
{
	RUI_PROP(FString, Label, 0)
	RUI_PROP(int32, Value, 1)
	RUI_PROP_EVENT(OnPing, 2)
	RUI_PROPS_BODY(FTestBoxProps, RUI_EQ(Label) RUI_EQ(Value))
};

namespace RuiTest
{
	inline FRuiElementTypeId BoxType()
	{
		static FRuiElementTypeId Id = RUI::InternElementType(FName(TEXT("Box")));
		return Id;
	}

	inline FRuiNode Box(FTestBoxProps InProps = FTestBoxProps(), TArray<FRuiNode> Children = {},
						FRuiKey Key = FRuiKey())
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = BoxType();
		Node.Props = MakeShared<FTestBoxProps>(MoveTemp(InProps));
		Node.Children = RUI::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	inline FTestBoxProps BoxProps(const FString& Label, int32 Value = 0)
	{
		FTestBoxProps P;
		P.SetLabel(Label);
		P.SetValue(Value);
		return P;
	}
} // namespace RuiTest

/** Everything a core test needs: host + reconciler + pump + tree access. */
struct FRuiTestHarness
{
	FRuiMockHost Host;
	TUniquePtr<FRuiReconciler> Reconciler;

	FRuiTestHarness() { Reconciler = MakeUnique<FRuiReconciler>(Host, Host.Root); }

	~FRuiTestHarness()
	{
		if (Reconciler.IsValid())
		{
			Reconciler->Unmount();
		}
	}

	void Mount(FRuiNode Node) { Reconciler->Render(MoveTemp(Node)); }
	void Pump(int32 Frames = 2) { Host.Pump(Frames); }

	FMockNode* RootNode() { return &Host.Root.Get(); }
	FMockNode* ChildAt(int32 i) { return Host.Root->Children.IsValidIndex(i) ? Host.Root->Children[i].Get() : nullptr; }

	/** Null-safe text read: a missing node fails its TestEqual instead of crashing the
	 *  whole automation run (a crash loses the report for every suite after it). */
	FString TextAt(int32 i)
	{
		FMockNode* N = ChildAt(i);
		return N ? N->TextOf() : FString(TEXT("<no node>"));
	}
};
