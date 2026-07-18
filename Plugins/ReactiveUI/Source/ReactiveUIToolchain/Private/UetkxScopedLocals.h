// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxScopedLocals — the C++ half of the family's pragmatic local-declaration tracker
// (LSP_COMPLETION_PLAN N-07, TD-034 #1/#2; the TS half lives in uetkxIndex.ts and the two MUST
// stay behaviorally identical). One pre-walk over a verbatim-C++ region records brace scopes and
// the locals declared in each — `Type Name =/;/{`, `auto Name`, structured bindings
// `auto [A, B]`, and a caller-supplied parameter seed — so the rewrite planes and the
// reference collectors can ask "is `Name` a LOCAL at this offset?" and skip it. A HEURISTIC by
// design (no clang): every recognized pattern is test-pinned; an unrecognized shadow is the
// documented residual (TECH_DEBT TD-034). Over-detection suppresses a rewrite/ref and fails
// LOUDLY downstream (C++ compile); under-detection is the silent mode — patterns favor
// precision. Module-private header (Toolchain-internal; consumed by UetkxCodegen.cpp and
// UetkxResolve.cpp).

#pragma once

#include "CoreMinimal.h"
#include "UetkxLexer.h"

class FUetkxScopedLocals
{
public:
	/** Pre-walk `RegionCp` (code points). `ParamSeed` = names visible for the whole region
	 *  (function/component parameters). */
	FUetkxScopedLocals(const TArray<int32>& RegionCp, const TArray<FString>& ParamSeed)
	{
		// scope 0 spans the whole region
		Scopes.Add({0, RegionCp.Num(), -1});
		for (const FString& Param : ParamSeed)
		{
			Decls.Add({Param, 0, 0});
		}
		Walk(RegionCp);
	}

	/** True when `Name` has a local declaration in scope at `Offset` (declared at or before it,
	 *  in this or an enclosing scope). */
	bool IsLocal(const FString& Name, int32 Offset) const
	{
		for (const FDeclRec& D : Decls)
		{
			if (D.Name != Name || D.At > Offset)
			{
				continue;
			}
			const FScope& S = Scopes[D.ScopeIdx];
			if (Offset >= S.Start && Offset < S.End)
			{
				return true;
			}
		}
		return false;
	}

	/** Every declared local name, scope/offset-agnostic (union incl. the param seed) — the
	 *  CONSERVATIVE visibility seed for markup-embedded expression islands (N5): an island runs
	 *  inside the impl body where the setup's locals are live, but the island's own tracker can't
	 *  see across regions, so the union suppresses instead (over-suppression = a silently missed
	 *  diagnostic, never a false one — the documented TD-034 residual direction). */
	TArray<FString> AllDeclNames() const
	{
		TSet<FString> Names;
		for (const FDeclRec& D : Decls)
		{
			Names.Add(D.Name);
		}
		return Names.Array();
	}

	/** The parameter NAMES of a verbatim C++ parameter list (`int32 Start, const FString& S = X`):
	 *  the last identifier of each top-level comma piece before that piece's `=` — the scope seed
	 *  for hook/util bodies (mirrors the TS paramNamesOf). */
	static TArray<FString> ParamNamesOf(const FString& ParamText)
	{
		TArray<FString> Out;
		if (ParamText.TrimStartAndEnd().IsEmpty())
		{
			return Out;
		}
		TArray<FString> Pieces;
		int32 Depth = 0;
		FString Cur;
		for (int32 i = 0; i < ParamText.Len(); ++i)
		{
			const TCHAR C = ParamText[i];
			if (C == '(' || C == '[' || C == '{' || C == '<')
			{
				++Depth;
			}
			else if (C == ')' || C == ']' || C == '}' || C == '>')
			{
				--Depth;
			}
			if (C == ',' && Depth == 0)
			{
				Pieces.Add(Cur);
				Cur.Empty();
				continue;
			}
			Cur.AppendChar(C);
		}
		if (!Cur.TrimStartAndEnd().IsEmpty())
		{
			Pieces.Add(Cur);
		}
		for (const FString& Piece : Pieces)
		{
			int32 D2 = 0;
			int32 Head = Piece.Len();
			for (int32 i = 0; i < Piece.Len(); ++i)
			{
				const TCHAR C = Piece[i];
				if (C == '(' || C == '[' || C == '{' || C == '<')
				{
					++D2;
				}
				else if (C == ')' || C == ']' || C == '}' || C == '>')
				{
					--D2;
				}
				else if (C == '=' && D2 == 0 && (i + 1 >= Piece.Len() || Piece[i + 1] != '='))
				{
					Head = i;
					break;
				}
			}
			const FString HeadText = Piece.Left(Head).TrimStartAndEnd();
			int32 e = HeadText.Len();
			while (e > 0 && FUetkxLexer::IsIdentCode(HeadText[e - 1]))
			{
				--e;
			}
			const FString Name = HeadText.Mid(e).TrimStartAndEnd();
			if (!Name.IsEmpty())
			{
				Out.Add(Name);
			}
		}
		return Out;
	}

private:
	struct FScope
	{
		int32 Start = 0;
		int32 End = 0;
		int32 Parent = -1;
	};
	struct FDeclRec
	{
		FString Name;
		int32 ScopeIdx = 0;
		int32 At = 0;
	};
	TArray<FScope> Scopes;
	TArray<FDeclRec> Decls;

