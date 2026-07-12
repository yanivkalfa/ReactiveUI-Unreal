// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Editor.Preview — TD-006: the in-editor .uetkx read-only preview core (FUetkxPreview).
// Verifies the scan -> interp -> mount pipeline: a valid component mounts a live widget; a source with
// no component or a parse error yields a placeholder + diagnostics (never a crash).

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
	// A compiled child component whose render would be observable IF it ran live in the editor.
	static FRuiNodeArray PreviewChildComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		return {RUI::TextBlock(FString(TEXT("CHILD_RAN_LIVE")))};
	}
	RUI_COMPONENT(PreviewChildComp)

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

	// ── a valid component mounts a live preview ─────────────────────────────────────────────────
	{
		const FString Source = TEXT("component Hello {\n\treturn (\n\t\t<VerticalBox></VerticalBox>\n\t);\n}\n");
		TSharedRef<FUetkxPreview> Preview = FUetkxPreview::FromSource(Source, TEXT("Hello"));
		TestTrue(TEXT("valid component mounts"), Preview->IsValid());
		TestEqual(TEXT("previewed the right component"), Preview->GetComponentName(), FString(TEXT("Hello")));
		TestTrue(TEXT("produced a real preview widget"), Preview->GetWidget() != SNullWidget::NullWidget);
	}

	// ── choosing a named component among several ────────────────────────────────────────────────
	{
		const FString Source = TEXT("component First {\n\treturn ( <VerticalBox></VerticalBox> );\n}\n")
			TEXT("component Second {\n\treturn ( <HorizontalBox></HorizontalBox> );\n}\n");
		TSharedRef<FUetkxPreview> Preview = FUetkxPreview::FromSource(Source, TEXT("Multi"), FName(TEXT("Second")));
		TestTrue(TEXT("named component mounts"), Preview->IsValid());
		TestEqual(TEXT("picked the named component"), Preview->GetComponentName(), FString(TEXT("Second")));

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
		// GetWidget never crashes — it returns the placeholder.
		Preview->GetWidget();
	}

	// ── a parse error is surfaced, not swallowed ────────────────────────────────────────────────
	{
		const FString Source = TEXT("component Broken {\n\treturn ( <VerticalBox>\n"); // unterminated
		TSharedRef<FUetkxPreview> Preview = FUetkxPreview::FromSource(Source, TEXT("Broken"));
		TestTrue(TEXT("a parse error produced at least one message"), Preview->GetMessages().Num() > 0);
		Preview->GetWidget(); // no crash
	}

	// ── P2 (bughunt): a referenced COMPILED child component is STUBBED, not run live at edit time ──
	{
		RUI::RegisterNamedFactory(FName(TEXT("BugfixPreviewChild")),
								  []() { return RUI::FC(&EditorPreviewTest::PreviewChildComp); });
		const FString Source = TEXT("component Host {\n\treturn ( <BugfixPreviewChild></BugfixPreviewChild> );\n}\n");
		TSharedRef<FUetkxPreview> Preview = FUetkxPreview::FromSource(Source, TEXT("Host"));
		TestTrue(TEXT("P2: host mounts"), Preview->IsValid());
		TArray<FString> Texts;
		CollectTexts(Preview->GetWidget().Get(), Texts);
		bool bRanLive = false, bStubbed = false;
		for (const FString& T : Texts)
		{
			bRanLive |= T.Contains(TEXT("CHILD_RAN_LIVE"));
			bStubbed |= T.Contains(TEXT("<BugfixPreviewChild/>"));
		}
		TestFalse(TEXT("P2: the compiled child did NOT run live in the editor"), bRanLive);
		TestTrue(TEXT("P2: the child rendered as an inert placeholder"), bStubbed);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
