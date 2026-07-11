// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The D-22(c)-(d) full-file contract harness: fixtures under
// Source/RuiHostTests/ContractFixtures/ (EXCLUDED from the RUICompile sweep — FindAll skips
// ContractFixtures paths) compile through the real pipeline and byte-compare against
// committed `.inl.expected` goldens; error fixtures pin their diagnostics via
// `.diags.expected`. Regenerate with `-run=RUIContractDump`; gate with `--check` (CI) or the
// ReactiveUI.Contract suite. Logic lives HERE so the commandlet and the suite cannot drift.

#pragma once

#include "CoreMinimal.h"

struct REACTIVEUITOOLCHAIN_API FUetkxContractResult
{
	int32 Total = 0;
	int32 Mismatched = 0; // includes missing goldens in check mode
	int32 Written = 0;	  // dump mode only
	TArray<FString> Messages;

	bool Passed() const { return Mismatched == 0; }
};

class REACTIVEUITOOLCHAIN_API FUetkxContract
{
public:
	static FString DefaultFixtureDir(); // <Project>/Source/RuiHostTests/ContractFixtures

	/** Deterministic text form of a diagnostics list (one `CODE sev @off+len: message` line). */
	static FString SerializeDiags(const struct FUetkxCompileOutput& Output);

	/** bWrite=true: (re)generate goldens for every fixture. bWrite=false: compare only
	 *  (missing golden = mismatch). Line endings are normalized before comparing. */
	static FUetkxContractResult Run(const FString& FixtureDir, bool bWrite);
};
