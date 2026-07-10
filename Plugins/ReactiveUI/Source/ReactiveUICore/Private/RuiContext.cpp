// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiContext.h"
#include "RuiReconciler.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiCoreHooks, Log, All);

const TCHAR* RuiHookKindName(ERuiHookKind Kind)
{
	switch (Kind)
	{
	case ERuiHookKind::State: return TEXT("state");
	case ERuiHookKind::Reducer: return TEXT("reducer");
	case ERuiHookKind::Ref: return TEXT("ref");
	case ERuiHookKind::Memo: return TEXT("memo");
	case ERuiHookKind::Deferred: return TEXT("deferred");
	case ERuiHookKind::Transition: return TEXT("transition");
	case ERuiHookKind::Stable: return TEXT("stable");
	case ERuiHookKind::SafeArea: return TEXT("safe_area");
	case ERuiHookKind::Signal: return TEXT("signal");
	case ERuiHookKind::Tween: return TEXT("tween");
	case ERuiHookKind::TweenValue: return TEXT("tween_value");
	case ERuiHookKind::Animate: return TEXT("animate");
	case ERuiHookKind::Sfx: return TEXT("sfx");
	case ERuiHookKind::Effect: return TEXT("effect");
	case ERuiHookKind::LayoutEffect: return TEXT("layout_effect");
	}
	return TEXT("?");
}

TWeakPtr<FRuiComponentState> FRuiContext::StateWeak() const
{
	return StateShared;
}

void FRuiContext::Record(ERuiHookKind Kind)
{
	if (FRuiConfig::IsHookValidationEnabled())
	{
		State.HookLog.Add(Kind);
	}
}

void FRuiContext::WarnOnce(FName Key, const FString& Msg)
{
	if (State.DiagWarned.Contains(Key))
	{
		return;
	}
	State.DiagWarned.Add(Key);
	FRuiDiagnostics::Emit(Msg);
	UE_LOG(LogRuiCoreHooks, Warning, TEXT("%s"), *Msg);
}

void FRuiContext::NotifyEffects()
{
	Reconciler.NotifyEffectKinds(Fiber, !State.Effects.IsEmpty(), !State.LayoutEffects.IsEmpty());
}

void FRuiContext::ReconcilerOnProvidedChanged(const void* Key)
{
	Reconciler.OnProvidedValueChanged(Fiber, Key);
}

void FRuiContext::StubSlot(ERuiHookKind Kind, const TCHAR* HookName, const TCHAR* Owner)
{
	Record(Kind);
	const int32 i = State.HookIndex++;
	if (i >= State.Hooks.Num())
	{
		State.Hooks.Emplace(MakeUnique<FRuiMarkerCell>(Kind));
	}
	FRuiMarkerCell* Cell = static_cast<FRuiMarkerCell*>(State.Hooks[i].Get());
	if (!Cell->bWarned)
	{
		Cell->bWarned = true;
		WarnOnce(FName(HookName), FString::Printf(
			TEXT("[ReactiveUI] %s is a Phase-owned stub (fully wired in %s; the v1 ship gate forbids stubs). It consumes its hook slot but does nothing yet."),
			HookName, Owner));
	}
}
