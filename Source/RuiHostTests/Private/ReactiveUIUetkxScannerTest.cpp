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
	auto KindName = [](EUetkxDeclKind K) -> const TCHAR*
	{
		switch (K)
		{
		case EUetkxDeclKind::Component:
			return TEXT("component");
		case EUetkxDeclKind::Hook:
			return TEXT("hook");
		case EUetkxDeclKind::Module:
			return TEXT("module");
		case EUetkxDeclKind::Value:
			return TEXT("value");
		case EUetkxDeclKind::Util:
			return TEXT("util");
		}
		return TEXT("module");
	};
	// ES-modules (M1): a single canonical import stringification used for BOTH the expected (JSON)
	// and actual (FUetkxImportDecl) sides, so rename/namespace/default forms compare exactly.
	// ES COMBINED forms compose the parts with `+`: `default:Def+a|b=>c<-spec` (default + named),
	// `default:Def+*X<-spec` (default + star) — mirrored byte-for-byte by contract.test.ts.
	auto NamedPieces = [](const TArray<FString>& Names, const TArray<FString>& Locals) -> FString
	{
		TArray<FString> Pieces;
		for (int32 n = 0; n < Names.Num(); ++n)
		{
			const FString Local = Locals.IsValidIndex(n) ? Locals[n] : Names[n];
			Pieces.Add(Local == Names[n] ? Names[n] : FString::Printf(TEXT("%s=>%s"), *Names[n], *Local));
		}
		return FString::Join(Pieces, TEXT("|"));
	};
	auto ComposeImportKey = [&NamedPieces](bool bDefault, const FString& Def, bool bNamespace, const FString& Ns,
										   const TArray<FString>& Names, const TArray<FString>& Locals,
										   const FString& Spec) -> FString
	{
		TArray<FString> Parts;
		if (bDefault)
		{
			Parts.Add(FString::Printf(TEXT("default:%s"), *Def));
		}
		if (bNamespace)
		{
			Parts.Add(FString::Printf(TEXT("*%s"), *Ns));
		}
		if (Names.Num() > 0 || Parts.IsEmpty())
		{
			Parts.Add(NamedPieces(Names, Locals));
		}
		return FString::Printf(TEXT("%s<-%s"), *FString::Join(Parts, TEXT("+")), *Spec);
	};
	auto ExpectedImportKey = [&ComposeImportKey](const TSharedPtr<FJsonObject>& O) -> FString
	{
		FString Spec, Ns, Def;
		O->TryGetStringField(TEXT("specifier"), Spec);
		const bool bNs = O->TryGetStringField(TEXT("namespace"), Ns);
		const bool bDef = O->TryGetStringField(TEXT("default"), Def);
		TArray<FString> Names, Locals;
		const TArray<TSharedPtr<FJsonValue>>* NArr = nullptr;
		if (O->TryGetArrayField(TEXT("names"), NArr))
		{
			for (const TSharedPtr<FJsonValue>& NV : *NArr)
			{
				Names.Add(NV->AsString());
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* LArr = nullptr;
		if (O->TryGetArrayField(TEXT("localNames"), LArr))
		{
			for (const TSharedPtr<FJsonValue>& LV : *LArr)
			{
				Locals.Add(LV->AsString());
			}
		}
		else
		{
			Locals = Names;
		}
		return ComposeImportKey(bDef, Def, bNs, Ns, Names, Locals, Spec);
	};
	auto ActualImportKey = [&ComposeImportKey](const FUetkxImportDecl& I) -> FString
	{
		return ComposeImportKey(I.bDefault, I.DefaultAlias, I.bNamespace, I.NamespaceAlias, I.Names, I.LocalNames,
								I.Specifier);
	};
	auto RunFileScan =
		[this, &Total, &Join, &KindName, &ExpectedImportKey, &ActualImportKey](const TSharedPtr<FJsonObject>& Case)
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
				ExpS.Add(ExpectedImportKey(V->AsObject()));
			}
			TArray<FString> ActS;
			for (const FUetkxImportDecl& I : Scan.Imports)
			{
				ActS.Add(ActualImportKey(I));
			}
			TestEqual(Label + TEXT(" imports"), Join(ActS, TEXT(";")), Join(ExpS, TEXT(";")));
		}

		FString ExpDefault;
		if (Expect->TryGetStringField(TEXT("defaultExport"), ExpDefault))
		{
			TestEqual(Label + TEXT(" defaultExport"), Scan.DefaultExportName, ExpDefault);
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
		TArray<FString> CNames, HNames, MNames, VNames, UNames;
		TArray<bool> CExp, HExp, MExp, VExp, UExp;
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
		for (const FUetkxValueDecl& D : Scan.Values)
		{
			VNames.Add(D.Name);
			VExp.Add(D.bExported);
		}
		for (const FUetkxUtilDecl& D : Scan.Utils)
		{
			UNames.Add(D.Name);
			UExp.Add(D.bExported);
		}
		CheckDecls(TEXT("components"), CNames, CExp);
		CheckDecls(TEXT("hooks"), HNames, HExp);
		CheckDecls(TEXT("modules"), MNames, MExp);
		CheckDecls(TEXT("values"), VNames, VExp);
		CheckDecls(TEXT("utils"), UNames, UExp);

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
				FString Nm;
				switch (P.Key)
				{
				case EUetkxDeclKind::Component:
					Nm = Scan.Components[P.Value].Name;
					break;
				case EUetkxDeclKind::Hook:
					Nm = Scan.Hooks[P.Value].Name;
					break;
				case EUetkxDeclKind::Module:
					Nm = Scan.Modules[P.Value].Name;
					break;
				case EUetkxDeclKind::Value:
					Nm = Scan.Values[P.Value].Name;
					break;
				case EUetkxDeclKind::Util:
					Nm = Scan.Utils[P.Value].Name;
					break;
				}
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

		// §4.3 HMR protection pin: a component's ordered hook-call kinds (the signature input).
		if (Expect->TryGetArrayField(TEXT("components"), Exp))
		{
			for (int32 x = 0; x < Exp->Num() && x < Scan.Components.Num(); ++x)
			{
				const TSharedPtr<FJsonObject> O = (*Exp)[x]->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* HooksExp = nullptr;
				if (O.IsValid() && O->TryGetArrayField(TEXT("hookCalls"), HooksExp))
				{
					TArray<FString> ExpH;
					for (const TSharedPtr<FJsonValue>& V : *HooksExp)
					{
						ExpH.Add(V->AsString());
					}
					TestEqual(Label + TEXT(" hookCalls"), Join(Scan.Components[x].HookCalls, TEXT(",")),
							  Join(ExpH, TEXT(",")));
				}
			}
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
