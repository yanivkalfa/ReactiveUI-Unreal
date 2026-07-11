// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.JsxScan — pins the position-gated whitelist: markup after boundary tokens
// is found; `a < b` comparisons never are; unbalanced markup terminates the scan with
// End == -1 (the caller diagnoses — family T1.2).

#include "Misc/AutomationTest.h"
#include "UetkxJsxScan.h"
#include "UetkxLexer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace JsxScanTest
{
	static TArray<FUetkxMarkupRange> Scan(const FString& Expr)
	{
		const TArray<int32> Src = FUetkxLexer::ToCodePoints(Expr);
		return FUetkxJsxScan::FindMarkupRanges(Src, 0, Src.Num());
	}
} // namespace JsxScanTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxJsxScanTest, "ReactiveUI.Uetkx.JsxScan",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxJsxScanTest::RunTest(const FString&)
{
	using namespace JsxScanTest;

	// comparisons are NEVER markup
	TestEqual(TEXT("a < b is a comparison"), Scan(TEXT("A < B && C > D")).Num(), 0);
	TestEqual(TEXT("i < N loop bound"), Scan(TEXT("i < Names.Num()")).Num(), 0);

	// markup at expression start
	{
		TArray<FUetkxMarkupRange> R = Scan(TEXT("<Panel />"));
		TestTrue(TEXT("start markup found"), R.Num() == 1 && R[0].Start == 0 && R[0].End == 9);
	}

	// short-circuit && carries the op for the emit-time desugar
	{
		TArray<FUetkxMarkupRange> R = Scan(TEXT("bOpen && <Panel />"));
		TestTrue(TEXT("&& markup found with op"), R.Num() == 1 && R[0].Op == TEXT("&&") && R[0].Start == 9);
	}
	{
		TArray<FUetkxMarkupRange> R = Scan(TEXT("Cached || <Fallback />"));
		TestTrue(TEXT("|| markup found with op"), R.Num() == 1 && R[0].Op == TEXT("||"));
	}

	// ternary: both branches found, `::` never a boundary
	{
		TArray<FUetkxMarkupRange> R = Scan(TEXT("bOn ? <A /> : <B />"));
		TestEqual(TEXT("ternary finds both branches"), R.Num(), 2);
	}
	TestEqual(TEXT(":: scope operator is not a boundary"), Scan(TEXT("FMath::Max(A, B)")).Num(), 0);

	// return inside a lambda body
	{
		TArray<FUetkxMarkupRange> R = Scan(TEXT("Items.Map([](auto It) { return <Row Item={It} />; })"));
		TestTrue(TEXT("lambda return markup found"), R.Num() == 1 && R[0].Op.IsEmpty());
	}

	// nested elements + attr holes with C++ strings never confuse the end
	{
		const FString Expr = TEXT("bX && <Outer A={ FString(TEXT(\"/> not an end\")) }><Inner /></Outer>");
		TArray<FUetkxMarkupRange> R = Scan(Expr);
		TestTrue(TEXT("nested end found past the decoy"), R.Num() == 1 && R[0].End == Expr.Len());
	}

	// comparison after a value even with a tag-like ident: `Depth <Panel` — boundary gate
	TestEqual(TEXT("no boundary -> less-than, even before ident"), Scan(TEXT("Depth <Panel")).Num(), 0);

	// unbalanced markup after a boundary terminates with End == -1 (caller diagnoses)
	{
		TArray<FUetkxMarkupRange> R = Scan(TEXT("bOpen && <Panel"));
		TestTrue(TEXT("unbalanced reported"), R.Num() == 1 && R[0].End == -1);
	}

	// simple = is a boundary; == is not
	{
		TestTrue(TEXT("assignment boundary"), Scan(TEXT("Node = <T />")).Num() == 1);
		TestEqual(TEXT("== is not a boundary"), Scan(TEXT("A == B")).Num(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
