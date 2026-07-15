// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Doom/DoomGameHook.h"

#include "Containers/Ticker.h"
#include "Doom/DoomGameLogic.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "RuiContext.h"
#include "RuiDemoSupport.h"

namespace RuiDoom
{
	namespace
	{
		/** Everything the loop owns, boxed so the ticker lambda can hold it weakly. */
		struct FDoomSession
		{
			FGameState State;
			FDoomBrushPool Brushes;
			FDoomFrameGeometry Geometry;
			int32 Level = -1;
			int32 Diff = -1;
			int32 RestartVersion = -1;
			FTSTicker::FDelegateHandle Ticker;
			bool bCaptured = false;
			bool bUserReleased = false; // ESC pressed — stay released until a re-capture click
			bool bShowFps = false;		// Ctrl+R toggle
			float FpsSmoothed = 0.f;

			~FDoomSession()
			{
				if (Ticker.IsValid())
				{
					FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
				}
			}
		};

		APlayerController* DemoPlayerController()
		{
			UWorld* World = RuiDemo::GetDemoWorld();
			return World != nullptr ? World->GetFirstPlayerController() : nullptr;
		}

		void SetMouseCaptured(FDoomSession& S, bool bCapture)
		{
			APlayerController* PC = DemoPlayerController();
			if (PC == nullptr || S.bCaptured == bCapture)
			{
				return;
			}
			S.bCaptured = bCapture;
			if (bCapture)
			{
				PC->bShowMouseCursor = false;
				FInputModeGameOnly Mode;
				Mode.SetConsumeCaptureMouseDown(true);
				PC->SetInputMode(Mode);
			}
			else
			{
				// The gallery's UI-friendly mode (matches ARuiDemoGameMode::BeginPlay).
				PC->bShowMouseCursor = true;
				FInputModeGameAndUI Mode;
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				Mode.SetHideCursorDuringCapture(false);
				PC->SetInputMode(Mode);
			}
		}

		/** Poll the controller into an FInputCmd (the accumulator the siblings needed is
		 *  unnecessary here — polling per tick sees held keys directly; one-shots use
		 *  WasInputKeyJustPressed). Returns a zero command when there is no player. */
		FInputCmd PollInput(FDoomSession& S)
		{
			FInputCmd Cmd;
			APlayerController* PC = DemoPlayerController();
			if (PC == nullptr)
			{
				return Cmd;
			}

			// Ctrl+R toggles the FPS readout (family convention — works captured or not).
			if (PC->WasInputKeyJustPressed(EKeys::R) &&
				(PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl)))
			{
				S.bShowFps = !S.bShowFps;
			}

			const bool bWantCapture = S.State.Player.Alive && !S.State.GameOver && !S.State.Victory && !S.bUserReleased;

