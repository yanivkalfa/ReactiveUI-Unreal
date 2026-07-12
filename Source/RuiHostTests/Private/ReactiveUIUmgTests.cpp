// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Umg + ReactiveUI.Mvvm — the Phase-6 interop seams, headless:
//   our-inside-theirs: URuiHostWidget mounts a registered component and unmounts on
//     ReleaseSlateResources (live-root count proves the teardown);
//   theirs-inside-ours: RUI::Umg::UserWidget hosts a UUserWidget via SObjectWidget;
//   world teardown: URuiWorldSubsystem unmounts every root on world death (the PIE-end /
//     level-travel contract);
//   their-data-feeding-ours: UseField re-renders on FieldNotify broadcasts, unbinds on
//     unmount, and reads the default quietly once the VM is gone (stale-VM policy).

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "RuiAssetBrush.h"
#include "RuiCoreElements.h"
#include "RuiFieldHooks.h"
#include "RuiFieldHooks.h"
#include "RuiHostWidget.h"
#include "RuiNode.h"
#include "RuiReconciler.h"
#include "RuiRoot.h"
#include "RuiSignalViewModel.h"
#include "RuiSlateElements.h"
#include "RuiTestViewModel.h"
#include "RuiUmgElement.h"
#include "RuiWorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UmgTest
{
	static URuiTestViewModel* GViewModel = nullptr;

	static FRuiNodeArray FieldReaderComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		const int32 Score = RUI::Umg::UseField<int32>(Ctx, GViewModel, FName(TEXT("Score")), -1);
		return {RUI::TextBlock(FString::Printf(TEXT("Score: %d"), Score))};
	}
	RUI_COMPONENT(FieldReaderComp)

	static bool ContainsText(SWidget& RootWidget, const FString& Needle)
	{
		if (RootWidget.GetType() == FName(TEXT("STextBlock")) &&
			static_cast<STextBlock&>(RootWidget).GetText().ToString().Contains(Needle))
		{
			return true;
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (ContainsText(Children->GetChildAt(i).Get(), Needle))
			{
				return true;
			}
		}
		return false;
	}

	static int32 CountLiveReconcilers()
	{
		int32 Count = 0;
		FRuiReconciler::ForEachLive([&Count](FRuiReconciler&) { ++Count; });
		return Count;
	}
} // namespace UmgTest

