// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Uetkx.Schema — pins the exported vocabulary JSON (what the LSP's markup
// intelligence consumes, Phase 5) and the New Component template (which must compile through
// the real pipeline — the template lives next to the grammar for exactly this reason).

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UetkxCodegen.h"
#include "UetkxConfig.h"
#include "UetkxFileActions.h"

#if WITH_DEV_AUTOMATION_TESTS

static bool HasString(const TArray<TSharedPtr<FJsonValue>>& Arr, const TCHAR* Value)
{
	for (const TSharedPtr<FJsonValue>& Entry : Arr)
	{
		if (Entry->AsString() == Value)
		{
			return true;
		}
	}
	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUetkxSchemaTest, "ReactiveUI.Uetkx.Schema",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUetkxSchemaTest::RunTest(const FString&)
{
	// ── the exported vocabulary ────────────────────────────────────────────────────────────
	{
		TSharedPtr<FJsonObject> Schema;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FUetkxCodegen::ExportSchemaJson()), Schema);
		if (!TestTrue(TEXT("schema parses"), Schema.IsValid()))
		{
			return false;
		}
		TestEqual(TEXT("schema v1"), (int32)Schema->GetNumberField(TEXT("v")), 1);

		const TSharedPtr<FJsonObject> Elements = Schema->GetObjectField(TEXT("elements"));
		// 15 Phase-2 + 14 Batch-2 (Phase 7) + Canvas (Doom Phase 0) + 8 Batch-3 wave 1
		// (WIDGET_COMPLETION_PLAN: ColorBlock, SimpleGradient, ComplexGradient, Hyperlink,
		// EnableBox, ScissorRectBox, BackgroundBlur, InvalidationPanel).
		TestEqual(TEXT("63 host tags"), Elements->Values.Num(), 63);
		const TSharedPtr<FJsonObject> Switcher = Elements->GetObjectField(TEXT("WidgetSwitcher"));
		TestEqual(TEXT("WidgetSwitcher factory"), Switcher->GetStringField(TEXT("factory")),
				  FString(TEXT("RUI::Slate::WidgetSwitcher")));
		TestEqual(TEXT("WidgetIndex is int"),
				  Switcher->GetObjectField(TEXT("attrs"))->GetStringField(TEXT("WidgetIndex")), FString(TEXT("int")));
		const TSharedPtr<FJsonObject> Button = Elements->GetObjectField(TEXT("Button"));
		TestEqual(TEXT("Button factory"), Button->GetStringField(TEXT("factory")), FString(TEXT("RUI::Slate::Button")));
		TestTrue(TEXT("Button takes children"), Button->GetBoolField(TEXT("children")));
		TestEqual(TEXT("OnClicked is an event"),
				  Button->GetObjectField(TEXT("attrs"))->GetStringField(TEXT("OnClicked")), FString(TEXT("event")));
		TestEqual(
			TEXT("TextBlock Text is text"),
			Elements->GetObjectField(TEXT("TextBlock"))->GetObjectField(TEXT("attrs"))->GetStringField(TEXT("Text")),
			FString(TEXT("text")));
		TestEqual(
			TEXT("DrawFn is expression-only"),
			Elements->GetObjectField(TEXT("RuiCanvas"))->GetObjectField(TEXT("attrs"))->GetStringField(TEXT("DrawFn")),
			FString(TEXT("expr")));
		TestFalse(TEXT("Spacer is childless"),
				  Elements->GetObjectField(TEXT("Spacer"))->GetBoolField(TEXT("children")));

		const TArray<TSharedPtr<FJsonValue>>& StyleKeys = Schema->GetArrayField(TEXT("styleKeys"));
		TestEqual(TEXT("16 style keys"), StyleKeys.Num(), 16);
		TestTrue(TEXT("Clipping styled"), HasString(StyleKeys, TEXT("Clipping")));
		TestTrue(TEXT("ToolTipText styled (P1 universal)"), HasString(StyleKeys, TEXT("ToolTipText")));
		TestTrue(TEXT("RenderOpacity styled"), HasString(StyleKeys, TEXT("RenderOpacity")));
		TestTrue(TEXT("Font.Size styled"), HasString(StyleKeys, TEXT("Font.Size")));

		TestEqual(TEXT("slot prefix"), Schema->GetStringField(TEXT("slotPrefix")), FString(TEXT("Slot.")));
		TestEqual(TEXT("16 canonical slot keys"), Schema->GetArrayField(TEXT("slotKeys")).Num(), 16);

		const TArray<TSharedPtr<FJsonValue>>& Hooks = Schema->GetArrayField(TEXT("hooks"));
		TestEqual(TEXT("all hooks exported"), Hooks.Num(), FUetkxFileScan::HookNames().Num());
		TestTrue(TEXT("UseState exported"), HasString(Hooks, TEXT("UseState")));
		TestTrue(TEXT("ProvideContext exported"), HasString(Hooks, TEXT("ProvideContext")));
	}

	// ── the New Component template compiles through the real pipeline ─────────────────────
	{
		const FString Scratch = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUI"), TEXT("FileActionsTest"));
		IFileManager::Get().DeleteDirectory(*Scratch, false, true);
		IFileManager::Get().MakeDirectory(*Scratch, true);

		const FString Created = FUetkxFileActions::CreateComponentFile(Scratch, TEXT("MyPanel"));
		TestTrue(TEXT("template created"), !Created.IsEmpty() && IFileManager::Get().FileExists(*Created));
		FString Source;
		FFileHelper::LoadFileToString(Source, *Created);
		TestTrue(TEXT("declares the component"), Source.Contains(TEXT("component MyPanel {")));
		const FUetkxCompileOutput Out = FUetkxCodegen::CompileSource(Source, TEXT("MyPanel"));
		TestTrue(TEXT("template compiles clean"), Out.bOk && Out.Diags.Num() == 0);

		TestTrue(TEXT("never overwrites"), FUetkxFileActions::CreateComponentFile(Scratch, TEXT("MyPanel")).IsEmpty());
		TestTrue(TEXT("rejects lowercase (UETKX2100 grammar)"),
				 FUetkxFileActions::CreateComponentFile(Scratch, TEXT("myPanel")).IsEmpty());
		TestTrue(TEXT("rejects non-ident"),
				 FUetkxFileActions::CreateComponentFile(Scratch, TEXT("My-Panel")).IsEmpty());

		IFileManager::Get().DeleteDirectory(*Scratch, false, true);
	}

	// ── FUetkxConfig walk-up: `~/` root key, no-merge shadowing, module-root default (M1) ─────
	{
		auto Norm = [](FString P)
		{
			P = FPaths::ConvertRelativePathToFull(P);
			FPaths::NormalizeDirectoryName(P);
			return P;
		};
		const FString Scratch = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUI"), TEXT("ConfigWalkTest"));
		IFileManager::Get().DeleteDirectory(*Scratch, false, true);

		// A fake module `MyMod` (its *.Build.cs marks the module root), with a root-declaring
		// config, a nested formatter-only config that must NOT inherit `root`, and a plain tree
		// with no module at all.
		const FString ModRoot = Scratch / TEXT("MyMod");
		const FString Screens = ModRoot / TEXT("Screens");
		const FString Sub = Screens / TEXT("Sub");
		const FString NoRoot = ModRoot / TEXT("NoRoot");
		const FString Plain = Scratch / TEXT("Plain");
		for (const FString& D : {Screens, Sub, NoRoot, Plain})
		{
			IFileManager::Get().MakeDirectory(*D, true);
		}
		auto Write = [](const FString& Path, const FString& Body)
		{ FFileHelper::SaveStringToFile(Body, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM); };
		Write(ModRoot / TEXT("MyMod.Build.cs"), TEXT("// stub module rules"));
		Write(ModRoot / TEXT("uetkx.config.json"), TEXT("{ \"root\": \"Screens\", \"printWidth\": 80 }"));
		Write(Sub / TEXT("uetkx.config.json"), TEXT("{ \"indentSize\": 4 }")); // formatter-only, no root
		Write(NoRoot / TEXT("uetkx.config.json"), TEXT("{ \"printWidth\": 42 }"));

		// Load (nearest wins): from Screens the winner is MyMod/uetkx.config.json.
		const FUetkxConfig FromScreens = FUetkxConfig::Load(Screens);
		TestEqual(TEXT("root config printWidth"), FromScreens.PrintWidth, 80);
		TestTrue(TEXT("root config declares root"), FromScreens.bHasRoot);
		TestEqual(TEXT("root string verbatim"), FromScreens.RootRelative, FString(TEXT("Screens")));
		TestEqual(TEXT("config dir is the module dir"), Norm(FromScreens.ConfigDir), Norm(ModRoot));

		// ModuleRootFor: any file under MyMod resolves to the *.Build.cs dir.
		TestEqual(TEXT("module root of a nested file"), Norm(FUetkxConfig::ModuleRootFor(Sub / TEXT("B.uetkx"))),
				  Norm(ModRoot));
		TestTrue(TEXT("no module root outside any module"),
				 FUetkxConfig::ModuleRootFor(Plain / TEXT("D.uetkx")).IsEmpty());

		// RootAnchorFor: declared root resolves relative to the config dir.
		TestEqual(TEXT("~/ = declared root"), Norm(FUetkxConfig::RootAnchorFor(Screens / TEXT("A.uetkx"))),
				  Norm(Screens));
		// No-merge shadowing (A5g): the nearer formatter-only config resets `~/` to the module
		// root — it does NOT inherit the ancestor's `root: Screens`.
		TestEqual(TEXT("~/ shadowed back to module root"), Norm(FUetkxConfig::RootAnchorFor(Sub / TEXT("B.uetkx"))),
				  Norm(ModRoot));
		// A config that simply omits `root` also falls back to the module root.
		TestEqual(TEXT("~/ missing-root default = module root"),
				  Norm(FUetkxConfig::RootAnchorFor(NoRoot / TEXT("C.uetkx"))), Norm(ModRoot));

		// No config anywhere: defaults, no root, empty config dir.
		const FUetkxConfig None = FUetkxConfig::Load(Plain);
		TestFalse(TEXT("no config declares no root"), None.bHasRoot);
		TestTrue(TEXT("no config dir"), None.ConfigDir.IsEmpty());
		TestEqual(TEXT("no config = default printWidth"), None.PrintWidth, 100);

		IFileManager::Get().DeleteDirectory(*Scratch, false, true);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
