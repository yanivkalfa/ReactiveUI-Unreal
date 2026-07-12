// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-001 — the family router, ported engine-blind (ReactiveUICore, no UObject). A React-Router-
// shaped, in-memory router: a <Router> provides an in-memory history; <Routes>/RUI::Routes match
// the current location against a pattern tree (`/users/:id`, `/files/*`); the hooks read the
// match + drive navigation. Everything is pure logic over context — portable to the Unity/Godot
// siblings as-is.
//
// Path patterns: `/a/b` literal, `:name` a named param (one segment), `*` a trailing splat
// (captured as the "*" param). Matching is case-sensitive, slash-normalized, and (for nested
// routes) prefix-aware.

#pragma once

#include "CoreMinimal.h"
#include "RuiNode.h"

class FRuiContext;

/** How the current location was reached (React-Router's NavigationType). */
enum class ERuiNavigationType : uint8
{
	Push,
	Replace,
	Pop
};

/** A parsed location: pathname + search (`?a=b`) + hash (`#x`). No origin — routing is in-memory. */
struct REACTIVEUICORE_API FRuiLocation
{
	FString Pathname = TEXT("/");
	FString Search; // includes the leading '?', or empty
	FString Hash;	// includes the leading '#', or empty
	FString Key;	// a per-entry key (history identity)

	bool operator==(const FRuiLocation& Other) const
	{
		return Pathname == Other.Pathname && Search == Other.Search && Hash == Other.Hash;
	}
	bool operator!=(const FRuiLocation& Other) const { return !(*this == Other); }

	/** Reassemble `pathname?search#hash`. */
	FString ToHref() const { return Pathname + Search + Hash; }
};

/** The result of matching a pattern against a pathname. */
struct REACTIVEUICORE_API FRuiPathMatch
{
	bool bMatched = false;
	TMap<FString, FString> Params;
	FString Pathname;	  // the portion that matched
	FString PathnameBase; // the base consumed before this match (nested routes)

	explicit operator bool() const { return bMatched; }
};

namespace RUI
{
	// ── pure helpers (the router_match suite exercises these directly) ─────────────────────

	/** Match `Pattern` against `Pathname`. `bEnd` = the pattern must consume the WHOLE pathname
	 *  (a leaf route); false allows a prefix match (a layout route with nested children). */
	REACTIVEUICORE_API FRuiPathMatch MatchPath(const FString& Pattern, const FString& Pathname, bool bEnd = true);

	/** Parse `pathname?search#hash` into a location. */
	REACTIVEUICORE_API FRuiLocation ParseLocation(const FString& Href);

	/** Parse a `?a=b&c=d` search string into a key→value map (first value wins). */
	REACTIVEUICORE_API TMap<FString, FString> ParseSearch(const FString& Search);

	/** Build a `?a=b&c=d` search string from a map (stable key order). */
	REACTIVEUICORE_API FString BuildSearch(const TMap<FString, FString>& Params);

	/** Resolve `To` against `From` (absolute `To` wins; relative joins onto From's directory). */
	REACTIVEUICORE_API FString ResolvePath(const FString& To, const FString& From);

	// ── the <Router> provider + declarative <Routes> + <Link> ──────────────────────────────

	/** A single declarative route: a `Path` pattern → `Element`, with optional nested `Children`
	 *  (a layout route whose Element should render an <Outlet/> = UseOutlet()). An empty Path with
	 *  bIndex marks the index route (matches the parent exactly). */
	struct REACTIVEUICORE_API FRuiRoute
	{
		FString Path;
		FRuiNode Element;
		bool bIndex = false;
		TArray<FRuiRoute> Children;
	};

	/** The in-memory router boundary. `InitialPath` seeds the first location. */
	REACTIVEUICORE_API FRuiNode Router(TArray<FRuiNode> Children, FString InitialPath = TEXT("/"),
									   FRuiKey Key = FRuiKey());

