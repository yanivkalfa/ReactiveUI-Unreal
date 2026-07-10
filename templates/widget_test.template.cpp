// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ── component-pipeline station 5 TEMPLATE ────────────────────────────────────────────────
// Per-widget automation test. Lives in Source/RuiHostTests/Private/Widgets/. Replace
// __TAG__/__SLATE_CLASS__; keep the four sections — they are the family contract for what
// EVERY widget must prove. Demonstrate red→green once before calling it done (break the
// adapter, watch this fail, restore) — a test that never failed asserts nothing.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRui__TAG__Test, "ReactiveUI.Widgets.__TAG__",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRui__TAG__Test::RunTest(const FString& Parameters)
{
	// Print per-section markers so hangs name their culprit (house rule).

	AddInfo(TEXT("[__TAG__] 1/4 create"));
	// Mount RUI::__TAG__() with initial props on the test root; assert the live widget is
	// S__SLATE_CLASS__ and every initial prop landed (read back via the widget's getters).

	AddInfo(TEXT("[__TAG__] 2/4 diff"));
	// Re-render with changed props; assert setters applied AND the widget pointer is
	// IDENTICAL (no reconstruction for settable props — style tweaks must not eat widgets).

	AddInfo(TEXT("[__TAG__] 3/4 removal semantics"));
	// Remove a STYLE prop → asserts default restored. Remove a PLAIN prop → asserts value
	// UNCHANGED (family semantic: removed plain props don't reset). Remove an EVENT prop →
	// proxy inner cleared.

	AddInfo(TEXT("[__TAG__] 4/4 reconstruct mask"));
	// If the widget has construct-only props: change one → assert the widget was REPLACED
	// (new pointer) and the rest of the props re-applied. If none: assert mask == 0.

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
