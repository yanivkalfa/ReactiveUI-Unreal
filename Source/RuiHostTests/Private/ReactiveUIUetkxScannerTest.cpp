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
#include "UetkxFileScan.h"
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

	// ── fileScan sections: run FUetkxFileScan::Scan over a whole source, assert the parsed shape.
	// Family-core import/export/mixed-decl grammar (`fileScan`) + per-leg hook casing (`fileScanLeg`)
	// — the same cases run by lsp-server's node --test (the mirror contract, A4).
	auto Join = [](const TArray<FString>& A, const TCHAR* Sep) { return FString::Join(A, Sep); };
	auto KindName = [](EUetkxDeclKind K)
	{
		return K == EUetkxDeclKind::Component ? TEXT("component")
			   : K == EUetkxDeclKind::Hook	  ? TEXT("hook")
											  : TEXT("module");
	};
	auto RunFileScan = [this, &Total, &Join, &KindName](const TSharedPtr<FJsonObject>& Case)
	{
		const FString Name = Case->GetStringField(TEXT("name"));
		const FString Input = Case->GetStringField(TEXT("input"));
		FString Basename;
		Case->TryGetStringField(TEXT("basename"), Basename);
		const TSharedPtr<FJsonObject> Expect = Case->GetObjectField(TEXT("expect"));
		const FUetkxFileScanResult Scan = FUetkxFileScan::Scan(Input, Basename);
		const FString Label = FString::Printf(TEXT("[fileScan] %s"), *Name);
		++Total;

		const TArray<TSharedPtr<FJsonValue>>* Exp = nullptr;
		if (Expect->TryGetArrayField(TEXT("imports"), Exp))
		{
			TArray<FString> ExpS;
			for (const TSharedPtr<FJsonValue>& V : *Exp)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				TArray<FString> Ns;
				for (const TSharedPtr<FJsonValue>& NV : O->GetArrayField(TEXT("names")))
				{
					Ns.Add(NV->AsString());
				}
				ExpS.Add(FString::Printf(TEXT("%s<-%s"), *Join(Ns, TEXT("|")), *O->GetStringField(TEXT("specifier"))));
			}
			TArray<FString> ActS;
			for (const FUetkxImportDecl& I : Scan.Imports)
			{
				ActS.Add(FString::Printf(TEXT("%s<-%s"), *Join(I.Names, TEXT("|")), *I.Specifier));
			}
			TestEqual(Label + TEXT(" imports"), Join(ActS, TEXT(";")), Join(ExpS, TEXT(";")));
		}

		auto CheckDecls = [&](const TCHAR* Field, const TArray<FString>& ActNames, const TArray<bool>& ActExported)
		{
			const TArray<TSharedPtr<FJsonValue>>* E = nullptr;
			if (!Expect->TryGetArrayField(Field, E))
			{
				return;
			}
			TArray<FString> ExpS;
			for (const TSharedPtr<FJsonValue>& V : *E)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				bool Exported = false;
				O->TryGetBoolField(TEXT("exported"), Exported);
				ExpS.Add(
					FString::Printf(TEXT("%s:%s"), *O->GetStringField(TEXT("name")), Exported ? TEXT("E") : TEXT("P")));
			}
			TArray<FString> ActS;
			for (int32 x = 0; x < ActNames.Num(); ++x)
			{
				ActS.Add(FString::Printf(TEXT("%s:%s"), *ActNames[x], ActExported[x] ? TEXT("E") : TEXT("P")));
			}
			TestEqual(Label + TEXT(" ") + Field, Join(ActS, TEXT(",")), Join(ExpS, TEXT(",")));
		};
		TArray<FString> CNames, HNames, MNames;
		TArray<bool> CExp, HExp, MExp;
		for (const FUetkxComponentDecl& D : Scan.Components)
		{
			CNames.Add(D.Name);
			CExp.Add(D.bExported);
		}
		for (const FUetkxHookDecl& D : Scan.Hooks)
		{
			HNames.Add(D.Name);
			HExp.Add(D.bExported);
		}
		for (const FUetkxModuleDecl& D : Scan.Modules)
		{
			MNames.Add(D.Name);
			MExp.Add(D.bExported);
		}
		CheckDecls(TEXT("components"), CNames, CExp);
		CheckDecls(TEXT("hooks"), HNames, HExp);
		CheckDecls(TEXT("modules"), MNames, MExp);

		if (Expect->TryGetArrayField(TEXT("order"), Exp))
		{
			TArray<FString> ExpS;
			for (const TSharedPtr<FJsonValue>& V : *Exp)
			{
				ExpS.Add(V->AsString());
			}
			TArray<FString> ActS;
			for (const TPair<EUetkxDeclKind, int32>& P : Scan.Order)
			{
				const FString Nm = P.Key == EUetkxDeclKind::Component ? Scan.Components[P.Value].Name
								   : P.Key == EUetkxDeclKind::Hook	  ? Scan.Hooks[P.Value].Name
																	  : Scan.Modules[P.Value].Name;
				ActS.Add(FString::Printf(TEXT("%s:%s"), KindName(P.Key), *Nm));
			}
			TestEqual(Label + TEXT(" order"), Join(ActS, TEXT(",")), Join(ExpS, TEXT(",")));
		}

		if (Expect->TryGetArrayField(TEXT("diags"), Exp))
		{
			TArray<FString> ExpS;
			for (const TSharedPtr<FJsonValue>& V : *Exp)
			{
				ExpS.Add(V->AsString());
			}
			ExpS.Sort();
			TArray<FString> ActS;
			for (const FUetkxDiag& D : Scan.Diags)
			{
				ActS.Add(D.Code);
			}
			ActS.Sort();
			TestEqual(Label + TEXT(" diags"), Join(ActS, TEXT(",")), Join(ExpS, TEXT(",")));
		}

		bool NoError = false;
		if (Expect->TryGetBoolField(TEXT("noError"), NoError) && NoError)
		{
			TestFalse(Label + TEXT(" no error"), Scan.HasError());
		}
	};
	for (const FString& Section : {FString(TEXT("fileScan")), FString(TEXT("fileScanLeg"))})
	{
		const TArray<TSharedPtr<FJsonValue>>* Cases = nullptr;
		if (TestTrue(FString::Printf(TEXT("section '%s' exists"), *Section),
					 RootObject->TryGetArrayField(Section, Cases)))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Cases)
			{
				RunFileScan(Value->AsObject());
			}
		}
	}

	AddInfo(FString::Printf(TEXT("scanner corpus: %d cases"), Total));
	TestTrue(TEXT("corpus is not accidentally empty"), Total >= 50);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
