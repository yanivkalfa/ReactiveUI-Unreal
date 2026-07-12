// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiRouter.h"

#include "RuiContext.h"
#include "RuiContextHandle.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiRouter, Log, All);

// ─────────────────────────────────────────────────────────────────────────────────────────
// Pure helpers — path matching / parsing (the router_match suite hits these directly)
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	/** Split a pathname into non-empty segments (leading/trailing/duplicate slashes ignored). */
	TArray<FString> Segments(const FString& Path)
	{
		TArray<FString> Out;
		Path.ParseIntoArray(Out, TEXT("/"), /*CullEmpty*/ true);
		return Out;
	}

	FString NormalizePathname(const FString& In)
	{
		const TArray<FString> Segs = Segments(In);
		if (Segs.Num() == 0)
		{
			return TEXT("/");
		}
		return TEXT("/") + FString::Join(Segs, TEXT("/"));
	}

	/** Index of the first `?` or `#` in a to-string (whichever is earlier), or INDEX_NONE. */
	int32 TailStart(const FString& To)
	{
		int32 Q, H;
		const bool bQ = To.FindChar(TEXT('?'), Q);
		const bool bH = To.FindChar(TEXT('#'), H);
		if (bQ && bH)
		{
			return FMath::Min(Q, H);
		}
		return bQ ? Q : (bH ? H : INDEX_NONE);
	}

	/** The ?search#hash tail of a to-string (so it can be re-appended once after path resolution). */
	FString ExtractQueryHash(const FString& To)
	{
		const int32 Cut = TailStart(To);
		return Cut == INDEX_NONE ? FString() : To.RightChop(Cut);
	}

	/** The PATHNAME part of a to-string, with any ?query#hash tail removed — what ResolvePath must see.
	 *  ResolvePath segments only on `/`, so passing the raw `To` embeds the tail into the resolved path
	 *  AND lets the caller append it again -> doubled search/hash (bughunt B2). Strip first. */
	FString StripQueryHash(const FString& To)
	{
		const int32 Cut = TailStart(To);
		return Cut == INDEX_NONE ? To : To.Left(Cut);
	}
} // namespace

FRuiPathMatch RUI::MatchPath(const FString& Pattern, const FString& Pathname, bool bEnd)
{
	FRuiPathMatch Match;
	const TArray<FString> PatSegs = Segments(Pattern);
	const TArray<FString> PathSegs = Segments(Pathname);

	TArray<FString> Consumed;
	int32 i = 0;
	for (int32 p = 0; p < PatSegs.Num(); ++p)
	{
		const FString& Seg = PatSegs[p];
		if (Seg == TEXT("*"))
		{
			// Splat: capture the rest of the path as the "*" param.
			TArray<FString> Rest;
			for (int32 k = i; k < PathSegs.Num(); ++k)
			{
				Rest.Add(PathSegs[k]);
				Consumed.Add(PathSegs[k]);
			}
			Match.Params.Add(TEXT("*"), FString::Join(Rest, TEXT("/")));
			i = PathSegs.Num();
			Match.bMatched = true;
			Match.Pathname = NormalizePathname(FString::Join(Consumed, TEXT("/")));
			return Match; // a splat is always terminal
		}
		if (i >= PathSegs.Num())
		{
			return Match; // pattern longer than path -> no match
		}
		if (Seg.StartsWith(TEXT(":")))
		{
			Match.Params.Add(Seg.RightChop(1), PathSegs[i]);
			Consumed.Add(PathSegs[i]);
			++i;
		}
		else if (Seg == PathSegs[i])
		{
			Consumed.Add(PathSegs[i]);
			++i;
		}
		else
		{
			return Match; // literal mismatch
		}
	}
	if (bEnd && i != PathSegs.Num())
	{
		return Match; // leftover path but a leaf pattern -> no full match
	}
	Match.bMatched = true;
	Match.Pathname = NormalizePathname(FString::Join(Consumed, TEXT("/")));
	return Match;
}

TMap<FString, FString> RUI::ParseSearch(const FString& Search)
{
	TMap<FString, FString> Out;
	FString S = Search;
	if (S.StartsWith(TEXT("?")))
	{
		S = S.RightChop(1);
	}
	TArray<FString> Pairs;
	S.ParseIntoArray(Pairs, TEXT("&"), true);
	for (const FString& Pair : Pairs)
	{
		// First value wins for a repeated key (documented contract, RuiRouter.h) — insert only when
		// absent (bughunt B3: FindOrAdd + assign was last-value-wins, contradicting the header).
		FString Key, Value;
		if (Pair.Split(TEXT("="), &Key, &Value))
		{
			if (!Out.Contains(Key))
			{
				Out.Add(Key, Value);
			}
		}
		else if (!Pair.IsEmpty() && !Out.Contains(Pair))
		{
			Out.Add(Pair, FString());
		}
	}
	return Out;
}

