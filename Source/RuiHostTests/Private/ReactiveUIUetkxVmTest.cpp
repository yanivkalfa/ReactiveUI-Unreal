// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Vm — the D-20 restricted expression VM: literals, operators, precedence,
// ternary, member access, whitelist calls (incl. the Printf subset), truthiness, and the
// REJECTION set (everything outside the subset must fail PARSE with a reason, never
// half-evaluate).

#include "Misc/AutomationTest.h"
#include "UetkxExprVm.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxVmTest, "ReactiveUI.Uetkx.Vm",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxVmTest::RunTest(const FString&)
{
	FUetkxExprEnv Env;
	Env.Vars.Add(TEXT("Count"), FRuiValue(static_cast<int64>(4)));
	Env.Vars.Add(TEXT("Label"), FRuiValue(FString(TEXT("hi"))));
	Env.Vars.Add(TEXT("bOn"), FRuiValue(true));
	Env.Vars.Add(TEXT("Pos"), FRuiValue(FVector2D(3.0, 7.0)));
	Env.Vars.Add(TEXT("Tint"), FRuiValue(FLinearColor(0.25f, 0.5f, 0.75f, 1.0f)));

	auto Eval = [&Env](const TCHAR* Expr)
	{
		TSharedPtr<FUetkxExprNode> Node = FUetkxExprVm::Parse(Expr);
		return Node.IsValid() ? FUetkxExprVm::Eval(*Node, Env) : FRuiValue();
	};
	auto EvalInt = [&Eval](const TCHAR* Expr) { return Eval(Expr).IntValue; };
	auto EvalBool = [&Eval](const TCHAR* Expr) { return Eval(Expr).BoolValue; };
	auto EvalStr = [&Eval](const TCHAR* Expr) { return FUetkxExprVm::ToDisplayString(Eval(Expr)); };

	// ── literals + operators + precedence ──────────────────────────────────────────────────
	TestEqual(TEXT("int literal"), EvalInt(TEXT("42")), static_cast<int64>(42));
	TestEqual(TEXT("negative literal"), EvalInt(TEXT("-7")), static_cast<int64>(-7));
	TestEqual(TEXT("precedence"), EvalInt(TEXT("2 + 3 * 4")), static_cast<int64>(14));
	TestEqual(TEXT("parens"), EvalInt(TEXT("(2 + 3) * 4")), static_cast<int64>(20));
	TestEqual(TEXT("modulo"), EvalInt(TEXT("Count % 3")), static_cast<int64>(1));
	TestEqual(TEXT("float math"), Eval(TEXT("0.5f * 4")).FloatValue, 2.0, 1e-9);
	TestEqual(TEXT("string concat"), EvalStr(TEXT("Label + \"!\"")), FString(TEXT("hi!")));
	TestTrue(TEXT("comparison"), EvalBool(TEXT("Count >= 4")));
	TestTrue(TEXT("logic + unary"), EvalBool(TEXT("bOn && !(Count < 2)")));
	TestTrue(TEXT("equality on strings"), EvalBool(TEXT("Label == \"hi\"")));
	TestEqual(TEXT("ternary"), EvalStr(TEXT("Count > 3 ? \"big\" : \"small\"")), FString(TEXT("big")));
	TestTrue(TEXT("short-circuit ||"), EvalBool(TEXT("true || Missing")));

	// ── members + methods ──────────────────────────────────────────────────────────────────
	TestEqual(TEXT("vector member"), Eval(TEXT("Pos.Y")).FloatValue, 7.0, 1e-9);
	TestEqual(TEXT("color member"), Eval(TEXT("Tint.G")).FloatValue, 0.5, 1e-4);
	TestEqual(TEXT("method sugar"), EvalStr(TEXT("Count.ToString()")), FString(TEXT("4")));
	TestTrue(TEXT("IsEmpty method"), EvalBool(TEXT("\"\".IsEmpty()")));

	// ── whitelist calls ────────────────────────────────────────────────────────────────────
	TestEqual(TEXT("TEXT literal"), EvalStr(TEXT("TEXT(\"raw\")")), FString(TEXT("raw")));
	TestEqual(TEXT("FText::FromString"), Eval(TEXT("FText::FromString(Label)")).TextValue.ToString(),
			  FString(TEXT("hi")));
	TestEqual(TEXT("Printf subset"), EvalStr(TEXT("FString::Printf(TEXT(\"Count: %d / %s\"), Count, Label)")),
			  FString(TEXT("Count: 4 / hi")));
	TestEqual(TEXT("Printf precision"), EvalStr(TEXT("FString::Printf(TEXT(\"%.2f\"), 1.5f)")), FString(TEXT("1.50")));
	TestEqual(TEXT("FMath::Clamp"), Eval(TEXT("FMath::Clamp(9, 0, 5)")).FloatValue, 5.0, 1e-9);
	TestEqual(TEXT("FName ctor"), Eval(TEXT("FName(TEXT(\"left\"))")).NameValue, FName(TEXT("left")));
	TestEqual(TEXT("FVector2D ctor"), Eval(TEXT("FVector2D(1.0f, 6.0f)")).Vector2Value.Y, 6.0, 1e-9);

	// ── rejection set: outside-subset must fail PARSE with a reason ────────────────────────
	{
		auto Rejects = [this](const TCHAR* Expr, const TCHAR* Label)
		{
			FString Why;
			TestFalse(Label, FUetkxExprVm::Parse(Expr, &Why).IsValid());
			TestFalse(FString::Printf(TEXT("%s has a reason"), Label), Why.IsEmpty());
		};
		Rejects(TEXT("Boxes->Current"), TEXT("pointer access rejected"));
		Rejects(TEXT("Items[0]"), TEXT("indexing rejected"));
		Rejects(TEXT("SomeRandomFn(1)"), TEXT("non-whitelisted call rejected"));
		Rejects(TEXT("[](){ return 1; }"), TEXT("lambda rejected"));
	}

	// ── total evaluation: runtime errors yield Null + notes, never crash ───────────────────
	{
		TArray<FString> Errors;
		TSharedPtr<FUetkxExprNode> Node = FUetkxExprVm::Parse(TEXT("Missing + 1"));
		if (TestTrue(TEXT("unknown ident parses"), Node.IsValid()))
		{
			const FRuiValue Result = FUetkxExprVm::Eval(*Node, Env, &Errors);
			TestTrue(TEXT("unknown ident -> error note"), Errors.Num() > 0);
			(void)Result;
		}
		Errors.Reset();
		Node = FUetkxExprVm::Parse(TEXT("1 / 0"));
		if (TestTrue(TEXT("div-zero parses"), Node.IsValid()))
		{
			const FRuiValue Result = FUetkxExprVm::Eval(*Node, Env, &Errors);
			TestTrue(TEXT("div-zero -> Null + note"), Result.IsNull() && Errors.Num() > 0);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
