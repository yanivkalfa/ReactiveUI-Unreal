// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Formatter — runs the shared golden corpus (uetkx-formatter-cases.json,
// the same file the lsp-server TS port replays in Phase 5) plus the uetkx.config.json
// walk-up. Every non-fellback case doubles as an idempotence pin: Format(expect) == expect.

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UetkxFormatter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxFormatterTest, "ReactiveUI.Uetkx.Formatter",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxFormatterTest::RunTest(const FString&)
{
	// ── the golden corpus ──────────────────────────────────────────────────────────────────
	{
		const FString CorpusPath = FPaths::Combine(
			FPaths::ProjectDir(), TEXT("ide-extensions/lsp-server/test-fixtures/uetkx-formatter-cases.json"));
		FString Json;
		if (!TestTrue(TEXT("corpus loads"), FFileHelper::LoadFileToString(Json, *CorpusPath)))
		{
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
		if (!TestTrue(TEXT("corpus parses"), Root.IsValid()))
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>& Cases = Root->GetArrayField(TEXT("cases"));
		TestTrue(TEXT("corpus is substantial"), Cases.Num() >= 12);
		for (const TSharedPtr<FJsonValue>& CaseValue : Cases)
		{
			const TSharedPtr<FJsonObject> Case = CaseValue->AsObject();
			const FString Name = Case->GetStringField(TEXT("name"));
			const FString Input = Case->GetStringField(TEXT("input"));
			FUetkxFormatOptions Options;
			const TSharedPtr<FJsonObject>* Opts = nullptr;
			if (Case->TryGetObjectField(TEXT("opts"), Opts))
			{
				double Num = 0.0;
				FString Str;
				bool bFlag = false;
				if ((*Opts)->TryGetNumberField(TEXT("printWidth"), Num))
				{
					Options.PrintWidth = static_cast<int32>(Num);
				}
				if ((*Opts)->TryGetStringField(TEXT("indentStyle"), Str))
				{
					Options.IndentStyle = Str;
				}
				if ((*Opts)->TryGetNumberField(TEXT("indentSize"), Num))
				{
					Options.IndentSize = static_cast<int32>(Num);
				}
				if ((*Opts)->TryGetBoolField(TEXT("singleAttributePerLine"), bFlag))
				{
					Options.bSingleAttributePerLine = bFlag;
				}
				if ((*Opts)->TryGetBoolField(TEXT("insertSpaceBeforeSelfClose"), bFlag))
				{
					Options.bInsertSpaceBeforeSelfClose = bFlag;
				}
			}
			const FUetkxFormatResult Result = FUetkxFormatter::Format(Input, Options);
			bool bExpectFellBack = false;
			Case->TryGetBoolField(TEXT("fellBack"), bExpectFellBack);
			if (bExpectFellBack)
			{
				TestTrue(FString::Printf(TEXT("[%s] fell back"), *Name), Result.bFellBack);
				TestEqual(FString::Printf(TEXT("[%s] verbatim"), *Name), Result.Text, Input);
				continue;
			}
			const FString Expect = Case->GetStringField(TEXT("expect"));
			TestFalse(FString::Printf(TEXT("[%s] no fallback"), *Name), Result.bFellBack);
			if (Result.Text != Expect)
			{
				AddError(FString::Printf(TEXT("[%s] output mismatch.\n--- got ---\n%s\n--- want ---\n%s"), *Name,
										 *Result.Text, *Expect));
				continue;
			}
			TestEqual(FString::Printf(TEXT("[%s] changed flag"), *Name), Result.bChanged, Input != Expect);
			// idempotence: the canonical form is a fixed point
			const FUetkxFormatResult Second = FUetkxFormatter::Format(Result.Text, Options);
			if (Second.Text != Result.Text)
			{
				AddError(FString::Printf(TEXT("[%s] NOT idempotent.\n--- first ---\n%s\n--- second ---\n%s"), *Name,
										 *Result.Text, *Second.Text));
			}
		}
	}

	// ── uetkx.config.json walk-up ──────────────────────────────────────────────────────────
	{
		IFileManager& FM = IFileManager::Get();
		const FString Scratch = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUI"), TEXT("FmtCfgTest"));
		FM.DeleteDirectory(*Scratch, false, true);
		FM.MakeDirectory(*(Scratch / TEXT("Sub/Deeper")), true);
		FFileHelper::SaveStringToFile(TEXT("{\"indentStyle\":\"space\",\"indentSize\":4,\"printWidth\":80}"),
									  *(Scratch / TEXT("uetkx.config.json")));

		const FUetkxFormatOptions FromWalkUp = FUetkxFormatter::ResolveOptions(Scratch / TEXT("Sub/Deeper"));
		TestEqual(TEXT("walk-up finds ancestor config"), FromWalkUp.IndentStyle, FString(TEXT("space")));
		TestEqual(TEXT("indentSize applied"), FromWalkUp.IndentSize, 4);
		TestEqual(TEXT("printWidth applied"), FromWalkUp.PrintWidth, 80);
		TestTrue(TEXT("unspecified keys keep defaults"), FromWalkUp.bInsertSpaceBeforeSelfClose);

		// nearer config wins over the ancestor
		FFileHelper::SaveStringToFile(TEXT("{\"indentStyle\":\"tab\"}"), *(Scratch / TEXT("Sub/uetkx.config.json")));
		TestEqual(TEXT("nearest config wins"),
				  FUetkxFormatter::ResolveOptions(Scratch / TEXT("Sub/Deeper")).IndentStyle, FString(TEXT("tab")));

		FM.DeleteDirectory(*Scratch, false, true);
		TestEqual(TEXT("defaults are tab"), FUetkxFormatOptions().IndentStyle, FString(TEXT("tab")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
