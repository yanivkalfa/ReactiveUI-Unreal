// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The Doom demo's game-loop hook — the Unreal translation of the family's `use_doom_game`:
// owns the FGameState + brush pool + frame geometry, registers a per-frame FTSTicker in a
// mount-once effect, and each tick: polls raw input from the player controller (no input
// preprocessor — the demo POLLS, honoring the library's CommonUI promise), advances the sim
// (GameLogic::Tick), rebuilds the quad lists (BuildFrameGeometry), and bumps a version
// UseState — our coalescing renders exactly once per frame (the Godot port's lesson; no
// manual 16 ms scheduler needed).
//
// Mouse capture: taken while playing (GameOnly input, hidden cursor, relative mouse deltas),
// released on death/victory/ESC/unmount so overlay + gallery buttons stay clickable; a click
// while released re-captures (and is swallowed, never fired).
//
// Markup calls it as a user hook — the compiler injects Ctx:
//   auto View = RuiDoom::UseDoomGame(Level, Diff, RestartVersion);

#pragma once

#include "Doom/DoomScreenGeometry.h"
#include "Doom/DoomScreenTypes.h"
#include "Doom/DoomTypes.h"

#include "CoreMinimal.h"

class FRuiContext;

namespace RuiDoom
{
	/** What the game screen renders from. Pointers live as long as the component (hook-owned);
	 *  pass `Version` down to child components — the state object mutates in place, so version
	 *  inequality is what defeats their props-equality bailouts. */
	struct FDoomGameView
	{
		const FGameState* State = nullptr;
		const FDoomFrameGeometry* Geometry = nullptr;
		int32 Version = 0;
		bool bShowFps = false; // Ctrl+R toggle (family convention — the Godot bootstrap's counter)
		float Fps = 0.f;	   // smoothed frames/sec, valid while bShowFps
	};

	/** The game loop. A change to Level/Diff/RestartVersion starts a fresh NewGame. */
	RUIDEMO_API FDoomGameView UseDoomGame(FRuiContext& Ctx, int32 Level, int32 Diff, int32 RestartVersion);
} // namespace RuiDoom
