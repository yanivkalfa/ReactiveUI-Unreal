// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// URuiWorldSubsystem — the per-world mount surface with the TEARDOWN CONTRACT: every root
// mounted through it is unmounted (cleanups, refs, host widgets) when the world dies — PIE
// end, level travel, world destruction — BEFORE the world's UObjects are GC'd. "See it on
// screen in one step": GetWorld()->GetSubsystem<URuiWorldSubsystem>()->MountNamed("Menu").

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"
#include "Subsystems/WorldSubsystem.h"
#include "RuiWorldSubsystem.generated.h"

class FRuiRoot;

UCLASS()
class REACTIVEUIUMG_API URuiWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Mount a registered component over the game viewport. Returns a handle id (for
	 *  UnmountHandle); INDEX_NONE when the name is unknown. */
	UFUNCTION(BlueprintCallable, Category = "ReactiveUI")
	int32 MountNamed(FName ComponentName, int32 ZOrder = 10);

	/** Mount an arbitrary node (C++ callers). */
	int32 MountNode(FRuiNode Node, int32 ZOrder = 10);

	UFUNCTION(BlueprintCallable, Category = "ReactiveUI")
	void UnmountHandle(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = "ReactiveUI")
	void UnmountAll();

	int32 NumLiveRoots() const { return Roots.Num(); }

	// UWorldSubsystem
	virtual void Deinitialize() override; // the teardown contract

private:
	TMap<int32, TSharedPtr<FRuiRoot>> Roots;
	int32 NextHandle = 1;
};
