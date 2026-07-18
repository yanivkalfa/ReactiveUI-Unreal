// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxFormatter.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UetkxConfig.h"
#include "UetkxFileScan.h"
#include "UetkxLexer.h"

namespace
{
	struct FFmtState
	{
		const FUetkxFormatOptions& O;
		bool bUnsafeStrAttr = false; // G-05 escape hatch -> verbatim fallback
	};

	FString Pad(int32 Indent, const FUetkxFormatOptions& O)
	{
		if (O.IndentStyle == TEXT("space"))
		{
			return FString::ChrN(Indent * O.IndentSize, ' ');
		}
		return FString::ChrN(Indent, '\t');
	}

	FString LeadingWs(const FString& S)
	{
		int32 i = 0;
		while (i < S.Len() && (S[i] == ' ' || S[i] == '\t'))
		{
			++i;
		}
		return S.Left(i);
	}

	FString StripLeadingWs(const FString& S)
	{
		return S.Mid(LeadingWs(S).Len());
	}

	/** Collapse runs of 2+ spaces to one OUTSIDE strings/comments (the same skip_noncode
	 *  primitive the scanner corpus cross-tests). */
	FString CollapseSpaces(const FString& S)
	{
		const TArray<int32> Cp = FUetkxLexer::ToCodePoints(S);
		const int32 N = Cp.Num();
		FString Out;
		int32 i = 0;
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Cp, i);
			if (j != i)
			{
				Out += FUetkxLexer::FromCodePoints(Cp, i, j - i);
				i = j;
				continue;
			}
			if (Cp[i] == ' ' && i + 1 < N && Cp[i + 1] == ' ')
			{
				Out += TEXT(" ");
				while (i < N && Cp[i] == ' ')
				{
					++i;
				}
				continue;
			}
			if (Cp[i] > 0xFFFF)
			{
				Out += FUetkxLexer::FromCodePoints(Cp, i, 1);
			}
			else
			{
				Out.AppendChar(static_cast<TCHAR>(Cp[i]));
			}
			++i;
		}
		return Out;
	}

	/** Inferred space-indent unit: the minimum POSITIVE DIFFERENCE between distinct leading-
	 *  space widths (NOT the min width — a block anchored at an offset would fold levels,
	 *  family Phase-D corruption find). Tab-only blocks keep unit 1. */
	int32 IndentUnit(const TArray<FString>& Lines)
	{
		TSet<int32> Widths;
		for (const FString& Line : Lines)
		{
			int32 Spaces = 0;
			for (int32 i = 0; i < Line.Len(); ++i)
			{
				if (Line[i] == ' ')
				{
					++Spaces;
				}
				else if (Line[i] == '\t')
				{
					continue;
				}
				else
				{
					break;
				}
			}
			if (Spaces > 0)
			{
				Widths.Add(Spaces);
			}
		}
		if (Widths.IsEmpty())
		{
			return 1;
		}
		TArray<int32> Sorted = Widths.Array();
		Sorted.Sort();
		int32 Unit = Sorted[0];
		for (int32 i = 1; i < Sorted.Num(); ++i)
		{
			const int32 D = Sorted[i] - Sorted[i - 1];
			if (D > 0 && D < Unit)
			{
				Unit = D;
			}
		}
		return FMath::Max(Unit, 1);
	}

	/** Depth of a line in whole levels: tab = `Unit` columns, space = 1, rounded. */
	int32 IndentDepth(const FString& S, int32 Unit)
	{
		int32 Cols = 0;
		for (int32 i = 0; i < S.Len(); ++i)
		{
			if (S[i] == '\t')
			{
				Cols += Unit;
			}
			else if (S[i] == ' ')
			{
				Cols += 1;
			}
			else
			{
				break;
			}
		}
		return FMath::RoundToInt32(static_cast<float>(Cols) / static_cast<float>(Unit));
	}

	/** One bool per line: true when the line STARTS inside a noncode span (multi-line string,
	 *  raw string, block comment, preprocessor continuation) opened on an EARLIER line —
	 *  re-indenting such a line would corrupt a runtime VALUE (G-02). */
	TArray<bool> StringLineMask(const TArray<FString>& Lines)
	{
		const TArray<int32> Cp = FUetkxLexer::ToCodePoints(FString::Join(Lines, TEXT("\n")));
		const int32 N = Cp.Num();
		TArray<int32> LineStarts;
		LineStarts.Add(0);
		for (int32 i = 0; i < N; ++i)
		{
			if (Cp[i] == '\n')
			{
				LineStarts.Add(i + 1);
			}
		}
		TArray<bool> Mask;
		Mask.Init(false, LineStarts.Num());
		int32 LineIdx = 0;
		int32 i = 0;
		while (i < N)
		{
			while (LineIdx + 1 < LineStarts.Num() && LineStarts[LineIdx + 1] <= i)
			{
				++LineIdx;
			}
			const int32 j = FUetkxLexer::SkipNoncode(Cp, i);
			if (j != i)
			{
				int32 EndLine = LineIdx;
				while (EndLine + 1 < LineStarts.Num() && LineStarts[EndLine + 1] <= j)
				{
					++EndLine;
				}
				for (int32 m = LineIdx + 1; m <= EndLine && m < Mask.Num(); ++m)
				{
					Mask[m] = true;
				}
				i = j;
				continue;
			}
			++i;
		}
		return Mask;
	}

	TArray<FString> SplitTrimBlankEdges(const FString& Code)
	{
		TArray<FString> Lines;
		Code.ParseIntoArray(Lines, TEXT("\n"), /*bCullEmpty*/ false);
		while (!Lines.IsEmpty() && Lines[0].TrimStartAndEnd().IsEmpty())
		{
			Lines.RemoveAt(0);
		}
		while (!Lines.IsEmpty() && Lines.Last().TrimStartAndEnd().IsEmpty())
		{
			Lines.Pop();
		}
		return Lines;
	}

	/** Re-indent embedded C++ to depth-based indentation anchored at Indent. Depth-based, not
	 *  character-preserving (mixed tab+space bodies normalize); interior blank lines stay
	 *  (G-03); masked lines stay byte-verbatim (G-02). */
	FString Reanchor(const FString& Code, int32 Indent, const FUetkxFormatOptions& O)
	{
		TArray<FString> Lines = SplitTrimBlankEdges(Code);
		if (Lines.IsEmpty())
		{
			return FString();
		}
		const TArray<bool> Mask = StringLineMask(Lines);
		TArray<FString> UnitLines;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (!Mask[i])
			{
				UnitLines.Add(Lines[i]);
			}
		}
		const int32 Unit = IndentUnit(UnitLines);
		int32 Anchor = -1, AnchorAny = -1;
		TArray<int32> Depths;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (Mask[i] || Lines[i].TrimStartAndEnd().IsEmpty())
			{
				Depths.Add(-1);
				continue;
			}
			const int32 D = IndentDepth(Lines[i], Unit);
			Depths.Add(D);
			if (AnchorAny == -1)
			{
				AnchorAny = D;
			}
			if (Anchor == -1 && !Lines[i].TrimStartAndEnd().StartsWith(TEXT("//")))
			{
				Anchor = D;
			}
		}
		if (Anchor == -1)
		{
			Anchor = AnchorAny; // comment-only block
		}
		FString Out;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (Mask[i])
			{
				Out += Lines[i] + TEXT("\n");
			}
			else if (Depths[i] == -1)
			{
				Out += TEXT("\n");
			}
			else
			{
				const int32 Level = Indent + FMath::Max(0, Depths[i] - Anchor);
				Out += Pad(Level, O) + CollapseSpaces(StripLeadingWs(Lines[i])) + TEXT("\n");
			}
		}
		return Out;
	}

	/** Re-anchor a body SEGMENT with the WHOLE body's unit/anchor geometry (a segment must
	 *  not re-anchor on its own first line — family _reanchor_rel). */
	FString ReanchorRel(const FString& Code, int32 Indent, int32 Unit, int32 Anchor, const FUetkxFormatOptions& O)
	{
		TArray<FString> Lines = SplitTrimBlankEdges(Code);
		if (Lines.IsEmpty())
		{
			return FString();
		}
		const TArray<bool> Mask = StringLineMask(Lines);
		FString Out;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (Mask[i])
			{
				Out += Lines[i] + TEXT("\n");
				continue;
			}
			if (Lines[i].TrimStartAndEnd().IsEmpty())
			{
				Out += TEXT("\n"); // interior blank line is real formatting (G-03)
				continue;
			}
			const int32 Level = Indent + FMath::Max(0, IndentDepth(Lines[i], Unit) - Anchor);
			Out += Pad(Level, O) + CollapseSpaces(StripLeadingWs(Lines[i])) + TEXT("\n");
		}
		return Out;
	}

	/** Whole-body indent geometry for ReanchorRel callers. */
	void BodyGeometry(const FString& Body, int32& OutUnit, int32& OutAnchor)
	{
		TArray<FString> Lines = SplitTrimBlankEdges(Body);
		const TArray<bool> Mask = StringLineMask(Lines);
		TArray<FString> UnitLines;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (!Mask[i])
			{
				UnitLines.Add(Lines[i]);
			}
		}
		OutUnit = IndentUnit(UnitLines);
		OutAnchor = -1;
		int32 AnchorAny = -1;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (Mask[i] || Lines[i].TrimStartAndEnd().IsEmpty())
			{
				continue;
			}
			const int32 D = IndentDepth(Lines[i], OutUnit);
			if (AnchorAny == -1)
			{
				AnchorAny = D;
			}
			if (OutAnchor == -1 && !Lines[i].TrimStartAndEnd().StartsWith(TEXT("//")))
			{
				OutAnchor = D;
				break;
			}
		}
		if (OutAnchor == -1)
		{
			OutAnchor = FMath::Max(AnchorAny, 0);
		}
	}

	// authored blank line at the start/end of an embedded block (family audit #1)
	bool HasLeadingBlank(const FString& S)
	{
		int32 i = 0;
		const int32 N = S.Len();
		while (i < N && (S[i] == ' ' || S[i] == '\t'))
		{
			++i;
		}
		if (i >= N || S[i] != '\n')
		{
			return false;
		}
		++i;
		while (i < N && (S[i] == ' ' || S[i] == '\t'))
		{
			++i;
		}
		return i < N && S[i] == '\n';
	}

	bool HasTrailingBlank(const FString& S)
	{
		int32 i = S.Len() - 1;
		while (i >= 0 && (S[i] == ' ' || S[i] == '\t'))
		{
			--i;
		}
		if (i < 0 || S[i] != '\n')
		{
			return false;
		}
		--i;
		while (i >= 0 && (S[i] == ' ' || S[i] == '\t'))
		{
			--i;
		}
		return i >= 0 && S[i] == '\n';
	}

	FString RStripWsNl(const FString& S)
	{
		int32 End = S.Len();
		while (End > 0 && (S[End - 1] == ' ' || S[End - 1] == '\t' || S[End - 1] == '\n'))
		{
			--End;
		}
		return S.Left(End);
	}

	FString LStripWsNl(const FString& S)
	{
		int32 Start = 0;
		while (Start < S.Len() && (S[Start] == ' ' || S[Start] == '\t' || S[Start] == '\n'))
		{
			++Start;
		}
		return S.Mid(Start);
	}

	FString FmtNode(const FUetkxNode& Node, int32 Indent, FFmtState& State);

	FString FmtAttr(const FUetkxAttr& Attr, FFmtState& State)
	{
		switch (Attr.Kind)
		{
		case EUetkxAttrKind::Str:
			// G-05: the parser cannot produce an embedded `"` today; if that ever changes
			// without teaching this function to re-escape, fall back instead of corrupting.
			if (Attr.Value.Contains(TEXT("\"")))
			{
				State.bUnsafeStrAttr = true;
			}
			return FString::Printf(TEXT("%s=\"%s\""), *Attr.Name, *Attr.Value);
		case EUetkxAttrKind::Expr:
			return FString::Printf(TEXT("%s={ %s }"), *Attr.Name, *Attr.Value.TrimStartAndEnd());
		case EUetkxAttrKind::Spread:
			return FString::Printf(TEXT("{...%s}"), *Attr.Value.TrimStartAndEnd());
		case EUetkxAttrKind::Comment:
			return Attr.Value; // `{/* ... */}` verbatim (T2.1)
		case EUetkxAttrKind::Bool:
			break;
		}
		return Attr.Name;
	}

	FString FmtChildren(const TArray<TSharedPtr<FUetkxNode>>& Children, int32 Indent, FFmtState& State)
	{
		FString Out;
		for (const TSharedPtr<FUetkxNode>& Child : Children)
		{
			if (Child.IsValid())
			{
				Out += FmtNode(*Child, Indent, State);
			}
		}
		return Out;
	}

	FString FmtElement(const FUetkxNode& Node, int32 Indent, FFmtState& State)
	{
		const FString P = Pad(Indent, State.O);
		TArray<FString> AttrStrs;
		for (const FUetkxAttr& Attr : Node.Attrs)
		{
			AttrStrs.Add(FmtAttr(Attr, State));
		}
		TArray<TSharedPtr<FUetkxNode>> Children;
		for (const TSharedPtr<FUetkxNode>& Child : Node.Children)
		{
			if (Child.IsValid())
			{
				Children.Add(Child);
			}
		}
		const bool bSelfClose = Children.IsEmpty();
		FString Head = FString::Printf(TEXT("<%s"), *Node.Tag);
		if (!AttrStrs.IsEmpty())
		{
			Head += TEXT(" ") + FString::Join(AttrStrs, TEXT(" "));
		}
		const FString SingleClose =
			bSelfClose ? (State.O.bInsertSpaceBeforeSelfClose ? TEXT(" />") : TEXT("/>")) : TEXT(">");
		const FString Single = Head + SingleClose;
		bool bWrap = State.O.bSingleAttributePerLine && AttrStrs.Num() > 1;
		if (!bWrap && P.Len() + Single.Len() > State.O.PrintWidth && AttrStrs.Num() > 1)
		{
			bWrap = true;
		}
		// TB-4 (TESTING_BUGS.md): an element whose ONLY child is one raw-text run stays inline —
		// `<Button …>+</Button>` — when the whole form fits PrintWidth. TS-identical (fmtElement).
		if (!bWrap && Children.Num() == 1 && Children[0]->Type == EUetkxNodeType::Text)
		{
			const FString Text = Children[0]->Value.TrimStartAndEnd();
			const FString Inline = FString::Printf(TEXT("%s>%s</%s>"), *Head, *Text, *Node.Tag);
			if (!Text.IsEmpty() && P.Len() + Inline.Len() <= State.O.PrintWidth)
			{
				return P + Inline + TEXT("\n");
			}
		}
		FString Out;
		if (!bWrap)
		{
			if (bSelfClose)
			{
				return P + Single + TEXT("\n");
			}
			Out += P + Single + TEXT("\n");
		}
		else
		{
			Out += P + FString::Printf(TEXT("<%s\n"), *Node.Tag);
			const FString APad = Pad(Indent + 1, State.O);
			for (int32 k = 0; k < AttrStrs.Num(); ++k)
			{
				const bool bLast = k == AttrStrs.Num() - 1;
				if (bLast && bSelfClose)
				{
					Out += APad + AttrStrs[k] + (State.O.bInsertSpaceBeforeSelfClose ? TEXT(" />") : TEXT("/>")) +
						   TEXT("\n");
				}
				else if (bLast)
				{
					Out += APad + AttrStrs[k] + TEXT("\n") + P + TEXT(">\n");
				}
				else
				{
					Out += APad + AttrStrs[k] + TEXT("\n");
				}
			}
			if (bSelfClose)
			{
				return Out;
			}
		}
		Out += FmtChildren(Children, Indent + 1, State);
		Out += P + FString::Printf(TEXT("</%s>\n"), *Node.Tag);
		return Out;
	}

	/** A directive body: leading C++ statements re-anchored with the WHOLE body's geometry +
	 *  the (last) markup return re-emitted canonically. Bodies the splitter cannot structure
	 *  (no markup return, trailing content after it, broken window) re-anchor verbatim — the
	 *  compiler reports errors, the formatter must never corrupt. */
	FString FmtBody(const FString& BodySrc, int32 Indent, FFmtState& State)
	{
		const TArray<int32> Body = FUetkxLexer::ToCodePoints(BodySrc);
		const FUetkxSplitReturn Split = FUetkxFileScan::SplitMarkupReturn(Body, /*bRequireMarkupPeek*/ false);
		if (!Split.bOk)
		{
			return Reanchor(BodySrc, Indent, State.O);
		}
		// anything after the close paren beyond whitespace + `;` -> preserve the whole body
		int32 Tail = Split.AfterParen;
		if (Tail < Body.Num() && Body[Tail] == ';')
		{
			++Tail;
		}
		if (!FUetkxLexer::FromCodePoints(Body, Tail, Body.Num() - Tail).TrimStartAndEnd().IsEmpty())
		{
			return Reanchor(BodySrc, Indent, State.O);
		}
		FUetkxMarkup Parser;
		const FUetkxParseResult Pr = Parser.Parse(Body, Split.MStart, Split.MEnd);
		if (!Pr.IsOk())
		{
			return Reanchor(BodySrc, Indent, State.O);
		}
		FString Out;
		const FString Lead = FUetkxLexer::FromCodePoints(Body, 0, Split.ReturnAt);
		if (!Lead.TrimStartAndEnd().IsEmpty())
		{
			int32 Unit = 1, Anchor = 0;
			BodyGeometry(BodySrc, Unit, Anchor);
			Out += ReanchorRel(Lead, Indent, Unit, Anchor, State.O);
		}
		const FString P = Pad(Indent, State.O);
		Out += P + TEXT("return (\n");
		for (const TSharedPtr<FUetkxNode>& Node : Pr.Nodes)
		{
			if (Node.IsValid())
			{
				Out += FmtNode(*Node, Indent + 1, State);
			}
		}
		Out += P + TEXT(");\n");
		return Out;
	}

	FString FmtIf(const FUetkxNode& Node, int32 Indent, FFmtState& State)
	{
		const FString P = Pad(Indent, State.O);
		FString Out;
		for (int32 k = 0; k < Node.Branches.Num(); ++k)
		{
			const FUetkxIfBranch& Branch = Node.Branches[k];
			if (k == 0)
			{
				Out += P + FString::Printf(TEXT("@if (%s) {\n"), *Branch.Cond.TrimStartAndEnd());
			}
			else
			{
				Out = RStripWsNl(Out) + FString::Printf(TEXT(" @elif (%s) {\n"), *Branch.Cond.TrimStartAndEnd());
			}
			Out += FmtBody(Branch.BodyMarkup, Indent + 1, State);
			Out += P + TEXT("}\n");
		}
		if (Node.ElseBody.IsSet())
		{
			Out = RStripWsNl(Out) + TEXT(" @else {\n");
			Out += FmtBody(Node.ElseBody.GetValue(), Indent + 1, State);
			Out += P + TEXT("}\n");
		}
		return Out;
	}

	FString FmtLoop(const FUetkxNode& Node, int32 Indent, FFmtState& State, const TCHAR* Keyword)
	{
		const FString P = Pad(Indent, State.O);
		FString Out = P + FString::Printf(TEXT("@%s (%s) {\n"), Keyword, *Node.Header.TrimStartAndEnd());
		Out += FmtBody(Node.BodyMarkup, Indent + 1, State);
		Out += P + TEXT("}\n");
		return Out;
	}

	FString FmtMatch(const FUetkxNode& Node, int32 Indent, FFmtState& State)
	{
		const FString P = Pad(Indent, State.O);
		FString Out = P + FString::Printf(TEXT("@match (%s) {\n"), *Node.Subject.TrimStartAndEnd());
		for (const FUetkxMatchCase& Case : Node.Cases)
		{
			Out += Pad(Indent + 1, State.O) + FString::Printf(TEXT("@case (%s) {\n"), *Case.Value.TrimStartAndEnd());
			Out += FmtBody(Case.BodyMarkup, Indent + 2, State);
			Out += Pad(Indent + 1, State.O) + TEXT("}\n");
		}
		if (Node.DefaultBody.IsSet())
		{
			Out += Pad(Indent + 1, State.O) + TEXT("@default {\n");
			Out += FmtBody(Node.DefaultBody.GetValue(), Indent + 2, State);
			Out += Pad(Indent + 1, State.O) + TEXT("}\n");
		}
		Out += P + TEXT("}\n");
		return Out;
	}

	FString FmtNode(const FUetkxNode& Node, int32 Indent, FFmtState& State)
	{
		switch (Node.Type)
		{
		case EUetkxNodeType::El:
			return FmtElement(Node, Indent, State);
		case EUetkxNodeType::Frag:
		{
			const FString Inner = FmtChildren(Node.Children, Indent + 1, State);
			const FString P = Pad(Indent, State.O);
			if (!Node.Named.IsEmpty())
			{
				// T2.2: the named <Fragment> alias keeps the author's spelling + attrs.
				FString Head = FString::Printf(TEXT("<%s"), *Node.Named);
				for (const FUetkxAttr& Attr : Node.Attrs)
				{
					Head += TEXT(" ") + FmtAttr(Attr, State);
				}
				return FString::Printf(TEXT("%s%s>\n%s%s</%s>\n"), *P, *Head, *Inner, *P, *Node.Named);
			}
			return FString::Printf(TEXT("%s<>\n%s%s</>\n"), *P, *Inner, *P);
		}
		case EUetkxNodeType::Comment:
			// T2.1: comments verbatim, re-anchored to the current indent.
			return Pad(Indent, State.O) + Node.Value.TrimStartAndEnd() + TEXT("\n");
		case EUetkxNodeType::Text:
			return Pad(Indent, State.O) + Node.Value.TrimStartAndEnd() + TEXT("\n");
		case EUetkxNodeType::Expr:
			return Pad(Indent, State.O) + FString::Printf(TEXT("{ %s }"), *Node.Code.TrimStartAndEnd()) + TEXT("\n");
		case EUetkxNodeType::If:
			return FmtIf(Node, Indent, State);
		case EUetkxNodeType::For:
			return FmtLoop(Node, Indent, State, TEXT("for"));
		case EUetkxNodeType::While:
			return FmtLoop(Node, Indent, State, TEXT("while"));
		case EUetkxNodeType::Match:
			return FmtMatch(Node, Indent, State);
		}
		return FString();
	}

	FString FmtParams(const TArray<FUetkxParam>& Params)
	{
		if (Params.IsEmpty())
		{
			return FString();
		}
		TArray<FString> Parts;
		for (const FUetkxParam& Param : Params)
		{
			FString Part = Param.Name;
			if (!Param.Type.IsEmpty())
			{
				Part += TEXT(": ") + Param.Type;
			}
			if (!Param.Default.IsEmpty())
			{
				Part += TEXT(" = ") + Param.Default;
			}
			Parts.Add(MoveTemp(Part));
		}
		return FString::Printf(TEXT("(%s)"), *FString::Join(Parts, TEXT(", ")));
	}

	/** ES-modules (U-01): the NEW-form C++-native param spelling `Type Name = Default` — the
	 *  formatter is FORM-PRESERVING (never migrates), so new-form components keep their own
	 *  grammar. Param-less new components still print `()` (C++ function syntax). */
	FString FmtNewParams(const TArray<FUetkxParam>& Params)
	{
		TArray<FString> Parts;
		for (const FUetkxParam& Param : Params)
		{
			FString Part = Param.Type.IsEmpty() ? Param.Name : Param.Type + TEXT(" ") + Param.Name;
			if (!Param.Default.IsEmpty())
			{
				Part += TEXT(" = ") + Param.Default;
			}
			Parts.Add(MoveTemp(Part));
		}
		return FString::Printf(TEXT("(%s)"), *FString::Join(Parts, TEXT(", ")));
	}

	/** ES-modules (U-09): the inline `export ` prefix — ONLY when the decl itself carried it
	 *  (ExportAt >= 0); a decl exported via `export { … };` keeps its bare head (the list
	 *  statement is preserved separately — re-prefixing would duplicate the export, 2324). */
	const TCHAR* ExportPrefix(bool bExported, int32 ExportAt)
	{
		return bExported && ExportAt >= 0 ? TEXT("export ") : TEXT("");
	}

	FString FmtComponent(const FUetkxComponentDecl& Decl, FFmtState& State)
	{
		// Form-preserving (ES-modules G-10): a legacy `component` wrapper keeps its wrapper; a
		// new-form decl keeps the plain `FRuiNode Name(...)` head. Migration is the codemod's job.
		FString Out = Decl.bLegacySyntax
						  ? FString::Printf(TEXT("%scomponent %s%s {\n"), ExportPrefix(Decl.bExported, Decl.ExportAt),
											*Decl.Name, *FmtParams(Decl.Params))
						  : FString::Printf(TEXT("%sFRuiNode %s%s {\n"), ExportPrefix(Decl.bExported, Decl.ExportAt),
											*Decl.Name, *FmtNewParams(Decl.Params));
		const FString Setup = Reanchor(Decl.Setup, 1, State.O);
		if (!Setup.IsEmpty())
		{
			if (HasLeadingBlank(Decl.Setup))
			{
				Out += TEXT("\n"); // keep an authored blank line after `{`
			}
			Out += Setup;
			if (HasTrailingBlank(Decl.Setup))
			{
				Out += TEXT("\n"); // keep an authored blank line before `return (`
			}
		}
		Out += Pad(1, State.O) + TEXT("return (\n");
		// every window node in order — the render root plus any sibling comments (T2.1)
		Out += FmtChildren(Decl.WindowNodes, 2, State);
		Out += Pad(1, State.O) + TEXT(");\n");
		Out += TEXT("}\n");
		return Out;
	}

	// A verbatim-C++ body (hook / module) canonically re-anchored at depth 1 with the authored
	// leading/trailing blank lines kept — the FmtComponent setup treatment, shared so the two
	// verbatim-body declaration kinds format identically.
	FString FmtVerbatimBody(const FString& Body, FFmtState& State)
	{
		FString Out;
		const FString Reanchored = Reanchor(Body, 1, State.O);
		if (!Reanchored.IsEmpty())
		{
			if (HasLeadingBlank(Body))
			{
				Out += TEXT("\n");
			}
			Out += Reanchored;
			if (HasTrailingBlank(Body))
			{
				Out += TEXT("\n");
			}
		}
		return Out;
	}

	FString FmtHook(const FUetkxHookDecl& Decl, FFmtState& State)
	{
		// Form-preserving (ES-modules G-10): legacy `hook Name(...) [-> Ret]` keeps its wrapper;
		// new-form keeps the leading return type (`void` explicit — the scanner recorded it).
		// Params/Ret are verbatim C++ (template commas the family grammar can't split) — trimmed,
		// never re-parsed.
		FString Header;
		if (Decl.bLegacySyntax)
		{
			Header = FString::Printf(TEXT("%shook %s(%s)"), ExportPrefix(Decl.bExported, Decl.ExportAt), *Decl.Name,
									 *Decl.Params.TrimStartAndEnd());
			const FString Ret = Decl.Ret.TrimStartAndEnd();
			if (!Ret.IsEmpty())
			{
				Header += FString::Printf(TEXT(" -> %s"), *Ret);
			}
		}
		else
		{
			const FString Ret = Decl.Ret.TrimStartAndEnd();
			Header = FString::Printf(TEXT("%s%s %s(%s)"), ExportPrefix(Decl.bExported, Decl.ExportAt),
									 Ret.IsEmpty() ? TEXT("void") : *Ret, *Decl.Name, *Decl.Params.TrimStartAndEnd());
		}
		return Header + TEXT(" {\n") + FmtVerbatimBody(Decl.Body, State) + TEXT("}\n");
	}

	FString FmtModule(const FUetkxModuleDecl& Decl, FFmtState& State)
	{
		FString Header = FString::Printf(TEXT("%smodule %s"), ExportPrefix(Decl.bExported, Decl.ExportAt), *Decl.Name);
		return Header + TEXT(" {\n") + FmtVerbatimBody(Decl.Body, State) + TEXT("}\n");
	}

	/** ES-modules (U-01): `[export] <Type> Name = <Init>;` — the header canonicalizes; the
	 *  initializer is verbatim C++ (trimmed, internal lines untouched — idempotent). */
	FString FmtValue(const FUetkxValueDecl& Decl)
	{
		const FString Type = Decl.Type.TrimStartAndEnd();
		return FString::Printf(TEXT("%s%s%s = %s;\n"), ExportPrefix(Decl.bExported, Decl.ExportAt),
							   Type.IsEmpty() ? TEXT("") : *(Type + TEXT(" ")), *Decl.Name,
							   *Decl.Init.TrimStartAndEnd());
	}

	/** ES-modules (U-01): `[export] <Ret> Name(params) { body }` — the util function form. */
	FString FmtUtil(const FUetkxUtilDecl& Decl, FFmtState& State)
	{
		const FString Header =
			FString::Printf(TEXT("%s%s %s(%s)"), ExportPrefix(Decl.bExported, Decl.ExportAt),
							*Decl.RetType.TrimStartAndEnd(), *Decl.Name, *Decl.Params.TrimStartAndEnd());
		return Header + TEXT(" {\n") + FmtVerbatimBody(Decl.Body, State) + TEXT("}\n");
	}

	/** ES-modules (U-08/U-09): true when `Text` consists SOLELY of `export { ... };` /
	 *  `export default Name;` statements (+ whitespace) — the two decl-region statements that
	 *  live BETWEEN declarations and must be preserved in place (each line re-emitted trimmed),
	 *  never treated as unknown content (which would force a verbatim fallback). */
	bool TryFmtExportStatements(const FString& Text, FString& OutFormatted)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return false;
		}
		TArray<FString> Lines;
		Trimmed.ParseIntoArray(Lines, TEXT("\n"), true);
		FString Out;
		for (const FString& Line : Lines)
		{
			const FString T = Line.TrimStartAndEnd();
			if (T.IsEmpty())
			{
				continue;
			}
			const bool bList = T.StartsWith(TEXT("export {")) || T.StartsWith(TEXT("export{"));
			const bool bDefault = T.StartsWith(TEXT("export default "));
			if (!bList && !bDefault)
			{
				return false;
			}
			Out += T + TEXT("\n");
		}
		OutFormatted = Out;
		return true;
	}
} // namespace