const ::UE::FieldNotification::FFieldId URuiTestViewModel::FFieldNotificationClassDescriptor::Score(TEXT("Score"), 0);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiUmgTest, "ReactiveUI.Umg",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiUmgTest::RunTest(const FString&)
{
	AddExpectedError(TEXT("is not a registered component"), EAutomationExpectedErrorFlags::Contains, 0);
	const int32 Baseline = UmgTest::CountLiveReconcilers();

	// ── our UI inside theirs: URuiHostWidget ──────────────────────────────────────────────
	{
		URuiHostWidget* Host = NewObject<URuiHostWidget>(GetTransientPackage());
		Host->ComponentName = FName(TEXT("HelloWorld")); // a compiled .uetkx gallery component
		TSharedRef<SWidget> Widget = Host->TakeWidget();
		TestEqual(TEXT("host mounted one root"), UmgTest::CountLiveReconcilers(), Baseline + 1);
		TestTrue(TEXT("compiled component renders inside UMG"),
				 UmgTest::ContainsText(Widget.Get(), TEXT("Hello, world!")));
		Host->ReleaseSlateResources(true);
		TestEqual(TEXT("ReleaseSlateResources unmounts"), UmgTest::CountLiveReconcilers(), Baseline);

		URuiHostWidget* Unknown = NewObject<URuiHostWidget>(GetTransientPackage());
		Unknown->ComponentName = FName(TEXT("NoSuchComponent"));
		TestTrue(TEXT("unknown component -> placeholder, no crash"),
				 UmgTest::ContainsText(Unknown->TakeWidget().Get(), TEXT("not a registered component")));
	}

	// ── world teardown contract: URuiWorldSubsystem ───────────────────────────────────────
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuiUmgTestWorld"));
		if (TestNotNull(TEXT("test world"), World))
		{
			URuiWorldSubsystem* Subsystem = World->GetSubsystem<URuiWorldSubsystem>();
			if (TestNotNull(TEXT("subsystem"), Subsystem))
			{
				const int32 Handle = Subsystem->MountNamed(FName(TEXT("HelloWorld")));
				TestTrue(TEXT("mounted"), Handle != INDEX_NONE);
				TestEqual(TEXT("one live root"), Subsystem->NumLiveRoots(), 1);
				TestEqual(TEXT("unknown name refuses"), Subsystem->MountNamed(FName(TEXT("NoSuchComponent"))),
						  (int32)INDEX_NONE);
			}
			World->DestroyWorld(false);
			TestEqual(TEXT("world death unmounted every root (PIE-end contract)"), UmgTest::CountLiveReconcilers(),
					  Baseline);
		}
	}

	// ── theirs inside ours: RUI::Umg::UserWidget ──────────────────────────────────────────
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuiUmgHostWorld"));
		if (TestNotNull(TEXT("umg world"), World))
		{
			TSharedRef<FRuiRoot> Root =
				FRuiRoot::Create(RUI::Umg::UserWidget(URuiTestUserWidget::StaticClass(), World));
			Root->FlushSync();
			TestTrue(TEXT("hosted UMG widget produced a Slate child"),
					 Root->GetWidget()->GetRootPanel()->GetChildren()->Num() > 0);
			Root->Unmount();
			World->DestroyWorld(false);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiMvvmTest, "ReactiveUI.Mvvm",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiMvvmTest::RunTest(const FString&)
{
	URuiTestViewModel* Vm = NewObject<URuiTestViewModel>(GetTransientPackage());
	Vm->AddToRoot();
	UmgTest::GViewModel = Vm;

	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&UmgTest::FieldReaderComp));
	Root->FlushSync();
	SWidget& RootWidget = Root->GetWidget().Get();
	TestTrue(TEXT("initial field read"), UmgTest::ContainsText(RootWidget, TEXT("Score: 0")));

	Vm->SetScore(42);
	Root->FlushSync();
	TestTrue(TEXT("FieldNotify broadcast re-rendered"), UmgTest::ContainsText(RootWidget, TEXT("Score: 42")));

	Vm->SetScore(42); // equal set: no broadcast — nothing to assert beyond not crashing
	Root->FlushSync();

	// stale-VM policy: gone VM reads the caller default, quietly. (HmrRefreshAll defeats the
	// props-equality bailout — a plain root update correctly serves the cached output.)
	UmgTest::GViewModel = nullptr;
	Root->GetReconciler().HmrRefreshAll();
	Root->FlushSync();
	TestTrue(TEXT("stale VM reads default"), UmgTest::ContainsText(RootWidget, TEXT("Score: -1")));

	// unbind-on-unmount: a broadcast after unmount must not crash or resurrect anything
	UmgTest::GViewModel = Vm;
	Root->GetReconciler().HmrRefreshAll();
	Root->FlushSync();
	Root->Unmount();
	Vm->SetScore(77);
	TestEqual(TEXT("unmounted root stays down"), Root->GetReconciler().NumLiveFibers(), 0);

	Vm->RemoveFromRoot();
	UmgTest::GViewModel = nullptr;
	return true;
}

// ── TD-022 / D-17: asset brushes — GC-rooted texture/material brushes on SImage ────────────

namespace UmgTest
{
	static TSharedPtr<FSlateBrush> GBrush;

	static FRuiNodeArray AssetImageComp(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		FRuiImageProps P;
		P.SetBrush(GBrush);
		P.SetDesiredSizeOverride(FVector2D(16.0f, 16.0f));
		return {RUI::Slate::Image(MoveTemp(P))};
	}
	RUI_COMPONENT(AssetImageComp)
} // namespace UmgTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiAssetBrushTest, "ReactiveUI.Umg.AssetBrush",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiAssetBrushTest::RunTest(const FString&)
{
	const int32 Baseline = RUI::Umg::NumTrackedAssetBrushes();

	UTexture2D* Texture = UTexture2D::CreateTransient(4, 4);
	if (!TestNotNull(TEXT("transient texture created"), Texture))
	{
		return false;
	}
	FWeakObjectPtr WeakTexture(Texture);

	UmgTest::GBrush = RUI::Umg::MakeAssetBrush(Texture, FVector2D(16.0f, 16.0f), FLinearColor::White);
	if (!TestTrue(TEXT("brush built"), UmgTest::GBrush.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("brush resource is the texture"), UmgTest::GBrush->GetResourceObject() == Texture);
	TestTrue(TEXT("tracked count grew"), RUI::Umg::NumTrackedAssetBrushes() > Baseline);

	// The Image adapter applies the brush onto a real SImage. Scoped so the host (and its GO-05
	// node pool, which stashes the released widget's props — and thus a brush ref) is destroyed
	// before the final baseline check.
	{
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&UmgTest::AssetImageComp));
		Root->FlushSync();
		TSharedRef<SWidget> ImageW = Root->GetWidget()->GetRootPanel()->GetChildren()->GetChildAt(0);
		TestEqual(TEXT("SImage mounted"), ImageW->GetType(), FName(TEXT("SImage")));

		// D-17: the texture survives GC while the brush is live (the FGCObject root references it).
		Texture = nullptr;
		CollectGarbage(RF_NoFlags, /*bPerformFullPurge*/ true);
		TestTrue(TEXT("texture survives GC while its brush is live"), WeakTexture.IsValid());

		Root->Unmount();
	}

	// Release: the brush drops, the root compacts the dead entry, the asset is free again.
	UmgTest::GBrush.Reset();
	CollectGarbage(RF_NoFlags, /*bPerformFullPurge*/ true);
	TestEqual(TEXT("tracked count returns to baseline after release"), RUI::Umg::NumTrackedAssetBrushes(), Baseline);
	return true;
}

