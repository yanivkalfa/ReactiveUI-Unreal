// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Gallery screens ported from the Unity sibling: CustomDraw (the RuiCanvas draw_fn
// trampoline — polygon by state, tint toggle, stable-callback + RedrawKey) and StressTest
// (N keyed boxes animated every frame through the full render→diff→commit pipeline; boxes
// move via RenderTranslation so layout stays put while the reconciler sweats).

#include "Framework/Application/SlateApplication.h"
#include "Math/RandomStream.h"
#include "Rendering/DrawElements.h"
#include "RuiContext.h"
#include "RuiDemoShared.h"
#include "Styling/CoreStyle.h"

using namespace RuiDemo;

// ── CustomDrawDemoFunc ────────────────────────────────────────────────────────────────────

namespace
{
	int32 DrawPolygon(const FGeometry& Geo, FSlateWindowElementList& Out, int32 LayerId, int32 Sides)
	{
		const FVector2D Center = FVector2D(Geo.GetLocalSize()) * 0.5;
		const double Radius = FMath::Min(Center.X, Center.Y) - 8.0;
		TArray<FVector2D> Points;
		for (int32 i = 0; i <= Sides; ++i)
		{
			const double Angle = 2.0 * PI * i / Sides - PI / 2.0;
			Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(Out, LayerId + 1, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None,
									 FLinearColor(0.4f, 0.8f, 1.0f), true, 2.0f);
		return LayerId + 1;
	}

	int32 DrawQuad(const FGeometry& Geo, FSlateWindowElementList& Out, int32 LayerId, bool bBlue)
	{
		FSlateDrawElement::MakeBox(Out, LayerId + 1, Geo.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"),
								   ESlateDrawEffect::None,
								   bBlue ? FLinearColor(0.2f, 0.4f, 0.9f) : FLinearColor(0.95f, 0.55f, 0.15f));
		return LayerId + 1;
	}

	int32 DrawScatter(const FGeometry& Geo, FSlateWindowElementList& Out, int32 LayerId)
	{
		// Re-seeds per PAINT — the whole point of the RedrawKey demo (same fn, new pixels).
		FRandomStream Rng(static_cast<int32>(FPlatformTime::Cycles()));
		const FVector2D Size = FVector2D(Geo.GetLocalSize());
		for (int32 i = 0; i < 60; ++i)
		{
			const FVector2D Pos(Rng.FRandRange(0.0f, Size.X - 3.0f), Rng.FRandRange(0.0f, Size.Y - 3.0f));
			FSlateDrawElement::MakeBox(Out, LayerId + 1,
									   Geo.ToPaintGeometry(FVector2D(3.0, 3.0), FSlateLayoutTransform(Pos)),
									   FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None,
									   FLinearColor(0.6f, 1.0f, 0.6f, Rng.FRandRange(0.4f, 1.0f)));
		}
		return LayerId + 1;
	}

	FRuiNode DrawCanvas(TSharedPtr<FRuiDrawFn> Fn, int64 RedrawKey = 0)
	{
		FRuiCanvasProps P;
		P.SetDrawFn(MoveTemp(Fn));
		P.SetRedrawKey(RedrawKey);
		P.SetCanvasSize(FVector2D(360.0f, 110.0f));
		return RUI::Slate::RuiCanvas(MoveTemp(P));
	}
} // namespace

static FRuiNodeArray CustomDrawComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Sides, SetSides] = Ctx.UseState<int32>(3);
	auto [bBlue, SetBlue] = Ctx.UseState<bool>(true);
	auto [RedrawTick, SetRedrawTick] = Ctx.UseState<int32>(0);
	TFunction<void(int32)> SetSidesFn = SetSides;
	TFunction<void(bool)> SetBlueFn = SetBlue;
	TFunction<void(int32)> SetTickFn = SetRedrawTick;
	const int32 SidesNow = Sides;
	const bool bBlueNow = bBlue;
	const int32 TickNow = RedrawTick;

	// 1+2: the draw fn's IDENTITY tracks its inputs (UseMemo deps) -> repaint on change.
	const TSharedPtr<FRuiDrawFn>& PolyFn = Ctx.UseMemo<TSharedPtr<FRuiDrawFn>>(
		[SidesNow]()
		{
			return RUI::Slate::MakeDrawFn([SidesNow](const FGeometry& G, FSlateWindowElementList& O, int32 L)
										  { return DrawPolygon(G, O, L, SidesNow); });
		},
		RUI::Deps(SidesNow));
	const TSharedPtr<FRuiDrawFn>& QuadFn = Ctx.UseMemo<TSharedPtr<FRuiDrawFn>>(
		[bBlueNow]()
		{
			return RUI::Slate::MakeDrawFn([bBlueNow](const FGeometry& G, FSlateWindowElementList& O, int32 L)
										  { return DrawQuad(G, O, L, bBlueNow); });
		},
		RUI::Deps(bBlueNow));
	// 3: STABLE fn + RedrawKey bump forces the repaint instead.
	const TSharedPtr<FRuiDrawFn>& ScatterFn =
		Ctx.UseMemo<TSharedPtr<FRuiDrawFn>>([]() { return RUI::Slate::MakeDrawFn(&DrawScatter); }, RUI::Deps());

	return {Padded(RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{StyledText(TEXT("Custom Rendering Demo (RuiCanvas draw_fn)"), 16.0f, FLinearColor(0.4f, 0.8f, 1.0f)), Gap(),
		 RUI::TextBlock(TEXT("1. Polygon — redraws when 'sides' state changes")), DrawCanvas(PolyFn), Gap(),
		 RUI::Slate::HorizontalBox(
			 FRuiHorizontalBoxProps(),
			 {LabeledButton(TEXT("Add side"), [SetSidesFn, SidesNow]() { SetSidesFn(SidesNow + 1); }),
			  LabeledButton(TEXT("Reset"), [SetSidesFn]() { SetSidesFn(3); }),
			  RUI::TextBlock(FString::Printf(TEXT("  sides = %d"), SidesNow))}),
		 Gap(), RUI::TextBlock(TEXT("2. Tinted quad — toggles color")), DrawCanvas(QuadFn), Gap(),
		 RUI::Slate::HorizontalBox(
			 FRuiHorizontalBoxProps(),
			 {LabeledButton(TEXT("Toggle color"), [SetBlueFn, bBlueNow]() { SetBlueFn(!bBlueNow); }),
			  RUI::TextBlock(bBlueNow ? TEXT("  blue") : TEXT("  orange"))}),
		 Gap(), RUI::TextBlock(TEXT("3. Stable callback + RedrawKey — repaint on demand")),
		 DrawCanvas(ScatterFn, TickNow), Gap(),
		 RUI::Slate::HorizontalBox(
			 FRuiHorizontalBoxProps(),
			 {LabeledButton(TEXT("Shuffle (bump RedrawKey)"), [SetTickFn, TickNow]() { SetTickFn(TickNow + 1); }),
			  RUI::TextBlock(FString::Printf(TEXT("  redrawKey = %d"), TickNow))})}))};
}
RUI_COMPONENT(CustomDrawComp)

