// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Widgets.Canvas — the absolute-placement panel (SCanvas wrapper, the Doom-demo
// framebuffer container): slot.position/slot.size land on the slot (expr AND string literal
// forms — SLOT-1), a slot-prop UPDATE mutates the LIVE FSlot in place (the hot path for
// per-frame quad movement), removal resets to the slot defaults, and paint order is child
// order (declaration order preserved through the adapter).

#include "Misc/AutomationTest.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiTypes.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SCanvas.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCanvasPanelTest, "ReactiveUI.Widgets.Canvas",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiCanvasPanelTest::RunTest(const FString&)
{
	IRuiElementAdapter* Adapter = RUI::Slate::FindAdapter(RUI::InternElementType(FName(TEXT("Canvas"))));
	if (!TestNotNull(TEXT("Canvas adapter registered"), Adapter))
	{
		return false;
	}

	FRuiEmptyProps Props;
	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	TestEqual(TEXT("creates an SCanvas"), Widget->GetType(), FName(TEXT("SCanvas")));
	SCanvas& Canvas = static_cast<SCanvas&>(Widget.Get());

	// ── insert: expr-form (Vector2 kind) and literal-form ("x,y") both resolve ────────────
	TSharedRef<SWidget> A = SNew(SSpacer);
	FRuiStyleDict SlotA;
	SlotA.Add(FName(TEXT("slot.position")), FRuiValue(FVector2D(20.0, 30.0)));
	SlotA.Add(FName(TEXT("slot.size")), FRuiValue(FVector2D(160.0, 90.0)));
	Adapter->InsertChild(Canvas, A, -1, &SlotA);

	TSharedRef<SWidget> B = SNew(SSpacer);
	FRuiStyleDict SlotB;
	SlotB.Add(FName(TEXT("slot.position")), FRuiValue(FString(TEXT("5,7"))));
	SlotB.Add(FName(TEXT("slot.size")), FRuiValue(FString(TEXT("64,64"))));
	Adapter->InsertChild(Canvas, B, -1, &SlotB);

	TestEqual(TEXT("two children"), Canvas.GetChildren()->Num(), 2);
	const SCanvas::FSlot& SA = static_cast<const SCanvas::FSlot&>(Canvas.GetChildren()->GetSlotAt(0));
	const SCanvas::FSlot& SB = static_cast<const SCanvas::FSlot&>(Canvas.GetChildren()->GetSlotAt(1));
	TestEqual(TEXT("A position (expr form)"), SA.GetPosition(), FVector2D(20.0, 30.0));
	TestEqual(TEXT("A size (expr form)"), SA.GetSize(), FVector2D(160.0, 90.0));
	TestEqual(TEXT("B position (string literal form — SLOT-1)"), SB.GetPosition(), FVector2D(5.0, 7.0));
	TestEqual(TEXT("B size (string literal form)"), SB.GetSize(), FVector2D(64.0, 64.0));
	TestTrue(TEXT("declaration order preserved (paint order = child order)"),
			 &Canvas.GetChildren()->GetChildAt(0).Get() == &A.Get());

	// ── the hot path: slot-prop UPDATE mutates the LIVE slot in place ──────────────────────
	const void* SlotAddr = &SA;
	FRuiStyleDict Moved;
	Moved.Add(FName(TEXT("slot.position")), FRuiValue(FVector2D(21.0, 30.0)));
	Moved.Add(FName(TEXT("slot.size")), FRuiValue(FVector2D(160.0, 92.0)));
	Adapter->UpdateChildSlotProps(Canvas, A, &Moved);

	const SCanvas::FSlot& SA2 = static_cast<const SCanvas::FSlot&>(Canvas.GetChildren()->GetSlotAt(0));
	TestEqual(TEXT("position updated"), SA2.GetPosition(), FVector2D(21.0, 30.0));
	TestEqual(TEXT("size updated"), SA2.GetSize(), FVector2D(160.0, 92.0));
	TestEqual(TEXT("SAME FSlot object — mutated in place, not reinserted"), (const void*)&SA2, SlotAddr);
	TestEqual(TEXT("still two children"), Canvas.GetChildren()->Num(), 2);

	// ── removal resets to the slot defaults (Position 0,0 / Size 1,1) ─────────────────────
	FRuiStyleDict Empty;
	Adapter->UpdateChildSlotProps(Canvas, A, &Empty);
	const SCanvas::FSlot& SA3 = static_cast<const SCanvas::FSlot&>(Canvas.GetChildren()->GetSlotAt(0));
	TestEqual(TEXT("removed position resets to 0,0"), SA3.GetPosition(), FVector2D::ZeroVector);
	TestEqual(TEXT("removed size resets to 1,1"), SA3.GetSize(), FVector2D(1.0, 1.0));

	// ── remove child ───────────────────────────────────────────────────────────────────────
	Adapter->RemoveChild(Canvas, B);
	TestEqual(TEXT("removed B"), Canvas.GetChildren()->Num(), 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
