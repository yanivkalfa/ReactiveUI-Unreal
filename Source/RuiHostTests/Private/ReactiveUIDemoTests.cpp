// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Demos — mounts the demo gallery headlessly (the demos_test.gd analogue): the
// real check that every demo screen renders without error, drives a state update through a
// real button closure, and unmounts clean.

#include "Misc/AutomationTest.h"
#include "RuiDemoScreens.h"
#include "RuiRoot.h"
#include "RuiSlateElements.h"
#include "RuiSlateHost.h"
#include "Widgets/Input/SButton.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DemoTest
{
	/** Depth-first hunt for the first widget of a type (the gallery is a real tree now). */
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

	static int32 CountByType(SWidget& RootWidget, FName TypeName)
	{
		int32 Count = RootWidget.GetType() == TypeName ? 1 : 0;
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			Count += CountByType(Children->GetChildAt(i).Get(), TypeName);
		}
		return Count;
	}
} // namespace DemoTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDemosTest, "ReactiveUI.Demos",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiDemosTest::RunTest(const FString&)
{
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RuiDemo::GalleryRoot());
	Root->FlushSync(); // the gallery's mount effect registers its style class

	SWidget& RootWidget = Root->GetWidget().Get();
	TestTrue(TEXT("gallery mounted something"), Root->GetWidget()->GetRootPanel()->GetChildren()->Num() > 0);
	TestTrue(TEXT("counter section renders a progress bar"),
			 DemoTest::FindByType(RootWidget, FName(TEXT("SProgressBar"))) != nullptr);
	TestTrue(TEXT("styled panels render an editable text"),
			 DemoTest::FindByType(RootWidget, FName(TEXT("SEditableTextBox"))) != nullptr);
	TestTrue(TEXT("checkbox present"), DemoTest::FindByType(RootWidget, FName(TEXT("SCheckBox"))) != nullptr);
	const int32 ButtonsBefore = DemoTest::CountByType(RootWidget, FName(TEXT("SButton")));
	TestTrue(TEXT("gallery has its buttons"), ButtonsBefore >= 5);

	AddInfo(TEXT("[demos] a real button click drives state -> re-render"));
	const int32 TextsBefore = DemoTest::CountByType(RootWidget, FName(TEXT("STextBlock")));
	// The keyed list's " add " button is the 3rd button (counter -, +, then add).
	// Find it by its label instead of position to stay robust.
	SWidget* AddButton = nullptr;
	{
		TArray<SWidget*> Stack{&RootWidget};
		while (!Stack.IsEmpty() && AddButton == nullptr)
		{
			SWidget* W = Stack.Pop(EAllowShrinking::No);
			if (W->GetType() == FName(TEXT("SButton")))
			{
				FChildren* Kids = W->GetChildren();
				if (Kids->Num() > 0 && Kids->GetChildAt(0)->GetType() == FName(TEXT("STextBlock")) &&
					StaticCastSharedRef<STextBlock>(Kids->GetChildAt(0))->GetText().ToString() == TEXT(" add "))
				{
					AddButton = W;
					break;
				}
			}
			FChildren* Children = W->GetChildren();
			for (int32 i = 0; i < Children->Num(); ++i)
			{
				Stack.Push(&Children->GetChildAt(i).Get());
			}
		}
	}
	if (TestNotNull(TEXT("found the add button by label"), AddButton))
	{
		static_cast<SButton*>(AddButton)->SimulateClick();
		Root->FlushSync();
		TestEqual(TEXT("one more list row after add"), DemoTest::CountByType(RootWidget, FName(TEXT("STextBlock"))),
				  TextsBefore + 1);
	}

	AddInfo(TEXT("[demos] clean unmount"));
	Root->Unmount();
	TestEqual(TEXT("overlay empty"), Root->GetWidget()->GetRootPanel()->GetChildren()->Num(), 0);
	TestEqual(TEXT("zero live fibers"), Root->GetReconciler().NumLiveFibers(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
