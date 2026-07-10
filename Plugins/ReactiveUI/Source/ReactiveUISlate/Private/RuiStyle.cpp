// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiStyle.h"

#include "RuiElementAdapter.h"
#include "RuiSlateLog.h"

namespace
{
	FCriticalSection GClassLock;
	TMap<FName, FRuiStyleDict> GStyleClasses;

	float AsFloat(const FRuiValue& V)
	{
		return V.Kind == FRuiValue::EKind::Int ? static_cast<float>(V.IntValue) : static_cast<float>(V.FloatValue);
	}

	EVisibility VisibilityOf(const FRuiValue& V)
	{
		const FName Name = V.Kind == FRuiValue::EKind::Name ? V.NameValue : FName(*V.StringValue);
		if (Name == FName(TEXT("collapsed")))
		{
			return EVisibility::Collapsed;
		}
		if (Name == FName(TEXT("hidden")))
		{
			return EVisibility::Hidden;
		}
		if (Name == FName(TEXT("hitTestInvisible")))
		{
			return EVisibility::HitTestInvisible;
		}
		return EVisibility::Visible;
	}

	void ApplyRenderTransform(SWidget& W, const FRuiStyleDict* Dict)
	{
		// translation/scale/angle compose into ONE render transform; absent -> identity.
		FVector2D Translation = FVector2D::ZeroVector;
		float Scale = 1.0f;
		float AngleDeg = 0.0f;
		bool bAny = false;
		if (Dict != nullptr)
		{
			if (const FRuiValue* T = Dict->Find(FName(TEXT("translation"))))
			{
				Translation = T->Vector2Value;
				bAny = true;
			}
			if (const FRuiValue* S = Dict->Find(FName(TEXT("scale"))))
			{
				Scale = AsFloat(*S);
				bAny = true;
			}
			if (const FRuiValue* A = Dict->Find(FName(TEXT("angle"))))
			{
				AngleDeg = AsFloat(*A);
				bAny = true;
			}
		}
		if (bAny)
		{
			W.SetRenderTransform(::TransformCast<FSlateRenderTransform>(
				::Concatenate(FScale2D(Scale), FQuat2D(FMath::DegreesToRadians(AngleDeg)), FVector2D(Translation))));
		}
		else
		{
			W.SetRenderTransform(TOptional<FSlateRenderTransform>()); // reset -> identity
		}
	}

	bool IsTransformKey(FName Key)
	{
		return Key == FName(TEXT("translation")) || Key == FName(TEXT("scale")) || Key == FName(TEXT("angle"));
	}

	/** Apply one generic key (Value null = RESET to default). Returns false when unknown. */
	bool ApplyGenericKey(SWidget& W, FName Key, const FRuiValue* Value)
	{
		if (Key == FName(TEXT("opacity")))
		{
			W.SetRenderOpacity(Value != nullptr ? AsFloat(*Value) : 1.0f);
			return true;
		}
		if (Key == FName(TEXT("visibility")))
		{
			W.SetVisibility(Value != nullptr ? VisibilityOf(*Value) : EVisibility::Visible);
			return true;
		}
		if (Key == FName(TEXT("enabled")))
		{
			W.SetEnabled(Value == nullptr || Value->BoolValue);
			return true;
		}
		return false; // transform keys handled together; everything else is adapter/unknown
	}

	void WarnUnknownKey(FName Key)
	{
		static FCriticalSection WarnLock;
		static TSet<FName> Warned;
		FScopeLock Lock(&WarnLock);
		if (!Warned.Contains(Key))
		{
			Warned.Add(Key);
			UE_LOG(LogRuiSlate, Warning, TEXT("[ReactiveUI] unknown style key '%s' (ignored)"), *Key.ToString());
		}
	}
} // namespace

namespace RUI::Slate
{
	void RegisterStyleClass(FName ClassName, FRuiStyleDict Style)
	{
		FScopeLock Lock(&GClassLock);
		GStyleClasses.Add(ClassName, MoveTemp(Style));
	}

	const FRuiStyleDict* FindStyleClass(FName ClassName)
	{
		FScopeLock Lock(&GClassLock);
		return GStyleClasses.Find(ClassName);
	}

	TSharedPtr<FRuiStyleDict> BuildEffectiveStyle(const TArray<FName>& Classes,
												  const TSharedPtr<FRuiStyleDict>& InlineStyle)
	{
		if (Classes.IsEmpty())
		{
			return InlineStyle; // the common case shares the inline dict, no copy
		}
		TSharedPtr<FRuiStyleDict> Out = MakeShared<FRuiStyleDict>();
		{
			FScopeLock Lock(&GClassLock);
			for (const FName& ClassName : Classes)
			{
				if (const FRuiStyleDict* ClassDict = GStyleClasses.Find(ClassName))
				{
					for (const TPair<FName, FRuiValue>& Pair : *ClassDict)
					{
						Out->Add(Pair.Key, Pair.Value);
					}
				}
				else
				{
					WarnUnknownKey(FName(*(TEXT("class:") + ClassName.ToString())));
				}
			}
		}
		if (InlineStyle.IsValid())
		{
			for (const TPair<FName, FRuiValue>& Pair : *InlineStyle)
			{
				Out->Add(Pair.Key, Pair.Value); // inline wins
			}
		}
		return Out->IsEmpty() ? TSharedPtr<FRuiStyleDict>() : Out;
	}

	void ApplyStyleDiff(SWidget& Widget, IRuiElementAdapter* Adapter, const FRuiStyleDict* Old,
						const FRuiStyleDict* New)
	{
		bool bTransformTouched = false;

		// Pass 1: keys in New — apply when new or changed.
		if (New != nullptr)
		{
			for (const TPair<FName, FRuiValue>& Pair : *New)
			{
				const FRuiValue* OldValue = Old != nullptr ? Old->Find(Pair.Key) : nullptr;
				if (OldValue != nullptr && *OldValue == Pair.Value)
				{
					continue;
				}
				if (IsTransformKey(Pair.Key))
				{
					bTransformTouched = true;
					continue;
				}
				if (Adapter != nullptr && Adapter->ApplyStyleKey(Widget, Pair.Key, &Pair.Value))
				{
					continue;
				}
				if (!ApplyGenericKey(Widget, Pair.Key, &Pair.Value))
				{
					WarnUnknownKey(Pair.Key);
				}
			}
		}

		// Pass 2: keys REMOVED (in Old, not in New) — reset to defaults (the family rule).
		if (Old != nullptr)
		{
			for (const TPair<FName, FRuiValue>& Pair : *Old)
			{
				if (New != nullptr && New->Contains(Pair.Key))
				{
					continue;
				}
				if (IsTransformKey(Pair.Key))
				{
					bTransformTouched = true;
					continue;
				}
				if (Adapter != nullptr && Adapter->ApplyStyleKey(Widget, Pair.Key, nullptr))
				{
					continue;
				}
				ApplyGenericKey(Widget, Pair.Key, nullptr); // unknown keys already warned on apply
			}
		}

		if (bTransformTouched)
		{
			ApplyRenderTransform(Widget, New); // recompose from the NEW dict (absent -> identity)
		}
	}
} // namespace RUI::Slate
