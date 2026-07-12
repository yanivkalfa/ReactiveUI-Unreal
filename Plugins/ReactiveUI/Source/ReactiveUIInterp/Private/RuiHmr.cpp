// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiHmr.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuiNode.h"
#include "RuiReconciler.h"
#include "UetkxFileScan.h"
#include "UetkxInterpComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiHmr, Log, All);

namespace
{
	struct FHmrSession
	{
		TMap<FName, uint32> InterpSigs; // last interp-applied signature per component
		FCriticalSection Lock;

		static FHmrSession& Get()
		{
			static FHmrSession Instance;
			return Instance;
		}
	};
} // namespace

FString FRuiHmrStatus::ToString() const
{
	return FString::Printf(
		TEXT("[RUI HMR] reloaded %d | refreshed %d session(s) | reset %d | linked %d%s | %s | %.0f ms%s"), Reloaded,
		Refreshed, Reset, Linked,
		KeptCompiled > 0 ? *FString::Printf(TEXT(" | kept-compiled %d (rebuild to apply)"), KeptCompiled) : TEXT(""),
		bGlobal ? TEXT("global") : TEXT("targeted"), Ms,
		Errors.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" | %d error(s)"), Errors.Num()));
}

FRuiHmrStatus FRuiHmr::ApplySource(const FString& Source, const FString& Basename, const TArray<FString>& Importers)
{
	const double Start = FPlatformTime::Seconds();
	FRuiHmrStatus Status;

	FUetkxFileScanResult Scan = FUetkxFileScan::Scan(Source, Basename);
	if (Scan.HasError())
	{
		// per-file isolation: the old definitions keep rendering
		for (const FUetkxDiag& Diag : Scan.Diags)
		{
			if (Diag.Severity == 0)
			{
				Status.Errors.Add(FString::Printf(TEXT("%s.uetkx: %s: %s"), *Basename, *Diag.Code, *Diag.Message));
			}
		}
		Status.Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
		return Status;
	}

	// MIXED-DECL v1 (M9): a file is a SEQUENCE of components + hooks + modules. Always swap the
	// components the interpreter CAN hot-apply (loop below); a support-only file has none and the
	// loop is a no-op. Hooks/modules carry arbitrary C++ the interpreter cannot swap — surface the
	// honest "rebuild" note (below) NAMING the import blast radius, not an error.

	FHmrSession& Session = FHmrSession::Get();
	for (const FUetkxComponentDecl& Decl : Scan.Components)
	{
		TSharedPtr<FUetkxInterpDef> Def = FUetkxInterpDef::Build(Decl);
		const FName ComponentId(*Decl.Name);
		const uint32 NewSig = FUetkxFileScan::HookSignature(Decl.HookCalls);
		for (const FString& Note : Def->Notes)
		{
			Status.Notes.Add(FString::Printf(TEXT("%s: %s"), *Decl.Name, *Note));
		}

		// The interpreter cannot faithfully run this component (an imported/user hook it can't execute, or
		// a non-setter event handler — e.g. `auto [C, Inc] = UseCounter(0)` + a button calling `Inc()`).
		// If a WORKING compiled version exists, DO NOT swap over it: a dead interp version would reset state
		// and break buttons until a rebuild (the exact break the owner hit). Instead RESTORE the compiled
		// version (clear any prior override) and tell the user a rebuild applies the edit — the honest,
		// non-destructive behavior. A brand-new component (no compiled factory) has nothing to preserve, so
		// it still swaps best-effort. Effects/memo-only gaps do NOT set bNeedsRebuild (those swaps proceed).
		if (Def->bNeedsRebuild && RUI::HasNamedFactory(ComponentId))
		{
			RUI::ClearComponentOverride(ComponentId);
			{
				FScopeLock Guard(&Session.Lock);
				Session.InterpSigs.Remove(ComponentId);
			}
			Status.Notes.Add(FString::Printf(
				TEXT("%s: kept the COMPILED version (state + buttons keep working) — the live interpreter can't "
					 "run this component's hooks/handlers; press Ctrl+Alt+F11 (Live Coding) to apply this edit."),
				*Decl.Name));
			++Status.KeptCompiled;
			continue;
		}

		bool bReset = false;
		bool bMigrate = false;
		{
			FScopeLock Guard(&Session.Lock);
			if (const uint32* PrevInterp = Session.InterpSigs.Find(ComponentId))
			{
				bReset = *PrevInterp != NewSig; // interp -> interp: preserve unless shape changed
			}
			else
			{
				// first swap this session: compiled cells are typed, interp cells are FRuiValue.
				bReset = true;
				const uint32 CompiledSig = RUI::FindHookSignature(ComponentId);
				if (CompiledSig != 0 && CompiledSig != NewSig)
				{
					// hook shape changed — a genuine reset (family rule), no migration.
					Status.Notes.Add(FString::Printf(TEXT("%s: state reset: hook shape changed"), *Decl.Name));
				}
				else
				{
					// same shape (or compiled sig unknown): TD-019 MIGRATES the typed cells into
					// the interpreter's FRuiValue cells — numeric/string/bool/text state survives
					// the first swap; only container/opaque slots re-init.
					bMigrate = true;
					Status.Notes.Add(
						FString::Printf(TEXT("%s: state migrated: first interp swap (representation)"), *Decl.Name));
				}
			}
			Session.InterpSigs.Add(ComponentId, NewSig);
		}
		if (bReset && !bMigrate)
		{
			++Status.Reset; // a migrating swap preserves user state — it is a reload, not a reset
		}

		if (!RUI::HasNamedFactory(ComponentId))
		{
			// hot-LINK: a component created after launch — reachable by name immediately
			TSharedPtr<FUetkxInterpDef> LinkDef = Def;
			RUI::RegisterNamedFactory(ComponentId, [LinkDef]() { return LinkDef->MakeNode(); });
			++Status.Linked;
		}
		RUI::SetComponentOverride(ComponentId, Def->MakeInvoke(), bReset, bMigrate);
		++Status.Reloaded;
	}

	// Hook/module edits (support-only OR the support half of a mixed file) recompiled but did not
	// interp-swap — name the import blast radius so the user knows which screens a rebuild affects.
	if (Scan.Hooks.Num() + Scan.Modules.Num() > 0)
	{
		const FString Blast =
			Importers.IsEmpty() ? FString(TEXT("(none recorded)")) : FString::Join(Importers, TEXT(", "));
		Status.Notes.Add(FString::Printf(
			TEXT("%s.uetkx: hook/module change — compiled only; importers: %s; Live Coding (Ctrl+Alt+F11) or rebuild, "
				 "then affected screens refresh"),
			*Basename, *Blast));
	}

	int32 Refreshed = 0;
	FRuiReconciler::ForEachLive(
		[&Refreshed](FRuiReconciler& Reconciler)
		{
			Reconciler.HmrRefreshAll();
			Reconciler.FlushSync();
			++Refreshed;
		});
	Status.Refreshed = Refreshed;
	Status.Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
	UE_LOG(LogRuiHmr, Display, TEXT("%s"), *Status.ToString());
	for (const FString& Note : Status.Notes)
	{
		UE_LOG(LogRuiHmr, Display, TEXT("[RUI HMR]   %s"), *Note);
	}
	return Status;
}

FRuiHmrStatus FRuiHmr::ApplyFile(const FString& UetkxPath, const TArray<FString>& Importers)
{
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *UetkxPath))
	{
		FRuiHmrStatus Status;
		Status.Errors.Add(FString::Printf(TEXT("%s: unreadable (editor save race?) — will retry"), *UetkxPath));
		return Status;
	}
	return ApplySource(Source, FPaths::GetBaseFilename(UetkxPath), Importers);
}

void FRuiHmr::ResetSession()
{
	FHmrSession& Session = FHmrSession::Get();
	TArray<FName> Names;
	{
		FScopeLock Guard(&Session.Lock);
		Session.InterpSigs.GetKeys(Names);
		Session.InterpSigs.Empty();
	}
	for (const FName& Name : Names)
	{
		RUI::ClearComponentOverride(Name);
	}
}
