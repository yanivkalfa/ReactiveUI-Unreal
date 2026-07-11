// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiWorldSubsystem.h"

#include "RuiRoot.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiSubsystem, Log, All);

int32 URuiWorldSubsystem::MountNamed(FName ComponentName, int32 ZOrder)
{
	if (!RUI::HasNamedFactory(ComponentName))
	{
		UE_LOG(LogRuiSubsystem, Error, TEXT("MountNamed: '%s' is not a registered component"),
			   *ComponentName.ToString());
		return INDEX_NONE;
	}
	return MountNode(RUI::Named(ComponentName), ZOrder);
}

int32 URuiWorldSubsystem::MountNode(FRuiNode Node, int32 ZOrder)
{
	// In a real game world the root parents to the viewport; worlds without one (headless
	// tests, dedicated servers) get a detached root — the teardown contract is identical.
	UWorld* World = GetWorld();
	TSharedPtr<FRuiRoot> Root;
	if (World && World->GetGameViewport())
	{
		Root = FRuiRoot::CreateInViewport(MoveTemp(Node), ZOrder);
	}
	else
	{
		Root = FRuiRoot::Create(MoveTemp(Node));
	}
	Root->FlushSync();
	const int32 Handle = NextHandle++;
	Roots.Add(Handle, Root);
	return Handle;
}

void URuiWorldSubsystem::UnmountHandle(int32 Handle)
{
	TSharedPtr<FRuiRoot> Root;
	if (Roots.RemoveAndCopyValue(Handle, Root) && Root.IsValid())
	{
		Root->Unmount();
	}
}

void URuiWorldSubsystem::UnmountAll()
{
	for (TPair<int32, TSharedPtr<FRuiRoot>>& Pair : Roots)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Unmount();
		}
	}
	Roots.Empty();
}

void URuiWorldSubsystem::Deinitialize()
{
	// PIE end / level travel / world death: every mounted root unmounts NOW — cleanups and
	// refs run before the world's UObjects are GC'd (the family teardown contract).
	if (Roots.Num() > 0)
	{
		UE_LOG(LogRuiSubsystem, Display, TEXT("world teardown: unmounting %d ReactiveUI root(s)"), Roots.Num());
	}
	UnmountAll();
	Super::Deinitialize();
}