FUetkxFormatResult FUetkxFormatter::Format(const FString& Source, const FUetkxFormatOptions& Options)
{
	FUetkxFormatResult Result;
	auto Verbatim = [&Source, &Result](bool bFellBack)
	{
		Result.Text = Source;
		Result.bChanged = false;
		Result.bFellBack = bFellBack;
		return Result;
	};
	const FUetkxFileScanResult Scan = FUetkxFileScan::Scan(Source, FString());
	if (Scan.Order.IsEmpty())
	{
		// no declaration at all: nothing to format (not a syntax error)
		return Verbatim(Scan.HasError() ? true : false);
	}
	if (Scan.HasError())
	{
		return Verbatim(true);
	}

	FFmtState State{Options};
	FString Out;
	// Scan offsets are CODE POINTS (D-18) — every raw slice of Source must go through the
	// code-point array, or a non-BMP char in a comment shifts all later slices.
	const TArray<int32> SrcCp = FUetkxLexer::ToCodePoints(Source);

	// The source-order start/end + canonical re-emit of any top-level declaration kind (M12
	// FmtHook/FmtModule): the mixed-decl file is a SEQUENCE (Scan.Order) of components + hooks +
	// modules, each canonicalized by its own formatter. TrueStart honors a preceding `export`.
	// NOTE (ES-modules M6): all three kind-switches carry EXPLICIT Value/Util cases — the old
	// `default:` fell through to Scan.Modules and would silently MIS-INDEX for any new kind.
	auto TrueStart = [&Scan](const TPair<EUetkxDeclKind, int32>& Ord) -> int32
	{
		auto Pick = [](bool bExported, int32 ExportAt, int32 At) { return bExported && ExportAt >= 0 ? ExportAt : At; };
		switch (Ord.Key)
		{
		case EUetkxDeclKind::Component:
			return Pick(Scan.Components[Ord.Value].bExported, Scan.Components[Ord.Value].ExportAt,
						Scan.Components[Ord.Value].At);
		case EUetkxDeclKind::Hook:
			return Pick(Scan.Hooks[Ord.Value].bExported, Scan.Hooks[Ord.Value].ExportAt, Scan.Hooks[Ord.Value].At);
		case EUetkxDeclKind::Module:
			return Pick(Scan.Modules[Ord.Value].bExported, Scan.Modules[Ord.Value].ExportAt,
						Scan.Modules[Ord.Value].At);
		case EUetkxDeclKind::Value:
			return Pick(Scan.Values[Ord.Value].bExported, Scan.Values[Ord.Value].ExportAt, Scan.Values[Ord.Value].At);
		case EUetkxDeclKind::Util:
			return Pick(Scan.Utils[Ord.Value].bExported, Scan.Utils[Ord.Value].ExportAt, Scan.Utils[Ord.Value].At);
		}
		return 0;
	};
	auto DeclNext = [&Scan](const TPair<EUetkxDeclKind, int32>& Ord) -> int32
	{
		switch (Ord.Key)
		{
		case EUetkxDeclKind::Component:
			return Scan.Components[Ord.Value].Next;
		case EUetkxDeclKind::Hook:
			return Scan.Hooks[Ord.Value].Next;
		case EUetkxDeclKind::Module:
			return Scan.Modules[Ord.Value].Next;
		case EUetkxDeclKind::Value:
			return Scan.Values[Ord.Value].Next;
		case EUetkxDeclKind::Util:
			return Scan.Utils[Ord.Value].Next;
		}
		return 0;
	};
	auto FmtDecl = [&Scan, &State](const TPair<EUetkxDeclKind, int32>& Ord) -> FString
	{
		switch (Ord.Key)
		{
		case EUetkxDeclKind::Component:
			return FmtComponent(Scan.Components[Ord.Value], State);
		case EUetkxDeclKind::Hook:
			return FmtHook(Scan.Hooks[Ord.Value], State);
		case EUetkxDeclKind::Module:
			return FmtModule(Scan.Modules[Ord.Value], State);
		case EUetkxDeclKind::Value:
			return FmtValue(Scan.Values[Ord.Value]);
		case EUetkxDeclKind::Util:
			return FmtUtil(Scan.Utils[Ord.Value], State);
		}
		return FString();
	};

	// Preamble (T1.3 + M11): canonicalized ONLY when it is nothing but whitespace + #include +
	// `import` lines. Leading comments or stray text are preserved byte-for-byte — Format Document
	// must never delete user content. Canonical order: #include block, blank, import block, blank.
	const FString Pre = FUetkxLexer::FromCodePoints(SrcCp, 0, TrueStart(Scan.Order[0]));
	bool bPreCanonical = true;
	{
		TArray<FString> PreLines;
		Pre.ParseIntoArray(PreLines, TEXT("\n"), false);
		for (const FString& Line : PreLines)
		{
			const FString T = Line.TrimStartAndEnd();
			if (!T.IsEmpty() && !T.StartsWith(TEXT("#include")) && !T.StartsWith(TEXT("import ")))
			{
				bPreCanonical = false;
				break;
			}
			// A trailing/standalone comment on a preamble line is NOT reconstructed structurally — emit
			// the preamble verbatim so it survives (bughunt FMT-1). Specifiers/include paths use single
			// slashes, so `//` or `/*` here only ever marks a comment.
			if (!T.IsEmpty() && (T.Contains(TEXT("//")) || T.Contains(TEXT("/*"))))
			{
				bPreCanonical = false;
				break;
			}
		}
	}
	if (!bPreCanonical)
	{
		Out += Pre;
	}
	else
	{
		FString Block;
		if (!Scan.PreambleIncludes.IsEmpty())
		{
			Block += FString::Join(Scan.PreambleIncludes, TEXT("\n")) + TEXT("\n");
		}
		if (!Scan.Imports.IsEmpty())
		{
			if (!Block.IsEmpty())
			{
				Block += TEXT("\n"); // blank between the #include block and the import block
			}
			for (const FUetkxImportDecl& Imp : Scan.Imports)
			{
				// `import "@Header.h"` (INCLUDE_RETIREMENT_PLAN.md §B) — a nameless host include,
				// spelled with its own shape rather than the named `{ ... } from` block.
				if (Imp.bHostInclude)
				{
					Block += FString::Printf(TEXT("import \"@%s\"\n"), *Imp.Specifier);
					continue;
				}
				// ES-modules (G-05): canonical import heads — named, `* as`, default, and the ES
				// COMBINED forms (`import Def, { A } from` / `import Def, * as X from`). One
				// declaration = one line; EVERY part prints (an exclusive-branch reprint would
				// silently DROP the named part of a combined line — Unity 0.9.1 field find).
				const bool bDef = Imp.bDefault;
				const bool bStar = Imp.bNamespace;
				TArray<FString> Heads;
				if (bDef)
				{
					Heads.Add(Imp.DefaultAlias);
				}
				if (bStar)
				{
					Heads.Add(FString::Printf(TEXT("* as %s"), *Imp.NamespaceAlias));
				}
				if (Imp.Names.Num() > 0 || (!bDef && !bStar))
				{
					TArray<FString> Pieces;
					for (int32 n = 0; n < Imp.Names.Num(); ++n)
					{
						const FString& Local = Imp.LocalNames.IsValidIndex(n) ? Imp.LocalNames[n] : Imp.Names[n];
						Pieces.Add(Local == Imp.Names[n]
									   ? Imp.Names[n]
									   : FString::Printf(TEXT("%s as %s"), *Imp.Names[n], *Local));
					}
					Heads.Add(FString::Printf(TEXT("{ %s }"), *FString::Join(Pieces, TEXT(", "))));
				}
				Block += FString::Printf(TEXT("import %s from \"%s\"\n"), *FString::Join(Heads, TEXT(", ")),
										 *Imp.Specifier);
			}
		}
		if (!Block.IsEmpty())
		{
			Out += Block + TEXT("\n"); // blank before the first declaration
		}
	}

	int32 Cursor = -1;
	for (int32 k = 0; k < Scan.Order.Num(); ++k)
	{
		const TPair<EUetkxDeclKind, int32>& Ord = Scan.Order[k];
		if (k > 0)
		{
			// content between declarations would be silently dropped by a re-emit — never
			// delete user text (T1.3): fall back verbatim. EXCEPTION (ES-modules U-08/U-09):
			// `export { … };` / `export default X;` are decl-region STATEMENTS — preserved in
			// place, each line trimmed-canonical.
			const FString Between = FUetkxLexer::FromCodePoints(SrcCp, Cursor, TrueStart(Ord) - Cursor);
			FString ExportStmts;
			if (!Between.TrimStartAndEnd().IsEmpty())
			{
				if (!TryFmtExportStatements(Between, ExportStmts))
				{
					return Verbatim(true);
				}
			}
			Out += TEXT("\n"); // exactly one blank line between declarations
			if (!ExportStmts.IsEmpty())
			{
				Out += ExportStmts + TEXT("\n");
			}
		}
		Out += FmtDecl(Ord);
		Cursor = DeclNext(Ord);
	}
	if (State.bUnsafeStrAttr)
	{
		return Verbatim(true); // G-05
	}

	// Trailing content after the last declaration: `export { … };` / `export default X;`
	// statements canonicalize in place (ES-modules); anything else round-trips verbatim after
	// exactly one canonical blank line (idempotent).
	if (Cursor >= 0 && Cursor < SrcCp.Num())
	{
		const FString Trailing = FUetkxLexer::FromCodePoints(SrcCp, Cursor, SrcCp.Num() - Cursor);
		if (!Trailing.TrimStartAndEnd().IsEmpty())
		{
			FString ExportStmts;
			if (TryFmtExportStatements(Trailing, ExportStmts))
			{
				Out = RStripWsNl(Out) + TEXT("\n") + ExportStmts;
			}
			else
			{
				Out = RStripWsNl(Out) + TEXT("\n\n") + LStripWsNl(Trailing);
			}
		}
	}
	Result.Text = RStripWsNl(Out) + TEXT("\n");
	Result.bChanged = Result.Text != Source;
	return Result;
}

FUetkxFormatOptions FUetkxFormatter::ResolveOptions(const FString& StartDir)
{
	// The walk-up + JSON parse lives in the shared FUetkxConfig::Load (M1) so the formatter and
	// the import resolver read one config through one code path (and can never disagree on which
	// config wins for a file). The formatter just projects the options fields.
	const FUetkxConfig Config = FUetkxConfig::Load(StartDir);
	FUetkxFormatOptions Options;
	Options.PrintWidth = Config.PrintWidth;
	Options.IndentStyle = Config.IndentStyle;
	Options.IndentSize = Config.IndentSize;
	Options.bSingleAttributePerLine = Config.bSingleAttributePerLine;
	Options.bInsertSpaceBeforeSelfClose = Config.bInsertSpaceBeforeSelfClose;
	return Options;
}