FString RUI::BuildSearch(const TMap<FString, FString>& Params)
{
	if (Params.Num() == 0)
	{
		return FString();
	}
	TArray<FString> Keys;
	Params.GetKeys(Keys);
	Keys.Sort(); // stable order
	TArray<FString> Pairs;
	for (const FString& Key : Keys)
	{
		Pairs.Add(Key + TEXT("=") + Params[Key]);
	}
	return TEXT("?") + FString::Join(Pairs, TEXT("&"));
}

FRuiLocation RUI::ParseLocation(const FString& Href)
{
	FRuiLocation Loc;
	FString Rest = Href;

	int32 HashIdx;
	if (Rest.FindChar(TEXT('#'), HashIdx))
	{
		Loc.Hash = Rest.RightChop(HashIdx);
		Rest = Rest.Left(HashIdx);
	}
	int32 QIdx;
	if (Rest.FindChar(TEXT('?'), QIdx))
	{
		Loc.Search = Rest.RightChop(QIdx);
		Rest = Rest.Left(QIdx);
	}
	Loc.Pathname = NormalizePathname(Rest);
	return Loc;
}

FString RUI::ResolvePath(const FString& To, const FString& From)
{
	if (To.StartsWith(TEXT("/")))
	{
		return NormalizePathname(To); // absolute
	}
	// Relative: join onto From's directory (From is a full pathname; drop its last segment).
	TArray<FString> Base = Segments(From);
	TArray<FString> Rel = Segments(To);
	for (const FString& Seg : Rel)
	{
		if (Seg == TEXT(".."))
		{
			if (Base.Num() > 0)
			{
				Base.Pop();
			}
		}
		else if (Seg != TEXT("."))
		{
			Base.Add(Seg);
		}
	}
	return NormalizePathname(FString::Join(Base, TEXT("/")));
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Context values + handles
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	struct FRuiBlockerEntry
	{
		TSharedRef<bool> bActive;
		TFunction<void(const FString&)> OnBlocked;
		FRuiBlockerEntry() : bActive(MakeShared<bool>(false)) {}
	};
	using FRuiBlockerList = TArray<TSharedPtr<FRuiBlockerEntry>>;

	/** Navigation + location context (provided by <Router>). Functions are EXCLUDED from equality
	 *  so consumers re-render only when the location/nav data changes, not every Router render. */
	struct FRuiRouterCtxValue
	{
		bool bInRouter = false;
		FRuiLocation Location;
		ERuiNavigationType NavType = ERuiNavigationType::Push;
		bool bCanGoBack = false;
		TFunction<void(const FString&, bool)> Navigate;
		TFunction<void(int32)> Go;
		TSharedPtr<FRuiBlockerList> Blockers;

		bool operator==(const FRuiRouterCtxValue& O) const
		{
			return bInRouter == O.bInRouter && Location == O.Location && NavType == O.NavType &&
				   bCanGoBack == O.bCanGoBack;
		}
		bool operator!=(const FRuiRouterCtxValue& O) const { return !(*this == O); }
	};

	/** Per-matched-route context (params + the nested Outlet element). */
	struct FRuiRouteCtxValue
	{
		TMap<FString, FString> Params;
		TSharedPtr<FRuiNode> Outlet; // the child element to render at UseOutlet()

		bool operator==(const FRuiRouteCtxValue& O) const
		{
			return Params.OrderIndependentCompareEqual(O.Params) && Outlet == O.Outlet;
		}
		bool operator!=(const FRuiRouteCtxValue& O) const { return !(*this == O); }
	};

	const TRuiContext<FRuiRouterCtxValue>& RouterContext()
	{
		static const TRuiContext<FRuiRouterCtxValue> Ctx(FRuiRouterCtxValue{}, FName(TEXT("RuiRouter")));
		return Ctx;
	}
	const TRuiContext<FRuiRouteCtxValue>& RouteContext()
	{
		static const TRuiContext<FRuiRouteCtxValue> Ctx(FRuiRouteCtxValue{}, FName(TEXT("RuiRoute")));
		return Ctx;
	}

	static const TMap<FString, FString> GEmptyParams;

	/** The in-memory history the Router mutates through a stable ref. */
	struct FRuiHistory
	{
		TArray<FRuiLocation> Back;
		FRuiLocation Current;
		TArray<FRuiLocation> Forward;
		ERuiNavigationType NavType = ERuiNavigationType::Push;
		int32 Serial = 0;
	};
} // namespace

