// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiSignalViewModel.h"

const ::UE::FieldNotification::FFieldId URuiSignalViewModel::FFieldNotificationClassDescriptor::Int(TEXT("Int"), 0);
const ::UE::FieldNotification::FFieldId URuiSignalViewModel::FFieldNotificationClassDescriptor::Float(TEXT("Float"), 1);
const ::UE::FieldNotification::FFieldId URuiSignalViewModel::FFieldNotificationClassDescriptor::Bool(TEXT("Bool"), 2);
const ::UE::FieldNotification::FFieldId URuiSignalViewModel::FFieldNotificationClassDescriptor::Text(TEXT("Text"), 3);

void URuiSignalViewModel::FFieldNotificationClassDescriptor::ForEachField(
	const UClass*, TFunctionRef<bool(::UE::FieldNotification::FFieldId)> Callback) const
{
	if (Callback(Int) && Callback(Float) && Callback(Bool))
	{
		Callback(Text);
	}
}

void URuiSignalViewModel::SetInt(int32 InValue)
{
	if (Int != InValue)
	{
		Int = InValue;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::Int);
	}
}

void URuiSignalViewModel::SetFloat(float InValue)
{
	if (Float != InValue)
	{
		Float = InValue;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::Float);
	}
}

void URuiSignalViewModel::SetBool(bool InValue)
{
	if (Bool != InValue)
	{
		Bool = InValue;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::Bool);
	}
}

void URuiSignalViewModel::SetText(const FText& InValue)
{
	if (!Text.EqualTo(InValue))
	{
		Text = InValue;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::Text);
	}
}

void URuiSignalViewModel::Set(const FRuiValue& Value)
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
		break; // Null / Vector2 / Color / Opaque have no generic field target
	}
}

FDelegateHandle URuiSignalViewModel::AddFieldValueChangedDelegate(::UE::FieldNotification::FFieldId InFieldId,
																  FFieldValueChangedDelegate InNewDelegate)
{
	return Delegates.Add(this, InFieldId, MoveTemp(InNewDelegate));
}

bool URuiSignalViewModel::RemoveFieldValueChangedDelegate(::UE::FieldNotification::FFieldId InFieldId,
														  FDelegateHandle InHandle)
{
	return Delegates.RemoveFrom(this, InFieldId, InHandle).bRemoved;
}

int32 URuiSignalViewModel::RemoveAllFieldValueChangedDelegates(FDelegateUserObjectConst InUserObject)
{
	return Delegates.RemoveAll(this, InUserObject).RemoveCount;
}

int32 URuiSignalViewModel::RemoveAllFieldValueChangedDelegates(::UE::FieldNotification::FFieldId InFieldId,
															   FDelegateUserObjectConst InUserObject)
{
	return Delegates.RemoveAll(this, InFieldId, InUserObject).RemoveCount;
}

const ::UE::FieldNotification::IClassDescriptor& URuiSignalViewModel::GetFieldNotificationDescriptor() const
{
	static FFieldNotificationClassDescriptor Descriptor;
	return Descriptor;
}

void URuiSignalViewModel::BroadcastFieldValueChanged(::UE::FieldNotification::FFieldId InFieldId)
{
	Delegates.Broadcast(this, InFieldId);
}
