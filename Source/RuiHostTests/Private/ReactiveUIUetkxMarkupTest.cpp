// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Markup — pins the byte-compatible grammar: node shapes, attribute kinds,
// rebasable offsets, the text model (braces literal mid-run), and the UETKX#### error codes
// (family numbers). Cases mirror the semantics guitkx_test.gd pins for the Godot dialect.

#include "Misc/AutomationTest.h"
#include "UetkxLexer.h"
#include "UetkxMarkup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UetkxMarkupTest
{
	static FUetkxParseResult ParseAll(const FString& Markup)
	{
		const TArray<int32> Src = FUetkxLexer::ToCodePoints(Markup);
		FUetkxMarkup Parser;
		return Parser.Parse(Src, 0, Src.Num());
	}
} // namespace UetkxMarkupTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxMarkupTest, "ReactiveUI.Uetkx.Markup",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxMarkupTest::RunTest(const FString&)
{
	using namespace UetkxMarkupTest;

	// ── element + every attribute kind ────────────────────────────────────────────────────
	{
		FUetkxParseResult R = ParseAll(TEXT(
			"<Button Text=\"hi\" Value={ Count + 1 } bEnabled {...Extra} {/* note */}>\n\t<TextBlock />\n</Button>"));
		TestTrue(TEXT("el: parses clean"), R.IsOk());
		if (TestEqual(TEXT("el: one root"), R.Nodes.Num(), 1))
		{
			const FUetkxNode& El = *R.Nodes[0];
			TestTrue(TEXT("el: type"), El.Type == EUetkxNodeType::El);
			TestEqual(TEXT("el: tag"), El.Tag, FString(TEXT("Button")));
			TestEqual(TEXT("el: attr count"), El.Attrs.Num(), 5);
			TestTrue(TEXT("attr str"), El.Attrs[0].Kind == EUetkxAttrKind::Str && El.Attrs[0].Value == TEXT("hi"));
			TestEqual(TEXT("attr str vat points at value text"), El.Attrs[0].Vat, 14);
			TestTrue(TEXT("attr expr"),
					 El.Attrs[1].Kind == EUetkxAttrKind::Expr && El.Attrs[1].Value == TEXT("Count + 1"));
			TestTrue(TEXT("attr bool"), El.Attrs[2].Kind == EUetkxAttrKind::Bool && El.Attrs[2].Value == TEXT("true"));
			TestTrue(TEXT("attr spread"),
					 El.Attrs[3].Kind == EUetkxAttrKind::Spread && El.Attrs[3].Value == TEXT("Extra"));
			TestTrue(TEXT("attr comment"), El.Attrs[4].Kind == EUetkxAttrKind::Comment);
			TestEqual(TEXT("el: one child"), El.Children.Num(), 1);
			TestTrue(TEXT("child self-closing el"),
					 El.Children[0]->Type == EUetkxNodeType::El && El.Children[0]->Tag == TEXT("TextBlock"));
		}
	}

	// ── fragments: <> and named <Fragment key={...}> ──────────────────────────────────────
	{
		FUetkxParseResult R = ParseAll(TEXT("<>\n<A /><B />\n</>"));
		TestTrue(TEXT("frag: ok"), R.IsOk());
		TestTrue(TEXT("frag: type + 2 children"),
				 R.Nodes.Num() == 1 && R.Nodes[0]->Type == EUetkxNodeType::Frag && R.Nodes[0]->Children.Num() == 2);

		FUetkxParseResult R2 = ParseAll(TEXT("<Fragment key={Id}><A /></Fragment>"));
		TestTrue(TEXT("Fragment: named alias with attrs kept"),
				 R2.IsOk() && R2.Nodes[0]->Type == EUetkxNodeType::Frag && R2.Nodes[0]->Named == TEXT("Fragment") &&
					 R2.Nodes[0]->Attrs.Num() == 1);
	}

	// ── text model: stops at < and @, braces mid-run are LITERAL ──────────────────────────
	{
		FUetkxParseResult R = ParseAll(TEXT("<T>Score {n} points</T>"));
		TestTrue(TEXT("text: ok"), R.IsOk());
		const FUetkxNode& El = *R.Nodes[0];
		if (TestEqual(TEXT("text: single text child (brace literal mid-run)"), El.Children.Num(), 1))
		{
			TestTrue(TEXT("text: value keeps braces"),
					 El.Children[0]->Type == EUetkxNodeType::Text && El.Children[0]->Value == TEXT("Score {n} points"));
		}
		// but an expr at NODE START is an expr child
		FUetkxParseResult R2 = ParseAll(TEXT("<T>{ Count } left</T>"));
		TestTrue(TEXT("expr at node start + trailing text"),
				 R2.IsOk() && R2.Nodes[0]->Children.Num() == 2 &&
					 R2.Nodes[0]->Children[0]->Type == EUetkxNodeType::Expr &&
					 R2.Nodes[0]->Children[0]->Code == TEXT("Count") &&
					 R2.Nodes[0]->Children[1]->Type == EUetkxNodeType::Text);
	}

	// ── comments: all three forms at node start ───────────────────────────────────────────
	{
		FUetkxParseResult R = ParseAll(TEXT("// line\n/* block */\n<!-- html -->\n<T />"));
		TestTrue(TEXT("comments: ok"), R.IsOk());
		TestEqual(TEXT("comments: 3 comment nodes + el"), R.Nodes.Num(), 4);
		TestTrue(TEXT("comment kinds"), R.Nodes[0]->Type == EUetkxNodeType::Comment &&
											R.Nodes[1]->Type == EUetkxNodeType::Comment &&
											R.Nodes[2]->Type == EUetkxNodeType::Comment);
	}

	// ── @if / @elif / @else with rebasable bodies ─────────────────────────────────────────
	{
		const FString Markup = TEXT("@if (A > 0) {\n\t<T />\n} @elif (B) {\n\t<U />\n} @else {\n\t<V />\n}");
		FUetkxParseResult R = ParseAll(Markup);
		TestTrue(TEXT("if: ok"), R.IsOk());
		const FUetkxNode& If = *R.Nodes[0];
		TestTrue(TEXT("if: type"), If.Type == EUetkxNodeType::If);
		TestEqual(TEXT("if: 2 branches"), If.Branches.Num(), 2);
		TestEqual(TEXT("if: cond text"), If.Branches[0].Cond, FString(TEXT("A > 0")));
		TestTrue(TEXT("if: else captured"), If.ElseBody.IsSet());
		// body offsets rebase: re-parsing branch 0's body finds <T /> at its rebased position
		const TArray<int32> Body = FUetkxLexer::ToCodePoints(If.Branches[0].BodyMarkup);
		FUetkxMarkup Nested;
		FUetkxParseResult NestedR = Nested.Parse(Body, 0, Body.Num());
		TestTrue(TEXT("if: body re-parses to the element"),
				 NestedR.IsOk() && NestedR.Nodes.Num() == 1 && NestedR.Nodes[0]->Tag == TEXT("T"));
		TestTrue(TEXT("if: BodyAt points into the original source"), If.Branches[0].BodyAt > 0);
	}

	// ── @for + @match ─────────────────────────────────────────────────────────────────────
	{
		FUetkxParseResult R = ParseAll(TEXT("@for (int32 i = 0; i < N; ++i) {\n\t<Row key={i} />\n}"));
		TestTrue(TEXT("for: ok + header"), R.IsOk() && R.Nodes[0]->Type == EUetkxNodeType::For &&
											   R.Nodes[0]->Header == TEXT("int32 i = 0; i < N; ++i"));

		FUetkxParseResult R2 =
			ParseAll(TEXT("@match (Mode) {\n@case (1) { <A /> }\n@case (2) { <B /> }\n@default { <C /> }\n}"));
		TestTrue(TEXT("match: ok"), R2.IsOk());
		TestTrue(TEXT("match: subject + 2 cases + default"),
				 R2.Nodes[0]->Type == EUetkxNodeType::Match && R2.Nodes[0]->Subject == TEXT("Mode") &&
					 R2.Nodes[0]->Cases.Num() == 2 && R2.Nodes[0]->DefaultBody.IsSet());
	}

	// ── C++-lexis inside {expr} attributes never confuses balancing ───────────────────────
	{
		FUetkxParseResult R = ParseAll(TEXT("<T V={ FString(TEXT(\")}\")) /* ) */ } />"));
		TestTrue(TEXT("expr attr: C++ string/comment with brackets inside"), R.IsOk());
		TestEqual(TEXT("expr attr: value verbatim"), R.Nodes[0]->Attrs[0].Value,
				  FString(TEXT("FString(TEXT(\")}\")) /* ) */")));
	}

	// ── error codes (family numbers) ──────────────────────────────────────────────────────
	{
		TestEqual(TEXT("0300 digit tag"), ParseAll(TEXT("<9x />")).ErrorCode, FString(TEXT("UETKX0300")));
		TestEqual(TEXT("0301 unclosed tag (no close at all)"), ParseAll(TEXT("<Box><T />")).ErrorCode,
				  FString(TEXT("UETKX0301")));
		TestEqual(TEXT("0303 malformed close (`</Bo` without `>`)"), ParseAll(TEXT("<Box><T /></Bo")).ErrorCode,
				  FString(TEXT("UETKX0303")));
		TestEqual(TEXT("0302 mismatched close"), ParseAll(TEXT("<A></B>")).ErrorCode, FString(TEXT("UETKX0302")));
		TestEqual(TEXT("0304 unclosed expr"), ParseAll(TEXT("<T V={ x />")).ErrorCode, FString(TEXT("UETKX0304")));
		TestEqual(TEXT("0305 unknown directive"), ParseAll(TEXT("@bogus (x) { }")).ErrorCode,
				  FString(TEXT("UETKX0305")));
		TestEqual(TEXT("2506 directive needs paren"), ParseAll(TEXT("@if { <T /> }")).ErrorCode,
				  FString(TEXT("UETKX2506")));
		TestEqual(TEXT("0300 truncated <at EOF (G-04)"), ParseAll(TEXT("<Box><")).ErrorCode,
				  FString(TEXT("UETKX0300")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