// ─────────────────────────────────────────────────────────────────────────────────────────
// <Router> — the in-memory history provider
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	struct FRuiRouterProps final : public FRuiPropsBase
	{
		FString InitialPath = TEXT("/");
		virtual bool Equals(const FRuiPropsBase& Other) const override
		{
			return BaseFieldsEqual(Other) && InitialPath == static_cast<const FRuiRouterProps&>(Other).InitialPath;
		}
	};

	bool AnyBlockerActive(const TSharedPtr<FRuiBlockerList>& Blockers, const FString& Href)
	{
		if (!Blockers.IsValid())
		{
			return false;
		}
		for (const TSharedPtr<FRuiBlockerEntry>& Entry : *Blockers)
		{
			if (Entry.IsValid() && *Entry->bActive)
			{
				if (Entry->OnBlocked)
				{
					Entry->OnBlocked(Href);
				}
				return true;
			}
		}
		return false;
	}

	FRuiNodeArray RouterComp(FRuiContext& Ctx, const FRuiRouterProps& Props, const TArray<FRuiNode>& Children)
	{
		TSharedRef<TRuiRef<FRuiHistory>> HistRef = Ctx.UseRef<FRuiHistory>();
		TSharedRef<TRuiRef<bool>> Primed = Ctx.UseRef<bool>(false);
		if (!Primed->Current)
		{
			Primed->Current = true;
			HistRef->Current.Current = RUI::ParseLocation(Props.InitialPath);
		}
		TSharedRef<TRuiRef<FRuiBlockerList>> Blockers = Ctx.UseRef<FRuiBlockerList>();

		TTuple<int32, TRuiSetter<int32>> Version = Ctx.UseState<int32>(0);
		const TRuiSetter<int32>& Bump = Version.Value;

		TSharedPtr<FRuiBlockerList> BlockerList(Blockers, &Blockers->Current); // alias ptr sharing HistRef lifetime

		auto Navigate = [HistRef, BlockerList, Bump](const FString& To, bool bReplace)
		{
			FRuiHistory& H = HistRef->Current;
			// Resolve only the pathname part, then re-append the ?query#hash tail ONCE (bughunt B2).
			const FString Resolved = RUI::ResolvePath(StripQueryHash(To), H.Current.Pathname);
			const FRuiLocation Next =
				RUI::ParseLocation(To.StartsWith(TEXT("/")) ? To : Resolved + ExtractQueryHash(To));
			if (AnyBlockerActive(BlockerList, Next.ToHref()))
			{
				return;
			}
			if (bReplace)
			{
				H.Current = Next;
				H.NavType = ERuiNavigationType::Replace;
			}
			else
			{
				H.Back.Add(H.Current);
				H.Current = Next;
				H.NavType = ERuiNavigationType::Push;
			}
			H.Forward.Reset();
			Bump([](const int32& V) { return V + 1; });
		};

		auto Go = [HistRef, Bump](int32 Delta)
		{
			FRuiHistory& H = HistRef->Current;
			if (Delta < 0)
			{
				for (int32 n = 0; n < -Delta && H.Back.Num() > 0; ++n)
				{
					H.Forward.Insert(H.Current, 0);
					H.Current = H.Back.Pop();
				}
			}
			else if (Delta > 0)
			{
				for (int32 n = 0; n < Delta && H.Forward.Num() > 0; ++n)
				{
					H.Back.Add(H.Current);
					H.Current = H.Forward[0];
					H.Forward.RemoveAt(0);
				}
			}
			H.NavType = ERuiNavigationType::Pop;
			Bump([](const int32& V) { return V + 1; });
		};

		FRuiRouterCtxValue Value;
		Value.bInRouter = true;
		Value.Location = HistRef->Current.Current;
		Value.NavType = HistRef->Current.NavType;
		Value.bCanGoBack = HistRef->Current.Back.Num() > 0;
		Value.Navigate = MoveTemp(Navigate);
		Value.Go = MoveTemp(Go);
		Value.Blockers = BlockerList;
		Ctx.ProvideContext(RouterContext(), MoveTemp(Value));

		return Children;
	}
	RUI_COMPONENT(RouterComp)
} // namespace

