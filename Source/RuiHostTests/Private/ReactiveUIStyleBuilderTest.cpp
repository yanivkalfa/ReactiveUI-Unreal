// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Style.Builder — TD-013: the typed fluent authoring API (RUI::Style() / RUI::Slot())
// produces FRuiStyleDicts that apply IDENTICALLY to the .uetkx markup path — proven by applying a
// built style dict to a real widget and a built slot dict through the box adapter.

#include "Misc/AutomationTest.h"
#include "RuiElementAdapter.h"
#include "RuiElementRegistry.h"
#include "RuiStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiStyleBuilderTest, "ReactiveUI.Style.Builder",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiStyleBuilderTest::RunTest(const FString&)
{
	// ── style builder: generic keys apply through ApplyStyleDiff (null adapter) ───────────────
	{
		TSharedRef<SBox> W = SNew(SBox);
		TSharedPtr<FRuiStyleDict> Dict = RUI::Style().RenderOpacity(0.5f).Enabled(false);
		TestEqual(TEXT("built dict has 2 keys"), Dict->Num(), 2);
		RUI::Slate::ApplyStyleDiff(*W, nullptr, nullptr, Dict.Get());
		TestEqual(TEXT("RenderOpacity applied"), W->GetRenderOpacity(), 0.5f);
		TestFalse(TEXT("Enabled(false) applied"), W->IsEnabled());

		// Removing a key (diff to an empty dict) RESETS it (style keys reset on removal, D-13).
		FRuiStyleDict Empty;
		RUI::Slate::ApplyStyleDiff(*W, nullptr, Dict.Get(), &Empty);
		TestEqual(TEXT("RenderOpacity reset to 1"), W->GetRenderOpacity(), 1.0f);
		TestTrue(TEXT("Enabled reset to true"), W->IsEnabled());
	}

	// ── slot builder: padding + alignment flow through the box adapter ────────────────────────
	{
		IRuiElementAdapter* VB = RUI::Slate::FindAdapter(RUI::InternElementType(FName(TEXT("VerticalBox"))));
		if (TestNotNull(TEXT("VerticalBox adapter"), VB))
		{
			TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
			TSharedRef<SWidget> Child = SNew(SSpacer);
			TSharedPtr<FRuiStyleDict> Slot = RUI::Slot().Padding(8.0f).HAlign(HAlign_Center);
			VB->InsertChild(*Box, Child, -1, Slot.Get());

			const SVerticalBox::FSlot& S = static_cast<const SVerticalBox::FSlot&>(Box->GetChildren()->GetSlotAt(0));
			TestEqual(TEXT("slot padding 8"), S.GetPadding(), FMargin(8.0f));
			TestEqual(TEXT("slot HAlign center"), (int32)S.GetHorizontalAlignment(), (int32)HAlign_Center);
		}
	}

	// ── the convertible builder can be passed where a TSharedPtr<FRuiStyleDict> is expected ───
	{
		TSharedPtr<FRuiStyleDict> Dict = RUI::Style().FontSize(16.0f).ColorAndOpacity(FLinearColor::Red);
		TestEqual(TEXT("Font.Size stored as float"), Dict->FindChecked(FName(TEXT("Font.Size"))).FloatValue, 16.0);
		const FRuiValue& Col = Dict->FindChecked(FName(TEXT("ColorAndOpacity")));
		TestEqual(TEXT("ColorAndOpacity stored as color kind"), (int32)Col.Kind, (int32)FRuiValue::EKind::Color);
		TestEqual(TEXT("color value red"), Col.ColorValue.R, 1.0f);
	}

	return true;
}

