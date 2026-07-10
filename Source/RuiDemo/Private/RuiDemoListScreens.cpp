// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Gallery screens ported from the Unity sibling: KeyedDiffLis (reorder/insert/remove keyed
// rows), TicTacToe (the game showcase), plus our StyledPanels (classes + controlled input).

#include "RuiContext.h"
#include "RuiDemoShared.h"
#include "RuiStyle.h"

using namespace RuiDemo;

// ── KeyedDiffLisDemoFunc ──────────────────────────────────────────────────────────────────

static FRuiNodeArray KeyedDiffComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	static const TCHAR* Seed[] = {TEXT("Alpha"), TEXT("Bravo"),	  TEXT("Charlie"), TEXT("Delta"),
								  TEXT("Echo"),	 TEXT("Foxtrot"), TEXT("Golf")};
	auto SeedItems = []()
	{
		TArray<FString> Out;
		for (const TCHAR* S : Seed)
		{
			Out.Add(S);
		}
		return Out;
	};

	auto [Items, SetItems] = Ctx.UseState<TArray<FString>>(SeedItems());
	auto [Ops, SetOps] = Ctx.UseState<int32>(0);
	auto [NextId, SetNextId] = Ctx.UseState<int32>(1);
	TFunction<void(TArray<FString>)> Set = SetItems;
	TFunction<void(int32)> SetOpCount = SetOps;
	TFunction<void(int32)> SetNext = SetNextId;
	const TArray<FString> Current = Items;
	const int32 OpsNow = Ops;
	const int32 IdNow = NextId;

	auto Apply = [Set, SetOpCount, OpsNow](TArray<FString> Next)
	{
		Set(MoveTemp(Next));
		SetOpCount(OpsNow + 1);
	};

	TArray<FRuiNode> Rows;
	Rows.Add(StyledText(TEXT("Keyed diff / LIS test"), 16.0f, FLinearColor(1.0f, 0.8f, 0.4f)));
	Rows.Add(RUI::TextBlock(TEXT("Reorder, insert, and remove keyed rows — rows stay mounted while order changes.")));
	Rows.Add(RUI::TextBlock(FString::Printf(TEXT("Operations performed: %d"), Ops)));
	Rows.Add(Gap());
	Rows.Add(RUI::Slate::HorizontalBox(FRuiHorizontalBoxProps(), {LabeledButton(TEXT("Reset"),
																				[Set, SetOpCount, SeedItems]()
																				{
																					Set(SeedItems());
																					SetOpCount(0);
																				}),
																  LabeledButton(TEXT("Reverse"),
																				[Apply, Current]()
																				{
																					TArray<FString> Next = Current;
																					Algo::Reverse(Next);
																					Apply(MoveTemp(Next));
																				}),
																  LabeledButton(TEXT("Rotate tail->head"),
																				[Apply, Current]()
																				{
																					if (!Current.IsEmpty())
																					{
																						TArray<FString> Next;
																						Next.Add(Current.Last());
																						for (int32 i = 0;
																							 i < Current.Num() - 1; ++i)
																						{
																							Next.Add(Current[i]);
																						}
																						Apply(MoveTemp(Next));
																					}
																				})}));
	Rows.Add(RUI::Slate::HorizontalBox(
		FRuiHorizontalBoxProps(), {LabeledButton(TEXT("Shuffle"),
												 [Apply, Current, OpsNow]()
												 {
													 TArray<FString> Next = Current;
													 FRandomStream Rng(OpsNow * 7919 + Next.Num());
													 for (int32 i = Next.Num() - 1; i > 0; --i)
													 {
														 Next.Swap(i, Rng.RandRange(0, i));
													 }
													 Apply(MoveTemp(Next));
												 }),
								   LabeledButton(TEXT("Insert new key"),
												 [Apply, Current, IdNow, SetNext]()
												 {
													 TArray<FString> Next = Current;
													 FRandomStream Rng(IdNow * 31 + Next.Num());
													 Next.Insert(FString::Printf(TEXT("New-%d"), IdNow),
																 Next.IsEmpty() ? 0 : Rng.RandRange(0, Next.Num() - 1));
													 SetNext(IdNow + 1);
													 Apply(MoveTemp(Next));
												 }),
								   LabeledButton(TEXT("Remove middle"),
												 [Apply, Current]()
												 {
													 if (!Current.IsEmpty())
													 {
														 TArray<FString> Next = Current;
														 Next.RemoveAt(Next.Num() / 2);
														 Apply(MoveTemp(Next));
													 }
												 })}));
	Rows.Add(Gap());
	for (int32 i = 0; i < Current.Num(); ++i)
	{
		FRuiNode Row = RUI::Slate::HorizontalBox(
			FRuiHorizontalBoxProps(),
			{RUI::TextBlock(FString::Printf(TEXT("Key: %s"), *Current[i])),
			 WithSlot(StyledText(FString::Printf(TEXT("   Index %d"), i), 9.0f, FLinearColor(0.3f, 0.9f, 0.9f)),
					  FName(TEXT("Slot.Padding")), FRuiValue(TEXT("10,0,0,0")))});
		Row.Key = FRuiKey(FName(*Current[i]));
		Rows.Add(MoveTemp(Row));
	}
	return {Padded(RUI::Slate::VerticalBox(FRuiVerticalBoxProps(), MoveTemp(Rows)))};
}
RUI_COMPONENT(KeyedDiffComp)

