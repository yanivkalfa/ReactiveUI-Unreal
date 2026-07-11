// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Schema — pins the exported vocabulary JSON (what the LSP's markup
// intelligence consumes, Phase 5) and the New Component template (which must compile through
// the real pipeline — the template lives next to the grammar for exactly this reason).

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UetkxCodegen.h"
#include "UetkxFileActions.h"

#if WITH_DEV_AUTOMATION_TESTS

static bool HasString(const TArray<TSharedPtr<FJsonValue>>& Arr, const TCHAR* Value)
{
	for (const TSharedPtr<FJsonValue>& Entry : Arr)
	{
		if (Entry->AsString() == Value)
		{
			return true;
		}
	}
	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxSchemaTest, "ReactiveUI.Uetkx.Schema",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxSchemaTest::RunTest(const FString&)
{
	// ── the exported vocabulary ────────────────────────────────────────────────────────────
	{
		TSharedPtr<FJsonObject> Schema;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FUetkxCodegen::ExportSchemaJson()), Schema);
		if (!TestTrue(TEXT("schema parses"), Schema.IsValid()))
		{
			return false;
		}
		TestEqual(TEXT("schema v1"), (int32)Schema->GetNumberField(TEXT("v")), 1);

		const TSharedPtr<FJsonObject> Elements = Schema->GetObjectField(TEXT("elements"));
		TestEqual(TEXT("15 host tags"), Elements->Values.Num(), 15);
		const TSharedPtr<FJsonObject> Button = Elements->GetObjectField(TEXT("Button"));
		TestEqual(TEXT("Button factory"), Button->GetStringField(TEXT("factory")), FString(TEXT("RUI::Slate::Button")));
		TestTrue(TEXT("Button takes children"), Button->GetBoolField(TEXT("children")));
		TestEqual(TEXT("OnClicked is an event"),
				  Button->GetObjectField(TEXT("attrs"))->GetStringField(TEXT("OnClicked")), FString(TEXT("event")));
		TestEqual(
			TEXT("TextBlock Text is text"),
			Elements->GetObjectField(TEXT("TextBlock"))->GetObjectField(TEXT("attrs"))->GetStringField(TEXT("Text")),
			FString(TEXT("text")));
		TestEqual(
			TEXT("DrawFn is expression-only"),
			Elements->GetObjectField(TEXT("RuiCanvas"))->GetObjectField(TEXT("attrs"))->GetStringField(TEXT("DrawFn")),
			FString(TEXT("expr")));
		TestFalse(TEXT("Spacer is childless"),
				  Elements->GetObjectField(TEXT("Spacer"))->GetBoolField(TEXT("children")));

		const TArray<TSharedPtr<FJsonValue>>& StyleKeys = Schema->GetArrayField(TEXT("styleKeys"));
		TestEqual(TEXT("12 style keys"), StyleKeys.Num(), 12);
		TestTrue(TEXT("RenderOpacity styled"), HasString(StyleKeys, TEXT("RenderOpacity")));
		TestTrue(TEXT("Font.Size styled"), HasString(StyleKeys, TEXT("Font.Size")));

		TestEqual(TEXT("slot prefix"), Schema->GetStringField(TEXT("slotPrefix")), FString(TEXT("Slot.")));
		TestEqual(TEXT("5 canonical slot keys"), Schema->GetArrayField(TEXT("slotKeys")).Num(), 5);

		const TArray<TSharedPtr<FJsonValue>>& Hooks = Schema->GetArrayField(TEXT("hooks"));
		TestEqual(TEXT("all hooks exported"), Hooks.Num(), FUetkxFileScan::HookNames().Num());
		TestTrue(TEXT("UseState exported"), HasString(Hooks, TEXT("UseState")));
		TestTrue(TEXT("ProvideContext exported"), HasString(Hooks, TEXT("ProvideContext")));
	}

	// ── the New Component template compiles through the real pipeline ─────────────────────
	{
		const FString Scratch = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUI"), TEXT("FileActionsTest"));
		IFileManager::Get().DeleteDirectory(*Scratch, false, true);
		IFileManager::Get().MakeDirectory(*Scratch, true);

		const FString Created = FUetkxFileActions::CreateComponentFile(Scratch, TEXT("MyPanel"));
		TestTrue(TEXT("template created"), !Created.IsEmpty() && IFileManager::Get().FileExists(*Created));
		FString Source;
		FFileHelper::LoadFileToString(Source, *Created);
		TestTrue(TEXT("declares the component"), Source.Contains(TEXT("component MyPanel {")));
		const FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, TEXT("MyPanel"));
		TestTrue(TEXT("template compiles clean"), Out.bOk && Out.Diags.Num() == 0);

		TestTrue(TEXT("never overwrites"), FUetkxFileActions::CreateComponentFile(Scratch, TEXT("MyPanel")).IsEmpty());
		TestTrue(TEXT("rejects lowercase (UETKX2100 grammar)"),
				 FUetkxFileActions::CreateComponentFile(Scratch, TEXT("myPanel")).IsEmpty());
		TestTrue(TEXT("rejects non-ident"),
				 FUetkxFileActions::CreateComponentFile(Scratch, TEXT("My-Panel")).IsEmpty());

		IFileManager::Get().DeleteDirectory(*Scratch, false, true);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
