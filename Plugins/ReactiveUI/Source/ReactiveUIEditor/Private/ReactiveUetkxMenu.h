// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// HMR v2 Phase 2 (D-HMR-7) — the top-level `ReactiveUetkx` main-menu, the C++ sibling of the Unity
// toolkit's `ReactiveUITK` menu. A thin UToolMenus registration: it opens the HMR window + the preview
// tab and hosts a Debug submenu. All behaviour lives in FUetkxHmrController / the tab spawners; this is
// only the entry points. Demos submenu is deferred (TD-HMR-DEMOS).

#pragma once

#include "CoreMinimal.h"

namespace ReactiveUetkxTabs
{
	// Shared with ReactiveUIEditorModule's nomad-tab registrations.
	extern const FName HmrWindow;
	extern const FName Preview;
} // namespace ReactiveUetkxTabs

class FReactiveUetkxMenu
{
public:
	/** Register the top-level menu on the editor's main menu bar (idempotent; call after ToolMenus is up). */
	static void Register();

	/** Remove the menu (module shutdown). */
	static void Unregister();
};