	static bool IsWsCp(int32 C) { return C == ' ' || C == '\t' || C == '\n' || C == '\r'; }
	static bool IsIdentStartCp(int32 C) { return FUetkxLexer::IsIdentCode(C) && !(C >= '0' && C <= '9'); }

	void Walk(const TArray<int32>& Cp)
	{
		const int32 N = Cp.Num();
		int32 Current = 0; // scope index
		bool bPrevTypeish = false;
		int32 i = 0;
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Cp, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			const int32 C = Cp[i];
			if (C == '{')
			{
				Scopes.Add({i, N, Current});
				Current = Scopes.Num() - 1;
				bPrevTypeish = false;
				++i;
				continue;
			}
			if (C == '}')
			{
				if (Current != 0)
				{
					Scopes[Current].End = i;
					Current = Scopes[Current].Parent;
				}
				bPrevTypeish = false;
				++i;
				continue;
			}
			if (IsIdentStartCp(C) && (i == 0 || !FUetkxLexer::IsIdentCode(Cp[i - 1])))
			{
				const int32 s = i;
				while (i < N && FUetkxLexer::IsIdentCode(Cp[i]))
				{
					++i;
				}
				const FString Ident = FUetkxLexer::FromCodePoints(Cp, s, i - s);

				// `auto [A, B] = …` — structured bindings declare every bound name.
				if (Ident == TEXT("auto"))
				{
					int32 p = i;
					while (p < N && IsWsCp(Cp[p]))
					{
						++p;
					}
					while (p < N && (Cp[p] == '&' || Cp[p] == '*'))
					{
						++p;
					}
					while (p < N && IsWsCp(Cp[p]))
					{
						++p;
					}
					if (p < N && Cp[p] == '[')
					{
						const int32 Close = FUetkxLexer::FindMatching(Cp, p);
						if (Close != -1)
						{
							int32 q = p + 1;
							while (q < Close)
							{
								while (q < Close && !IsIdentStartCp(Cp[q]))
								{
									++q;
								}
								const int32 bs = q;
								while (q < Close && FUetkxLexer::IsIdentCode(Cp[q]))
								{
									++q;
								}
								if (q > bs)
								{
									Decls.Add({FUetkxLexer::FromCodePoints(Cp, bs, q - bs), Current, bs});
								}
							}
							i = Close + 1;
							bPrevTypeish = false;
							continue;
						}
					}
					bPrevTypeish = true;
					continue;
				}

				// member/scope prefixes never start a declaration name
				bool bMember = false, bScope = false;
				for (int32 k = s - 1; k >= 0; --k)
				{
					const int32 P = Cp[k];
					if (P == ' ' || P == '\t')
					{
						continue;
					}
					bMember = (P == '.') || (P == '>' && k > 0 && Cp[k - 1] == '-');
					bScope = (P == ':') && k > 0 && Cp[k - 1] == ':';
					break;
				}
				int32 p2 = i;
				while (p2 < N && (Cp[p2] == ' ' || Cp[p2] == '\t'))
				{
					++p2;
				}
				const bool bEq =
					p2 < N && Cp[p2] == '=' && (p2 + 1 >= N || Cp[p2 + 1] != '=') &&
					(s == 0 || (Cp[s - 1] != '=' && Cp[s - 1] != '!' && Cp[s - 1] != '<' && Cp[s - 1] != '>'));
				const bool bSemi = p2 < N && Cp[p2] == ';';
				const bool bBrace = p2 < N && Cp[p2] == '{';
				if (!bMember && !bScope && bPrevTypeish && (bEq || bSemi || bBrace))
				{
					Decls.Add({Ident, Current, s});
					bPrevTypeish = false;
					continue;
				}
				// keywords that can't head a type keep typeish OFF; plain idents turn it on
				bPrevTypeish = Ident != TEXT("return") && Ident != TEXT("if") && Ident != TEXT("else") &&
							   Ident != TEXT("for") && Ident != TEXT("while") && Ident != TEXT("switch") &&
							   Ident != TEXT("case") && Ident != TEXT("break") && Ident != TEXT("continue") &&
							   Ident != TEXT("new") && Ident != TEXT("delete") && Ident != TEXT("sizeof");
				continue;
			}
			// whitespace + type decorations keep the type-ish lookbehind alive (the TS bughunt:
			// a space between the type and the name must not reset it); commas keep it for
			// `Type A = 1, B = 2`; everything else resets. A colon keeps it ONLY as part of a
			// `::` qual (`std::atomic X`) — a LONE ternary/label colon resets (the IMPORT-3 rule;
			// `U = a ? B : B;` otherwise reads the second arm as a `Type Name;` declaration).
			if (IsWsCp(C) || C == '<' || C == '>' || C == '&' || C == '*' || C == ',' ||
				(C == ':' && ((i + 1 < N && Cp[i + 1] == ':') || (i > 0 && Cp[i - 1] == ':'))))
			{
				++i;
				continue;
			}
			bPrevTypeish = false;
			++i;
		}
	}
};