// ── Styled panels (ours — classes + controlled input) ─────────────────────────────────────

static FRuiNodeArray StyledPanelsComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [bDim, SetDim] = Ctx.UseState<bool>(false);
	auto [Text, SetText] = Ctx.UseState<FString>(FString(TEXT("type here")));
	TFunction<void(bool)> SetDimFn = SetDim;
	TFunction<void(FString)> SetTextFn = SetText;

	FRuiCheckBoxProps Check;
	Check.SetbIsChecked(bDim);
	Check.SetOnCheckStateChanged(FRuiCallback::Create([SetDimFn](const FRuiValue& V) { SetDimFn(V.BoolValue); }));

	FRuiEditableTextBoxProps Edit;
	Edit.SetText(FText::FromString(Text));
	Edit.SetOnTextChanged(FRuiCallback::Create([SetTextFn](const FRuiValue& V) { SetTextFn(V.TextValue.ToString()); }));

	FRuiNode Mirror = RUI::TextBlock(FString::Printf(TEXT("mirror: %s"), *Text));
	TSharedRef<FRuiTextBlockProps> MirrorProps =
		MakeShared<FRuiTextBlockProps>(static_cast<const FRuiTextBlockProps&>(*Mirror.Props));
	if (bDim)
	{
		MirrorProps->Classes.Add(FName(TEXT("rui-demo-dim")));
	}
	Mirror.Props = MirrorProps;

	return {Padded(RUI::Slate::VerticalBox(
		FRuiVerticalBoxProps(), {StyledText(TEXT("Styled panels"), 16.0f, FLinearColor(0.6f, 1.0f, 0.6f)), Gap(),
								 RUI::Slate::CheckBox(MoveTemp(Check), {RUI::TextBlock(TEXT("dim the mirror"))}), Gap(),
								 RUI::Slate::EditableTextBox(MoveTemp(Edit)), Gap(), MoveTemp(Mirror)}))};
}
RUI_COMPONENT(StyledPanelsComp)

// ── TicTacToe ─────────────────────────────────────────────────────────────────────────────