// ── StressTest ────────────────────────────────────────────────────────────────────────────

namespace
{
	constexpr float StressAreaW = 620.0f;
	constexpr float StressAreaH = 320.0f;

	struct FStressBox
	{
		int32 Id = 0;
		FVector2D Pos = FVector2D::ZeroVector;
		FVector2D Vel = FVector2D::ZeroVector;
		float Size = 10.0f;
		FLinearColor Color = FLinearColor::White;
	};

	struct FStressStats
	{
		float AvgFps = 0.0f;
		float Elapsed = 0.0f;
		int32 Frames = 0;
		bool bFinished = false;
		bool operator==(const FStressStats& O) const
		{
			return AvgFps == O.AvgFps && Elapsed == O.Elapsed && Frames == O.Frames && bFinished == O.bFinished;
		}
	};
} // namespace

static FRuiNodeArray StressTestComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [CountText, SetCountText] = Ctx.UseState<FString>(FString(TEXT("300")));
	auto [DurationText, SetDurationText] = Ctx.UseState<FString>(FString(TEXT("10")));
	auto [bRunning, SetRunning] = Ctx.UseState<bool>(false);
	auto [Version, SetVersion] = Ctx.UseState<int32>(0);
	auto [Stats, SetStats] = Ctx.UseState<FStressStats>(FStressStats());
	auto [RenderTick, SetRenderTick] = Ctx.UseState<int32>(0);
	(void)RenderTick; // consumed implicitly: each bump re-renders the box field below

	TFunction<void(FString)> SetCountFn = SetCountText;
	TFunction<void(FString)> SetDurationFn = SetDurationText;
	TFunction<void(bool)> SetRunningFn = SetRunning;
	TFunction<void(int32)> SetVersionFn = SetVersion;
	TFunction<void(FStressStats)> SetStatsFn = SetStats;
	TFunction<void(int32)> SetRenderTickFn = SetRenderTick;

	// Simulation state lives OUTSIDE render state (mutated per tick, read by render).
	TSharedRef<TRuiRef<TArray<FStressBox>>> Boxes = Ctx.UseRef<TArray<FStressBox>>();

	const bool bRunningNow = bRunning;
	const int32 VersionNow = Version;
	const FString CountNow = CountText;
	const FString DurationNow = DurationText;
	IRuiHostConfig* Host = &Ctx.GetHost();

	// The frame loop: a self-re-arming RequestFrame chain (the Suspense poll-driver pattern).
	Ctx.UseEffect(
		[bRunningNow, CountNow, DurationNow, Boxes, Host, SetStatsFn, SetRenderTickFn, SetRunningFn,
		 VersionNow]() -> TFunction<void()>
		{
			if (!bRunningNow)
			{
				return []() {};
			}
			const int32 Count = FMath::Clamp(FCString::Atoi(*CountNow), 1, 10000);
			const float Duration = FMath::Clamp(FCString::Atof(*DurationNow), 1.0f, 600.0f);

			TArray<FStressBox>& B = Boxes->Current;
			B.Reset(Count);
			FRandomStream Rng(VersionNow * 7919 + Count);
			for (int32 i = 0; i < Count; ++i)
			{
				FStressBox Box;
				Box.Id = i;
				Box.Size = Rng.FRandRange(6.0f, 18.0f);
				Box.Pos = FVector2D(Rng.FRandRange(0.0f, StressAreaW - Box.Size),
									Rng.FRandRange(0.0f, StressAreaH - Box.Size));
				Box.Vel = FVector2D(Rng.FRandRange(-120.0f, 120.0f), Rng.FRandRange(-120.0f, 120.0f));
				Box.Color = FLinearColor::MakeFromHSV8(static_cast<uint8>(Rng.RandRange(0, 255)), 180, 230);
				B.Add(Box);
			}

			TSharedRef<bool> Cancelled = MakeShared<bool>(false);
			TSharedRef<double> Start = MakeShared<double>(FPlatformTime::Seconds());
			TSharedRef<double> Last = MakeShared<double>(*Start);
			TSharedRef<int32> Frames = MakeShared<int32>(0);
			TSharedRef<int32> Tick = MakeShared<int32>(0);

			TSharedRef<TFunction<void()>> Loop = MakeShared<TFunction<void()>>();
			*Loop = [Cancelled, Start, Last, Frames, Tick, Boxes, Host, Duration, SetStatsFn, SetRenderTickFn,
					 SetRunningFn, Loop]()
			{
				if (*Cancelled)
				{
					return;
				}
				const double Now = FPlatformTime::Seconds();
				const float Dt = FMath::Min(static_cast<float>(Now - *Last), 0.05f);
				*Last = Now;
				++(*Frames);
				for (FStressBox& Box : Boxes->Current)
				{
					Box.Pos += Box.Vel * Dt;
					if (Box.Pos.X < 0.0f || Box.Pos.X > StressAreaW - Box.Size)
					{
						Box.Vel.X *= -1.0f;
						Box.Pos.X = FMath::Clamp<double>(Box.Pos.X, 0.0, StressAreaW - Box.Size);
					}
					if (Box.Pos.Y < 0.0f || Box.Pos.Y > StressAreaH - Box.Size)
					{
						Box.Vel.Y *= -1.0f;
						Box.Pos.Y = FMath::Clamp<double>(Box.Pos.Y, 0.0, StressAreaH - Box.Size);
					}
				}
				const float Elapsed = static_cast<float>(Now - *Start);
				FStressStats S;
				S.Elapsed = Elapsed;
				S.Frames = *Frames;
				S.AvgFps = Elapsed > 0.0f ? *Frames / Elapsed : 0.0f;
				S.bFinished = Elapsed >= Duration;
				SetStatsFn(S);
				if (S.bFinished)
				{
					SetRunningFn(false); // effect cleanup cancels the loop
					return;
				}
				SetRenderTickFn(++(*Tick)); // one coalesced re-render per frame
				Host->RequestFrame(*Loop);
			};
			Host->RequestFrame(*Loop);
			return [Cancelled]() { *Cancelled = true; };
		},
		RUI::Deps(bRunningNow, VersionNow));

	const FStressStats S = Stats;
	FString Status;
	if (S.bFinished)
	{
		Status = FString::Printf(TEXT("DONE — %d boxes | Avg FPS: %.1f | Duration: %.1fs | Frames: %d"),
								 Boxes->Current.Num(), S.AvgFps, S.Elapsed, S.Frames);
	}
	else if (bRunningNow)
	{
		Status = FString::Printf(TEXT("Stress — %d boxes | Avg FPS: %.1f | %.1fs | Frames: %d"), Boxes->Current.Num(),
								 S.AvgFps, S.Elapsed, S.Frames);
	}
	else
	{
		Status = TEXT("Stress Test — Ready (open `stat ReactiveUI` to watch the reconciler)");
	}

	FRuiEditableTextBoxProps DurationField;
	DurationField.SetText(FText::FromString(DurationNow));
	DurationField.SetOnTextChanged(
		FRuiCallback::Create([SetDurationFn](const FRuiValue& V) { SetDurationFn(V.TextValue.ToString()); }));
	FRuiEditableTextBoxProps CountField;
	CountField.SetText(FText::FromString(CountNow));
	CountField.SetOnTextChanged(
		FRuiCallback::Create([SetCountFn](const FRuiValue& V) { SetCountFn(V.TextValue.ToString()); }));

	// The box field: N keyed widgets whose style changes EVERY frame — the actual stress.
	TArray<FRuiNode> BoxNodes;
	BoxNodes.Reserve(Boxes->Current.Num());
	for (const FStressBox& Box : Boxes->Current)
	{
		FRuiBorderProps Fill;
		Fill.SetBorderImage(FName(TEXT("WhiteBrush"))); // solid fill, tinted below
		Fill.SetBorderBackgroundColor(Box.Color);
		FRuiBoxProps SizeBox;
		SizeBox.SetWidthOverride(Box.Size);
		SizeBox.SetHeightOverride(Box.Size);
		FRuiNode Node = RUI::Slate::Box(MoveTemp(SizeBox), {RUI::Slate::Border(MoveTemp(Fill))});
		TSharedRef<FRuiBoxProps> Props = MakeShared<FRuiBoxProps>(static_cast<const FRuiBoxProps&>(*Node.Props));
		Props->Style = MakeShared<FRuiStyleDict>();
		Props->Style->Add(FName(TEXT("RenderTranslation")), FRuiValue(Box.Pos));
		Props->SlotProps = MakeShared<FRuiStyleDict>();
		Props->SlotProps->Add(FName(TEXT("Slot.HAlign")), FRuiValue(FName(TEXT("left"))));
		Props->SlotProps->Add(FName(TEXT("Slot.VAlign")), FRuiValue(FName(TEXT("top"))));
		Node.Props = Props;
		Node.Key = FRuiKey(Box.Id);
		BoxNodes.Add(MoveTemp(Node));
	}

	FRuiBoxProps Area;
	Area.SetWidthOverride(StressAreaW);
	Area.SetHeightOverride(StressAreaH);

	const int32 VersionForStart = VersionNow;
	return {Padded(RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(),
		{StyledText(Status, 12.0f, FLinearColor(1.0f, 0.85f, 0.5f)), Gap(),
		 RUI::Slate::HorizontalBox(
			 FRuiHorizontalBoxProps(),
			 {RUI::TextBlock(TEXT("Duration(s): ")), RUI::Slate::EditableTextBox(MoveTemp(DurationField)),
			  RUI::TextBlock(TEXT("  Boxes: ")), RUI::Slate::EditableTextBox(MoveTemp(CountField)),
			  WithSlot(LabeledButton(bRunningNow ? TEXT("Running...") : (S.bFinished ? TEXT("Restart") : TEXT("Start")),
									 [bRunningNow, SetRunningFn, SetVersionFn, VersionForStart]()
									 {
										 if (!bRunningNow)
										 {
											 SetVersionFn(VersionForStart + 1);
											 SetRunningFn(true);
										 }
									 }),
					   FName(TEXT("Slot.Padding")), FRuiValue(TEXT("8,0,0,0")))}),
		 Gap(), RUI::Slate::Box(MoveTemp(Area), {RUI::Slate::Overlay(FRuiOverlayProps(), MoveTemp(BoxNodes))})}))};
}
RUI_COMPONENT(StressTestComp)

namespace RuiDemo
{
	FRuiNode CustomDrawScreen()
	{
		return RUI::FC(&CustomDrawComp);
	}
	FRuiNode StressTestScreen()
	{
		return RUI::FC(&StressTestComp);
	}
} // namespace RuiDemo
