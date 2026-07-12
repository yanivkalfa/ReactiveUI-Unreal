// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// HMR v2 Phase 3 (D-HMR settings) — the ReactiveUetkx editor settings, the C++ sibling of the Unity
// toolkit's HMR window toggles. EditorPerProjectUser (per-user, not committed), surfaced under Project
// Settings ▸ Plugins ▸ ReactiveUetkx and mirrored by the checkboxes in the HMR window. The watcher +
// controller read these at runtime.
//
// NOTE ON SHORTCUTS: the two HMR chords are NOT stored here. Unreal already persists command chords via
// the input binding manager (EditorKeyBindings.ini), edited from Editor Preferences ▸ Keyboard Shortcuts
// AND the in-window recorder — one source of truth, nothing to keep in sync. See FReactiveUetkxCommands.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ReactiveUetkxEditorSettings.generated.h"

UCLASS(config = EditorPerProjectUser, defaultconfig, meta = (DisplayName = "ReactiveUetkx"))
class UReactiveUetkxEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }

	/** Post a transient editor notification each time HMR applies a live patch. */
	UPROPERTY(config, EditAnywhere, Category = "Hot Reload",
			  meta = (DisplayName = "Show swap notifications"))
	bool bShowNotifications = true;

	/** Log every watcher event + sweep decision to LogRuiEditor (noisy; for diagnosing missed reloads). */
	UPROPERTY(config, EditAnywhere, Category = "Hot Reload", meta = (DisplayName = "Verbose watcher trace"))
	bool bVerboseWatcher = false;

	/** When HMR stops, also disable the Live Coding session so external builds work again immediately.
	 *  Off by default: keeping the session on makes the next Start instant. */
	UPROPERTY(config, EditAnywhere, Category = "Hot Reload",
			  meta = (DisplayName = "Disable Live Coding session on Stop"))
	bool bDisableSessionOnStop = false;

	/** Coalesce a burst of file events into one sweep after this quiet period (ms). */
	UPROPERTY(config, EditAnywhere, Category = "Hot Reload", meta = (ClampMin = "0", ClampMax = "2000",
			  DisplayName = "Debounce (ms)"))
	int32 DebounceMs = 300;

	/** Project-relative roots the watcher scans for .uetkx changes. */
	UPROPERTY(config, EditAnywhere, Category = "Hot Reload", meta = (DisplayName = "Watched roots"))
	TArray<FString> WatchedRoots = {TEXT("Source"), TEXT("Plugins")};
};
