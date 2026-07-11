// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxMarkup — recursive-descent parser: a markup window (the inside of `return ( ... )`)
// -> an AST. Line-for-line port of the family's guitkx_markup.gd (byte-compatible grammar;
// the diagnostics keep the family numbers as UETKX####). Control-flow directives
// (@if/@for/@while/@match) are carried as RAW body strings + rebasable offsets for the
// emitter/interpreter to lower.
//
// POSITIONS: every node carries `At` — the CODE POINT offset of its first character in the
// source given to Parse (D-18). Extracted-substring fields carry a companion offset into the
// SAME source: attr `Vat` (value text start; -1 for bool), expr `Vat`, control-flow
// `BodyAt`/`ElseBodyAt`/`DefaultBodyAt` (-1 when absent), attr `End` (one past the last
// char). Offsets compose: re-parsing a BodyMarkup substring rebases nested offsets by adding
// the node's BodyAt. Parse errors carry ErrorAt the same way.
//
// TEXT MODEL (family T2.4): a text run stops only at `<` or `@`; `{expr}` interpolation is
// recognized at NODE START only; braces inside an ongoing text run are LITERAL (the compiler
// warns UETKX0150). Comments: `//`, `/* */`, `<!-- -->` at node start; `{/* ... */}` inside
// attribute lists.

#pragma once

#include "CoreMinimal.h"

enum class EUetkxNodeType : uint8
{
	El,
	Frag,
	Text,
	Expr,
	Comment,
	If,
	For,
	While,
	Match
};

enum class EUetkxAttrKind : uint8
{
	Str,
	Expr,
	Bool,
	Spread,
	Comment
};

struct REACTIVEUIINTERP_API FUetkxAttr
{
	FString Name;
	EUetkxAttrKind Kind = EUetkxAttrKind::Bool;
	FString Value;
	int32 At = -1;
	int32 Vat = -1;
	int32 End = -1;
};

struct REACTIVEUIINTERP_API FUetkxIfBranch
{
	FString Cond;
	FString BodyMarkup;
	int32 BodyAt = -1;
};

struct REACTIVEUIINTERP_API FUetkxMatchCase
{
	FString Value;
	FString BodyMarkup;
	int32 BodyAt = -1;
};

struct REACTIVEUIINTERP_API FUetkxNode
{
	EUetkxNodeType Type = EUetkxNodeType::Text;
	int32 At = -1;

	// El
	FString Tag;
	TArray<FUetkxAttr> Attrs; // also Frag when spelled <Fragment ...>
	TArray<TSharedPtr<FUetkxNode>> Children;
	int32 Line = 0;

	// Frag
	FString Named; // author spelling when <Fragment>; empty for <>

	// Text (trimmed) / Comment (raw)
	FString Value;

	// Expr
	FString Code;
	int32 Vat = -1;

	// If
	TArray<FUetkxIfBranch> Branches;
	TOptional<FString> ElseBody;
	int32 ElseBodyAt = -1;

	// For / While
	FString Header;
	FString BodyMarkup;
	int32 BodyAt = -1;

	// Match
	FString Subject;
	TArray<FUetkxMatchCase> Cases;
	TOptional<FString> DefaultBody;
	int32 DefaultBodyAt = -1;
};

struct REACTIVEUIINTERP_API FUetkxParseResult
{
	TArray<TSharedPtr<FUetkxNode>> Nodes;
	FString ErrorCode; // "" when clean (UETKX####)
	FString ErrorMsg;
	int32 ErrorAt = -1;

	bool IsOk() const { return ErrorCode.IsEmpty(); }
	FString ErrorLegacy() const
	{
		return ErrorCode.IsEmpty() ? FString() : FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMsg);
	}
};

class REACTIVEUIINTERP_API FUetkxMarkup
{
public:
	/** Parse the top-level nodes of the markup window [Start, End) of Src (code points). */
	FUetkxParseResult Parse(const TArray<int32>& Src, int32 Start, int32 End);

private:
	struct FNodesResult
	{
		TArray<TSharedPtr<FUetkxNode>> Nodes;
		int32 Next = 0;
	};
	struct FNodeResult
	{
		TSharedPtr<FUetkxNode> Node;
		int32 Next = 0;
	};
	struct FAttrResult
	{
		FUetkxAttr Attr;
		bool bValid = false;
		int32 Next = 0;
	};
	struct FSpanResult
	{
		FString Text;
		int32 Next = 0;
		int32 At = -1;
	};

	void Fail(const TCHAR* Code, const FString& Msg, int32 At);
	FNodesResult ParseNodes(int32 Start, int32 End);
	FNodeResult ParseElement(int32 OpenIndex, int32 End);
	TSharedPtr<FUetkxNode> MakeEl(const FString& Tag, TArray<FUetkxAttr>&& Attrs,
								  TArray<TSharedPtr<FUetkxNode>>&& Children, int32 Line, int32 At);
	FAttrResult ParseAttribute(int32 Start, int32 End);
	FNodeResult ParseText(int32 Start, int32 End);
	FNodeResult ParseDirective(int32 At, int32 End);
	FSpanResult ReadParen(int32 Index, int32 End);
	FSpanResult ReadBraceBody(int32 Index, int32 End);
	FNodeResult ParseIf(int32 At, int32 End);
	FNodeResult ParseLoop(int32 At, int32 End, EUetkxNodeType Kind, int32 KeywordLen);
	FNodeResult ParseMatch(int32 At, int32 End);

	int32 SkipWs(int32 Index, int32 End) const;
	static bool IsTagChar(int32 C);
	static bool IsAttrNameChar(int32 C);
	int32 LineOf(int32 Index) const;
	FString Slice(int32 Start, int32 End) const;		// raw substring [Start, End)
	FString SliceTrimmed(int32 Start, int32 End) const; // strip_edges parity
	int32 FindSeq(const TCHAR* Seq, int32 From) const;	// _src.find(seq, from) parity (-1 none)

	const TArray<int32>* Src = nullptr;
	FString ErrCode;
	FString ErrMsg;
	int32 ErrAt = -1;
	TArray<int32> LineStarts;
};
