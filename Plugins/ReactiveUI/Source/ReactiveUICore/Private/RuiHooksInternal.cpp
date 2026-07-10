// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiHooksInternal.h"

namespace RUI
{
	bool DepsChanged(const FRuiDeps& Prev, const FRuiDeps& Next)
	{
		// Family _deps_changed: unset ("no deps") on either side => always changed.
		if (!Prev.IsSet() || !Next.IsSet())
		{
			return true;
		}
		const TArray<FRuiDep>& A = Prev.GetValue();
		const TArray<FRuiDep>& B = Next.GetValue();
		if (A.Num() != B.Num())
		{
			return true;
		}
		for (int32 i = 0; i < A.Num(); ++i)
		{
			if (!(A[i] == B[i]))
			{
				return true;
			}
		}
		return false;
	}
} // namespace RUI
