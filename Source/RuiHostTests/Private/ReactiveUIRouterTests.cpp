// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Router.Match — the pure matching/parsing primitives (patterns, params, splats,
// search, resolve). ReactiveUI.Router.Spine — the navigation flow end to end: <Router> history,
// <Routes> matching + nested outlets, UseParams/UseSearchParams, back stack, and UseBlocker.

#include "Misc/AutomationTest.h"
#include "RuiCoreElements.h"
#include "RuiMockHost.h"
#include "RuiNode.h"
#include "RuiRouter.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUI_ROUTER_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// ── ReactiveUI.Router.Match ────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiRouterMatchTest, "ReactiveUI.Router.Match", RUI_ROUTER_FLAGS)
bool FRuiRouterMatchTest::RunTest(const FString&)
{
	// static + params + splats
	{
		FRuiPathMatch M = RUI::MatchPath(TEXT("/users/:id"), TEXT("/users/42"));
		TestTrue(TEXT("param route matches"), M.bMatched);
		TestEqual(TEXT("param captured"), M.Params.FindRef(TEXT("id")), FString(TEXT("42")));
	}
	TestFalse(TEXT("leaf pattern rejects leftover path"), RUI::MatchPath(TEXT("/users"), TEXT("/users/42")).bMatched);
	TestTrue(TEXT("prefix (layout) accepts leftover"),
			 RUI::MatchPath(TEXT("/users"), TEXT("/users/42"), false).bMatched);
	{
		FRuiPathMatch M = RUI::MatchPath(TEXT("/files/*"), TEXT("/files/a/b/c"));
		TestTrue(TEXT("splat matches"), M.bMatched);
		TestEqual(TEXT("splat captured"), M.Params.FindRef(TEXT("*")), FString(TEXT("a/b/c")));
	}
	TestTrue(TEXT("root matches root"), RUI::MatchPath(TEXT("/"), TEXT("/")).bMatched);
	TestFalse(TEXT("literal mismatch"), RUI::MatchPath(TEXT("/a"), TEXT("/b")).bMatched);

	// parse / build search
	{
		const TMap<FString, FString> P = RUI::ParseSearch(TEXT("?q=hi&page=2"));
		TestEqual(TEXT("search q"), P.FindRef(TEXT("q")), FString(TEXT("hi")));
		TestEqual(TEXT("search page"), P.FindRef(TEXT("page")), FString(TEXT("2")));
		TMap<FString, FString> Build;
		Build.Add(TEXT("b"), TEXT("2"));
		Build.Add(TEXT("a"), TEXT("1"));
		TestEqual(TEXT("build search is key-sorted"), RUI::BuildSearch(Build), FString(TEXT("?a=1&b=2")));
	}

	// parse location
	{
		const FRuiLocation L = RUI::ParseLocation(TEXT("/shop/items?sort=price#top"));
		TestEqual(TEXT("pathname"), L.Pathname, FString(TEXT("/shop/items")));
		TestEqual(TEXT("search"), L.Search, FString(TEXT("?sort=price")));
		TestEqual(TEXT("hash"), L.Hash, FString(TEXT("#top")));
	}

	// resolve
	TestEqual(TEXT("absolute resolve"), RUI::ResolvePath(TEXT("/a/b"), TEXT("/x/y")), FString(TEXT("/a/b")));
	// React-Router treats the current pathname as a directory context: a bare relative segment
	// appends, and `..` climbs.
	TestEqual(TEXT("relative resolve appends"), RUI::ResolvePath(TEXT("edit"), TEXT("/users/42")),
			  FString(TEXT("/users/42/edit")));
	TestEqual(TEXT("dot-dot resolve climbs"), RUI::ResolvePath(TEXT("../list"), TEXT("/users/42")),
			  FString(TEXT("/users/list")));
	return true;
}

// ── ReactiveUI.Router.Spine ────────────────────────────────────────────────────────────────

namespace RouterTest
{
	static TFunction<void(const FString&, bool)> GNavigate;
	static TFunction<void()> GBack;
	static bool GCanGoBack = false;
	static TFunction<void(TMap<FString, FString>)> GSetSearch;
	static TMap<FString, FString> GSearch;
	static bool GBlock = false;
	static int32 GBlockedCount = 0;

	static FRuiNodeArray UserScreen(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		const TMap<FString, FString>& Params = UseParams(Ctx);
		return {RUI::TextBlock(TEXT("USER:") + Params.FindRef(TEXT("id")))};
	}
	RUI_COMPONENT(UserScreen)

	static FRuiNodeArray DashLayout(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		return {RUI::TextBlock(TEXT("LAYOUT")), UseOutlet(Ctx)};
	}
	RUI_COMPONENT(DashLayout)

