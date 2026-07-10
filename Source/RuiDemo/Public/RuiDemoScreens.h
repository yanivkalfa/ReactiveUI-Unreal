// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The Phase 2 demo gallery (hand-written C++ builder API — markup arrives with Phase 3):
// a hooks-driven counter, a dynamic keyed list, and styled panels. Mounted in PIE by
// ARuiDemoGameMode; mounted headless by ReactiveUI.Demos (the demos_test.gd analogue).

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"

namespace RuiDemo
{
	/** The gallery root vnode (mount with FRuiRoot::CreateInViewport / ::Create). */
	RUIDEMO_API FRuiNode GalleryRoot();
} // namespace RuiDemo
