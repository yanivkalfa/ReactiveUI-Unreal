// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxFileActions — file-level actions behind the editor UX (New Component, open in the
// external IDE). Lives with the compiler so the template stays next to the grammar it must
// satisfy (a test compiles it). Content-Browser visibility for source-tree .uetkx is post-v1
// (see TECH_DEBT: source files are not assets; needs a UAssetDefinition decision).

#pragma once

#include "CoreMinimal.h"

class REACTIVEUITOOLCHAIN_API FUetkxFileActions
{
public:
	/** Create `<Dir>/<Name>.uetkx` from the starter template. Name must be UpperCamel (the
	 *  component grammar). Never overwrites. Returns the created path, or "" on refusal. */
	static FString CreateComponentFile(const FString& Dir, const FString& Name);

	/** Open a .uetkx in the OS-default external editor (MessageLog links, context menus). */
	static void OpenExternal(const FString& Path);
};