// ── TD-002: @theme tokens + @uss stylesheets (the third style layer) ────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiStylesheetTest, "ReactiveUI.Style.Stylesheet",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiStylesheetTest::RunTest(const FString&)
{
	using namespace RUI::Slate;

	// ── the value grammar ────────────────────────────────────────────────────────────────────
	TestEqual(TEXT("hex -> color"), (int32)ParseStyleValue(TEXT("#ff0000")).Kind, (int32)FRuiValue::EKind::Color);
	TestEqual(TEXT("hex red channel"), ParseStyleValue(TEXT("#ff0000")).ColorValue.R, 1.0f);
	TestEqual(TEXT("int"), (int32)ParseStyleValue(TEXT("8")).Kind, (int32)FRuiValue::EKind::Int);
	TestEqual(TEXT("int value"), ParseStyleValue(TEXT("8")).IntValue, (int64)8);
	TestEqual(TEXT("float"), (int32)ParseStyleValue(TEXT("0.5")).Kind, (int32)FRuiValue::EKind::Float);
	TestEqual(TEXT("bool"), (int32)ParseStyleValue(TEXT("true")).Kind, (int32)FRuiValue::EKind::Bool);
	TestTrue(TEXT("token stays a $string"), ParseStyleValue(TEXT("$primary")).StringValue == TEXT("$primary"));
	TestEqual(TEXT("bare ident -> name"), (int32)ParseStyleValue(TEXT("center")).Kind, (int32)FRuiValue::EKind::Name);
	TestEqual(TEXT("pair -> vector2"), (int32)ParseStyleValue(TEXT("4,2")).Kind, (int32)FRuiValue::EKind::Vector2);

	// ── load a stylesheet: a theme + a class that references a token ──────────────────────────
	const FString Source = TEXT(R"(
		@theme uss_test_dark {
			accent: #00ff00;   /* a token */
			fade: 0.25;
		}
		.uss_test_card {
			RenderOpacity: $fade;      // token ref
			ColorAndOpacity: $accent;
			Enabled: false;
		}
	)");
	const int32 Count = LoadStylesheet(Source);
	TestEqual(TEXT("registered 1 theme + 1 class"), Count, 2);

	// Without an active theme, the token stays unresolved (a $string).
	SetActiveTheme(NAME_None);
	{
		TSharedPtr<FRuiStyleDict> Eff = BuildEffectiveStyle({FName(TEXT("uss_test_card"))}, nullptr);
		TestTrue(TEXT("class merged"), Eff.IsValid());
		TestTrue(TEXT("token unresolved without a theme"),
				 Eff->FindChecked(FName(TEXT("RenderOpacity"))).StringValue == TEXT("$fade"));
	}

	// With the theme active, tokens resolve to the theme's values.
	SetActiveTheme(FName(TEXT("uss_test_dark")));
	TestEqual(TEXT("active theme set"), GetActiveTheme(), FName(TEXT("uss_test_dark")));
	{
		TSharedPtr<FRuiStyleDict> Eff = BuildEffectiveStyle({FName(TEXT("uss_test_card"))}, nullptr);
		const FRuiValue& Op = Eff->FindChecked(FName(TEXT("RenderOpacity")));
		TestEqual(TEXT("RenderOpacity token resolved to float"), (int32)Op.Kind, (int32)FRuiValue::EKind::Float);
		TestEqual(TEXT("resolved fade value"), Op.FloatValue, 0.25);
		const FRuiValue& Col = Eff->FindChecked(FName(TEXT("ColorAndOpacity")));
		TestEqual(TEXT("ColorAndOpacity token resolved to color"), (int32)Col.Kind, (int32)FRuiValue::EKind::Color);
		TestEqual(TEXT("resolved accent green"), Col.ColorValue.G, 1.0f);

		// The resolved value actually applies to a widget (RenderOpacity from the token).
		TSharedRef<SBox> W = SNew(SBox);
		ApplyStyleDiff(*W, nullptr, nullptr, Eff.Get());
		TestEqual(TEXT("token-driven RenderOpacity applied"), W->GetRenderOpacity(), 0.25f);
	}

	// Cascade: inline style wins over the class.
	{
		TSharedRef<FRuiStyleDict> Inline = MakeShared<FRuiStyleDict>();
		Inline->Add(FName(TEXT("RenderOpacity")), FRuiValue(1.0f));
		TSharedPtr<FRuiStyleDict> Eff = BuildEffectiveStyle({FName(TEXT("uss_test_card"))}, Inline);
		TestEqual(TEXT("inline overrides the class/token"), Eff->FindChecked(FName(TEXT("RenderOpacity"))).FloatValue,
				  1.0);
	}

	SetActiveTheme(NAME_None); // leave the global registry as we found it
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
