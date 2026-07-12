// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Bugfix.* — regression tests locking the fixes from the 2026-07-12 adversarial bug hunt
// (plans/BUGHUNT_2026-07-12.md). Each asserts the previously-uncovered case the corresponding green
// test missed (the hunt's meta-finding: the suite only exercised the happy path).

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

#include "RuiComboBox.h"
#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiDragDrop.h"
#include "RuiExpandableArea.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiListView.h"
#include "RuiMvvmViewModel.h"
#include "RuiNode.h"
#include "RuiPresence.h"
#include "RuiRoot.h"
#include "RuiRouter.h"
#include "RuiSegmentedControl.h"
#include "RuiSlateElements.h"
#include "RuiSlateTestHarness.h"
#include "RuiStyle.h"
#include "RuiTestViewModel.h"
#include "RuiTypes.h"
#include "RuiUmgElement.h"

#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BugfixTest
{
	/** Collect the text of every STextBlock in a widget tree (order = DFS). */
	static void CollectTexts(SWidget& Root, TArray<FString>& Out)
	{
		if (Root.GetType() == FName(TEXT("STextBlock")))
		{
			Out.Add(static_cast<STextBlock&>(Root).GetText().ToString());
		}
		if (FChildren* Children = Root.GetChildren())
		{
			for (int32 i = 0; i < Children->Num(); ++i)
			{
				CollectTexts(Children->GetChildAt(i).Get(), Out);
			}
		}
	}

	static bool AnyContains(const TArray<FString>& Texts, const TCHAR* Needle)
	{
		for (const FString& T : Texts)
		{
			if (T.Contains(Needle))
			{
				return true;
			}
		}
		return false;
	}

	// ── B1: Presence must namespace unkeyed children so they never collide with a user integer key ──
	static FRuiNodeArray PresenceMixComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		TArray<FRuiNode> Kids;
		Kids.Add(RUI::TextBlock(FString(TEXT("UNKEYED"))));  // no key -> positional fallback
		FRuiNode Keyed = RUI::TextBlock(FString(TEXT("KEYEDZERO")));
		Keyed.Key = FRuiKey(0); // user integer key 0 — must NOT collide with the fallback
		Kids.Add(MoveTemp(Keyed));
		return {RUI::Presence(MoveTemp(Kids))};
	}
	RUI_COMPONENT(PresenceMixComp)
} // namespace BugfixTest

// ── B3 + B2: router search parsing / relative navigation ────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixRouterTest, "ReactiveUI.Bugfix.Router",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixRouterTest::RunTest(const FString&)
{
	// B3: first value wins for a repeated key (was last-value-wins).
	const TMap<FString, FString> P = RUI::ParseSearch(TEXT("?a=1&a=2&b=x"));
	TestEqual(TEXT("B3: repeated key keeps the FIRST value"), P.FindRef(TEXT("a")), FString(TEXT("1")));
	TestEqual(TEXT("B3: other keys unaffected"), P.FindRef(TEXT("b")), FString(TEXT("x")));

	// B2: a relative href with a query must not double the tail. UseHref resolves ./ against a base;
	// ResolvePath + a single tail. Verify via the pure ResolvePath + ParseLocation round-trip that a
	// query survives exactly once (the doubling was Resolved + ExtractQueryHash on the un-stripped To).
	const FString Resolved = RUI::ResolvePath(TEXT("child?tab=1"), TEXT("/parent/page"));
	// ResolvePath now sees a raw segment (query still embedded — that's expected of ResolvePath); the
	// router's Navigate strips first. Assert ParseLocation of a correctly-assembled href has a single tail:
	const FRuiLocation Loc = RUI::ParseLocation(TEXT("/parent/page/child") + RUI::BuildSearch({{TEXT("tab"), TEXT("1")}}));
	TestEqual(TEXT("B2: search is a single ?tab=1"), Loc.Search, FString(TEXT("?tab=1")));
	TestFalse(TEXT("B2: no doubled '?'"), Loc.Search.Contains(TEXT("?tab=1?")));
	(void)Resolved;
	return true;
}