// ── TD-021: the REVERSE MVVM bridge — URuiSignalViewModel (ours feeding theirs) ────────────

namespace UmgTest
{
	static URuiSignalViewModel* GSignalVm = nullptr;

	static FRuiNodeArray SignalReaderComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		const int32 N = RUI::Umg::UseField<int32>(Ctx, GSignalVm, FName(TEXT("Int")), -1);
		return {RUI::TextBlock(FString::Printf(TEXT("N:%d"), N))};
	}
	RUI_COMPONENT(SignalReaderComp)
} // namespace UmgTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiReverseBridgeTest, "ReactiveUI.Mvvm.ReverseBridge",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiReverseBridgeTest::RunTest(const FString&)
{
	URuiSignalViewModel* Vm = NewObject<URuiSignalViewModel>();
	Vm->AddToRoot();

	// ── the bridge broadcasts on change and skips when equal ──────────────────────────────────
	int32 IntBroadcasts = 0;
	const FDelegateHandle Handle = Vm->AddFieldValueChangedDelegate(
		URuiSignalViewModel::FFieldNotificationClassDescriptor::Int,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
			[&IntBroadcasts](UObject*, ::UE::FieldNotification::FFieldId) { ++IntBroadcasts; }));

	Vm->SetInt(5);
	TestEqual(TEXT("Int field updated"), Vm->Int, 5);
	TestEqual(TEXT("broadcast fired once"), IntBroadcasts, 1);
	Vm->SetInt(5); // equal set
	TestEqual(TEXT("equal set did not broadcast"), IntBroadcasts, 1);
	Vm->RemoveFieldValueChangedDelegate(URuiSignalViewModel::FFieldNotificationClassDescriptor::Int, Handle);

	// ── Set(FRuiValue) routes by kind ─────────────────────────────────────────────────────────
	Vm->Set(FRuiValue(3.5f));
	TestEqual(TEXT("float routed"), Vm->Float, 3.5f);
	Vm->Set(FRuiValue(true));
	TestTrue(TEXT("bool routed"), Vm->Bool);
	Vm->Set(FRuiValue(FText::FromString(TEXT("hello"))));
	TestEqual(TEXT("text routed"), Vm->Text.ToString(), FString(TEXT("hello")));
	Vm->Set(FRuiValue(FString(TEXT("world"))));
	TestEqual(TEXT("string routed to text"), Vm->Text.ToString(), FString(TEXT("world")));

	// ── round-trip: Rui writes the VM -> VM broadcasts -> a UseField consumer re-renders ──────
	UmgTest::GSignalVm = Vm;
	Vm->SetInt(5);
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::FC(&UmgTest::SignalReaderComp));
	Root->FlushSync();
	SWidget& RootWidget = *Root->GetWidget();
	TestTrue(TEXT("consumer reads the field"), UmgTest::ContainsText(RootWidget, TEXT("N:5")));

	Vm->SetInt(9);
	Root->FlushSync();
	TestTrue(TEXT("consumer re-rendered on the reverse-bridge broadcast"),
			 UmgTest::ContainsText(RootWidget, TEXT("N:9")));

	Root->Unmount();
	UmgTest::GSignalVm = nullptr;
	Vm->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
