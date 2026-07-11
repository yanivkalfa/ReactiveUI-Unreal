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
		TestEqual(
			TEXT("2100 lowercase component"),
			FUetkxCodegen::CompileSource(TEXT("component tiny { return ( <Spacer /> ); }"), TEXT("tiny")).Diags[0].Code,
			FString(TEXT("UETKX2100")));
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
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
