// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxPreview.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuiCoreElements.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "UetkxFileScan.h"
#include "Widgets/Text/STextBlock.h"

// HMR v2 (D-HMR-8): the interpreter is deleted, so the preview mounts the COMPILED component by name
// (the one HMR/RUICompile registered). It reflects the LAST COMPILED state — after an edit, a save
// recompiles it (and, with HMR mode on, Live Coding patches it live). This is the real component, not
// an approximation; its effects run as they would in PIE.

TSharedRef<FUetkxPreview> FUetkxPreview::FromSource(const FString& Source, const FString& Basename,
													FName InComponentName)
{
	TSharedRef<FUetkxPreview> Preview = MakeShared<FUetkxPreview>();

	const FUetkxFileScanResult Scan = FUetkxFileScan::Scan(Source, Basename);
	for (const FUetkxDiag& Diag : Scan.Diags)
	{
		const TCHAR* Sev = Diag.Severity == 0 ? TEXT("error") : Diag.Severity == 1 ? TEXT("warning") : TEXT("hint");
		Preview->Messages.Add(FString::Printf(TEXT("[%s %s] %s"), Sev, *Diag.Code, *Diag.Message));
	}

	if (Scan.Components.Num() == 0)
	{
		Preview->Messages.Add(TEXT("[preview] no component declaration to render."));
		return Preview;
	}

	// Pick the requested component, else the first declared.
	FString Wanted = InComponentName.IsNone() ? Scan.Components[0].Name : InComponentName.ToString();
	bool bDeclared = false;
	bool bWantedExported = false;
	for (const FUetkxComponentDecl& Candidate : Scan.Components)
	{
		if (Candidate.Name == Wanted)
		{
			bDeclared = true;
			bWantedExported = Candidate.bExported;
			break;
		}
	}
	if (!bDeclared)
	{
		Preview->Messages.Add(FString::Printf(TEXT("[preview] component '%s' not found in this file."), *Wanted));
		return Preview;
	}

	const FName ComponentId(*Wanted);
	if (!RUI::HasNamedFactory(ComponentId))
	{
		// TD-026 (ES-modules M3): a PRIVATE component never registers a named factory (tree-shaken
		// by design) — "not compiled yet" would be a lie; say what actually unlocks the preview.
		if (!bWantedExported)
		{
			Preview->Messages.Add(FString::Printf(
				TEXT("[preview] component '%s' is private (file-local) — add `export` to its declaration to "
					 "preview it. Private components never register a factory."),
				*Wanted));
			return Preview;
		}
		Preview->Messages.Add(FString::Printf(
			TEXT("[preview] component '%s' is not compiled yet — save the file (or run RUICompile) so it registers, "
				 "then reopen the preview. The preview mounts the COMPILED component."),
			*Wanted));
		return Preview;
	}

	// Mount the real compiled component (interactive; effects run as in PIE).
	Preview->ComponentName = Wanted;
	Preview->Root = FRuiRoot::Create(RUI::Named(ComponentId));
	Preview->Root->FlushSync();
	Preview->bMounted = true;
	Preview->Messages.Add(FString::Printf(TEXT("[preview] mounted the compiled component '%s' (live)."), *Wanted));
	return Preview;
}

TSharedRef<FUetkxPreview> FUetkxPreview::FromFile(const FString& FilePath, FName InComponentName)
{
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *FilePath))
	{
		TSharedRef<FUetkxPreview> Preview = MakeShared<FUetkxPreview>();
		Preview->Messages.Add(FString::Printf(TEXT("[preview] could not read '%s'."), *FilePath));
		return Preview;
	}
	return FromSource(Source, FPaths::GetBaseFilename(FilePath), InComponentName);
}

FUetkxPreview::~FUetkxPreview()
{
	if (Root.IsValid())
	{
		Root->Unmount();
		Root.Reset();
	}
}

TSharedRef<SWidget> FUetkxPreview::GetWidget() const
{
	if (Root.IsValid())
	{
		return Root->GetWidget();
	}
	return SNew(STextBlock).Text(NSLOCTEXT("ReactiveUI", "PreviewUnavailable", "No preview — see the messages below."));
}
