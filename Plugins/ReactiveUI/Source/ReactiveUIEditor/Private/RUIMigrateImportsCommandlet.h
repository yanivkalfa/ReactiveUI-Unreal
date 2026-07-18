// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// `-run=RUIMigrateImports` — the one-shot, IDEMPOTENT, re-runnable codemod (A8d) that migrates a
// .uetkx tree to the strict import/export world: it adds `export` to EVERY existing declaration
// (export-everything, A3 — by-name C++/UMG consumers keep resolving) and inserts the `import { … }
// from "…"` lines every file needs, derived from a FRESH reference scan (component tags + user
// hook calls + module quals against the export tables — NOT sidecar edges, A3). Cross-module edges
// are reported (UETKX2308) and NOT written — they surface a real ownership decision. Finishes by
// compiling the whole tree WITH resolution and requiring ZERO diagnostics (the strict-day-one
// merge gate). Re-running is a no-op (already-exported decls + already-imported names are skipped).
//
// `-tidy` (INCLUDE_RETIREMENT_PLAN.md §C, Unity's `--tidy` analogue): an ADDITIONAL pass, run
// before export/import migration, that rewrites each file's preamble so it holds ONLY import
// lines — a raw `#include "X.h"` on the auto-included prelude list (FUetkxFileScan::
// AutoIncludedHeaders) is DELETED; a surviving one converts to `import "@X.h"`. Idempotent; a
// preamble with stray comments/content is left untouched (logged) rather than risk destroying
// user text. Omitting `-tidy` runs exactly the pre-existing export/import migration.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "RUIMigrateImportsCommandlet.generated.h"

UCLASS()
class URUIMigrateImportsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URUIMigrateImportsCommandlet()
	{
		IsClient = false;
		IsServer = false;
		IsEditor = true;
		LogToConsole = true;
	}

	virtual int32 Main(const FString& Params) override;
};

/** `-run=RUIMigrateEsModules` — the ES-modules codemod (ES_MODULES_EXECUTION_PLAN.md M7, G-10):
 *  IDEMPOTENT, re-runnable, scanned-record-driven (never raw-regex over params). Pipeline:
 *  1. tidy (the -tidy pass, unconditional here) → 2. export-normalize (pass 1) →
 *  3. wrapper rewrite: `component Name(P: T = D)` → `FRuiNode Name(T P = D)` (param colon-flip
 *  from the scanned records; param-less gains `()`); `hook UseX(raw) -> R` → `R UseX(raw)`
 *  (no `->` ⇒ `void`); `module M { members }` HOISTS each simple member (`inline const T N = i;`
 *  / `inline const T N{i};` → `[export] T N = …;`, member functions → utils) — a module holding
 *  anything else (struct/class/using/typedef/static…) is LEFT WRAPPED and reported (its 2320
 *  stays; the summary names it); same-file `M::x` refs strip to `x`; other files' `{ M }`
 *  imports convert to `import * as M from "…"` (their `M::x` bodies keep compiling through the
 *  U-03 strip plane, zero body edits) → 4. insert/fix imports (pass 2) → 5. the zero-diagnostics
 *  compile gate: zero errors AND zero UETKX2320 warnings outside the explicitly-skipped module
 *  files (counted from compile results, NOT the Message Log — the watcher's log path only shows
 *  severity-0). The old `-run=RUIMigrateImports` stays working, untouched. */
UCLASS()
class URUIMigrateEsModulesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URUIMigrateEsModulesCommandlet()
	{
		IsClient = false;
		IsServer = false;
		IsEditor = true;
		LogToConsole = true;
	}

	virtual int32 Main(const FString& Params) override;
};
