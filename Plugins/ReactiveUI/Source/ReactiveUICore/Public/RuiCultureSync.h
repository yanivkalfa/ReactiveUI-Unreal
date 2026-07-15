// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace RUI
{
	/** Culture-change → root re-render (MASTER_PLAN Phase 7 item 2).
	 *
	 *  FText itself is lazy — NSLOCTEXT literals and FText::Format results re-resolve on a culture
	 *  switch — but anything a component BAKED during render (an FString conversion, a
	 *  culture-dependent branch, a per-culture number it formatted itself) only heals on the next
	 *  render. Subscribing the reconciler to the text-revision event closes that gap: every live
	 *  root re-renders once per culture change, so what the player sees is always the current
	 *  culture's output.
	 *
	 *  Registered by the ReactiveUICore module startup; Register is idempotent (tests may call it
	 *  again — unit suites do not run StartupModule). Game-thread contract: the engine broadcasts
	 *  text-revision changes on the game thread (SetCurrentCulture is a game-thread API); off-thread
	 *  broadcasts are ignored defensively rather than racing the fiber tree. */
	REACTIVEUICORE_API void RegisterCultureSync();
	REACTIVEUICORE_API void UnregisterCultureSync();

	/** The handler body (also the test seam): mark every fiber of every live root dirty — the same
	 *  semantics HMR uses, because the failure mode is the same: bailout caches serving output
	 *  rendered under a stale environment. Coalesces into one re-render per root. */
	REACTIVEUICORE_API void RefreshAllRootsForCultureChange();
} // namespace RUI