FRuiNode RUI::Router(TArray<FRuiNode> Children, FString InitialPath, FRuiKey Key)
{
	FRuiRouterProps Props;
	Props.InitialPath = MoveTemp(InitialPath);
	return RUI::FC<FRuiRouterProps>(&RouterComp, MoveTemp(Props), MoveTemp(Children), Key);
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Route matching tree + <Routes> / UseRoutes / UseOutlet
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	using RUI::FRuiRoute;

	int32 SpecificityScore(const FRuiRoute& Route)
	{
		// static segments rank highest, params next, splats last, longer patterns first.
		int32 Score = 0;
		for (const FString& Seg : Segments(Route.Path))
		{
			if (Seg == TEXT("*"))
			{
				Score += 1;
			}
			else if (Seg.StartsWith(TEXT(":")))
			{
				Score += 3;
			}
			else
			{
				Score += 5;
			}
		}
		if (Route.bIndex)
		{
			Score += 2;
		}
		return Score;
	}

	/** Remove the matched prefix from RemainingPath, returning the leftover (relative) pathname. */
	FString StripPrefix(const FString& RemainingPath, const FString& MatchedPathname)
	{
		const TArray<FString> Rem = Segments(RemainingPath);
		const int32 Consumed = Segments(MatchedPathname).Num();
		TArray<FString> Left;
		for (int32 i = Consumed; i < Rem.Num(); ++i)
		{
			Left.Add(Rem[i]);
		}
		return NormalizePathname(FString::Join(Left, TEXT("/")));
	}

	/** Depth-first best match. Fills Chain (parent..leaf) + merged Params. */
	bool MatchInto(const TArray<FRuiRoute>& Routes, const FString& RemainingPath, TMap<FString, FString>& Params,
				   TArray<const FRuiRoute*>& Chain)
	{
		TArray<const FRuiRoute*> Ranked;
		for (const FRuiRoute& R : Routes)
		{
			Ranked.Add(&R);
		}
		Ranked.Sort([](const FRuiRoute& A, const FRuiRoute& B) { return SpecificityScore(A) > SpecificityScore(B); });

		const bool bAtEnd = Segments(RemainingPath).Num() == 0;
		for (const FRuiRoute* Route : Ranked)
		{
			if (Route->bIndex)
			{
				if (bAtEnd)
				{
					Chain.Add(Route);
					return true;
				}
				continue;
			}
			const bool bLayout = Route->Children.Num() > 0;
			const FRuiPathMatch M = RUI::MatchPath(Route->Path, RemainingPath, /*bEnd*/ !bLayout);
			if (!M.bMatched)
			{
				continue;
			}
			Params.Append(M.Params);
			Chain.Add(Route);
			if (bLayout)
			{
				const FString Rest = StripPrefix(RemainingPath, M.Pathname);
				TArray<const FRuiRoute*> ChildChain;
				MatchInto(Route->Children, Rest, Params, ChildChain); // layout matches even with no child
				Chain.Append(ChildChain);
			}
			return true;
		}
		return false;
	}

	/** Wraps a matched route's Element: provides {params, outlet} then renders the element. */
	struct FRuiRouteHostProps final : public FRuiPropsBase
	{
		TSharedPtr<FRuiRouteCtxValue> Ctx;
		TSharedPtr<FRuiNode> Element;
		virtual bool Equals(const FRuiPropsBase& Other) const override
		{
			const FRuiRouteHostProps& O = static_cast<const FRuiRouteHostProps&>(Other);
			return BaseFieldsEqual(Other) && Element == O.Element &&
				   ((Ctx == O.Ctx) || (Ctx.IsValid() && O.Ctx.IsValid() && *Ctx == *O.Ctx));
		}
	};

	FRuiNodeArray RouteHostComp(FRuiContext& Ctx, const FRuiRouteHostProps& Props, const TArray<FRuiNode>&)
	{
		if (Props.Ctx.IsValid())
		{
			Ctx.ProvideContext(RouteContext(), *Props.Ctx);
		}
		return {Props.Element.IsValid() ? *Props.Element : RUI::Fragment({})};
	}
	RUI_COMPONENT(RouteHostComp)

	FRuiNode WrapRoute(const TSharedPtr<FRuiRouteCtxValue>& RouteCtx, const FRuiNode& Element)
	{
		FRuiRouteHostProps Props;
		Props.Ctx = RouteCtx;
		Props.Element = MakeShared<FRuiNode>(Element);
		return RUI::FC<FRuiRouteHostProps>(&RouteHostComp, MoveTemp(Props), {}, FRuiKey());
	}
} // namespace

