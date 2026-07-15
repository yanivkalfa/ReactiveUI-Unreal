// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiCultureSync.h"

#include "HAL/PlatformTLS.h"
#include "Internationalization/TextLocalizationManager.h"
#include "RuiReconciler.h"

namespace
{
	FDelegateHandle GCultureSyncHandle;
} // namespace

namespace RUI
{
	void RefreshAllRootsForCultureChange()
	{
		if (!IsInGameThread())
		{
			// The engine broadcasts on the game thread; anything else would race the fiber tree.
			return;
		}
		FRuiReconciler::ForEachLive([](FRuiReconciler& Reconciler) { Reconciler.HmrRefreshAll(); });
	}

	void RegisterCultureSync()
	{
		if (GCultureSyncHandle.IsValid())
		{
			return; // idempotent — the module registers once; tests may call again
		}
		GCultureSyncHandle =
			FTextLocalizationManager::Get().OnTextRevisionChangedEvent.AddStatic(&RefreshAllRootsForCultureChange);
	}

	void UnregisterCultureSync()
	{
		if (GCultureSyncHandle.IsValid())
		{
			FTextLocalizationManager::Get().OnTextRevisionChangedEvent.Remove(GCultureSyncHandle);
			GCultureSyncHandle.Reset();
		}
	}
} // namespace RUI
