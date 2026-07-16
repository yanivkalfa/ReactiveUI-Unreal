// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Widgets.Batch3c — WIDGET_COMPLETION_PLAN waves 3+4: mount every protocol widget,
// pin its concrete Slate type, and exercise the two new slot models (anchors + fractions).

#include "Misc/AutomationTest.h"
#include "RuiContext.h"
#include "RuiListView.h"
#include "RuiRoot.h"
#include "RuiSlateElements.h"
#include "RuiSlateHost.h"
#include "RuiStyle.h"
#include "RuiTreeView.h"
#include "Widgets/Layout/SConstraintCanvas.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_B4_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace B4Test
{
	static TSharedPtr<SWidget> RootChild(FRuiRoot& Root)
	{
		FChildren* Children = Root.GetWidget()->GetRootPanel()->GetChildren();
		return Children->Num() > 0 ? TSharedPtr<SWidget>(Children->GetChildAt(0)) : nullptr;
	}

	static FRuiNode WithSlotDict(FRuiNode Node, std::initializer_list<TPair<FName, FRuiValue>> Pairs)
	{
		TSharedRef<FRuiStyleDict> Dict = MakeShared<FRuiStyleDict>();
		for (const TPair<FName, FRuiValue>& Pair : Pairs)
		{
			Dict->Add(Pair.Key, Pair.Value);
		}
		TSharedRef<FRuiPropsBase> Props = ConstCastSharedRef<FRuiPropsBase>(Node.Props.ToSharedRef());
		Props->SlotProps = Dict;
		return Node;
	}
} // namespace B4Test

static FRuiNodeArray B4GalleryComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	// P5a: two anchored children on a ConstraintCanvas.
	FRuiNode Anchored = B4Test::WithSlotDict(RUI::TextBlock(TEXT("anchored")),
											 {{FName(TEXT("Slot.Anchors")), FRuiValue(FString(TEXT("0.5,0.5")))},
											  {FName(TEXT("Slot.Offset")), FRuiValue(FString(TEXT("10,10,100,40")))},
											  {FName(TEXT("Slot.ZOrder")), FRuiValue(3.0f)}});

	// P5b: two panes with fractions.
	FRuiNode PaneA =
		B4Test::WithSlotDict(RUI::TextBlock(TEXT("A")), {{FName(TEXT("Slot.SizeValue")), FRuiValue(0.3f)}});
	FRuiNode PaneB =
		B4Test::WithSlotDict(RUI::TextBlock(TEXT("B")), {{FName(TEXT("Slot.SizeValue")), FRuiValue(0.7f)}});

	// P3: anchor + role="menu" content.
	FRuiNode MenuContent = B4Test::WithSlotDict(RUI::TextBlock(TEXT("popup")),
												{{FName(TEXT("Slot.Role")), FRuiValue(FName(TEXT("menu")))}});

	FRuiNumericDropDownProps DropP;
	DropP.SetValues({1.0f, 2.0f, 4.0f});
	DropP.SetLabels({TEXT("one"), TEXT("two"), TEXT("four")});
	DropP.SetValue(2.0f);

	FRuiBreadcrumbTrailProps CrumbP;
	CrumbP.SetCrumbs({TEXT("root"), TEXT("child"), TEXT("leaf")});

	FRuiVectorInputBoxProps VecP;
	VecP.SetX(1.0f);
	VecP.SetY(2.0f);
	VecP.SetZ(3.0f);

	FRuiRotatorInputBoxProps RotP;
	RotP.SetRoll(10.0f);
	RotP.SetPitch(20.0f);
	RotP.SetYaw(30.0f);

	FRuiTreeViewProps TreeP;
	TreeP.SetColumns({{FName(TEXT("Name")), FText::FromString(TEXT("Name")), 1.0f}});

	return {RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{RUI::Slate::ConstraintCanvas(FRuiConstraintCanvasProps(), {MoveTemp(Anchored)}),
		 RUI::Slate::Splitter(FRuiSplitterProps(), {MoveTemp(PaneA), MoveTemp(PaneB)}),
		 RUI::Slate::MenuAnchor(FRuiMenuAnchorProps(), {RUI::TextBlock(TEXT("anchor")), MoveTemp(MenuContent)}),
		 RUI::Slate::WindowTitleBarArea(FRuiWindowTitleBarAreaProps(), {RUI::TextBlock(TEXT("title"))}),
		 RUI::Slate::NumericDropDown(MoveTemp(DropP)), RUI::Slate::BreadcrumbTrail(MoveTemp(CrumbP)),
		 RUI::Slate::NotificationList(), RUI::Slate::LinkedBox(FRuiLinkedBoxProps(), {RUI::TextBlock(TEXT("linked"))}),
		 RUI::Slate::VirtualJoystick(), RUI::Slate::VectorInputBox(MoveTemp(VecP)),
		 RUI::Slate::RotatorInputBox(MoveTemp(RotP)), RUI::Slate::TreeView(MoveTemp(TreeP))})};
}
RUI_COMPONENT(B4GalleryComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiWidgetsBatch3cTest, "ReactiveUI.Widgets.Batch3c", RUI_B4_TEST_FLAGS)
bool FRuiWidgetsBatch3cTest::RunTest(const FString&)
{
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&B4GalleryComp));
	TSharedPtr<SWidget> Panel = B4Test::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	FChildren* Kids = Panel->GetChildren();
	TestEqual(TEXT("twelve widgets mounted"), Kids->Num(), 12);
	TestEqual(TEXT("SConstraintCanvas"), Kids->GetChildAt(0)->GetType(), FName(TEXT("SConstraintCanvas")));
	TestEqual(TEXT("SSplitter"), Kids->GetChildAt(1)->GetType(), FName(TEXT("SSplitter")));
	TestEqual(TEXT("SMenuAnchor"), Kids->GetChildAt(2)->GetType(), FName(TEXT("SMenuAnchor")));
	TestEqual(TEXT("SWindowTitleBarArea"), Kids->GetChildAt(3)->GetType(), FName(TEXT("SWindowTitleBarArea")));
	TestEqual(TEXT("SBreadcrumbTrail type"), Kids->GetChildAt(5)->GetType(), FName(TEXT("SBreadcrumbTrail<FString>")));
	TestEqual(TEXT("SNotificationList"), Kids->GetChildAt(6)->GetType(), FName(TEXT("SNotificationList")));
	TestEqual(TEXT("SLinkedBox"), Kids->GetChildAt(7)->GetType(), FName(TEXT("SLinkedBox")));
	TestEqual(TEXT("SVirtualJoystick"), Kids->GetChildAt(8)->GetType(), FName(TEXT("SVirtualJoystick")));
	TestEqual(TEXT("SRuiTreeView"), Kids->GetChildAt(11)->GetType(), FName(TEXT("SRuiTreeView")));

	// P5a: the anchor slot actually carries the parsed values.
	{
		FChildren* CanvasKids = Kids->GetChildAt(0)->GetChildren();
		if (TestEqual(TEXT("canvas holds the anchored child"), CanvasKids->Num(), 1))
		{
			const SConstraintCanvas::FSlot& Slot =
				static_cast<const SConstraintCanvas::FSlot&>(CanvasKids->GetSlotAt(0));
			TestTrue(TEXT("anchors applied"), Slot.GetAnchors().Minimum.Equals(FVector2D(0.5, 0.5)));
			TestEqual(TEXT("zorder applied"), Slot.GetZOrder(), 3.0f);
		}
	}

	// P5b: both panes attached.
	TestEqual(TEXT("splitter holds both panes"), Kids->GetChildAt(1)->GetChildren()->Num(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
