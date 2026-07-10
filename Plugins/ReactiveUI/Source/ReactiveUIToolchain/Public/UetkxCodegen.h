// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxCodegen — the single codegen authority (D-18): compiles .uetkx source into the
// committed sibling `Foo.uetkx.inl` — reflection-free C++ (no UCLASS/UPROPERTY, D-19.2):
// a typed props struct, the component function (setup spliced VERBATIM with hooks
// auto-prefixed to Ctx.*), the markup lowered to the D-33 builder vocabulary, the baked
// __RUI_HOOK_SIG constant (state-reset detection, Phase 4), RUI::RegisterComponentId, and
// an inline wrapper `FRuiNode <Name>(Props, Children, Key)` for cross-component references.
// FText string literals emit as NSLOCTEXT (self-namespaced per file — D-32).

#pragma once

#include "CoreMinimal.h"
#include "UetkxFileScan.h"

struct REACTIVEUITOOLCHAIN_API FUetkxCompileOutput
{
	bool bOk = false;
	FString Inl; // the generated .uetkx.inl text ("" on failure)
	TArray<FUetkxDiag> Diags;
	TArray<FString> ComponentNames; // bindings (name -> this file), for refs/aggregator order
	uint32 HookSig = 0;				// first component's hook signature (interp swap key)
};

class REACTIVEUITOOLCHAIN_API FUetkxCodegen
{
public:
	/** Compile one .uetkx source. Basename = file stem (binding + NSLOCTEXT namespace). */
	static FUetkxCompileOutput CompileSource(const FString& Source, const FString& Basename);
};