	// Captures navigation surfaces + registers a blocker; renders the declarative Routes.
	static FRuiNodeArray Shell(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		GNavigate = UseNavigate(Ctx);
		TTuple<bool, TFunction<void()>> Back = UseBackStack(Ctx);
		GCanGoBack = Back.Key;
		GBack = Back.Value;
		TTuple<TMap<FString, FString>, TFunction<void(TMap<FString, FString>)>> SP = UseSearchParams(Ctx);
		GSearch = SP.Key;
		GSetSearch = SP.Value;
		UseBlocker(Ctx, GBlock, [](const FString&) { ++GBlockedCount; });

		TArray<RUI::FRuiRoute> RouteList;
		RouteList.Add({TEXT("/home"), RUI::TextBlock(TEXT("HOME"))});
		RouteList.Add({TEXT("/users/:id"), RUI::FC(&UserScreen)});
		RUI::FRuiRoute Dash;
		Dash.Path = TEXT("/dash");
		Dash.Element = RUI::FC(&DashLayout);
		Dash.Children.Add({FString(), RUI::TextBlock(TEXT("DASH-INDEX")), /*bIndex*/ true, {}});
		Dash.Children.Add({TEXT("settings"), RUI::TextBlock(TEXT("DASH-SETTINGS"))});
		RouteList.Add(Dash);

		return {RUI::Routes(MoveTemp(RouteList))};
	}
	RUI_COMPONENT(Shell)

	static FRuiNodeArray App(FRuiContext&, const FRuiEmptyProps&, const TArray<FRuiNode>&)
	{
		return {RUI::Router({RUI::FC(&Shell)}, TEXT("/home"))};
	}
	RUI_COMPONENT(App)

	static void CollectText(FMockNode* Node, TArray<FString>& Out)
	{
		const FString T = Node->TextOf();
		if (!T.IsEmpty())
		{
			Out.Add(T);
		}
		for (const TSharedPtr<FMockNode>& C : Node->Children)
		{
			if (C.IsValid() && !C->bReleased)
			{
				CollectText(C.Get(), Out);
			}
		}
	}
} // namespace RouterTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiRouterSpineTest, "ReactiveUI.Router.Spine", RUI_ROUTER_FLAGS)
bool FRuiRouterSpineTest::RunTest(const FString&)
{
	using namespace RouterTest;
	GBlock = false;
	GBlockedCount = 0;

	FRuiTestHarness H;
	auto Texts = [&H]()
	{
		TArray<FString> Out;
		CollectText(&H.Host.Root.Get(), Out);
		return Out;
	};
	auto Nav = [&H](const FString& To, bool bReplace = false)
	{
		GNavigate(To, bReplace);
		H.Reconciler->FlushSync();
	};

	H.Mount(RUI::FC(&App));

	TestTrue(TEXT("initial route /home"), Texts().Contains(FString(TEXT("HOME"))));

	Nav(TEXT("/users/42"));
	TestTrue(TEXT("param screen renders with param"), Texts().Contains(FString(TEXT("USER:42"))));
	TestFalse(TEXT("home gone"), Texts().Contains(FString(TEXT("HOME"))));

	// nested layout + index outlet
	Nav(TEXT("/dash"));
	{
		const TArray<FString> T = Texts();
		TestTrue(TEXT("layout renders"), T.Contains(FString(TEXT("LAYOUT"))));
		TestTrue(TEXT("index outlet renders"), T.Contains(FString(TEXT("DASH-INDEX"))));
	}
	Nav(TEXT("/dash/settings"));
	{
		const TArray<FString> T = Texts();
		TestTrue(TEXT("layout still renders"), T.Contains(FString(TEXT("LAYOUT"))));
		TestTrue(TEXT("settings outlet renders"), T.Contains(FString(TEXT("DASH-SETTINGS"))));
		TestFalse(TEXT("index gone"), T.Contains(FString(TEXT("DASH-INDEX"))));
	}

	// back stack: /home -> /users/42 -> /dash -> /dash/settings; go back once -> /dash
	TestTrue(TEXT("can go back"), GCanGoBack);
	GBack();
	H.Reconciler->FlushSync();
	TestTrue(TEXT("back to /dash index"), Texts().Contains(FString(TEXT("DASH-INDEX"))));

	// search params: navigate with a query, read it, then set it
	Nav(TEXT("/home?q=hi"));
	TestEqual(TEXT("search param read"), GSearch.FindRef(TEXT("q")), FString(TEXT("hi")));
	GSetSearch(
		[]
		{
			TMap<FString, FString> M;
			M.Add(TEXT("q"), TEXT("bye"));
			return M;
		}());
	H.Reconciler->FlushSync();
	TestEqual(TEXT("search param updated"), GSearch.FindRef(TEXT("q")), FString(TEXT("bye")));

	// blocker: while blocking, a navigate is intercepted (element does not change)
	GBlock = true;
	H.Reconciler->HmrRefreshAll(); // re-render so UseBlocker sees bBlock=true
	H.Reconciler->FlushSync();
	const int32 BlockedBefore = GBlockedCount;
	Nav(TEXT("/users/7"));
	TestEqual(TEXT("blocked navigation fired the blocker"), GBlockedCount, BlockedBefore + 1);
	TestFalse(TEXT("blocked navigation did not change route"), Texts().Contains(FString(TEXT("USER:7"))));

	GBlock = false;
	GNavigate = nullptr;
	GBack = nullptr;
	GSetSearch = nullptr;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
