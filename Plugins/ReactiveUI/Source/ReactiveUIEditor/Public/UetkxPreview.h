// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-006 — the in-editor `.uetkx` READ-ONLY live preview. FUetkxPreview is the headless-testable
// core: it scans a `.uetkx` source, picks a component, builds the dev-loop interpreter definition,
// and mounts it as a live ReactiveUI root — collecting parse diagnostics + interp fallback notes for
// display. The editor tab (SUetkxPreviewPanel) is a thin Slate shell around it: pick a file, see it
// render, read the notes. Preview only — the interpreter's honest subset (D-20) renders structure +
// UseState; anything it skips is surfaced as a note, never a silent gap.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

class FRuiRoot;

class REACTIVEUIEDITOR_API FUetkxPreview : public TSharedFromThis<FUetkxPreview>
{
public:
	/** Build a preview from `.uetkx` source. `ComponentName` picks a component (None = the first).
	 *  Always returns a preview: on failure it carries diagnostics + a placeholder widget. */
	static TSharedRef<FUetkxPreview> FromSource(const FString& Source, const FString& Basename,
												FName ComponentName = NAME_None);

	/** Read a `.uetkx` file and preview it (None = the first component). */
	static TSharedRef<FUetkxPreview> FromFile(const FString& FilePath, FName ComponentName = NAME_None);

	~FUetkxPreview();

	/** The mounted preview widget, or a placeholder when the build failed. */
	TSharedRef<SWidget> GetWidget() const;

	/** True when a component mounted successfully. */
	bool IsValid() const { return bMounted; }

	/** Parse diagnostics + interpreter fallback notes, in order, for the panel's message list. */
	const TArray<FString>& GetMessages() const { return Messages; }

	/** The component that was previewed (empty when none mounted). */
	const FString& GetComponentName() const { return ComponentName; }

private:
	TSharedPtr<FRuiRoot> Root;
	TArray<FString> Messages;
	FString ComponentName;
	bool bMounted = false;
};