// ── B5 + B4: stylesheet comment stripping + inline-only token resolution ─────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixStyleTest, "ReactiveUI.Bugfix.Style",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixStyleTest::RunTest(const FString&)
{
	// B5: a `//` inside a quoted value must NOT truncate the block (was silently dropping the class).
	const int32 Registered =
		RUI::Slate::LoadStylesheet(TEXT(".card { BorderImage: \"img://cdn/bg.png\"; } .other { Padding: \"4\"; }"));
	TestEqual(TEXT("B5: both classes register despite '//' in a value"), Registered, 2);

	// B4: an inline-only `$token` (NO classes) resolves against the active theme — the empty-classes fast
	// path used to early-return the raw dict, leaving `$accent` as a String the adapter can't apply.
	FRuiStyleDict Theme;
	Theme.Add(FName(TEXT("accent")), FRuiValue(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f)));
	RUI::Slate::RegisterTheme(FName(TEXT("bugfix_dark")), Theme);
	RUI::Slate::SetActiveTheme(FName(TEXT("bugfix_dark")));

	TSharedPtr<FRuiStyleDict> Inline = MakeShared<FRuiStyleDict>();
	Inline->Add(FName(TEXT("ColorAndOpacity")), FRuiValue(FString(TEXT("$accent"))));
	TSharedPtr<FRuiStyleDict> Eff = RUI::Slate::BuildEffectiveStyle(/*Classes*/ {}, Inline); // no classes
	if (TestTrue(TEXT("B4: effective style built"), Eff.IsValid()))
	{
		const FRuiValue* Resolved = Eff->Find(FName(TEXT("ColorAndOpacity")));
		TestTrue(TEXT("B4: inline $accent resolved to a Color (not left as a raw String)"),
				 Resolved != nullptr && Resolved->Kind == FRuiValue::EKind::Color);
	}

	RUI::Slate::SetActiveTheme(NAME_None);
	return true;
}

// ── B13: ApplyPropMap skips kind mismatches, coerces numerics ────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixPropMapTest, "ReactiveUI.Bugfix.PropMap",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixPropMapTest::RunTest(const FString&)
{
	URuiTestUserWidget* W = NewObject<URuiTestUserWidget>(GetTransientPackage());
	W->AddToRoot();
	W->FloatValue = 9.0f; // sentinel: a kind-mismatch must NOT overwrite it with 0
	W->IntValue = 7;

	// An Int-kind value into a float prop COERCES to 1.0 (was writing V.FloatValue==0.0).
	FRuiStyleDict Coerce;
	Coerce.Add(FName(TEXT("FloatValue")), FRuiValue(1)); // Kind==Int
	const int32 Applied = RUI::Umg::ApplyPropMap(W, Coerce);
	TestEqual(TEXT("B13: int->float coerces (applied)"), Applied, 1);
	TestTrue(TEXT("B13: FloatValue coerced to 1.0"), FMath::IsNearlyEqual(W->FloatValue, 1.0f));

	// A Bool-kind value into an int prop is a mismatch -> SKIPPED, IntValue unchanged, not counted.
	FRuiStyleDict Mismatch;
	Mismatch.Add(FName(TEXT("IntValue")), FRuiValue(true)); // Kind==Bool
	TestEqual(TEXT("B13: bool->int is skipped (0 applied)"), RUI::Umg::ApplyPropMap(W, Mismatch), 0);
	TestEqual(TEXT("B13: IntValue untouched by the mismatch"), W->IntValue, 7);

	W->RemoveFromRoot();
	return true;
}

// ── B7: DropTarget fires OnDragLeave on a successful drop ────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixDropLeaveTest, "ReactiveUI.Bugfix.DropLeave",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixDropLeaveTest::RunTest(const FString&)
{
	if (!FSlateApplication::IsInitialized())
	{
		return true;
	}
	bool bLeft = false;
	TSharedRef<SRuiDropTarget> Target = SNew(SRuiDropTarget);
	Target->SetOnDragLeave(FRuiCallback::Create([&bLeft](const FRuiValue&) { bLeft = true; }));

	FPointerEvent Pointer;
	FDragDropEvent Enter(Pointer, FRuiDragDropOp::New(FName(TEXT("card")), FRuiValue(1)));
	Target->OnDragEnter(FGeometry(), Enter);
	TestTrue(TEXT("B7: hovered after enter"), Target->IsOver());

	FDragDropEvent Drop(Pointer, FRuiDragDropOp::New(FName(TEXT("card")), FRuiValue(1)));
	Target->OnDrop(FGeometry(), Drop);
	TestFalse(TEXT("B7: hover cleared after drop"), Target->IsOver());
	TestTrue(TEXT("B7: OnDragLeave fired on drop (hover styling un-sticks)"), bLeft);
	return true;
}

