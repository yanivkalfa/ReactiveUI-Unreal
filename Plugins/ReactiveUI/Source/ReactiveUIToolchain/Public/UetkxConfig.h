// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxConfig — the ONE `uetkx.config.json` reader (M1). A single walk-up loader shared by the
// formatter (indent/print-width options) and the import resolver (the `~/` family-root anchor),
// so both agree on which config wins for a given file. Walk-up is NEAREST-CONFIG-WINS with NO
// merge (A5g): a nearer formatter-only config does NOT inherit an ancestor's `"root"` — it resets
// `~/` to the module-root default. Malformed JSON = defaults (never half-applied).
//
// The `~/` anchor (A1/round-3): the config's `"root"` key, a path RELATIVE to the config file's
// own directory, names the family root. With no config, or a config that omits `"root"`, the
// anchor is the file's UE MODULE ROOT (nearest `*.Build.cs` ancestor dir) — the same rule the
// aggregator groups by. `~/` never escapes its module; a `"root"` resolving outside the owning
// module is a config error surfaced as UETKX2308 at resolution time (the resolver's job, M4).

#pragma once

#include "CoreMinimal.h"

struct REACTIVEUITOOLCHAIN_API FUetkxConfig
{
	// Formatter-facing options (defaults identical to FUetkxFormatOptions today).
	int32 PrintWidth = 100;
	FString IndentStyle = TEXT("tab"); // "tab" | "space"
	int32 IndentSize = 2;
	bool bSingleAttributePerLine = false;
	bool bInsertSpaceBeforeSelfClose = true;

	// The winning config's `"root"` declaration (A5g). bHasRoot is false when no config was found
	// OR the found config omitted `"root"` — in both cases the `~/` anchor falls back to the
	// module root. RootRelative is the raw string as written; ConfigDir is the absolute directory
	// of the config that won (empty when none was found).
	bool bHasRoot = false;
	FString RootRelative;
	FString ConfigDir;

	/** Walk UP from Dir to the nearest `uetkx.config.json` (≤32 levels, nearest wins, NO merge;
	 *  malformed = defaults). The one loader the formatter and resolver both consume. */
	static FUetkxConfig Load(const FString& Dir);

	/** The nearest `*.Build.cs` ancestor directory of Path (the UE module root, dir name ==
	 *  module name by UBT convention), or "" if none. Absolute, `/`-normalized. */
	static FString ModuleRootFor(const FString& Path);

	/** The `~/` anchor for a .uetkx file: the nearest config's resolved `"root"` when it declared
	 *  one (ConfigDir / RootRelative, collapsed), otherwise ModuleRootFor(UetkxAbsPath). Absolute,
	 *  `/`-normalized, no trailing slash. "" only when neither a root nor a module could be found.
	 *  The caller (resolver) is responsible for the "root escapes its module" UETKX2308 check. */
	static FString RootAnchorFor(const FString& UetkxAbsPath);
};
