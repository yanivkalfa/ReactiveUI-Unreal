// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Hmr — the save→screen loop, headless (the watcher-simulation the plan calls
// for: ApplySource IS the injected file-change event). Ports hmr_test.gd semantics:
// hot-LINK a new component, drive interpreted state through a real click, swap a text edit
// with state PRESERVED, swap a hook-shape change with the deliberate reset, parse-error
// isolation (old UI keeps rendering), and the interp↔schema vocabulary parity.

#include "Misc/AutomationTest.h"
#include "RuiHmr.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "UetkxCodegen.h"
#include "UetkxInterpElements.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HmrTest
{
	static bool ContainsText(SWidget& RootWidget, const FString& Needle)
	{
		if (RootWidget.GetType() == FName(TEXT("STextBlock")) &&
			static_cast<STextBlock&>(RootWidget).GetText().ToString().Contains(Needle))
		{
			return true;
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (ContainsText(Children->GetChildAt(i).Get(), Needle))
			{
				return true;
			}
		}
		return false;
	}

	static SButton* FindFirstButton(SWidget& RootWidget)
	{
		if (RootWidget.GetType() == FName(TEXT("SButton")))
		{
			return static_cast<SButton*>(&RootWidget);
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (SButton* Found = FindFirstButton(Children->GetChildAt(i).Get()))
			{
				return Found;
			}
		}
		return nullptr;
	}
} // namespace HmrTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiHmrTest, "ReactiveUI.Hmr",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiHmrTest::RunTest(const FString&)
{
	FRuiHmr::ResetSession();

	// ── the interp vocabulary covers every schema element (single-vocabulary law) ──────────
	{
		FUetkxInterpElements::RegisterBuiltins();
		TSharedPtr<FJsonObject> Schema;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FUetkxCodegen::ExportSchemaJson()), Schema);
		if (TestTrue(TEXT("schema parses"), Schema.IsValid()))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Element :
				 Schema->GetObjectField(TEXT("elements"))->Values)
			{
				TestTrue(FString::Printf(TEXT("interp builds <%s>"), *Element.Key),
						 FUetkxInterpElements::Has(FName(*Element.Key)));
			}
			for (const TSharedPtr<FJsonValue>& Key : Schema->GetArrayField(TEXT("styleKeys")))
			{
				TestTrue(FString::Printf(TEXT("interp styles %s"), *Key->AsString()),
						 FUetkxInterpElements::StyleKeys().Contains(Key->AsString()));
			}
		}
	}

	const FString V1 =
		TEXT("component HotDemo {\n"
			 "\tauto [Count, SetCount] = UseState<int32>(0);\n"
			 "\tTFunction<void(int32)> Set = SetCount;\n"
			 "\tconst int32 Now = Count;\n"
			 "\treturn (\n"
			 "\t\t<VerticalBox>\n"
			 "\t\t\t<TextBlock Text={ FText::FromString(FString::Printf(TEXT(\"Count: %d\"), Count)) } />\n"
			 "\t\t\t<Button OnClicked={ Set(Now + 1) }>+</Button>\n"
			 "\t\t\t@if (Count > 0) {\n"
			 "\t\t\t\treturn ( <TextBlock Text=\"nonzero\" /> );\n"
			 "\t\t\t}\n"
			 "\t\t</VerticalBox>\n"
			 "\t);\n"
			 "}\n");

	// ── hot-LINK: the component did not exist before this session ─────────────────────────
	{
		const FRuiHmrStatus Status = FRuiHmr::ApplySource(V1, TEXT("HotDemo"));
		TestTrue(TEXT("v1 applies clean"), Status.Ok());
		TestEqual(TEXT("v1 reloaded"), Status.Reloaded, 1);
		TestEqual(TEXT("v1 linked (new name)"), Status.Linked, 1);
		TestTrue(TEXT("name now resolvable"), RUI::HasNamedFactory(FName(TEXT("HotDemo"))));
	}

	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("HotDemo"))));
	Root->FlushSync();
	SWidget& RootWidget = Root->GetWidget().Get();
	TestTrue(TEXT("interp renders initial state"), HmrTest::ContainsText(RootWidget, TEXT("Count: 0")));
	TestFalse(TEXT("@if false branch hidden"), HmrTest::ContainsText(RootWidget, TEXT("nonzero")));

	// ── interpreted event: a real click drives interpreted state ──────────────────────────
	{
		SButton* Plus = HmrTest::FindFirstButton(RootWidget);
		if (TestNotNull(TEXT("found the interpreted + button"), Plus))
		{
			Plus->SimulateClick();
			Root->FlushSync();
			TestTrue(TEXT("state advanced through the VM"), HmrTest::ContainsText(RootWidget, TEXT("Count: 1")));
			TestTrue(TEXT("@if now true"), HmrTest::ContainsText(RootWidget, TEXT("nonzero")));
		}
	}

	// ── text edit swap: same hook shape -> state PRESERVED (the headline) ──────────────────
	{
		const FString V2 = V1.Replace(TEXT("Count: %d"), TEXT("Total: %d"));
		const FRuiHmrStatus Status = FRuiHmr::ApplySource(V2, TEXT("HotDemo"));
		TestTrue(TEXT("v2 applies clean"), Status.Ok());
		TestEqual(TEXT("v2 preserves state (no reset)"), Status.Reset, 0);
		TestTrue(TEXT("v2 refreshed a live session"), Status.Refreshed >= 1);
		TestTrue(TEXT("new text, OLD state"), HmrTest::ContainsText(RootWidget, TEXT("Total: 1")));
	}

	// ── hook-shape edit: deliberate state reset, counted ───────────────────────────────────
	{
		const FString V3 = V1.Replace(TEXT("auto [Count, SetCount] = UseState<int32>(0);"),
									  TEXT("auto [Count, SetCount] = UseState<int32>(0);\n\tauto [Extra, SetExtra] = "
										   "UseState<int32>(9);"));
		const FRuiHmrStatus Status = FRuiHmr::ApplySource(V3, TEXT("HotDemo"));
		TestTrue(TEXT("v3 applies clean"), Status.Ok());
		TestEqual(TEXT("v3 resets state (hook shape changed)"), Status.Reset, 1);
		TestTrue(TEXT("state re-initialized"), HmrTest::ContainsText(RootWidget, TEXT("Count: 0")));
	}

	// ── parse error: per-file isolation — the old definition keeps rendering ──────────────
	{
		const FRuiHmrStatus Status =
			FRuiHmr::ApplySource(TEXT("component HotDemo { return ( <Box> ); }"), TEXT("HotDemo"));
		TestFalse(TEXT("broken save reports errors"), Status.Ok());
		TestEqual(TEXT("nothing swapped"), Status.Reloaded, 0);
		Root->FlushSync();
		TestTrue(TEXT("old UI still live"), HmrTest::ContainsText(RootWidget, TEXT("Count: 0")));
	}

	Root->Unmount();
	FRuiHmr::ResetSession();
	TestFalse(TEXT("session reset clears the override"),
			  RUI::FindComponentOverride(FName(TEXT("HotDemo"))).Invoke.IsValid());

	// ── mixed-decl HMR (M9): a component+hook file swaps the component AND notes the rebuild ──────
	{
		FRuiHmr::ResetSession();
		const FString Mixed =
			TEXT("export component MixHmr {\n\treturn ( <Spacer /> );\n}\n") TEXT("export hook UseMixHelper() {\n}\n");
		const FRuiHmrStatus Status = FRuiHmr::ApplySource(Mixed, TEXT("MixHmr"));
		TestTrue(TEXT("mixed file ok"), Status.Ok());
		TestEqual(TEXT("mixed file swaps its component"), Status.Reloaded, 1);
		TestTrue(
			TEXT("mixed file notes the hook/module rebuild"),
			Status.Notes.ContainsByPredicate([](const FString& N) { return N.Contains(TEXT("hook/module change")); }));
	}

	// ── hook-only file names its import blast radius ──────────────────────────────────────────
	{
		FRuiHmr::ResetSession();
		const FRuiHmrStatus Status = FRuiHmr::ApplySource(TEXT("export hook UseShared() {\n}\n"), TEXT("Shared"),
														  {TEXT("ScreenA"), TEXT("ScreenB")});
		TestEqual(TEXT("hook-only file swaps nothing"), Status.Reloaded, 0);
		TestTrue(
			TEXT("rebuild note lists the importers"),
			Status.Notes.ContainsByPredicate([](const FString& N) { return N.Contains(TEXT("ScreenA, ScreenB")); }));
	}

	FRuiHmr::ResetSession();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
