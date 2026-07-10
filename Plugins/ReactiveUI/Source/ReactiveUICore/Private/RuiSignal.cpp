// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiSignal.h"

namespace
{
	TMap<FName, TSharedPtr<FRuiSignalBase>>& SignalRegistry()
	{
		static TMap<FName, TSharedPtr<FRuiSignalBase>> Registry;
		return Registry;
	}
} // namespace

namespace RUI
{
	TSharedPtr<FRuiSignalBase>* FindOrAddSignalSlot(FName Key)
	{
		return &SignalRegistry().FindOrAdd(Key);
	}

	TSharedPtr<FRuiSignalBase> TryGetSignal(FName Key)
	{
		if (const TSharedPtr<FRuiSignalBase>* Found = SignalRegistry().Find(Key))
		{
			return *Found;
		}
		return nullptr;
	}

	bool HasSignal(FName Key)
	{
		return SignalRegistry().Contains(Key);
	}

	void ClearSignals()
	{
		SignalRegistry().Empty();
	}
} // namespace RUI
