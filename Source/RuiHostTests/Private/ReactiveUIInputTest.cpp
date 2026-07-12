// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Slate.Shortcut — TD-004 keyboard-shortcut half: the FRuiShortcut chord matcher and
// the UseShortcut hook lifecycle (register on mount → fire on the chord → unregister on unmount).

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiInput.h"
#include "RuiNode.h"
#include "RuiRoot.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ShortcutTest
{
	static int32 GFires = 0;

	static FRuiNodeArray ShortcutComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		RUI::Slate::FRuiShortcut Chord;
		Chord.Key = EKeys::S;
		Chord.bCtrl = true;
		RUI::Slate::UseShortcut(Ctx, Chord, []() { ++GFires; });
		return {RUI::TextBlock(FString(TEXT("shortcut host")))};
	}
	RUI_COMPONENT(ShortcutComp)

	static FKeyEvent MakeKey(FKey Key, bool bCtrl)
	{
		const FModifierKeysState Mods(false, false, bCtrl, false, false, false, false, false, false);
		return FKeyEvent(Key, Mods, 0, /*bIsRepeat*/ false, 0, 0);
	}
} // namespace ShortcutTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiShortcutTest, "ReactiveUI.Slate.Shortcut",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiShortcutTest::RunTest(const FString&)
{
	using namespace RUI::Slate;

	// ── the chord matcher (pure) ──────────────────────────────────────────────────────────────
	FRuiShortcut CtrlS;
	CtrlS.Key = EKeys::S;
	CtrlS.bCtrl = true;
	TestTrue(TEXT("Ctrl+S matches the Ctrl+S chord"), CtrlS.Matches(ShortcutTest::MakeKey(EKeys::S, true)));
	TestFalse(TEXT("S alone does NOT match Ctrl+S"), CtrlS.Matches(ShortcutTest::MakeKey(EKeys::S, false)));
	TestFalse(TEXT("Ctrl+A does NOT match Ctrl+S"), CtrlS.Matches(ShortcutTest::MakeKey(EKeys::A, true)));

	FRuiShortcut ShiftS;
	ShiftS.Key = EKeys::S;
	ShiftS.bShift = true;
	TestNotEqual(TEXT("different chords have different dep keys"), CtrlS.DepKey(), ShiftS.DepKey());

	// ── the hook: register on mount, fire on the chord, unregister on unmount ──────────────────
	if (FSlateApplication::IsInitialized())
	{
		ShortcutTest::GFires = 0;
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&ShortcutTest::ShortcutComp));
		Root->FlushSync(); // commit + run effects (registers the pre-processor)

		FSlateApplication::Get().ProcessKeyDownEvent(ShortcutTest::MakeKey(EKeys::S, true));
		TestEqual(TEXT("Ctrl+S fired the shortcut while mounted"), ShortcutTest::GFires, 1);

		FSlateApplication::Get().ProcessKeyDownEvent(ShortcutTest::MakeKey(EKeys::A, true));
		TestEqual(TEXT("a non-matching chord does not fire"), ShortcutTest::GFires, 1);

		Root->Unmount(); // cleanup runs → unregisters the pre-processor
		FSlateApplication::Get().ProcessKeyDownEvent(ShortcutTest::MakeKey(EKeys::S, true));
		TestEqual(TEXT("the shortcut no longer fires after unmount"), ShortcutTest::GFires, 1);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
