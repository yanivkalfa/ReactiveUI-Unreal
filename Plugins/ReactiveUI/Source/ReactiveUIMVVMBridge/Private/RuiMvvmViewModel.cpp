// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiMvvmViewModel.h"

#include "Engine/GameInstance.h"
#include "MVVMGameSubsystem.h"
#include "RuiTypes.h" // FRuiValue
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"

void URuiMvvmViewModel::SetInt(int32 InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(IntValue, InValue);
}

void URuiMvvmViewModel::SetFloat(float InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(FloatValue, InValue);
}

void URuiMvvmViewModel::SetBool(bool InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(BoolValue, InValue);
}

void URuiMvvmViewModel::SetText(const FText& InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(TextValue, InValue);
}

void URuiMvvmViewModel::Set(const FRuiValue& Value)
{
	switch (Value.Kind)
	{
	case FRuiValue::EKind::Bool:
		SetBool(Value.BoolValue);
		break;
	case FRuiValue::EKind::Int:
		SetInt(static_cast<int32>(Value.IntValue));
		break;
	case FRuiValue::EKind::Float:
		SetFloat(static_cast<float>(Value.FloatValue));
		break;
	case FRuiValue::EKind::Text:
		SetText(Value.TextValue);
		break;
	case FRuiValue::EKind::String:
		SetText(FText::FromString(Value.StringValue));
		break;
	case FRuiValue::EKind::Name:
		SetText(FText::FromName(Value.NameValue));
		break;
	default:
		break; // Null / Vector2 / Color / Opaque have no scalar field here
	}
}

namespace
{
	UMVVMViewModelCollectionObject* GlobalCollection(UGameInstance* GameInstance)
	{
		if (GameInstance == nullptr)
		{
			return nullptr;
		}
		if (UMVVMGameSubsystem* Subsystem = GameInstance->GetSubsystem<UMVVMGameSubsystem>())
		{
			return Subsystem->GetViewModelCollection();
		}
		return nullptr;
	}
} // namespace

bool RUI::Mvvm::RegisterGlobalViewModel(UGameInstance* GameInstance, FName ContextName, UMVVMViewModelBase* ViewModel)
{
	if (ViewModel == nullptr)
	{
		return false;
	}
	UMVVMViewModelCollectionObject* Collection = GlobalCollection(GameInstance);
	if (Collection == nullptr)
	{
		return false;
	}
	FMVVMViewModelContext Context;
	Context.ContextClass = ViewModel->GetClass();
	Context.ContextName = ContextName;
	const bool bAdded = Collection->AddViewModelInstance(Context, ViewModel);

	// Also register a URuiMvvmViewModel BASE-class alias for a subclassed viewmodel, so
	// FindGlobalViewModel resolves it whether the caller passes the concrete class or relies on the
	// base-class default (bughunt P1 — the engine's context match is exact class + name, so a subclass
	// registered only under its own class was unresolvable via the default). The name still disambiguates.
	if (bAdded && ViewModel->GetClass() != URuiMvvmViewModel::StaticClass() && ViewModel->IsA<URuiMvvmViewModel>())
	{
		FMVVMViewModelContext BaseAlias;
		BaseAlias.ContextClass = URuiMvvmViewModel::StaticClass();
		BaseAlias.ContextName = ContextName;
		Collection->AddViewModelInstance(BaseAlias, ViewModel); // best-effort; collides only on duplicate name
	}
	return bAdded;
}

UMVVMViewModelBase* RUI::Mvvm::FindGlobalViewModel(UGameInstance* GameInstance, FName ContextName,
												   TSubclassOf<UMVVMViewModelBase> ContextClass)
{
	UMVVMViewModelCollectionObject* Collection = GlobalCollection(GameInstance);
	if (Collection == nullptr)
	{
		return nullptr;
	}
	FMVVMViewModelContext Context;
	Context.ContextClass = ContextClass.Get() != nullptr
							   ? ContextClass
							   : TSubclassOf<UMVVMViewModelBase>(URuiMvvmViewModel::StaticClass());
	Context.ContextName = ContextName;
	return Collection->FindViewModelInstance(Context);
}
