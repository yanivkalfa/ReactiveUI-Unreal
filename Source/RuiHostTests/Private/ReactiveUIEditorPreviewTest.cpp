// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Editor.Preview — TD-006: the in-editor .uetkx read-only preview core (FUetkxPreview).
// HMR v2 (D-HMR-8) deleted the interpreter, so the preview no longer approximates a component — it
// scans a source for its component name and mounts the COMPILED component by name (the one RUICompile
// /HMR registered). This suite verifies: a compiled component mounts its REAL live widget; an
// uncompiled component reports "not compiled yet" (never a wrong approximation); no-component / parse
// errors yield a placeholder + diagnostics, never a crash.

#include "Misc/AutomationTest.h"
#include "RuiContext.h"
#include "RuiCoreElements.h"
#include "RuiNode.h"
#include "UetkxPreview.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace EditorPreviewTest
{
	// A compiled component: the preview mounts THIS (the real thing), so its render is observable.
	static FRuiNodeArray PreviewHello(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		return {RUI::TextBlock(FString(TEXT("HELLO_RAN_LIVE")))};
	}

	// A compiled child referenced by another component — the preview mounts the real tree, so a
	// compiled child now DOES run live (the opposite of the deleted interpreter's inert stub).
	static FRuiNodeArray PreviewChildComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		return {RUI::TextBlock(FString(TEXT("CHILD_RAN_LIVE")))};
	}
	static FRuiNodeArray PreviewHost(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		return {RUI::FC(&PreviewChildComp)};
	}

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

	static bool AnyText(SWidget& Root, const TCHAR* Needle)
	{
		TArray<FString> Texts;
		CollectTexts(Root, Texts);
		for (const FString& T : Texts)
		{
			if (T.Contains(Needle))
			{
				return true;
			}
		}
		return false;
	}

	static bool AnyMessageContains(const TArray<FString>& Messages, const TCHAR* Needle)
	{
		for (const FString& Message : Messages)
		{
			if (Message.Contains(Needle))
			{
				return true;
			}
		}
		return false;
	}
} // namespace EditorPreviewTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiEditorPreviewTest, "ReactiveUI.Editor.Preview",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiEditorPreviewTest::RunTest(const FString&)
{
	using namespace EditorPreviewTest;

	// ── a compiled component mounts its REAL live widget ────────────────────────────────────────
	{
		RUI::RegisterNamedFactory(FName(TEXT("PreviewHello")), []() { return RUI::FC(&PreviewHello); });
		const FString Source = TEXT("component PreviewHello {\n\treturn (\n\t\t<VerticalBox></VerticalBox>\n\t);\n}\n");
		TSharedRef<FUetkxPreview> Preview = FUetkxPreview::FromSource(Source, TEXT("PreviewHello"));
		TestTrue(TEXT("compiled component mounts"), Preview->IsValid());
		TestEqual(TEXT("previewed the right component"), Preview->GetComponentName(), FString(TEXT("PreviewHello")));
		TestTrue(TEXT("mounted the real compiled render"), AnyText(Preview->GetWidget().Get(), TEXT("HELLO_RAN_LIVE")));
	}

	// ── an uncompiled component reports "not compiled yet", it does NOT approximate ──────────────
	{
		const FString Source = TEXT("component NeverCompiled {\n\treturn ( <VerticalBox></VerticalBox> );\n}\n");
		TSharedRef<FUetkxPreview> Preview = FUetkxPreview::FromSource(Source, TEXT("NeverCompiled"));
		TestFalse(TEXT("uncompiled component does not mount"), Preview->IsValid());
		TestTrue(TEXT("explains it is not compiled yet"),
				 AnyMessageContains(Preview->GetMessages(), TEXT("not compiled")));
		Preview->GetWidget(); // placeholder, no crash
	}

	// ── choosing a named component among several (the named one is compiled) ─────────────────────
	{
		const FString Source = TEXT("component First {\n\treturn ( <VerticalBox></VerticalBox> );\n}\n")
			TEXT("component PreviewHello {\n\treturn ( <HorizontalBox></HorizontalBox> );\n}\n");
		TSharedRef<FUetkxPreview> Preview =
			FUetkxPreview::FromSource(Source, TEXT("Multi"), FName(TEXT("PreviewHello")));
		TestTrue(TEXT("named compiled component mounts"), Preview->IsValid());
		TestEqual(TEXT("picked the named component"), Preview->GetComponentName(), FString(TEXT("PreviewHello")));

		TSharedRef<FUetkxPreview> Missing = FUetkxPreview::FromSource(Source, TEXT("Multi"), FName(TEXT("Nope")));
		TestFalse(TEXT("a missing component name does not mount"), Missing->IsValid());
		TestTrue(TEXT("reports the missing component"), AnyMessageContains(Missing->GetMessages(), TEXT("not found")));
	}

	// ── a source with no component yields a placeholder + a message ─────────────────────────────
	{
		const FString Source = TEXT("module Styles {\n\t// no component here\n}\n");
		TSharedRef<FUetkxPreview> Preview = FUetkxPreview::FromSource(Source, TEXT("NoComp"));
		TestFalse(TEXT("no component -> not mounted"), Preview->IsValid());
		TestTrue(TEXT("explains there is no component"),
				 AnyMessageContains(Preview->GetMessages(), TEXT("no component")));
		Preview->GetWidget(); // placeholder, never crashes
	}

	// ── a parse error is surfaced, not swallowed ────────────────────────────────────────────────
	{
		const FString Source = TEXT("component Broken {\n\treturn ( <VerticalBox>\n"); // unterminated
		TSharedRef<FUetkxPreview> Preview = FUetkxPreview::FromSource(Source, TEXT("Broken"));
		TestTrue(TEXT("a parse error produced at least one message"), Preview->GetMessages().Num() > 0);
		Preview->GetWidget(); // no crash
	}

	// ── the preview mounts the REAL compiled tree: a referenced compiled child RUNS live ─────────
	//    (HMR v2: the preview is the true component, not the interpreter's inert stub — effects run.)
	{
		RUI::RegisterNamedFactory(FName(TEXT("PreviewHostComp")), []() { return RUI::FC(&PreviewHost); });
		const FString Source = TEXT("component PreviewHostComp {\n\treturn ( <PreviewChildComp/> );\n}\n");
		TSharedRef<FUetkxPreview> Preview = FUetkxPreview::FromSource(Source, TEXT("PreviewHostComp"));
		TestTrue(TEXT("host mounts"), Preview->IsValid());
		TestTrue(TEXT("the referenced compiled child ran live in the real tree"),
				 AnyText(Preview->GetWidget().Get(), TEXT("CHILD_RAN_LIVE")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
