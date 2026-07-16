// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxFileScan — the .uetkx FILE grammar: preamble directives, `component Name(params) {}`
// declarations, the body splitter (setup vs the LAST top-level markup `return ( ... )` —
// family T1.4 useLastReturn parity), and the hook-call sequence (order + kinds) that both the
// compiler (bakes __RUI_HOOK_SIG) and the Phase 4 interpreter (computes live) hash for the
// deliberate state-reset-on-hook-shape-change semantics. Lives in Interp per D-27.
//
// Params use the family decl grammar: `Name: Type = Default, ...` (types are C++ type names).
// Diagnostics carry family numbers (2100/2101/2102/0108/0303/0304); UE-only limits use the
// UETKX3xxx space (D-18).

#pragma once

#include "CoreMinimal.h"
#include "UetkxMarkup.h"

struct REACTIVEUIINTERP_API FUetkxDiag
{
	FString Code;
	int32 Severity = 0; // 0 err / 1 warn / 2 hint (sidecar schema v2)
	FString Message;
	int32 Offset = -1; // code points into the ORIGINAL source; -1 = whole file
	int32 Length = 1;
};

struct REACTIVEUIINTERP_API FUetkxParam
{
	FString Name;
	FString Type;
	FString Default; // empty = required
};

/** The three declaration kinds a .uetkx file may hold, in any number and any order (FULL
 *  MIXED-DECL v1, A1). The scan records the source ORDER as (kind, index-into-its-array) pairs so
 *  the emitter can lower declarations in the order the author wrote them. */
enum class EUetkxDeclKind : uint8
{
	Component,
	Hook,
	Module
};

/** `import { A, B } from "specifier"` — a PREAMBLE-ONLY static import (A1): named bindings only
 *  (no default, no `*`), string-literal specifier (extensionless; `./` `../` `~/` forms resolved
 *  by the import resolver, M4). Names/NameAts are 1:1; SpecifierAt/At are code-point offsets into
 *  the original source (D-18). */
struct REACTIVEUIINTERP_API FUetkxImportDecl
{
	TArray<FString> Names;
	TArray<int32> NameAts;
	FString Specifier;
	int32 SpecifierAt = -1; // offset of the opening quote
	int32 At = -1;			// offset of the `import` keyword
};

/** One markup `return ( ... )` span inside a component body (wave G early returns — the
 *  Unity verbatim-emit model: the body splices verbatim, every markup return lowers in
 *  place). Offsets are into the BODY code points. */
struct REACTIVEUIINTERP_API FUetkxReturnSpan
{
	int32 ReturnAt = -1; // offset of `return`
	int32 MStart = -1;	 // first char inside `(`
	int32 MEnd = -1;	 // index OF the closing `)`
	int32 AfterParen = -1;
	bool bTopLevel = false;		 // at brace+paren depth 0 (a statement of the body itself)
	TSharedPtr<FUetkxNode> Root; // the span's single render root (filled by the component scan)
};

struct REACTIVEUIINTERP_API FUetkxComponentDecl
{
	FString Name;
	bool bExported = false; // `export component` — cross-file addressable (privacy is opt-in, A3)
	int32 At = -1;			// the `component` keyword offset (the `export`, if any, precedes it)
	int32 ExportAt = -1;	// the `export` keyword offset when bExported, else -1 (the decl's true start)
	int32 NameAt = -1;
	TArray<FUetkxParam> Params;
	FString Setup; // body text before the FINAL (top-level) markup return (verbatim C++)
	int32 SetupAt = -1;
	FString Body; // entire body text (formatter/diagnostics)
	int32 BodyAt = -1;
	TSharedPtr<FUetkxNode> Root; // the FINAL render root (the last, top-level markup return)
	TArray<TSharedPtr<FUetkxNode>> WindowNodes;
	/** ALL markup returns in source order (wave G): the last one is the top-level anchor
	 *  (== Root); earlier ones are early returns the emitter splices in place. Single-return
	 *  bodies have exactly one entry — the emitter's legacy path (byte-stable goldens). */
	TArray<FUetkxReturnSpan> Returns;
	TArray<FString> HookCalls; // ordered hook kinds in setup ("UseState", "UseEffect", ...)
	int32 Next = -1;		   // just past the closing brace
};

/** `hook UseName(params) [-> Ret] { body }` — a user hook (support-file declaration). The
 *  params/body are VERBATIM C++ (unlike component params, which use the `Name: Type` decl
 *  grammar) — C++ params can carry template commas the family grammar can't split. */
struct REACTIVEUIINTERP_API FUetkxHookDecl
{
	FString Name;
	bool bExported = false; // `export hook`
	int32 At = -1;
	int32 ExportAt = -1; // the `export` keyword offset when bExported, else -1 (the decl's true start)
	int32 NameAt = -1;
	FString Params; // verbatim C++ parameter list (may be empty)
	FString Ret;	// verbatim return type; empty = void (family: omitted arrow ⇒ void)
	FString Body;	// verbatim C++ body
	int32 BodyAt = -1;
	int32 Next = -1;
};

