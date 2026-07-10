// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Compile-coverage TU: includes EVERY public header of the module so a header that only
// compiles by include-order luck (or not at all) fails HERE, in this module, on every
// build — not in the first downstream consumer. Also the honest IWYU check for our own
// public surface. Add each new public header to this list in the same commit.

#include "RuiTypes.h"
#include "RuiPropsBase.h"
#include "RuiElementRegistry.h"
#include "RuiNode.h"
#include "RuiHostConfig.h"
#include "RuiHooksInternal.h"
#include "RuiComponentState.h"
#include "RuiFiber.h"

// Instantiate the templates a header-only consumer would (template errors surface at
// instantiation, not parse).
namespace RuiCoreChecks
{
	static void CompileCheck()
	{
		TRuiSetter<int32> IntSetter;
		TRuiSetter<FString> StrSetter;
		(void)(IntSetter == IntSetter);
		(void)(StrSetter == StrSetter);

		FRuiDeps D = RUI::Deps(1, 2.0f, TEXT("x"), FName(TEXT("n")));
		(void)RUI::DepsChanged(D, D);

		TRuiStateCell<int32> State(0);
		TRuiRefCell<float> Ref(0.0f);
		TRuiMemoCell<FString> Memo;
		TRuiDeferredCell<int32> Deferred;
		(void)State; (void)Ref; (void)Memo; (void)Deferred;

		FRuiFiberSlab Slab;
		FRuiFiber* F = Slab.Acquire();
		Slab.Release(F);
	}

	// Never called; exists so the linker keeps the instantiations honest without running.
	void* Sink = (void*)&CompileCheck;
}
