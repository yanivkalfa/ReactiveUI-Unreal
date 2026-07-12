// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Widgets.ExpandableArea — TD-012 tail: the two-named-slot widget. Drives the adapter
// directly (as the slot suite does) to prove role-based child routing (header/body), controlled
// expansion (skip-when-equal SetExpanded), and content removal.

#include "Misc/AutomationTest.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiExpandableArea.h"
#include "RuiTypes.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiExpandableAreaTest, "ReactiveUI.Widgets.ExpandableArea",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiExpandableAreaTest::RunTest(const FString&)
{
	IRuiElementAdapter* Adapter = RUI::Slate::FindAdapter(RUI::Slate::ExpandableAreaType());
	if (!TestNotNull(TEXT("ExpandableArea adapter registered"), Adapter))
	{
		return false;
	}

	// Construct expanded.
	FRuiExpandableAreaProps Props;
	Props.SetbIsExpanded(true);
	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuiExpandableArea& Area = static_cast<SRuiExpandableArea&>(Widget.Get());
	Adapter->ApplyDiff(Area, nullptr, Props);
	TestTrue(TEXT("starts expanded"), Area.IsExpanded());

	// Route two children by role.
	TSharedRef<SWidget> Header = SNew(STextBlock).Text(FText::FromString(TEXT("H")));
	TSharedRef<SWidget> Body = SNew(STextBlock).Text(FText::FromString(TEXT("B")));

	FRuiStyleDict HeaderSlot;
	HeaderSlot.Add(FName(TEXT("slot.role")), FRuiValue(FName(TEXT("header"))));
	FRuiStyleDict BodySlot;
	BodySlot.Add(FName(TEXT("slot.role")), FRuiValue(FName(TEXT("body"))));

	Adapter->InsertChild(Area, Header, 0, &HeaderSlot);
	Adapter->InsertChild(Area, Body, 1, &BodySlot);
	TestTrue(TEXT("header child routed to the header holder"), Area.GetRoleContent(FName(TEXT("header"))) == Header);
	TestTrue(TEXT("body child routed to the body holder"), Area.GetRoleContent(FName(TEXT("body"))) == Body);

	// A child with no role defaults to the body holder.
	{
		FRuiExpandableAreaProps P2;
		P2.SetbIsExpanded(true);
		TSharedRef<SWidget> W2 = Adapter->CreateWidget(P2, nullptr);
		SRuiExpandableArea& A2 = static_cast<SRuiExpandableArea&>(W2.Get());
		TSharedRef<SWidget> Only = SNew(STextBlock).Text(FText::FromString(TEXT("X")));
		Adapter->InsertChild(A2, Only, 0, nullptr);
		TestTrue(TEXT("role-less child defaults to body"), A2.GetRoleContent(FName(TEXT("body"))) == Only);
	}

	// Controlled collapse: a new prop value drives SetExpanded.
	FRuiExpandableAreaProps Collapsed;
	Collapsed.SetbIsExpanded(false);
	Adapter->ApplyDiff(Area, &Props, Collapsed);
	TestFalse(TEXT("controlled prop collapsed it"), Area.IsExpanded());

	// And back open.
	Adapter->ApplyDiff(Area, &Collapsed, Props);
	TestTrue(TEXT("controlled prop re-expanded it"), Area.IsExpanded());

	// Removing the body child clears its holder; the header stays.
	Adapter->RemoveChild(Area, Body);
	TestNull(TEXT("body holder cleared on remove"), Area.GetRoleContent(FName(TEXT("body"))).Get());
	TestTrue(TEXT("header holder untouched"), Area.GetRoleContent(FName(TEXT("header"))) == Header);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