static FRuiNodeArray TicTacToeComp(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto EmptyGrid = []()
	{
		TArray<FString> G;
		G.SetNum(9);
		return G;
	};
	auto [bStarted, SetStarted] = Ctx.UseState<bool>(false);
	auto [Turn, SetTurn] = Ctx.UseState<FString>(FString(TEXT("X")));
	auto [Grid, SetGrid] = Ctx.UseState<TArray<FString>>(EmptyGrid());
	auto [Winner, SetWinner] = Ctx.UseState<FString>(FString());

	TFunction<void(bool)> SetStartedFn = SetStarted;
	TFunction<void(FString)> SetTurnFn = SetTurn;
	TFunction<void(TArray<FString>)> SetGridFn = SetGrid;
	TFunction<void(FString)> SetWinnerFn = SetWinner;
	const TArray<FString> GridNow = Grid;
	const FString TurnNow = Turn;
	const FString WinnerNow = Winner;

	auto Restart = [SetStartedFn, SetTurnFn, SetGridFn, SetWinnerFn, EmptyGrid]()
	{
		SetStartedFn(true);
		SetTurnFn(FString(TEXT("X")));
		SetWinnerFn(FString());
		SetGridFn(EmptyGrid());
	};

	auto CheckWinner = [](const TArray<FString>& G) -> FString
	{
		static const int32 Lines[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
										  {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
		for (const int32* L : Lines)
		{
			if (!G[L[0]].IsEmpty() && G[L[0]] == G[L[1]] && G[L[0]] == G[L[2]])
			{
				return G[L[0]];
			}
		}
		return FString();
	};

	int32 MovesLeft = 0;
	for (const FString& Cell : GridNow)
	{
		if (Cell.IsEmpty())
		{
			++MovesLeft;
		}
	}

	TArray<FRuiNode> Rows;
	Rows.Add(StyledText(TEXT("Welcome to Tic Tac Toe!"), 16.0f, FLinearColor(0.4f, 0.8f, 1.0f)));
	Rows.Add(Gap());
	Rows.Add(RUI::Slate::HorizontalBox(FRuiHorizontalBoxProps(),
									   {LabeledButton(bStarted ? TEXT("Restart Game") : TEXT("Start Game"), Restart)}));
	if (bStarted)
	{
		Rows.Add(Gap());
		if (!WinnerNow.IsEmpty())
		{
			Rows.Add(StyledText(FString::Printf(TEXT("Player %s is the winner"), *WinnerNow), 14.0f,
								FLinearColor(0.6f, 1.0f, 0.6f)));
		}
		else if (MovesLeft == 0)
		{
			Rows.Add(StyledText(TEXT("Draw!"), 14.0f, FLinearColor(1.0f, 0.8f, 0.4f)));
		}
		else
		{
			Rows.Add(RUI::TextBlock(FString::Printf(TEXT("Player turn: %s   (moves left: %d)"), *TurnNow, MovesLeft)));
		}
		Rows.Add(Gap());
		for (int32 R = 0; R < 3; ++R)
		{
			TArray<FRuiNode> Cells;
			for (int32 C = 0; C < 3; ++C)
			{
				const int32 Index = R * 3 + C;
				FRuiBoxProps CellBox;
				CellBox.SetWidthOverride(44.0f);
				CellBox.SetHeightOverride(44.0f);
				FRuiNode Cell = RUI::Slate::Box(
					MoveTemp(CellBox),
					{LabeledButton(
						GridNow[Index].IsEmpty() ? TEXT(" ") : *GridNow[Index],
						[SetGridFn, SetTurnFn, SetWinnerFn, CheckWinner, GridNow, TurnNow, WinnerNow, Index]()
						{
							if (!WinnerNow.IsEmpty() || !GridNow[Index].IsEmpty())
							{
								return;
							}
							TArray<FString> Next = GridNow;
							Next[Index] = TurnNow;
							SetGridFn(Next);
							const FString Win = CheckWinner(Next);
							if (!Win.IsEmpty())
							{
								SetWinnerFn(Win);
							}
							else
							{
								SetTurnFn(TurnNow == TEXT("X") ? FString(TEXT("O")) : FString(TEXT("X")));
							}
						})});
				Cell.Key = FRuiKey(Index);
				Cells.Add(MoveTemp(Cell));
			}
			FRuiNode Row = RUI::Slate::HorizontalBox(FRuiHorizontalBoxProps(), MoveTemp(Cells));
			Row.Key = FRuiKey(100 + R);
			Rows.Add(MoveTemp(Row));
		}
	}
	return {Padded(RUI::Slate::VerticalBox(FRuiVerticalBoxProps(), MoveTemp(Rows)))};
}
RUI_COMPONENT(TicTacToeComp)

namespace RuiDemo
{
	FRuiNode KeyedDiffScreen()
	{
		return RUI::FC(&KeyedDiffComp);
	}
	FRuiNode StyledPanelsScreen()
	{
		return RUI::FC(&StyledPanelsComp);
	}
	FRuiNode TicTacToeScreen()
	{
		return RUI::FC(&TicTacToeComp);
	}
} // namespace RuiDemo
