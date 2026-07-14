// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Core.Portal — the previously untested structural primitive (audit §13: "Portal:
// ZERO tests"). Covers: children re-parent under the TARGET host node (not the render position),
// context flows from the RENDER position, toggling the portal off removes the out-of-tree
// children, and a full unmount leaves the target clean.

#include "Misc/AutomationTest.h"
#include "RuiContextHandle.h"
#include "RuiCoreElements.h"
#include "RuiMockHost.h"
#include "RuiNode.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_PORTAL_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace PortalTest
{
	static TRuiContext<int32> GPortalCtx(0, FName(TEXT("PortalTestCtx")));
	static TSharedPtr<FRuiHostHandle> GTargetRef;
	static TFunction<void(bool)> GSetShow;
	static int32 GSeenCtx = -1;

	// Consumer INSIDE the portal — proves context flows from the render position.
	static FRuiNodeArray PortalContent(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		GSeenCtx = Ctx.UseContext(GPortalCtx);
		return {RUI::TextBlock(TEXT("PORTALED"))};
	}
	RUI_COMPONENT(PortalContent)

	// Renders a target box (handle captured via Ref) and, when toggled, portals content into it.
	static FRuiNodeArray PortalHost(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		auto [bShow, SetShow] = Ctx.UseState<bool>(false);
		GSetShow = SetShow;
		Ctx.ProvideContext(GPortalCtx, 7);

		FTestBoxProps TargetProps = RuiTest::BoxProps(TEXT("target"));
		TargetProps.Ref = [](const FRuiHostHandle& H)
		{
			if (GTargetRef.IsValid())
			{
				*GTargetRef = H;
			}
		};

		TArray<FRuiNode> Children;
		Children.Add(RuiTest::Box(MoveTemp(TargetProps), {}));
		Children.Add(RUI::TextBlock(TEXT("SIBLING")));
		if (bShow && GTargetRef.IsValid() && GTargetRef->IsValid())
		{
			Children.Add(RUI::Portal(*GTargetRef, {RUI::FC(&PortalContent)}));
		}
		return {RuiTest::Box(RuiTest::BoxProps(TEXT("root")), MoveTemp(Children))};
	}
	RUI_COMPONENT(PortalHost)
} // namespace PortalTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiCorePortalTest, "ReactiveUI.Core.Portal", RUI_PORTAL_FLAGS)
bool FRuiCorePortalTest::RunTest(const FString&)
{
	using namespace PortalTest;
	GTargetRef = MakeShared<FRuiHostHandle>();
	GSeenCtx = -1;

	FRuiTestHarness H;
	H.Mount(RUI::FC(&PortalHost));

	FMockNode* Root = H.ChildAt(0);
	if (!TestEqual(TEXT("target + sibling mounted, no portal yet"), Root->Children.Num(), 2))
	{
		return false;
	}
	FMockNode* Target = static_cast<FMockNode*>(GTargetRef->Get());
	if (!TestNotNull(TEXT("target handle captured"), Target))
	{
		return false;
	}
	TestEqual(TEXT("target starts empty"), Target->Children.Num(), 0);

	// Toggle the portal on: content must appear UNDER THE TARGET, not at the render position.
	GSetShow(true);
	H.Pump();
	TestEqual(TEXT("portal child re-parented under the target"), Target->Children.Num(), 1);
	TestEqual(TEXT("portal content rendered"), Target->Children.Num() == 1 ? Target->Children[0]->TextOf() : FString(),
			  FString(TEXT("PORTALED")));
	TestEqual(TEXT("render position did NOT gain the child"), Root->Children.Num(), 2);
	TestEqual(TEXT("context flowed through the portal (render position provider)"), GSeenCtx, 7);

	// Toggle off: the out-of-tree child is torn down.
	GSetShow(false);
	H.Pump();
	TestEqual(TEXT("portal off removes the out-of-tree child"), Target->Children.Num(), 0);

	// On again, then FULL unmount: the target must be left clean (fiber owns the content).
	GSetShow(true);
	H.Pump();
	TestEqual(TEXT("portal back on"), Target->Children.Num(), 1);
	H.Reconciler->Unmount();
	TestEqual(TEXT("full unmount tears the portal content down"), Target->Children.Num(), 0);

	GSetShow = nullptr;
	GTargetRef.Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