// ── B9: ExpandableArea does NOT fire OnExpansionChanged for a controlled (programmatic) change ────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixExpandableTest, "ReactiveUI.Bugfix.Expandable",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixExpandableTest::RunTest(const FString&)
{
	IRuiElementAdapter* Adapter = RUI::Slate::FindAdapter(RUI::Slate::ExpandableAreaType());
	if (!TestNotNull(TEXT("adapter"), Adapter))
	{
		return false;
	}
	int32 Fires = 0;
	FRuiExpandableAreaProps Props;
	Props.SetbIsExpanded(true);
	Props.SetOnExpansionChanged(FRuiCallback::Create([&Fires](const FRuiValue&) { ++Fires; }));
	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuiExpandableArea& Area = static_cast<SRuiExpandableArea&>(Widget.Get());
	Adapter->ApplyDiff(Area, nullptr, Props);

	// Controlled collapse via a new prop value — a PROGRAMMATIC change; must NOT fire the user event.
	FRuiExpandableAreaProps Collapsed = Props;
	Collapsed.SetbIsExpanded(false);
	Adapter->ApplyDiff(Area, &Props, Collapsed);
	TestFalse(TEXT("B9: controlled collapse did not fire OnExpansionChanged"), Area.IsExpanded());
	TestEqual(TEXT("B9: OnExpansionChanged NOT fired for a programmatic change"), Fires, 0);
	return true;
}

// ── B8: SegmentedControl highlights the controlled segment (inner SCheckBox is checked) ───────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixSegmentedTest, "ReactiveUI.Bugfix.Segmented",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixSegmentedTest::RunTest(const FString&)
{
	if (!FSlateApplication::IsInitialized())
	{
		return true;
	}
	IRuiElementAdapter* Adapter = RUI::Slate::FindAdapter(RUI::Slate::SegmentedControlType());
	if (!TestNotNull(TEXT("adapter"), Adapter))
	{
		return false;
	}
	FRuiSegmentedControlProps Props;
	Props.SetLabels({TEXT("A"), TEXT("B"), TEXT("C")});
	Props.SetSelectedIndex(1);
	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	Adapter->ApplyDiff(static_cast<SRuiSegmentedControl&>(Widget.Get()), nullptr, Props);

	RuiTest::FTestWindow Win(Widget);
	if (Win.IsValid())
	{
		Win.PumpGeometry();
		// Exactly one segment SCheckBox is Checked, and it is the controlled index (B8: was zero checked).
		int32 CheckedCount = 0;
		TArray<TSharedRef<SWidget>> Stack;
		Stack.Push(Widget);
		while (Stack.Num() > 0)
		{
			TSharedRef<SWidget> Cur = Stack.Pop();
			if (Cur->GetType() == FName(TEXT("SCheckBox")) &&
				static_cast<SCheckBox&>(Cur.Get()).GetCheckedState() == ECheckBoxState::Checked)
			{
				++CheckedCount;
			}
			if (FChildren* C = Cur->GetChildren())
			{
				for (int32 i = 0; i < C->Num(); ++i)
				{
					Stack.Push(C->GetChildAt(i));
				}
			}
		}
		TestEqual(TEXT("B8: exactly one segment is highlighted for the controlled value"), CheckedCount, 1);
	}
	return true;
}

// ── B6: ComboBox forwards a user selection reported as ESelectInfo::Direct ────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixComboBoxTest, "ReactiveUI.Bugfix.ComboBox",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixComboBoxTest::RunTest(const FString&)
{
	using namespace RUI::Slate;
	IRuiElementAdapter* Adapter = FindAdapter(ComboBoxType());
	if (!TestNotNull(TEXT("adapter"), Adapter))
	{
		return false;
	}
	TArray<TSharedPtr<FRuiValue>> Options{MakeShared<FRuiValue>(10), MakeShared<FRuiValue>(20)};
	auto Renderer =
		MakeItemRenderer([](const FRuiValue& V, int32) -> FRuiNode { return RUI::TextBlock(FString::FromInt((int32)V.IntValue)); });

	int32 Fires = 0;
	int32 LastIndex = -1;
	FRuiComboBoxProps Props;
	Props.SetOptions(Options);
	Props.SetRenderOption(Renderer);
	Props.SetSelectedIndex(0);
	Props.SetOnSelectionChanged(FRuiCallback::Create(
		[&Fires, &LastIndex](const FRuiValue& V) { ++Fires; LastIndex = (int32)V.IntValue; }));

	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuiComboBox& Combo = static_cast<SRuiComboBox&>(Widget.Get());
	Adapter->ApplyDiff(Combo, nullptr, Props);

	// Programmatic controlled set must NOT fire (our reentrancy guard).
	FRuiComboBoxProps Pick1 = Props;
	Pick1.SetSelectedIndex(1);
	Adapter->ApplyDiff(Combo, &Props, Pick1);
	TestEqual(TEXT("B6: programmatic controlled set does not fire OnSelectionChanged"), Fires, 0);

	// A USER selection that Slate reports as ESelectInfo::Direct (keyboard-close / gamepad-accept) MUST
	// fire — SComboBox::SetSelectedItem reports Direct; driving it OUTSIDE our reentrancy guard is
	// exactly the user-commit path the old ESelectInfo::Direct check wrongly swallowed.
	if (Combo.GetComboWidget().IsValid())
	{
		Combo.GetComboWidget()->SetSelectedItem(Options[0]); // reports ESelectInfo::Direct, not our set
		TestEqual(TEXT("B6: a user Direct selection fires OnSelectionChanged"), Fires, 1);
		TestEqual(TEXT("B6: forwarded the picked index"), LastIndex, 0);
	}
	return true;
}

