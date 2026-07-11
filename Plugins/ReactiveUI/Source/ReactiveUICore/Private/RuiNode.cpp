// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiNode.h"
#include "RuiElementRegistry.h"

// ─────────────────────────────────────────────────────────────────────────────────────────
// Component registry (D-05)
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	struct FRuiComponentRegistry
	{
		// Fn pointer → registered FName. Pointers may be RE-REGISTERED after Live Coding
		// relocates code (RUI_COMPONENT's static initializer runs again in the patched
		// module); the NAME is the identity, so re-pointing is exactly the desired behavior.
		TMap<void*, FName> PtrToId;
		FCriticalSection Lock;

		static FRuiComponentRegistry& Get()
		{
			static FRuiComponentRegistry Instance;
			return Instance;
		}
	};

	struct FRuiElementTypeRegistry
	{
		TMap<FName, uint16> TagToId;
		TArray<FName> IdToTag; // index = id - 1
		FCriticalSection Lock;

		static FRuiElementTypeRegistry& Get()
		{
			static FRuiElementTypeRegistry Instance;
			return Instance;
		}
	};

	struct FRuiNamedFactoryRegistry
	{
		TMap<FName, TFunction<FRuiNode()>> Factories;
		FCriticalSection Lock;

		static FRuiNamedFactoryRegistry& Get()
		{
			static FRuiNamedFactoryRegistry Instance;
			return Instance;
		}
	};
} // namespace

namespace RUI
{
	FName RegisterComponentId(void* FnPtr, FName Id)
	{
		FRuiComponentRegistry& Reg = FRuiComponentRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		Reg.PtrToId.Add(FnPtr, Id);
		return Id;
	}

	FName FindComponentId(void* FnPtr)
	{
		FRuiComponentRegistry& Reg = FRuiComponentRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const FName* Found = Reg.PtrToId.Find(FnPtr))
		{
			return *Found;
		}
		return NAME_None;
	}

	bool RegisterNamedFactory(FName Name, TFunction<FRuiNode()> Factory)
	{
		FRuiNamedFactoryRegistry& Reg = FRuiNamedFactoryRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		Reg.Factories.Add(Name, MoveTemp(Factory)); // replace-on-re-register (Live Coding/HMR)
		return true;
	}

	FRuiNode Named(FName Name)
	{
		TFunction<FRuiNode()> Factory;
		{
			FRuiNamedFactoryRegistry& Reg = FRuiNamedFactoryRegistry::Get();
			FScopeLock Guard(&Reg.Lock);
			if (const TFunction<FRuiNode()>* Found = Reg.Factories.Find(Name))
			{
				Factory = *Found;
			}
		}
		return Factory ? Factory() : Fragment({});
	}

	bool HasNamedFactory(FName Name)
	{
		FRuiNamedFactoryRegistry& Reg = FRuiNamedFactoryRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		return Reg.Factories.Contains(Name);
	}

	namespace
	{
		struct FRuiHmrRegistry
		{
			TMap<FName, uint32> HookSignatures;
			TMap<FName, FRuiComponentOverride> Overrides;
			uint32 NextGeneration = 1;
			FCriticalSection Lock;

			static FRuiHmrRegistry& Get()
			{
				static FRuiHmrRegistry Instance;
				return Instance;
			}
		};
	} // namespace

	void RegisterHookSignature(FName ComponentId, uint32 Signature)
	{
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		Reg.HookSignatures.Add(ComponentId, Signature);
	}

	uint32 FindHookSignature(FName ComponentId)
	{
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const uint32* Found = Reg.HookSignatures.Find(ComponentId))
		{
			return *Found;
		}
		return 0;
	}

	void SetComponentOverride(FName ComponentId, TSharedPtr<FRuiComponentInvoke> Invoke, bool bResetState)
	{
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		FRuiComponentOverride& Entry = Reg.Overrides.FindOrAdd(ComponentId);
		Entry.Invoke = MoveTemp(Invoke);
		Entry.Generation = Reg.NextGeneration++;
		Entry.bResetState = bResetState;
	}

	void ClearComponentOverride(FName ComponentId)
	{
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		Reg.Overrides.Remove(ComponentId);
	}

	FRuiComponentOverride FindComponentOverride(FName ComponentId)
	{
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const FRuiComponentOverride* Found = Reg.Overrides.Find(ComponentId))
		{
			return *Found;
		}
		return FRuiComponentOverride();
	}

	FRuiElementTypeId InternElementType(FName Tag)
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const uint16* Found = Reg.TagToId.Find(Tag))
		{
			return FRuiElementTypeId{*Found};
		}
		checkf(Reg.IdToTag.Num() < MAX_uint16 - 1, TEXT("element type registry overflow"));
		Reg.IdToTag.Add(Tag);
		const uint16 NewId = static_cast<uint16>(Reg.IdToTag.Num()); // ids start at 1; 0 = invalid
		Reg.TagToId.Add(Tag, NewId);
		return FRuiElementTypeId{NewId};
	}

	FRuiElementTypeId FindElementType(FName Tag)
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const uint16* Found = Reg.TagToId.Find(Tag))
		{
			return FRuiElementTypeId{*Found};
		}
		return FRuiElementTypeId{};
	}

	FName GetElementTypeName(FRuiElementTypeId Id)
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (Id.IsValid() && Id.Value <= Reg.IdToTag.Num())
		{
			return Reg.IdToTag[Id.Value - 1];
		}
		return NAME_None;
	}

	int32 NumElementTypes()
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		return Reg.IdToTag.Num();
	}

	// ── structural factories ──────────────────────────────────────────────────────────

	FRuiChildren MakeChildren(TArray<FRuiNode> InChildren)
	{
		if (InChildren.IsEmpty())
		{
			return nullptr;
		}
		return MakeShared<const TArray<FRuiNode>>(MoveTemp(InChildren));
	}

	FRuiNode Fragment(TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Fragment;
		Node.Children = MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuiNode Portal(FRuiPortalHandle Target, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Portal;
		Node.PortalTarget = MoveTemp(Target);
		Node.Children = MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuiNode ErrorBoundary(FRuiNode Fallback, TArray<FRuiNode> Children, FRuiKey ResetKey,
						   TFunction<void(const FString&)> OnError, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::ErrorBoundary;
		Node.EbFallback = MakeShared<FRuiNode>(MoveTemp(Fallback));
		Node.EbOnError = MoveTemp(OnError);
		Node.EbResetKey = ResetKey;
		Node.Children = MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}
} // namespace RUI
