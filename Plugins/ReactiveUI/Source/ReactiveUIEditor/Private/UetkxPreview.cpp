// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxPreview.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuiNode.h"
#include "RuiRoot.h"
#include "UetkxFileScan.h"
#include "UetkxInterpComponent.h"
#include "Widgets/Text/STextBlock.h"

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

	const FUetkxComponentDecl* Decl = nullptr;
	if (InComponentName.IsNone())
	{
		Decl = &Scan.Components[0];
	}
	else
	{
		const FString Wanted = InComponentName.ToString();
		for (const FUetkxComponentDecl& Candidate : Scan.Components)
		{
			if (Candidate.Name == Wanted)
			{
				Decl = &Candidate;
				break;
			}
		}
	}
	if (Decl == nullptr)
	{
		Preview->Messages.Add(
			FString::Printf(TEXT("[preview] component '%s' not found."), *InComponentName.ToString()));
		return Preview;
	}

	if (Scan.HasError())
	{
		// A parse error was already listed above; the interpreter renders best-effort from what parsed.
		Preview->Messages.Add(TEXT("[preview] source has parse errors — preview may be incomplete."));
	}

	TSharedPtr<FUetkxInterpDef> Def = FUetkxInterpDef::Build(*Decl);
	if (!Def.IsValid())
	{
		Preview->Messages.Add(TEXT("[preview] interpreter could not build this component."));
		return Preview;
	}
	for (const FString& Note : Def->Notes)
	{
		Preview->Messages.Add(FString::Printf(TEXT("[interp] %s"), *Note));
	}

	Preview->ComponentName = Decl->Name;
	Preview->Root = FRuiRoot::Create(Def->MakeNode());
	Preview->Root->FlushSync();
	Preview->bMounted = true;
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
