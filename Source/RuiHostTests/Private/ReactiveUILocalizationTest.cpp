// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Localization (MASTER_PLAN Phase 7 item 2): the culture-change → root re-render mechanism
// (RuiCultureSync) and a tripwire on the committed gather output (the RuiDemo localization
// target must keep gathering the markup's NSLOCTEXT entries from the committed *.uetkx.inl —
// masks live in Config/Localization/RuiDemo_Gather.ini; regenerate via
//   UnrealEditor-Cmd <proj> -run=GatherText -config=Config/Localization/RuiDemo_Gather.ini).

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiCultureSync.h"
#include "RuiMockHost.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace LocTestState
{
	static int32 RenderCount = 0;
} // namespace LocTestState

static FRuiNodeArray LocProbeComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	++LocTestState::RenderCount;
	// The FText below is what a compiled .uetkx literal becomes (self-namespaced NSLOCTEXT) —
	// lazy, so it re-resolves on culture change; the re-render heals anything baked around it.
	return {RUI::TextBlock(NSLOCTEXT("Uetkx.LocProbe", "LocProbe_0", "Localized probe"))};
}
RUI_COMPONENT(LocProbeComp)

// ─────────────────────────────────────────────────────────────────────────────────────────
// The refresh mechanism: every live root re-renders when the handler fires.
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiLocRefreshTest, "ReactiveUI.Loc.CultureRefreshMechanism", RUI_TEST_FLAGS)
bool FRuiLocRefreshTest::RunTest(const FString&)
{
	LocTestState::RenderCount = 0;
	FRuiTestHarness H;

	AddInfo(TEXT("[loc] 1/2 mount probe"));
	H.Mount(RUI::FC(&LocProbeComp));
	TestEqual(TEXT("initial render count == 1"), LocTestState::RenderCount, 1);

	AddInfo(TEXT("[loc] 2/2 refresh handler -> one coalesced re-render"));
	RUI::RefreshAllRootsForCultureChange();
	H.Pump();
	TestEqual(TEXT("refresh re-rendered the live root"), LocTestState::RenderCount, 2);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// The wiring: a REAL culture change (text-revision bump) reaches the reconciler.
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiLocCultureChangeTest, "ReactiveUI.Loc.CultureChangeRerenders", RUI_TEST_FLAGS)
bool FRuiLocCultureChangeTest::RunTest(const FString&)
{
	LocTestState::RenderCount = 0;
	// Unit suites do not run StartupModule (house rule) — subscribe here; Register is idempotent
	// so this coexists with the module's own registration when the whole battery runs.
	RUI::RegisterCultureSync();

	FRuiTestHarness H;
	H.Mount(RUI::FC(&LocProbeComp));
	TestEqual(TEXT("initial render count == 1"), LocTestState::RenderCount, 1);

	FInternationalization& I18N = FInternationalization::Get();
	const FString OriginalCulture = I18N.GetCurrentCulture()->GetName();
	const FString OtherCulture = OriginalCulture.StartsWith(TEXT("fr")) ? TEXT("de") : TEXT("fr");

	AddInfo(FString::Printf(TEXT("[loc] culture switch %s -> %s"), *OriginalCulture, *OtherCulture));
	if (!I18N.SetCurrentCulture(OtherCulture))
	{
		AddWarning(
			FString::Printf(TEXT("culture '%s' unavailable on this machine — wiring not exercised"), *OtherCulture));
		return true;
	}
	H.Pump();
	const int32 AfterSwitch = LocTestState::RenderCount;

	// Restore FIRST so a failing assertion can't leave the process in a foreign culture.
	I18N.SetCurrentCulture(OriginalCulture);
	H.Pump();

	TestTrue(TEXT("culture change re-rendered the live root"), AfterSwitch >= 2);
	TestTrue(TEXT("restore re-rendered again"), LocTestState::RenderCount > AfterSwitch);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Gather output tripwire: the committed manifest carries the markup's Uetkx.* namespaces.
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiLocGatherManifestTest, "ReactiveUI.Loc.GatherManifest", RUI_TEST_FLAGS)
bool FRuiLocGatherManifestTest::RunTest(const FString&)
{
	const FString ManifestPath =
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization/RuiDemo/RuiDemo.manifest"));
	FString Manifest;
	if (!TestTrue(TEXT("RuiDemo.manifest exists (run the GatherText commandlet — see this file's header)"),
				  FFileHelper::LoadFileToString(Manifest, *ManifestPath)))
	{
		return false;
	}
	// The .uetkx codegen self-namespaces every literal as "Uetkx.<Basename>"; the manifest nests
	// the dotted form as a "Uetkx" parent namespace with per-file subnamespaces.
	TestTrue(TEXT("manifest gathered markup text (the 'Uetkx' namespace is present)"),
			 Manifest.Contains(TEXT("\"Uetkx\"")));
	// And the plugin's own runtime strings gather too (palette category, design-time labels).
	TestTrue(TEXT("manifest gathered plugin strings (the 'ReactiveUI' namespace is present)"),
			 Manifest.Contains(TEXT("\"ReactiveUI\"")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