	/** Render the best match for the current location from `RouteList` (nesting via Outlet). */
	REACTIVEUICORE_API FRuiNode Routes(TArray<FRuiRoute> RouteList, FRuiKey Key = FRuiKey());

	/** A navigation link: renders `Children` and navigates to `To` on click (needs a host
	 *  wrapper that forwards clicks — the Slate Button/Text `OnClicked`; this is the core node). */
	REACTIVEUICORE_API FRuiNode Link(FString To, TArray<FRuiNode> Children, bool bReplace = false,
									 FRuiKey Key = FRuiKey());
} // namespace RUI

// ─────────────────────────────────────────────────────────────────────────────────────────
// The 17 family router hooks (free functions taking FRuiContext& first — the family convention).
// ─────────────────────────────────────────────────────────────────────────────────────────

/** True when called inside a <Router>. */
REACTIVEUICORE_API bool UseInRouterContext(FRuiContext& Ctx);

/** The current location (pathname/search/hash). */
REACTIVEUICORE_API const FRuiLocation& UseLocation(FRuiContext& Ctx);

/** Convenience: the current pathname. */
REACTIVEUICORE_API FString UsePathname(FRuiContext& Ctx);

/** Convenience: the current search string (with the leading '?'). */
REACTIVEUICORE_API FString UseSearch(FRuiContext& Ctx);

/** How the current location was reached. */
REACTIVEUICORE_API ERuiNavigationType UseNavigationType(FRuiContext& Ctx);

/** Imperative navigation: To may be absolute or relative; bReplace swaps the top entry. */
REACTIVEUICORE_API TFunction<void(const FString& To, bool bReplace)> UseNavigate(FRuiContext& Ctx);

/** Relative history motion: Go(-1) = back, Go(1) = forward. */
REACTIVEUICORE_API TFunction<void(int32 Delta)> UseGo(FRuiContext& Ctx);

/** { bCanGoBack, GoBack() } — the common back-button surface. */
REACTIVEUICORE_API TTuple<bool, TFunction<void()>> UseBackStack(FRuiContext& Ctx);

/** The params captured by the nearest matched route. */
REACTIVEUICORE_API const TMap<FString, FString>& UseParams(FRuiContext& Ctx);

/** [ params, SetParams ] over the location's search string. */
REACTIVEUICORE_API TTuple<TMap<FString, FString>, TFunction<void(TMap<FString, FString>)>>
UseSearchParams(FRuiContext& Ctx);

/** Match `Pattern` against the current pathname (nullptr-ish via bMatched). */
REACTIVEUICORE_API FRuiPathMatch UseMatch(FRuiContext& Ctx, const FString& Pattern);

/** True when `Pattern` matches the current pathname (NavLink active state). */
REACTIVEUICORE_API bool UseIsActive(FRuiContext& Ctx, const FString& Pattern, bool bEnd = false);

/** Resolve `To` against the current location's pathname. */
REACTIVEUICORE_API FString UseResolvedPath(FRuiContext& Ctx, const FString& To);

/** The href a link to `To` would point at (resolved + reassembled). */
REACTIVEUICORE_API FString UseHref(FRuiContext& Ctx, const FString& To);

/** The nested matched child element (rendered by a layout route's Element). */
REACTIVEUICORE_API FRuiNode UseOutlet(FRuiContext& Ctx);

/** Declarative routing as a hook: render the best match for `RouteList`. */
REACTIVEUICORE_API FRuiNode UseRoutes(FRuiContext& Ctx, const TArray<RUI::FRuiRoute>& RouteList);

/** Register a navigation blocker: while `bBlock`, navigations are intercepted and `OnBlocked`
 *  (the attempted href) fires instead of committing. Returns nothing (fire-and-forget guard). */
REACTIVEUICORE_API void UseBlocker(FRuiContext& Ctx, bool bBlock,
								   TFunction<void(const FString& AttemptedHref)> OnBlocked);
