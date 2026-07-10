// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Scanner — runs the D-22 contract corpus (uetkx-scanner-cases.json)
// against FUetkxLexer. The SAME file is run by lsp-server's node --test: the corpus is the
// lockstep mechanism between the C++ compiler and the TypeScript IDE scanner (the family's
// scanner-cases.json pattern). All offsets are Unicode code points (D-18).

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UetkxLexer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UetkxScannerTest
{
	static int32 RunSection(const FString& Section, const FString& Input, int32 At)
	{
		const TArray<int32> Src = FUetkxLexer::ToCodePoints(Input);
		if (Section == TEXT("skipNoncode"))
		{
			return FUetkxLexer::SkipNoncode(Src, At);
		}
		if (Section == TEXT("skipNoncodeMarkup"))
		{
			return FUetkxLexer::SkipNoncodeMarkup(Src, At);
		}
		if (Section == TEXT("findMatching"))
		{
			return FUetkxLexer::FindMatching(Src, At);
		}
		if (Section == TEXT("findMatchingMarkup"))
		{
			return FUetkxLexer::FindMatchingMarkup(Src, At);
		}
		return TNumericLimits<int32>::Min();
	}
} // namespace UetkxScannerTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxScannerTest, "ReactiveUI.Uetkx.Scanner",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxScannerTest::RunTest(const FString&)
{
	const FString CorpusPath =
		FPaths::Combine(FPaths::ProjectDir(), TEXT("ide-extensions/lsp-server/test-fixtures/uetkx-scanner-cases.json"));
	FString Json;
	if (!TestTrue(TEXT("corpus file loads"), FFileHelper::LoadFileToString(Json, *CorpusPath)))
	{
		return false;
	}
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!TestTrue(TEXT("corpus parses as JSON"),
				  FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid()))
	{
		return false;
	}

	int32 Total = 0;
	auto RunCase = [this, &Total](const FString& Section, const TSharedPtr<FJsonObject>& Case)
	{
		const FString Name = Case->GetStringField(TEXT("name"));
		const FString Input = Case->GetStringField(TEXT("input"));
		const int32 At = static_cast<int32>(Case->GetNumberField(TEXT("at")));
		const int32 Expect = static_cast<int32>(Case->GetNumberField(TEXT("expect")));
		const int32 Got = UetkxScannerTest::RunSection(Section, Input, At);
		++Total;
		TestEqual(FString::Printf(TEXT("[%s] %s"), *Section, *Name), Got, Expect);
	};

	for (const FString& Section : {FString(TEXT("skipNoncode")), FString(TEXT("findMatching")),
								   FString(TEXT("skipNoncodeMarkup")), FString(TEXT("findMatchingMarkup"))})
	{
		const TArray<TSharedPtr<FJsonValue>>* Cases = nullptr;
		if (TestTrue(FString::Printf(TEXT("section '%s' exists"), *Section),
					 RootObject->TryGetArrayField(Section, Cases)))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Cases)
			{
				RunCase(Section, Value->AsObject());
			}
		}
	}

	// nonBmp cases carry their target section per-case.
	const TArray<TSharedPtr<FJsonValue>>* NonBmp = nullptr;
	if (TestTrue(TEXT("section 'nonBmp' exists"), RootObject->TryGetArrayField(TEXT("nonBmp"), NonBmp)))
	{
		for (const TSharedPtr<FJsonValue>& Value : *NonBmp)
		{
			const TSharedPtr<FJsonObject> Case = Value->AsObject();
			RunCase(Case->GetStringField(TEXT("section")), Case);
		}
	}

	AddInfo(FString::Printf(TEXT("scanner corpus: %d cases"), Total));
	TestTrue(TEXT("corpus is not accidentally empty"), Total >= 50);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