/** `module Name { body }` — verbatim C++ declarations (style dicts, types, statics). */
struct REACTIVEUIINTERP_API FUetkxModuleDecl
{
	FString Name;
	bool bExported = false; // `export module`
	int32 At = -1;
	int32 ExportAt = -1; // the `export` keyword offset when bExported, else -1 (the decl's true start)
	int32 NameAt = -1;
	FString Body; // verbatim C++
	int32 BodyAt = -1;
	int32 Next = -1;
};

struct REACTIVEUIINTERP_API FUetkxFileScanResult
{
	TArray<FString> PreambleIncludes; // verbatim `#include ...` lines from the preamble
	TArray<FUetkxImportDecl> Imports; // preamble `import { … } from "…"` declarations (A1)
	// FULL MIXED-DECL v1 (A1): a file is a SEQUENCE of any number of components + hooks + modules
	// in any order. Each array holds its own kind; `Order` records the source order across all
	// three so the emitter lowers declarations exactly as written (>1 component is a LINT warn,
	// UETKX2105, not an error).
	TArray<FUetkxComponentDecl> Components;
	TArray<FUetkxHookDecl> Hooks;
	TArray<FUetkxModuleDecl> Modules;
	TArray<TPair<EUetkxDeclKind, int32>> Order; // (kind, index-into-that-kind's-array), source order
	TArray<FUetkxDiag> Diags;

	// TRANSITION helper (A1): "has no markup" — true when the file declares no component. Retires
	// from dispatch decisions in M3 (codegen emits per `Order`); kept until the HMR rewrite (M9)
	// still reads it. NOT a file-KIND classifier anymore — a mixed file has both markup and hooks.
	bool IsSupportFile() const { return Components.IsEmpty() && (Hooks.Num() + Modules.Num()) > 0; }

	bool HasError() const
	{
		for (const FUetkxDiag& Diag : Diags)
		{
			if (Diag.Severity == 0)
			{
				return true;
			}
		}
		return false;
	}
};

/** One declaration's identity, as seen by the cheap preamble scan (no markup parse). */
struct REACTIVEUIINTERP_API FUetkxPreambleDecl
{
	FString Name;
	EUetkxDeclKind Kind = EUetkxDeclKind::Component;
	bool bExported = false;
	int32 At = -1;		 // the decl keyword offset (a preceding `export`, if any, is at ExportAt)
	int32 ExportAt = -1; // the `export` keyword offset when bExported, else -1 (codemod insert point)
};

/** The result of ScanPreamble: imports + declaration identities in source order, WITHOUT parsing
 *  component markup. The import resolver reads this to build export tables + export hashes cheaply
 *  and without tripping on a target file's markup errors (A2 source-truth graph). */
struct REACTIVEUIINTERP_API FUetkxPreambleScan
{
	TArray<FUetkxImportDecl> Imports;
	TArray<FUetkxPreambleDecl> Decls;
	int32 FirstDeclAt = -1; // offset where the first declaration begins (preamble end); -1 = none
};

/** The (last) top-level markup `return ( ... )` inside a body — the ONE splitter shared by
 *  the file scan, the emitter, and the formatter (they must never disagree on the window). */
struct REACTIVEUIINTERP_API FUetkxSplitReturn
{
	bool bOk = false;
	int32 ReturnAt = -1; // offset of `return`
	int32 MStart = -1;	 // first char inside `(`
	int32 MEnd = -1;	 // index OF the closing `)`
	int32 AfterParen = -1;
};

class REACTIVEUIINTERP_API FUetkxFileScan
{
public:
	/** The 20 hook names (auto-prefix + signature scanning). */
	static const TArray<FString>& HookNames();

	/** FNV-1a (2166136261/16777619, 32-bit) over the ordered hook-kind sequence. */
	static uint32 HookSignature(const TArray<FString>& HookCalls);

	/** Scan a whole .uetkx source: preamble + every component declaration. */
	static FUetkxFileScanResult Scan(const FString& Source, const FString& Basename);

	/** Cheap scan: preamble imports + declaration identities (name/kind/exported) in source order,
	 *  WITHOUT parsing component markup. Used by the import resolver for export tables + hashes; it
	 *  never fails on a target's markup errors (best-effort — stops at the first unparseable decl
	 *  header). */
	static FUetkxPreambleScan ScanPreamble(const FString& Source);

	/** Find the LAST top-level `return ( ... )` in a body (T1.4 semantics). With
	 *  bRequireMarkupPeek the window must START like markup (`<`/`@`/`{`, after leading markup
	 *  comments) — the component scan's rule; without it any parenthesized return counts — the
	 *  directive-body rule (EmitBody/formatter). */
	static FUetkxSplitReturn SplitMarkupReturn(const TArray<int32>& Body, bool bRequireMarkupPeek);

	/** ALL markup returns in a component body, source order (wave G early returns). Paren/
	 *  bracket nesting hides a `return` (call arguments); brace nesting does NOT (if/else
	 *  blocks — the early-return shape). The window must peek like markup: `<`/`@` anywhere,
	 *  `{` (expression window) only at top level — a brace-init `return ({...})` inside a
	 *  lambda must stay verbatim C++. Roots are NOT filled here (the component scan parses
	 *  each window). */
	static TArray<FUetkxReturnSpan> CollectMarkupReturns(const TArray<int32>& Body);
};
