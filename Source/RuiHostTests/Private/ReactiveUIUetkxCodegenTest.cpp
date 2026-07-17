// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Codegen — compiles real .uetkx sources through the full pipeline
// (file scan → markup parse → C++ emit) and pins the generated shapes: props struct +
// Equals, hook auto-prefixing (Ctx.*), the baked __RUI_HOOK_SIG, NSLOCTEXT for text,
// registration, control-flow lowering, and the diagnostics that must fail a compile.
// (The generated code COMPILING is proven end-to-end by the gallery conversion milestone —
// its .inl files build into RuiDemo through the aggregator.)

#include "Misc/AutomationTest.h"
#include "UetkxCodegen.h"
#include "UetkxFileScan.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxCodegenTest, "ReactiveUI.Uetkx.Codegen",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxCodegenTest::RunTest(const FString&)
{
	// ── the counter (the family's hello-world of state) ───────────────────────────────────
	{
		const FString Source = TEXT(R"UETKX(
export component Counter(StartAt: int32 = 0) {
	auto [Count, SetCount] = UseState(StartAt);
	TFunction<void(int32)> Set = SetCount;
	const int32 Now = Count;
	return (
		<VerticalBox>
			<TextBlock Text={ FText::AsNumber(Count) } />
			<Button OnClicked={ Set(Now + 1) }>+1</Button>
			@if (Count > 3) {
				return ( <TextBlock Text="big!" /> )
			}
		</VerticalBox>
	);
}
)UETKX");
		FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, TEXT("Counter"));
		for (const FUetkxDiag& Diag : Out.Diags)
		{
			AddInfo(FString::Printf(TEXT("diag %s: %s @%d"), *Diag.Code, *Diag.Message, Diag.Offset));
		}
		if (!TestTrue(TEXT("counter compiles"), Out.bOk))
		{
			return false;
		}
		TestTrue(TEXT("props struct emitted"),
				 Out.Inl.Contains(TEXT("struct FCounterUetkxProps final : public FRuiPropsBase")));
		TestTrue(TEXT("param with default"), Out.Inl.Contains(TEXT("int32 StartAt = 0;")));
		TestTrue(TEXT("hook auto-prefixed"), Out.Inl.Contains(TEXT("Ctx.UseState(StartAt)")));
		TestTrue(TEXT("impl signature"),
				 Out.Inl.Contains(
					 TEXT("static FRuiNodeArray Counter_UetkxImpl(FRuiContext& Ctx, const FCounterUetkxProps& Props")));
		TestTrue(
			TEXT("registration emitted"),
			Out.Inl.Contains(TEXT("RUI::RegisterComponentId((void*)&Counter_UetkxImpl, FName(TEXT(\"Counter\")))")));
		TestTrue(TEXT("hook sig baked"), Out.Inl.Contains(TEXT("Counter_RUI_HOOK_SIG = 0x")));
		TestTrue(TEXT("wrapper for cross-component refs"), Out.Inl.Contains(TEXT("inline FRuiNode Counter(")));
		TestTrue(TEXT("event lowered with the Value payload"),
				 Out.Inl.Contains(
					 TEXT("P.SetOnClicked(FRuiCallback::Create([=](const FRuiValue& Value) { Set(Now + 1); }))")));
		TestTrue(TEXT("text child NSLOCTEXT"), Out.Inl.Contains(TEXT("NSLOCTEXT(\"Uetkx.Counter\"")));
		TestTrue(TEXT("@if lowered to if"), Out.Inl.Contains(TEXT("if (Count > 3)")));
		TestTrue(TEXT("factory targeted"), Out.Inl.Contains(TEXT("RUI::Slate::VerticalBox(MoveTemp(P), MoveTemp(Ch)")));
		TestTrue(
			TEXT("named factory self-registers"),
			Out.Inl.Contains(TEXT("RUI::RegisterNamedFactory(FName(TEXT(\"Counter\")), []() { return Counter(); })")));
		TestEqual(TEXT("hook sig from one UseState"), Out.HookSig, FUetkxFileScan::HookSignature({TEXT("UseState")}));
	}

	// ── keyed @for + style keys + Slot.* + cross-component reference ──────────────────────
	{
		const FString Source = TEXT(R"UETKX(
component RowList(Names: TArray<FString>) {
	return (
		<VerticalBox>
			@for (int32 i = 0; i < Names.Num(); ++i) {
				return ( <TextBlock key={i} Text={ FText::FromString(Names[i]) } RenderOpacity={0.5f} Slot.Padding="0,4,0,0" /> )
			}
			<StatusPanel Label="footer" />
		</VerticalBox>
	);
}
)UETKX");
		FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, TEXT("RowList"));
		if (!TestTrue(TEXT("rowlist compiles"), Out.bOk))
		{
			return false;
		}
		TestTrue(TEXT("@for lowered"), Out.Inl.Contains(TEXT("for (int32 i = 0; i < Names.Num(); ++i)")));
		TestTrue(TEXT("key expr lowered"), Out.Inl.Contains(TEXT("FRuiKey(i)")));
		TestTrue(TEXT("style key on TextBlock"),
				 Out.Inl.Contains(TEXT("__Style->Add(FName(TEXT(\"RenderOpacity\")), FRuiValue(0.5f))")));
		TestTrue(TEXT("slot key routed to SlotProps"),
				 Out.Inl.Contains(TEXT("__Slot->Add(FName(TEXT(\"Slot.Padding\")), FRuiValue(TEXT(\"0,4,0,0\")))")));
		TestTrue(TEXT("cross-component call emitted"), Out.Inl.Contains(TEXT("FStatusPanelUetkxProps P;")));
		TestTrue(TEXT("component prop assigned"), Out.Inl.Contains(TEXT("P.Label = TEXT(\"footer\");")));
	}

	// ── leaf widgets: (Props, Key) arity, no children; classes is a universal attr ────────
	{
		FUetkxCompileOutput Leaf = FUetkxCodegen::CompileSource(
			TEXT(
				"component Leafy { return ( <VerticalBox classes=\"card dim\"> <Spacer Size={ FVector2D(1.f, 6.f) } /> "
				"</VerticalBox> ); }"),
			TEXT("Leafy"));
		if (TestTrue(TEXT("leafy compiles"), Leaf.bOk))
		{
			TestTrue(TEXT("leaf factory takes no children"),
					 Leaf.Inl.Contains(TEXT("return RUI::Slate::Spacer(MoveTemp(P), FRuiKey());")));
			TestTrue(TEXT("classes lowered"), Leaf.Inl.Contains(TEXT("P.Classes.Add(FName(TEXT(\"card\")));")) &&
												  Leaf.Inl.Contains(TEXT("P.Classes.Add(FName(TEXT(\"dim\")));")));
		}
		FUetkxCompileOutput Bad = FUetkxCodegen::CompileSource(
			TEXT("component Nope { return ( <Spacer><TextBlock Text=\"x\" /></Spacer> ); }"), TEXT("Nope"));
		TestTrue(TEXT("children on a leaf widget fail (3005)"),
				 !Bad.bOk &&
					 Bad.Diags.ContainsByPredicate([](const FUetkxDiag& D) { return D.Code == TEXT("UETKX3005"); }));
	}

	// ── diagnostics that fail the compile ─────────────────────────────────────────────────
	{
		{
			FUetkxCompileOutput Lower =
				FUetkxCodegen::CompileSource(TEXT("component tiny { return ( <Spacer /> ); }"), TEXT("tiny"));
			TestTrue(TEXT("2100 lowercase component"), Lower.Diags.ContainsByPredicate([](const FUetkxDiag& D)
																					   { return D.Code == TEXT("UETKX2100"); }));
		}
		FUetkxCompileOutput NoReturn =
			FUetkxCodegen::CompileSource(TEXT("component Empty { int32 A = 1; }"), TEXT("Empty"));
		TestTrue(TEXT("2101 no markup return"), !NoReturn.bOk && NoReturn.Diags.Last().Code == TEXT("UETKX2101"));
		FUetkxCompileOutput BadAttr =
			FUetkxCodegen::CompileSource(TEXT("component Bad { return ( <Button Bogus=\"x\" /> ); }"), TEXT("Bad"));
		TestTrue(TEXT("0105 unknown attribute"), !BadAttr.bOk);
		bool bFound0105 = false;
		for (const FUetkxDiag& Diag : BadAttr.Diags)
		{
			bFound0105 |= Diag.Code == TEXT("UETKX0105");
		}
		TestTrue(TEXT("0105 code present"), bFound0105);
	}

	// ── §4 markup everywhere: markup-as-value lowers in place (statement positions) ───────
	{
		const FString Source = TEXT(R"UETKX(
component CardStack(Names: TArray<FString>) {
	auto [Count, SetCount] = UseState(0);
	auto Card = (<VerticalBox><TextBlock Text="hi" /></VerticalBox>);
	FRuiNode Bare = <Spacer />;
	TArray<FRuiNode> Rows;
	Rows.Add(<TextBlock Text="row" />);
	return (
		<VerticalBox>
			{ Card }
			{ Bare }
			<Button OnClicked={ SetCount(Count + 1) }>go</Button>
		</VerticalBox>
	);
}
)UETKX");
		FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, TEXT("CardStack"));
		for (const FUetkxDiag& Diag : Out.Diags)
		{
			AddInfo(FString::Printf(TEXT("diag %s: %s @%d"), *Diag.Code, *Diag.Message, Diag.Offset));
		}
		if (TestTrue(TEXT("markup-as-value compiles"), Out.bOk))
		{
			TestTrue(TEXT("paren value markup lowered"),
					 Out.Inl.Contains(TEXT("auto Card = (")) && Out.Inl.Contains(TEXT("RUI::Slate::VerticalBox(")));
			TestTrue(TEXT("bare `=` value markup lowered"),
					 Out.Inl.Contains(TEXT("FRuiNode Bare = ")) && !Out.Inl.Contains(TEXT("<Spacer />")));
			TestTrue(TEXT("call-argument markup lowered"),
					 Out.Inl.Contains(TEXT("Rows.Add(")) && !Out.Inl.Contains(TEXT("<TextBlock")));
			TestTrue(TEXT("no raw markup leaks into the emitted C++"), !Out.Inl.Contains(TEXT("<VerticalBox")));
			TestEqual(TEXT("hook sig only sees the real hook"), Out.HookSig,
					  FUetkxFileScan::HookSignature({TEXT("UseState")}));
		}

		// value-markup edits must NOT perturb the hook signature (§4.3 HMR protection)
		FUetkxCompileOutput Edited = FUetkxCodegen::CompileSource(
			FString(Source).Replace(TEXT("Text=\"hi\""), TEXT("Text=\"hello there\"")), TEXT("CardStack"));
		if (TestTrue(TEXT("edited variant compiles"), Edited.bOk))
		{
			TestEqual(TEXT("value-markup edit keeps the hook sig"), Edited.HookSig, Out.HookSig);
		}
	}

	// ── §4: the narrowed 0114 (paren-less markup return) + rules of hooks 0013-0016 ───────
	{
		FUetkxCompileOutput BareRet =
			FUetkxCodegen::CompileSource(TEXT("component BareRet {\n\treturn <Spacer />;\n}"), TEXT("BareRet"));
		TestTrue(TEXT("paren-less markup return fails"), !BareRet.bOk);
		TestTrue(TEXT("0114 narrowed to the paren-less return"),
				 BareRet.Diags.ContainsByPredicate([](const FUetkxDiag& D) { return D.Code == TEXT("UETKX0114"); }));

		auto HasCode = [](const FUetkxCompileOutput& O, const TCHAR* Code)
		{ return O.Diags.ContainsByPredicate([Code](const FUetkxDiag& D) { return D.Code == Code; }); };
		FUetkxCompileOutput HookIf =
			FUetkxCodegen::CompileSource(TEXT("component HookIf(Flag: bool = false) {\n\tif (Flag) {\n\t\tauto [A, "
											  "SetA] = UseState(0);\n\t}\n\treturn "
											  "( <Spacer /> );\n}"),
										 TEXT("HookIf"));
		TestTrue(TEXT("0013 hook in if"), !HookIf.bOk && HasCode(HookIf, TEXT("UETKX0013")));

		FUetkxCompileOutput HookFor =
			FUetkxCodegen::CompileSource(TEXT("component HookFor {\n\tfor (int32 i = 0; i < 3; ++i) {\n\t\tauto [A, "
											  "SetA] = UseState(i);\n\t}\n\treturn "
											  "( <Spacer /> );\n}"),
										 TEXT("HookFor"));
		TestTrue(TEXT("0014 hook in loop"), !HookFor.bOk && HasCode(HookFor, TEXT("UETKX0014")));

		FUetkxCompileOutput HookLambda = FUetkxCodegen::CompileSource(
			TEXT("component HookLambda {\n\tauto Fn = [&]() { auto [A, SetA] = UseState(0); };\n\treturn ( <Spacer /> "
				 ");\n}"),
			TEXT("HookLambda"));
		TestTrue(TEXT("0016 hook in lambda"), !HookLambda.bOk && HasCode(HookLambda, TEXT("UETKX0016")));

		FUetkxCompileOutput HookAttr = FUetkxCodegen::CompileSource(
			TEXT("component HookAttr {\n\treturn ( <TextBlock Text={ FText::AsNumber(UseState(0)) } /> );\n}"),
			TEXT("HookAttr"));
		TestTrue(TEXT("0013 hook in a markup attr expression"), HasCode(HookAttr, TEXT("UETKX0013")));

		FUetkxCompileOutput HookEvent = FUetkxCodegen::CompileSource(
			TEXT("component HookEvent {\n\treturn ( <Button OnClicked={ UseState(0) }>x</Button> );\n}"),
			TEXT("HookEvent"));
		TestTrue(TEXT("0016 hook in an event attr"), HasCode(HookEvent, TEXT("UETKX0016")));

		FUetkxCompileOutput HookDirective = FUetkxCodegen::CompileSource(
			TEXT("component HookDirective {\n\treturn (\n\t\t<VerticalBox>\n\t\t\t@for (int32 i = 0; i < 2; ++i) "
				 "{\n\t\t\t\tauto [A, SetA] = UseState(i);\n\t\t\t\treturn ( <Spacer key={i} /> )\n\t\t\t}\n\t\t"
				 "</VerticalBox>\n\t);\n}"),
			TEXT("HookDirective"));
		TestTrue(TEXT("0014 hook in an @for body"), HasCode(HookDirective, TEXT("UETKX0014")));

		// unconditional top-level hooks (incl. structured bindings + init-captures) stay clean
		FUetkxCompileOutput CleanHooks =
			FUetkxCodegen::CompileSource(TEXT("component CleanHooks {\n\tauto [A, SetA] = "
											  "UseState(0);\n\tUseEffect([Copy = A]() {}, {});\n\treturn ( "
											  "<Spacer /> );\n}"),
										 TEXT("CleanHooks"));
		TestTrue(TEXT("top-level hooks emit no 0013-0016"),
				 CleanHooks.bOk && !HasCode(CleanHooks, TEXT("UETKX0013")) && !HasCode(CleanHooks, TEXT("UETKX0014")) &&
					 !HasCode(CleanHooks, TEXT("UETKX0015")) && !HasCode(CleanHooks, TEXT("UETKX0016")));
	}

	// ── hook-sig stability: same shape -> same sig; different shape -> different ──────────
	{
		const uint32 A = FUetkxFileScan::HookSignature({TEXT("UseState"), TEXT("UseEffect")});
		const uint32 B = FUetkxFileScan::HookSignature({TEXT("UseState"), TEXT("UseEffect")});
		const uint32 C = FUetkxFileScan::HookSignature({TEXT("UseEffect"), TEXT("UseState")});
		TestEqual(TEXT("sig deterministic"), A, B);
		TestNotEqual(TEXT("sig order-sensitive"), A, C);
	}

	// ── exports/privacy (M5): detail namespace + tree-shake + same-file qualification ─────────
	{
		// A file with an EXPORTED component that references a PRIVATE same-file component, hook, and
		// module — the private decls wrap in RuiPriv_<Basename> and their references qualify.
		const FString Src = TEXT("module RowStyle {\n\tinline const int32 Gap = 4;\n}\n")
			TEXT("hook UseLocalCount() -> int32 {\n\treturn 0;\n}\n")
				TEXT("component Row {\n\treturn ( <Spacer /> );\n}\n") TEXT("export component Panel {\n")
					TEXT("\tint32 Pad = RowStyle::Gap;\n") TEXT("\tauto V = UseLocalCount();\n")
						TEXT("\treturn ( <VerticalBox> <Row /> </VerticalBox> );\n}\n");
		const FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Src, TEXT("Panel"));
		if (TestTrue(TEXT("privacy sample compiles"), Out.bOk))
		{
			// exported component: file scope + named factory.
			TestTrue(
				TEXT("exported component registers a named factory"),
				Out.Inl.Contains(TEXT("RUI::RegisterNamedFactory(FName(TEXT(\"Panel\")), []() { return Panel(); })")));
			TestTrue(TEXT("only exported decls in the export ledger"),
					 Out.ExportedNames.Num() == 1 && Out.ExportedNames[0] == TEXT("Panel"));
			// private decls wrap in the per-file detail namespace.
			TestTrue(TEXT("private component wrapped"), Out.Inl.Contains(TEXT("namespace RuiPriv_Panel")));
			TestTrue(TEXT("private component NOT globally registered"),
					 !Out.Inl.Contains(TEXT("RegisterNamedFactory(FName(TEXT(\"Row\"))")));
			// same-file references reach into the detail namespace.
			TestTrue(TEXT("private component tag qualified"), Out.Inl.Contains(TEXT("RuiPriv_Panel::FRowUetkxProps")) &&
																  Out.Inl.Contains(TEXT("RuiPriv_Panel::Row(")));
			TestTrue(TEXT("private hook call qualified"), Out.Inl.Contains(TEXT("RuiPriv_Panel::UseLocalCount(Ctx)")));
			TestTrue(TEXT("private module qual qualified"), Out.Inl.Contains(TEXT("RuiPriv_Panel::RowStyle::Gap")));
		}
	}

	// ── two-phase emit shape (M6): phase guards + defaults on the DECL wrapper only ───────────────
	{
		const FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(
			TEXT("export component TwoPhase(Title: FText) {\n\treturn ( <Spacer /> );\n}\n"), TEXT("TwoPhase"));
		if (TestTrue(TEXT("two-phase sample compiles"), Out.bOk))
		{
			TestTrue(TEXT("has the decl-phase guard"), Out.Inl.Contains(TEXT("#if defined(RUI_UETKX_DECL_PHASE)")));
			TestTrue(TEXT("has the body-phase #else"), Out.Inl.Contains(TEXT("#else")));
			TestTrue(TEXT("has #endif"), Out.Inl.Contains(TEXT("#endif")));
			// The DECL phase carries the complete struct + a defaulted forward declaration.
			TestTrue(TEXT("decl phase has the complete props struct"),
					 Out.Inl.Contains(TEXT("struct FTwoPhaseUetkxProps final : public FRuiPropsBase")));
			TestTrue(TEXT("decl-phase wrapper is a DEFAULTED forward declaration"),
					 Out.Inl.Contains(
						 TEXT("inline FRuiNode TwoPhase(FTwoPhaseUetkxProps InProps = FTwoPhaseUetkxProps(), "
							  "TArray<FRuiNode> InChildren = TArray<FRuiNode>(), FRuiKey InKey = FRuiKey());")));
			// The BODY phase repeats the signature WITHOUT defaults (C++ redefinition rule).
			TestTrue(TEXT("body-phase wrapper definition drops the defaults"),
					 Out.Inl.Contains(TEXT("inline FRuiNode TwoPhase(FTwoPhaseUetkxProps InProps, TArray<FRuiNode> "
										   "InChildren, FRuiKey InKey)\n{")));
			TestFalse(TEXT("no defaulted wrapper DEFINITION (would be a redefinition error)"),
					  Out.Inl.Contains(TEXT("FRuiKey InKey = FRuiKey())\n{")));
			// The impl lives in the body phase; the struct does NOT repeat there.
			const int32 Split = Out.Inl.Find(TEXT("#else"));
			TestTrue(TEXT("impl is after #else (body phase)"),
					 Split >= 0 && Out.Inl.Find(TEXT("TwoPhase_UetkxImpl")) > Split);
		}
	}

	// ── ES-modules M2: value/util emission + the alias plane (U-03/U-04) ─────────────────────────
	{
		// Typed + inferred value exports: DECL-phase-only `inline const`, no body-phase trace.
		const FUetkxCompileOutput Vals = FUetkxCodegen::CompileSource(
			TEXT("export FLinearColor Cool = FLinearColor(0.2f, 0.6f, 0.9f, 1.0f);\n")
				TEXT("export Accent = FLinearColor(0.9f, 0.2f, 0.2f, 1.0f);\n"),
			TEXT("Palette2"));
		if (TestTrue(TEXT("value exports compile"), Vals.bOk))
		{
			TestTrue(TEXT("typed value emits inline const <T>"),
					 Vals.Inl.Contains(TEXT("inline const FLinearColor Cool =")));
			TestTrue(TEXT("inferred value emits inline const auto"),
					 Vals.Inl.Contains(TEXT("inline const auto Accent =")));
			const int32 Split = Vals.Inl.Find(TEXT("#else"));
			TestTrue(TEXT("values are DECL-phase-only (before #else)"),
					 Split >= 0 && Vals.Inl.Find(TEXT("Cool")) < Split && Vals.Inl.Find(TEXT("Cool"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) < Split);
			TestTrue(TEXT("exported values join the export ledger"),
					 Vals.ExportedNames.Contains(TEXT("Cool")) && Vals.ExportedNames.Contains(TEXT("Accent")));
			TestTrue(TEXT("a value/util-only file is a support file"), Vals.bSupportFile);
		}

		// Util: fwd-decl (DECL phase) + definition (BODY phase), no Ctx injection into the signature.
		const FUetkxCompileOutput Util = FUetkxCodegen::CompileSource(
			TEXT("export FString FormatScore(int32 Score) {\n\treturn FString::FromInt(Score);\n}\n"), TEXT("ScoreFmt"));
		if (TestTrue(TEXT("util compiles"), Util.bOk))
		{
			const int32 Split = Util.Inl.Find(TEXT("#else"));
			TestTrue(TEXT("util fwd-decl in the DECL phase"),
					 Split >= 0 && Util.Inl.Find(TEXT("inline FString FormatScore(int32 Score);")) < Split);
			TestTrue(TEXT("util definition in the BODY phase"),
					 Util.Inl.Find(TEXT("inline FString FormatScore(int32 Score)\n{")) > Split);
			TestFalse(TEXT("no Ctx in a util signature"), Util.Inl.Contains(TEXT("FormatScore(FRuiContext&")));
		}

		// Private value + util: wrapped in the detail namespace, same-file references qualified.
		const FUetkxCompileOutput Priv = FUetkxCodegen::CompileSource(
			TEXT("FLinearColor RowTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);\n")
				TEXT("FString Pad(FString S) {\n\treturn S;\n}\n") TEXT("export FRuiNode Panel2() {\n")
					TEXT("\tauto T = RowTint;\n\tauto S = Pad(FString());\n\treturn ( <Spacer /> );\n}\n"),
			TEXT("Panel2"));
		if (TestTrue(TEXT("private value/util sample compiles"), Priv.bOk))
		{
			TestTrue(TEXT("private value wrapped"), Priv.Inl.Contains(TEXT("namespace RuiPriv_Panel2")));
			TestTrue(TEXT("private value reference qualified"), Priv.Inl.Contains(TEXT("RuiPriv_Panel2::RowTint")));
			TestTrue(TEXT("private util call qualified"), Priv.Inl.Contains(TEXT("RuiPriv_Panel2::Pad(")));
			TestTrue(TEXT("exported list excludes privates"),
					 Priv.ExportedNames.Num() == 1 && Priv.ExportedNames[0] == TEXT("Panel2"));
		}

		// New-form component + hook feed the EXISTING emitters (props struct + registrations).
		const FUetkxCompileOutput NewForm = FUetkxCodegen::CompileSource(
			TEXT("export int32 UseTick(int32 Start) {\n\treturn Start;\n}\n")
				TEXT("export FRuiNode Chip2(FString Label, int32 Count = 0) {\n")
					TEXT("\tauto V = UseTick(1);\n\treturn ( <TextBlock Text={ FText::FromString(Label) } /> );\n}\n"),
			TEXT("Chip2"));
		if (TestTrue(TEXT("new-form component + hook compile"), NewForm.bOk))
		{
			TestTrue(TEXT("props struct emitted"), NewForm.Inl.Contains(TEXT("struct FChip2UetkxProps")));
			TestTrue(TEXT("C++-native param -> props field with default"),
					 NewForm.Inl.Contains(TEXT("int32 Count = 0;")) && NewForm.Inl.Contains(TEXT("FString Label")));
			TestTrue(TEXT("new-form hook takes Ctx first"),
					 NewForm.Inl.Contains(TEXT("inline int32 UseTick(FRuiContext& Ctx, int32 Start)")));
			TestTrue(TEXT("hook call site Ctx-injected"), NewForm.Inl.Contains(TEXT("UseTick(Ctx, 1)")));
			TestTrue(TEXT("named factory registered"),
					 NewForm.Inl.Contains(TEXT("RegisterNamedFactory(FName(TEXT(\"Chip2\"))")));
		}

		// Alias plane: rename import rewrites the tag, a hook call, and a bare value reference;
		// `* as` strips the namespace qual. (No resolver — emission-plane behavior only.)
		const FUetkxCompileOutput Alias = FUetkxCodegen::CompileSource(
			TEXT("import { StatusChip as Chip } from \"./StatusChip\"\n")
				TEXT("import { UseCounter as UseTick } from \"./Hooks2\"\n")
					TEXT("import { Cool as Primary } from \"./Palette2\"\n")
						TEXT("import * as Palette from \"./Palette2\"\n") TEXT("export FRuiNode AliasUser() {\n")
							TEXT("\tauto A = UseTick(1);\n\tauto B = Primary;\n\tauto C = Palette::Accent;\n")
								TEXT("\treturn ( <Chip /> );\n}\n"),
			TEXT("AliasUser"));
		if (TestTrue(TEXT("alias sample compiles"), Alias.bOk))
		{
			TestTrue(TEXT("renamed tag emits the target props+factory"),
					 Alias.Inl.Contains(TEXT("FStatusChipUetkxProps")) && Alias.Inl.Contains(TEXT("StatusChip(")));
			TestFalse(TEXT("the local tag alias never reaches the C++"), Alias.Inl.Contains(TEXT("FChipUetkxProps")));
			TestTrue(TEXT("renamed hook call rewrites to the target + Ctx"),
					 Alias.Inl.Contains(TEXT("UseCounter(Ctx, 1)")));
			TestTrue(TEXT("renamed value reference rewrites"), Alias.Inl.Contains(TEXT("auto B = Cool;")));
			TestTrue(TEXT("namespace qual strips"), Alias.Inl.Contains(TEXT("auto C = Accent;")));
			TestTrue(TEXT("Uses records the TARGET component"), Alias.Uses.Contains(TEXT("StatusChip")));
			TestFalse(TEXT("Uses does not record the alias"), Alias.Uses.Contains(TEXT("Chip")));
		}

		// Mixed 5-kind file: source order preserved within each phase region.
		const FUetkxCompileOutput Mixed = FUetkxCodegen::CompileSource(
			TEXT("export FRuiNode Widget5() {\n\treturn ( <Spacer /> );\n}\n")
				TEXT("export int32 UseFive(int32 A) {\n\treturn A;\n}\n")
					TEXT("export FLinearColor Five = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);\n")
						TEXT("export FString FmtFive(int32 S) {\n\treturn FString::FromInt(S);\n}\n")
							TEXT("export module FiveStyles { inline const float P5 = 5.0f; }\n"),
			TEXT("Widget5"));
		if (TestTrue(TEXT("mixed 5-kind file compiles"), Mixed.bOk))
		{
			TestTrue(TEXT("all five kinds present"),
					 Mixed.Inl.Contains(TEXT("FWidget5UetkxProps")) && Mixed.Inl.Contains(TEXT("UseFive")) &&
						 Mixed.Inl.Contains(TEXT("inline const FLinearColor Five")) &&
						 Mixed.Inl.Contains(TEXT("FmtFive")) && Mixed.Inl.Contains(TEXT("namespace FiveStyles")));
			TestTrue(TEXT("exported ledger carries all five"),
					 Mixed.ExportedNames.Contains(TEXT("Widget5")) && Mixed.ExportedNames.Contains(TEXT("UseFive")) &&
						 Mixed.ExportedNames.Contains(TEXT("Five")) && Mixed.ExportedNames.Contains(TEXT("FmtFive")) &&
						 Mixed.ExportedNames.Contains(TEXT("FiveStyles")));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