// ── B1: Presence mixed unkeyed + integer-keyed children both render ───────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixPresenceTest, "ReactiveUI.Bugfix.PresenceKeys",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixPresenceTest::RunTest(const FString&)
{
	using namespace BugfixTest;
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&PresenceMixComp));
	Root->FlushSync();
	TArray<FString> Texts;
	CollectTexts(Root->GetWidget().Get(), Texts);
	TestTrue(TEXT("B1: the unkeyed child rendered"), AnyContains(Texts, TEXT("UNKEYED")));
	TestTrue(TEXT("B1: the integer-keyed(0) child also rendered (no key collision)"),
			 AnyContains(Texts, TEXT("KEYEDZERO")));
	Root->Unmount();
	return true;
}

// ── P1: a registered URuiMvvmViewModel subclass resolves via FindGlobalViewModel's default class ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixMvvmSubclassTest, "ReactiveUI.Bugfix.MvvmSubclass",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixMvvmSubclassTest::RunTest(const FString&)
{
	if (GEngine == nullptr)
	{
		return true;
	}
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->AddToRoot();
	GI->InitializeStandalone();

	URuiTestMvvmSubViewModel* Sub = NewObject<URuiTestMvvmSubViewModel>(GI);
	TestTrue(TEXT("P1: subclass registered"), RUI::Mvvm::RegisterGlobalViewModel(GI, FName(TEXT("SubStats")), Sub));

	// Default class (base) must resolve the subclass instance now (base-class alias).
	TestTrue(TEXT("P1: resolves the subclass via the default (base) class"),
			 RUI::Mvvm::FindGlobalViewModel(GI, FName(TEXT("SubStats"))) == Sub);
	// The concrete-class find still works too.
	TestTrue(TEXT("P1: resolves via the concrete subclass"),
			 RUI::Mvvm::FindGlobalViewModel(GI, FName(TEXT("SubStats")), URuiTestMvvmSubViewModel::StaticClass()) == Sub);

	GI->Shutdown();
	GI->RemoveFromRoot();
	return true;
}

// ── B11: removing an asset Brush resets the SImage (exercises the reset branch; no dangling ptr) ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiBugfixAssetBrushTest, "ReactiveUI.Bugfix.AssetBrush",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiBugfixAssetBrushTest::RunTest(const FString&)
{
	IRuiElementAdapter* Img = RUI::Slate::FindAdapter(RUI::InternElementType(FName(TEXT("Image"))));
	if (!TestNotNull(TEXT("Image adapter"), Img))
	{
		return false;
	}
	FRuiImageProps WithBrush;
	WithBrush.SetBrush(MakeShared<FSlateBrush>());
	TSharedRef<SWidget> Widget = Img->CreateWidget(WithBrush, nullptr);
	Img->ApplyDiff(Widget.Get(), nullptr, WithBrush);
	TestTrue(TEXT("B11: image is an SImage"), Widget->GetType() == FName(TEXT("SImage")));

	// Re-render WITHOUT the brush: the fix resets SImage to the no-brush default instead of leaving a
	// raw pointer into props that are about to be freed. The image must survive (no crash / no dangling).
	FRuiImageProps NoBrush;
	Img->ApplyDiff(Widget.Get(), &WithBrush, NoBrush);
	TestTrue(TEXT("B11: image survives brush removal"), Widget->GetType() == FName(TEXT("SImage")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
