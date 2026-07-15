// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// "Their data feeding ours": UseField subscribes a component to a FieldNotify field on any
// INotifyFieldValueChanged UObject (UMVVMViewModelBase, UUserWidget, or any custom VM — the
// engine FieldNotification module, no MVVM-plugin dependency). The component re-renders when
// the field broadcasts; the subscription unbinds on cleanup; a dead/stale VM reads as the
// caller's default and stays quiet (the recorded stale-VM policy).

#pragma once

#include "CoreMinimal.h"
#include "FieldNotificationId.h"
#include "INotifyFieldValueChanged.h"
#include "RuiContext.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakInterfacePtr.h"

namespace RUI::Umg
{
	namespace Private
	{
		/** Subscribe once (slot-stable) + re-render on broadcast. Returns whether live. */
		REACTIVEUIUMG_API bool UseFieldSubscription(FRuiContext& Ctx, UObject* ViewModel, FName FieldName);

		/** Read a reflected property value; false when unreadable/type-mismatched. */
		template <typename T> bool ReadProperty(UObject* Object, FName PropertyName, T& Out)
		{
			if (!Object)
			{
				return false;
			}
			const FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
			if (!Property)
			{
				return false;
			}
			const void* Value = Property->ContainerPtrToValuePtr<void>(Object);
			if constexpr (std::is_same_v<T, int32>)
			{
				if (const FIntProperty* Typed = CastField<FIntProperty>(Property))
				{
					Out = Typed->GetPropertyValue(Value);
					return true;
				}
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				if (const FFloatProperty* Typed = CastField<FFloatProperty>(Property))
				{
					Out = Typed->GetPropertyValue(Value);
					return true;
				}
				if (const FDoubleProperty* Typed = CastField<FDoubleProperty>(Property))
				{
					Out = static_cast<float>(Typed->GetPropertyValue(Value));
					return true;
				}
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				if (const FBoolProperty* Typed = CastField<FBoolProperty>(Property))
				{
					Out = Typed->GetPropertyValue(Value);
					return true;
				}
			}
			else if constexpr (std::is_same_v<T, FString>)
			{
				if (const FStrProperty* Typed = CastField<FStrProperty>(Property))
				{
					Out = Typed->GetPropertyValue(Value);
					return true;
				}
			}
			else if constexpr (std::is_same_v<T, FText>)
			{
				if (const FTextProperty* Typed = CastField<FTextProperty>(Property))
				{
					Out = Typed->GetPropertyValue(Value);
					return true;
				}
			}
			return false;
		}
	} // namespace Private

	/**
	 * Read a FieldNotify field reactively: subscribes on first render (one hook slot; stable
	 * across renders), re-renders the component when the VM broadcasts the field, unbinds on
	 * cleanup/unmount. A null/collected VM returns Default (stale-VM policy: quiet).
	 *
	 *   const int32 Score = RUI::Umg::UseField<int32>(Ctx, MyVm, "Score", 0);
	 */
	template <typename T> T UseField(FRuiContext& Ctx, UObject* ViewModel, FName FieldName, T Default = T())
	{
		Private::UseFieldSubscription(Ctx, ViewModel, FieldName);
		T Value = Default;
		Private::ReadProperty<T>(ViewModel, FieldName, Value);
		return Value;
	}

	/**
	 * Create-and-OWN a viewmodel for the component's lifetime: constructed on first render
	 * (transient outer), GC-rooted by a strong pointer living in a hook slot, released when the
	 * component unmounts. The hand-rolled `UseMemo` + `TStrongObjectPtr` pattern, packaged.
	 * Template call — pass Ctx explicitly (like UseField):
	 *
	 *   URuiSignalViewModel* Vm = RUI::Umg::UseOwnedViewModel<URuiSignalViewModel>(Ctx);
	 *   const int32 N = RUI::Umg::UseField<int32>(Ctx, Vm, "Int", 0);
	 */
	template <typename T> T* UseOwnedViewModel(FRuiContext& Ctx)
	{
		const TSharedRef<TRuiRef<TStrongObjectPtr<T>>>& Box = Ctx.UseRef<TStrongObjectPtr<T>>();
		if (!Box->Current.IsValid())
		{
			Box->Current = TStrongObjectPtr<T>(NewObject<T>(GetTransientPackage()));
		}
		return Box->Current.Get();
	}
} // namespace RUI::Umg
