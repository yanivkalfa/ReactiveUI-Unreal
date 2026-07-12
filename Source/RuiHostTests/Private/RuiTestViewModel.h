// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// A minimal FieldNotify view model for the Mvvm suite — implements INotifyFieldValueChanged
// directly over the engine FieldNotification module (deliberately NO MVVM-plugin dependency:
// the bridge must serve any FieldNotify object, UMVVMViewModelBase included).

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "FieldNotificationDelegate.h"
#include "INotifyFieldValueChanged.h"
#include "RuiMvvmViewModel.h" // URuiMvvmViewModel (P1 regression subclass)
#include "UObject/Object.h"
#include "RuiTestViewModel.generated.h"

UCLASS()
class URuiTestViewModel : public UObject, public INotifyFieldValueChanged
{
	GENERATED_BODY()

public:
	struct FFieldNotificationClassDescriptor : public ::UE::FieldNotification::IClassDescriptor
	{
		static const ::UE::FieldNotification::FFieldId Score;
		virtual void ForEachField(const UClass* Class,
								  TFunctionRef<bool(::UE::FieldNotification::FFieldId)> Callback) const override
		{
			Callback(Score);
		}
	};

	UPROPERTY()
	int32 Score = 0;

	void SetScore(int32 InScore)
	{
		if (Score != InScore)
		{
			Score = InScore;
			Delegates.Broadcast(this, FFieldNotificationClassDescriptor::Score);
		}
	}

	// INotifyFieldValueChanged
	virtual FDelegateHandle AddFieldValueChangedDelegate(::UE::FieldNotification::FFieldId InFieldId,
														 FFieldValueChangedDelegate InNewDelegate) override
	{
		return Delegates.Add(this, InFieldId, MoveTemp(InNewDelegate));
	}
	virtual bool RemoveFieldValueChangedDelegate(::UE::FieldNotification::FFieldId InFieldId,
												 FDelegateHandle InHandle) override
	{
		return Delegates.RemoveFrom(this, InFieldId, InHandle).bRemoved;
	}
	virtual int32 RemoveAllFieldValueChangedDelegates(FDelegateUserObjectConst InUserObject) override
	{
		return Delegates.RemoveAll(this, InUserObject).RemoveCount;
	}
	virtual int32 RemoveAllFieldValueChangedDelegates(::UE::FieldNotification::FFieldId InFieldId,
													  FDelegateUserObjectConst InUserObject) override
	{
		return Delegates.RemoveAll(this, InFieldId, InUserObject).RemoveCount;
	}
	virtual const ::UE::FieldNotification::IClassDescriptor& GetFieldNotificationDescriptor() const override
	{
		static FFieldNotificationClassDescriptor Descriptor;
		return Descriptor;
	}
	virtual void BroadcastFieldValueChanged(::UE::FieldNotification::FFieldId InFieldId) override
	{
		Delegates.Broadcast(this, InFieldId);
	}

private:
	::UE::FieldNotification::FFieldMulticastDelegate Delegates;
};

/** Concrete UUserWidget for the theirs-inside-ours test (UUserWidget itself is abstract). The typed
 *  properties are the targets the TD-021 prop-map bridge sets by reflection. */
UCLASS()
class URuiTestUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 IntValue = 0;

	UPROPERTY()
	float FloatValue = 0.0f;

	UPROPERTY()
	bool BoolValue = false;

	UPROPERTY()
	FText TextValue;

	UPROPERTY()
	FString StringValue;
};

/** A URuiMvvmViewModel SUBCLASS — the intended MVVM extension point (add FieldNotify props). Used by
 *  the bughunt-P1 regression: registering it must be resolvable via FindGlobalViewModel's default class. */
UCLASS()
class URuiTestMvvmSubViewModel : public URuiMvvmViewModel
{
	GENERATED_BODY()
};