			// ESC releases; a click while released re-captures (swallowed, never fired).
			if (S.bCaptured && PC->WasInputKeyJustPressed(EKeys::Escape))
			{
				S.bUserReleased = true;
				SetMouseCaptured(S, false);
			}
			else if (!S.bCaptured && bWantCapture)
			{
				SetMouseCaptured(S, true);
			}
			else if (S.bUserReleased && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
			{
				S.bUserReleased = false;
				SetMouseCaptured(S, true);
				return Cmd; // swallow the re-capture click
			}
			if (S.bCaptured && !(S.State.Player.Alive && !S.State.GameOver && !S.State.Victory))
			{
				SetMouseCaptured(S, false); // death/victory frees the cursor for the overlay
			}
			if (!S.bCaptured)
			{
				return Cmd; // released = the UI owns input
			}

			Cmd.Forward = PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up);
			Cmd.Back = PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down);
			Cmd.StrafeLeft = PC->IsInputKeyDown(EKeys::A);
			Cmd.StrafeRight = PC->IsInputKeyDown(EKeys::D);
			Cmd.TurnLeft = PC->IsInputKeyDown(EKeys::Left);
			Cmd.TurnRight = PC->IsInputKeyDown(EKeys::Right);
			Cmd.Run = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
			Cmd.Attack = PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::LeftControl);
			Cmd.Use = PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::SpaceBar);
			Cmd.Jump = PC->IsInputKeyDown(EKeys::SpaceBar);
			Cmd.Crouch = PC->IsInputKeyDown(EKeys::C);

			static const FKey WeaponKeys[] = {EKeys::One,  EKeys::Two, EKeys::Three, EKeys::Four,
											  EKeys::Five, EKeys::Six, EKeys::Seven};
			for (int32 i = 0; i < UE_ARRAY_COUNT(WeaponKeys); ++i)
			{
				if (PC->WasInputKeyJustPressed(WeaponKeys[i]))
				{
					Cmd.WeaponSwitch = static_cast<uint8>(i + 1);
					break;
				}
			}

			// RAW mouse counts (≈ pixels), NOT GetInputMouseDelta — the engine massages key
			// values through the MouseX/MouseY axis config (BaseInput.ini Sensitivity=0.07),
			// which crushed the family's pixel-calibrated constants to 7% ("sensitivity too
			// low", first playtest). Raw is what the siblings' event handlers see.
			float DX = 0.f, DY = 0.f;
			if (PC->PlayerInput != nullptr)
			{
				DX = PC->PlayerInput->GetRawKeyValue(EKeys::MouseX);
				DY = PC->PlayerInput->GetRawKeyValue(EKeys::MouseY);
			}
			Cmd.YawDelta = DX * C::MOUSE_YAW_SENS;
			// UE's MouseY is positive-UP (Unity's convention); FInputCmd carries the Y-DOWN
			// pixel delta the ported game logic consumes (it applies MOUSE_PITCH_SENS itself —
			// the old `DY * MOUSE_PITCH_SENS` here both double-applied the sens and inverted
			// the axis; same trap the Godot port documented in its bootstrap).
			Cmd.PitchDelta = -DY;
			return Cmd;
		}
	} // namespace

	FDoomGameView UseDoomGame(FRuiContext& Ctx, int32 Level, int32 Diff, int32 RestartVersion)
	{
		const TSharedRef<TRuiRef<TSharedPtr<FDoomSession>>>& Box = Ctx.UseRef<TSharedPtr<FDoomSession>>();
		auto [Version, SetVersion] = Ctx.UseState<int32>(0);

		if (!Box->Current.IsValid())
		{
			Box->Current = MakeShared<FDoomSession>();
		}
		const TSharedPtr<FDoomSession>& Session = Box->Current;

		// (Re)start on parameter change — level/difficulty select or an overlay Restart.
		if (Session->Level != Level || Session->Diff != Diff || Session->RestartVersion != RestartVersion)
		{
			Session->Level = Level;
			Session->Diff = Diff;
			Session->RestartVersion = RestartVersion;
			Session->State = NewGame(Level, static_cast<EDifficulty>(Diff));
			Session->Brushes.Reset();
			Session->bUserReleased = false;
			BuildFrameGeometry(Session->State, Session->Brushes, FVector2D(C::VIEWPORT_W, C::VIEWPORT_H),
							   Session->Geometry);
		}

		// The loop: one FTSTicker for the component lifetime; per-frame sim + geometry + one
		// coalesced re-render via the version bump (a functional update — no stale captures).
		TWeakPtr<FDoomSession> WeakSession = Session;
		TRuiSetter<int32> Bump = SetVersion;
		Ctx.UseEffect(
			[WeakSession, Bump]() -> FRuiEffectCleanup
			{
				TSharedPtr<FDoomSession> Pinned = WeakSession.Pin();
				if (Pinned.IsValid())
				{
					Pinned->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
						[WeakSession, Bump](float Dt)
						{
							TSharedPtr<FDoomSession> S = WeakSession.Pin();
							if (!S.IsValid())
							{
								return false; // session died — unregister
							}
							const float Clamped = FMath::Min(Dt, 0.05f); // hitch guard (family rule)
							// Smoothed FPS for the Ctrl+R readout (UNclamped dt — real wall time).
							const float Inst = Dt > KINDA_SMALL_NUMBER ? 1.f / Dt : 0.f;
							S->FpsSmoothed = S->FpsSmoothed <= 0.f ? Inst : FMath::Lerp(S->FpsSmoothed, Inst, 0.1f);
							Tick(S->State, Clamped, PollInput(*S));
							BuildFrameGeometry(S->State, S->Brushes, FVector2D(C::VIEWPORT_W, C::VIEWPORT_H),
											   S->Geometry);
							Bump([](const int32& V) { return V + 1; });
							return true;
						}));
				}
				return [WeakSession]()
				{
					if (TSharedPtr<FDoomSession> Pinned = WeakSession.Pin())
					{
						if (Pinned->Ticker.IsValid())
						{
							FTSTicker::GetCoreTicker().RemoveTicker(Pinned->Ticker);
							Pinned->Ticker.Reset();
						}
						SetMouseCaptured(*Pinned, false); // never leave PIE cursorless
					}
				};
			},
			RUI::Deps());

		FDoomGameView View;
		View.State = &Session->State;
		View.Geometry = &Session->Geometry;
		View.Version = Version;
		View.bShowFps = Session->bShowFps;
		View.Fps = Session->FpsSmoothed;
		return View;
	}
} // namespace RuiDoom