FRuiNode UseRoutes(FRuiContext& Ctx, const TArray<RUI::FRuiRoute>& RouteList)
{
	const FRuiLocation& Loc = UseLocation(Ctx);

	TMap<FString, FString> Params;
	TArray<const RUI::FRuiRoute*> Chain;
	if (!MatchInto(RouteList, Loc.Pathname, Params, Chain) || Chain.Num() == 0)
	{
		return RUI::Fragment({});
	}

	// Build leaf -> root: each level provides the SAME merged params + the child as its Outlet.
	FRuiNode Element = RUI::Fragment({}); // the leaf's outlet is empty
	for (int32 i = Chain.Num() - 1; i >= 0; --i)
	{
		TSharedPtr<FRuiRouteCtxValue> RouteCtx = MakeShared<FRuiRouteCtxValue>();
		RouteCtx->Params = Params;
		RouteCtx->Outlet = MakeShared<FRuiNode>(Element);
		Element = WrapRoute(RouteCtx, Chain[i]->Element);
	}
	return Element;
}

FRuiNode RUI::Routes(TArray<FRuiRoute> RouteList, FRuiKey Key)
{
	// A tiny component so UseRoutes runs inside the render tree (hooks-legal) under the Router.
	struct FRoutesProps final : public FRuiPropsBase
	{
		TSharedPtr<TArray<FRuiRoute>> List;
		virtual bool Equals(const FRuiPropsBase& Other) const override
		{
			return BaseFieldsEqual(Other) && List == static_cast<const FRoutesProps&>(Other).List;
		}
	};
	struct FLocal
	{
		static FRuiNodeArray Comp(FRuiContext& Ctx, const FRoutesProps& P, const TArray<FRuiNode>&)
		{
			return {P.List.IsValid() ? UseRoutes(Ctx, *P.List) : RUI::Fragment({})};
		}
	};
	static const FName Id = RUI::RegisterComponentId((void*)&FLocal::Comp, FName(TEXT("RuiRoutesComp")));
	(void)Id;
	FRoutesProps Props;
	Props.List = MakeShared<TArray<FRuiRoute>>(MoveTemp(RouteList));
	return RUI::FC<FRoutesProps>(&FLocal::Comp, MoveTemp(Props), {}, Key);
}

FRuiNode UseOutlet(FRuiContext& Ctx)
{
	const FRuiRouteCtxValue& Route = Ctx.UseContext(RouteContext());
	return Route.Outlet.IsValid() ? *Route.Outlet : RUI::Fragment({});
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// <Link>
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	struct FRuiLinkProps final : public FRuiPropsBase
	{
		FString To;
		bool bReplace = false;
		virtual bool Equals(const FRuiPropsBase& Other) const override
		{
			const FRuiLinkProps& O = static_cast<const FRuiLinkProps&>(Other);
			return BaseFieldsEqual(Other) && To == O.To && bReplace == O.bReplace;
		}
	};

	FRuiNodeArray LinkComp(FRuiContext& Ctx, const FRuiLinkProps& Props, const TArray<FRuiNode>& Children)
	{
		// The core Link just forwards its children; a host wrapper (Slate Button/Text) binds the
		// click to the navigate captured here. We expose navigate through context so the wrapper
		// can call it — but for the pure-core node we simply pass children through (the Slate
		// layer's RUI::Slate::Link binds OnClicked). Kept minimal + host-agnostic.
		(void)Ctx;
		(void)Props;
		return Children;
	}
	RUI_COMPONENT(LinkComp)
} // namespace

