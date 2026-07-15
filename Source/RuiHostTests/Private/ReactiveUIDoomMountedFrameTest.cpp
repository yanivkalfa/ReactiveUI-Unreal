// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Doom.MountedFrame — mounted-framebuffer invariants of the REAL game screen (the
// field-test class the sim suite can't reach: markup -> reconciler -> live SCanvas slots).
// Born as the probe that caught the invisible-tint bug: every quad routed ColorAndOpacity
// through the style dict, the Image adapter had no handler, and the two alpha-0 full-viewport
// flash quads painted opaque white over the whole frame.

#include "Doom/DoomTypes.h"

#include "Misc/AutomationTest.h"
#include "RuiElementRegistry.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SCanvas.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DoomMounted
{
	// SImage keeps GetColorAndOpacityAttribute protected — a data-free peek subclass re-exports
	// it for assertions (layout-identical, so the cast is safe).
	struct FImageTintPeek : public SImage
	{
		using SImage::GetColorAndOpacityAttribute;
	};

	static SWidget* FindByType(SWidget& W, const FName& Type)
	{
		if (W.GetType() == Type)
		{
			return &W;
		}
		FChildren* Children = W.GetChildren();
		for (int32 i = 0; Children != nullptr && i < Children->Num(); ++i)
		{
			if (SWidget* Found = FindByType(Children->GetChildAt(i).Get(), Type))
			{
				return Found;
			}
		}
		return nullptr;
	}
} // namespace DoomMounted

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiDoomMountedFrameTest, "ReactiveUI.Doom.MountedFrame",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiDoomMountedFrameTest::RunTest(const FString&)
{
	if (!RUI::HasNamedFactory(FName(TEXT("DoomGameScreen"))))
	{
		AddError(TEXT("DoomGameScreen is not registered"));
		return false;
	}
	TSharedRef<FRuiRoot> Root = FRuiRoot::Create(RUI::Named(FName(TEXT("DoomGameScreen"))));
	Root->FlushSync();

	SWidget* CanvasW = DoomMounted::FindByType(Root->GetWidget().Get(), FName(TEXT("SCanvas")));
	if (!TestNotNull(TEXT("an SCanvas exists in the mounted game screen"), CanvasW))
	{
		Root->Unmount();
		return false;
	}
	FChildren* Children = CanvasW->GetChildren();
	const int32 Num = Children != nullptr ? Children->Num() : 0;
	AddInfo(FString::Printf(TEXT("[doom] canvas children: %d"), Num));
	TestTrue(TEXT("canvas holds a frame's worth of quads (>50)"), Num > 50);

	// Slot geometry actually reached the live SCanvas slots (the markup Slot.* -> adapter path).
	int32 NonDefaultPos = 0, NonDefaultSize = 0;
	for (int32 i = 0; i < Num; ++i)
	{
		const SCanvas::FSlot& Slot = static_cast<const SCanvas::FSlot&>(Children->GetSlotAt(i));
		NonDefaultPos += Slot.GetPosition().Equals(FVector2D::ZeroVector) ? 0 : 1;
		NonDefaultSize += Slot.GetSize().Equals(FVector2D(1.0, 1.0)) ? 0 : 1;
	}
	AddInfo(FString::Printf(TEXT("[doom] non-default positions: %d / %d, sizes: %d / %d"), NonDefaultPos, Num,
							NonDefaultSize, Num));
	TestTrue(TEXT("most quads carry real positions"), NonDefaultPos > Num / 2);
	TestTrue(TEXT("most quads carry real sizes"), NonDefaultSize > Num / 2);

	// The last two quads are the hurt/pickup flashes: full-viewport, always emitted, and
	// TRANSPARENT at spawn — if their style tint ever stops reaching the widget again, they
	// paint opaque white over the whole frame (the original playtest symptom).
	if (TestTrue(TEXT("at least the overlay pass mounted"), Num >= 2))
	{
		for (int32 i = Num - 2; i < Num; ++i)
		{
			const SCanvas::FSlot& Slot = static_cast<const SCanvas::FSlot&>(Children->GetSlotAt(i));
			TSharedRef<SWidget> Child = Children->GetChildAt(i);
			TestEqual(TEXT("flash quad is an SImage"), Child->GetType(), FName(TEXT("SImage")));
			TestTrue(TEXT("flash quad covers the viewport"),
					 Slot.GetSize().X >= RuiDoom::C::VIEWPORT_W && Slot.GetSize().Y >= RuiDoom::C::VIEWPORT_H);
			const FLinearColor Tint = static_cast<DoomMounted::FImageTintPeek&>(static_cast<SImage&>(Child.Get()))
										  .GetColorAndOpacityAttribute()
										  .Get()
										  .GetSpecifiedColor();
			AddInfo(FString::Printf(TEXT("[doom] flash quad %d tint alpha: %.3f"), i, Tint.A));
			TestTrue(TEXT("flash quad is transparent at spawn (tint reached the widget)"), Tint.A < KINDA_SMALL_NUMBER);
		}
	}

	Root->Unmount();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
