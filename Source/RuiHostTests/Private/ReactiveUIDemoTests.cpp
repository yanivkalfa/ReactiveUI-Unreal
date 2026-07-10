// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Demos — the demos_test.gd analogue: every gallery entry mounts headlessly and
// unmounts clean (zero live fibers), a real button click drives state through a screen, and
// the shell's menu switches screens (old screen unmounted, new one mounted).

#include "Misc/AutomationTest.h"
#include "RuiDemoScreens.h"
#include "RuiRoot.h"
#include "RuiSlateElements.h"
#include "RuiSlateHost.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DemoTest
{
	static SWidget* FindByType(SWidget& RootWidget, FName TypeName)
	{
		if (RootWidget.GetType() == TypeName)
		{
			return &RootWidget;
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (SWidget* Found = FindByType(Children->GetChildAt(i).Get(), TypeName))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** First STextBlock whose text CONTAINS Needle (depth-first). */
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

	/** First SButton whose label CONTAINS Needle. */
	static SButton* FindButton(SWidget& RootWidget, const FString& Needle)
	{
		if (RootWidget.GetType() == FName(TEXT("SButton")))
		{
			FChildren* Kids = RootWidget.GetChildren();
			if (Kids->Num() > 0 && Kids->GetChildAt(0)->GetType() == FName(TEXT("STextBlock")) &&
				StaticCastSharedRef<STextBlock>(Kids->GetChildAt(0))->GetText().ToString().Contains(Needle))
			{
				return static_cast<SButton*>(&RootWidget);
			}
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (SButton* Found = FindButton(Children->GetChildAt(i).Get(), Needle))
			{
				return Found;
			}
		}
		return nullptr;
	}
} // namespace DemoTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDemosTest, "ReactiveUI.Demos",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiDemosTest::RunTest(const FString&)
{
	AddInfo(TEXT("[demos] every gallery entry mounts headlessly and unmounts clean"));
	for (const RuiDemo::FRuiDemoEntry& Entry : RuiDemo::GetGalleryEntries())
	{
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(Entry.Make());
		Root->FlushSync();
		TestTrue(FString::Printf(TEXT("'%s' mounted something"), *Entry.Name),
				 Root->GetWidget()->GetRootPanel()->GetChildren()->Num() > 0);
		Root->Unmount();
		TestEqual(FString::Printf(TEXT("'%s' unmounted to zero fibers"), *Entry.Name),
				  Root->GetReconciler().NumLiveFibers(), 0);
	}

	AddInfo(TEXT("[demos] a real click drives the counter screen"));
	{
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RuiDemo::GetGalleryEntries()[1].Make()); // Counter
		Root->FlushSync();
		SWidget& RootWidget = Root->GetWidget().Get();
		TestTrue(TEXT("starts at 0"), DemoTest::ContainsText(RootWidget, TEXT("Count: 0")));
		SButton* Plus = DemoTest::FindButton(RootWidget, TEXT("+"));
		if (TestNotNull(TEXT("found the + button"), Plus))
		{
			Plus->SimulateClick();
			Root->FlushSync();
			TestTrue(TEXT("count incremented"), DemoTest::ContainsText(RootWidget, TEXT("Count: 1")));
		}
	}

	AddInfo(TEXT("[demos] the shell menu switches screens (remount on switch)"));
	{
		TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RuiDemo::GalleryRoot());
		Root->FlushSync();
		SWidget& RootWidget = Root->GetWidget().Get();
		TestTrue(TEXT("starts on Hello World"), DemoTest::ContainsText(RootWidget, TEXT("Hello, world!")));

		SButton* TicTacToe = DemoTest::FindButton(RootWidget, TEXT("Tic Tac Toe"));
		if (TestNotNull(TEXT("found the Tic Tac Toe menu button"), TicTacToe))
		{
			TicTacToe->SimulateClick();
			Root->FlushSync();
			TestTrue(TEXT("switched to Tic Tac Toe"),
					 DemoTest::ContainsText(RootWidget, TEXT("Welcome to Tic Tac Toe!")));
			TestFalse(TEXT("Hello World unmounted"), DemoTest::ContainsText(RootWidget, TEXT("Hello, world!")));

			SButton* Start = DemoTest::FindButton(RootWidget, TEXT("Start Game"));
			if (TestNotNull(TEXT("found Start Game"), Start))
			{
				Start->SimulateClick();
				Root->FlushSync();
				TestTrue(TEXT("game board appeared"), DemoTest::ContainsText(RootWidget, TEXT("Player turn: X")));
			}
		}
		Root->Unmount();
		TestEqual(TEXT("gallery unmounts to zero fibers"), Root->GetReconciler().NumLiveFibers(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
