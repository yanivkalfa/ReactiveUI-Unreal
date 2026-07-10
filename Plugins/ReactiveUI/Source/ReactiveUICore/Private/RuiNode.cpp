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
}

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

	FRuiElementTypeId InternElementType(FName Tag)
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const uint16* Found = Reg.TagToId.Find(Tag))
		{
			return FRuiElementTypeId{ *Found };
		}
		checkf(Reg.IdToTag.Num() < MAX_uint16 - 1, TEXT("element type registry overflow"));
		Reg.IdToTag.Add(Tag);
		const uint16 NewId = static_cast<uint16>(Reg.IdToTag.Num()); // ids start at 1; 0 = invalid
		Reg.TagToId.Add(Tag, NewId);
		return FRuiElementTypeId{ NewId };
	}

	FRuiElementTypeId FindElementType(FName Tag)
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const uint16* Found = Reg.TagToId.Find(Tag))
		{
			return FRuiElementTypeId{ *Found };
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
}