FRuiNode RUI::Link(FString To, TArray<FRuiNode> Children, bool bReplace, FRuiKey Key)
{
	FRuiLinkProps Props;
	Props.To = MoveTemp(To);
	Props.bReplace = bReplace;
	return RUI::FC<FRuiLinkProps>(&LinkComp, MoveTemp(Props), MoveTemp(Children), Key);
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// The 17 hooks
// ─────────────────────────────────────────────────────────────────────────────────────────

bool UseInRouterContext(FRuiContext& Ctx)
{
	return Ctx.UseContext(RouterContext()).bInRouter;
}

const FRuiLocation& UseLocation(FRuiContext& Ctx)
{
	return Ctx.UseContext(RouterContext()).Location;
}

FString UsePathname(FRuiContext& Ctx)
{
	return UseLocation(Ctx).Pathname;
}

FString UseSearch(FRuiContext& Ctx)
{
	return UseLocation(Ctx).Search;
}

ERuiNavigationType UseNavigationType(FRuiContext& Ctx)
{
	return Ctx.UseContext(RouterContext()).NavType;
}

TFunction<void(const FString&, bool)> UseNavigate(FRuiContext& Ctx)
{
	const FRuiRouterCtxValue& R = Ctx.UseContext(RouterContext());
	if (R.Navigate)
	{
		return R.Navigate;
	}
	return [](const FString&, bool) {};
}

TFunction<void(int32)> UseGo(FRuiContext& Ctx)
{
	const FRuiRouterCtxValue& R = Ctx.UseContext(RouterContext());
	if (R.Go)
	{
		return R.Go;
	}
	return [](int32) {};
}

TTuple<bool, TFunction<void()>> UseBackStack(FRuiContext& Ctx)
{
	const FRuiRouterCtxValue& R = Ctx.UseContext(RouterContext());
	TFunction<void(int32)> Go = R.Go;
	return MakeTuple(R.bCanGoBack, TFunction<void()>(
									   [Go]()
									   {
										   if (Go)
										   {
											   Go(-1);
										   }
									   }));
}

const TMap<FString, FString>& UseParams(FRuiContext& Ctx)
{
	return Ctx.UseContext(RouteContext()).Params;
}

TTuple<TMap<FString, FString>, TFunction<void(TMap<FString, FString>)>> UseSearchParams(FRuiContext& Ctx)
{
	const FRuiRouterCtxValue& R = Ctx.UseContext(RouterContext());
	TMap<FString, FString> Current = RUI::ParseSearch(R.Location.Search);
	const FString Pathname = R.Location.Pathname;
	TFunction<void(const FString&, bool)> Navigate = R.Navigate;
	TFunction<void(TMap<FString, FString>)> Set = [Navigate, Pathname](TMap<FString, FString> Next)
	{
		if (Navigate)
		{
			Navigate(Pathname + RUI::BuildSearch(Next), /*bReplace*/ true);
		}
	};
	return MakeTuple(MoveTemp(Current), MoveTemp(Set));
}

FRuiPathMatch UseMatch(FRuiContext& Ctx, const FString& Pattern)
{
	return RUI::MatchPath(Pattern, UseLocation(Ctx).Pathname, /*bEnd*/ true);
}

bool UseIsActive(FRuiContext& Ctx, const FString& Pattern, bool bEnd)
{
	return RUI::MatchPath(Pattern, UseLocation(Ctx).Pathname, bEnd).bMatched;
}

FString UseResolvedPath(FRuiContext& Ctx, const FString& To)
{
	return RUI::ResolvePath(To, UseLocation(Ctx).Pathname);
}

FString UseHref(FRuiContext& Ctx, const FString& To)
{
	if (To.StartsWith(TEXT("/")))
	{
		return RUI::ParseLocation(To).ToHref();
	}
	// Resolve only the pathname, then append the tail once (bughunt B2 — was doubling via
	// UseResolvedPath(To) embedding the tail AND `+ Tail` re-appending it).
	return RUI::ResolvePath(StripQueryHash(To), UseLocation(Ctx).Pathname) + ExtractQueryHash(To);
}

void UseBlocker(FRuiContext& Ctx, bool bBlock, TFunction<void(const FString&)> OnBlocked)
{
	const FRuiRouterCtxValue& R = Ctx.UseContext(RouterContext());
	TSharedPtr<FRuiBlockerList> List = R.Blockers;

	// A stable entry, registered on mount / unregistered on unmount; bActive refreshed each render.
	TSharedRef<TRuiRef<TSharedPtr<FRuiBlockerEntry>>> EntryRef = Ctx.UseRef<TSharedPtr<FRuiBlockerEntry>>();
	if (!EntryRef->Current.IsValid())
	{
		EntryRef->Current = MakeShared<FRuiBlockerEntry>();
	}
	EntryRef->Current->OnBlocked = OnBlocked;
	*EntryRef->Current->bActive = bBlock;

	TSharedPtr<FRuiBlockerEntry> Entry = EntryRef->Current;
	Ctx.UseEffect(
		[List, Entry]() -> FRuiEffectCleanup
		{
			if (List.IsValid())
			{
				List->Add(Entry);
			}
			return [List, Entry]()
			{
				if (List.IsValid())
				{
					List->Remove(Entry);
				}
			};
		},
		RUI::Deps()); // mount-only registration; bActive/OnBlocked mutate in place each render
}
