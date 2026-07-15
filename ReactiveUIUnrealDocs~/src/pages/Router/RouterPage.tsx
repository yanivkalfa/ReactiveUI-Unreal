import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const ROUTES = `#include "RuiRouter.h"

import { HomeScreen } from "./HomeScreen"
import { UserScreen } from "./UserScreen"

// A Router owns an in-memory history; Routes renders the best match for the
// current location. Routes are data (FRuiRoute), not tags.
export component AppShell {
	TArray<RUI::FRuiRoute> RouteList;
	RouteList.Add(RUI::FRuiRoute{ TEXT("/"),          HomeScreen() });
	RouteList.Add(RUI::FRuiRoute{ TEXT("/users/:id"), UserScreen() });

	return (
		<Overlay>
			{ RUI::Router({ RUI::Routes(MoveTemp(RouteList)) }, /*InitialPath*/ TEXT("/")) }
		</Overlay>
	);
}`

const HOOKS_SAMPLE = `// Inside a routed component — the compiler passes Ctx to every Use* call for you.
export component UserScreen {
	const FString Path = UsePathname();
	const TMap<FString, FString>& Params = UseParams();   // { "id": "42" }
	auto Navigate = UseNavigate();                        // Navigate(TEXT("/users/7"), false)

	// A parent route renders its matched child wherever it embeds UseOutlet:
	return (
		<VerticalBox>
			<TextBlock Text={ RUI::Fmt(TEXT("User {}"), Params[TEXT("id")]) } />
			{ UseOutlet() }
		</VerticalBox>
	);
}`

const HOOKS: Array<[string, string]> = [
  ['UseNavigate / UseGo / UseBackStack', 'Imperative navigation — push/replace a path, walk history.'],
  ['UseLocation / UsePathname / UseSearch', 'The current location, its path, its query string.'],
  ['UseParams / UseSearchParams', 'Dynamic :segments of the active route / read-write query params.'],
  ['UseMatch / UseIsActive / UseResolvedPath / UseHref', 'Test or resolve a path against the current location.'],
  ['UseOutlet / UseRoutes', 'The matched child element / evaluate a route table in place.'],
  ['UseNavigationType / UseInRouterContext', 'How the location changed / whether a router is above.'],
  ['UseBlocker', 'Intercept a navigation (e.g. an unsaved-changes guard).'],
]

export const RouterPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Router
    </Typography>
    <Typography variant="body1" paragraph>
      An in-memory router — inspired by React Router — for menus, settings flows and nested game
      screens. A <code>Router</code> holds the routing state; <code>Routes</code> picks the best
      match by ranked path; each matched route renders its element, and nested routes render where
      the parent calls <code>UseOutlet</code>. Like the other structural primitives, the router is
      a <code>RUI::</code> API (used from C++ or inside <code>{'{ expr }'}</code>), not a set of
      markup tags.
    </Typography>
    <CodeBlock code={ROUTES} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Routes are data
    </Typography>
    <Typography variant="body1" paragraph>
      <code>FRuiRoute</code> carries <code>Path</code> (with <code>:param</code> segments),{' '}
      <code>Element</code>, <code>bIndex</code> (the default child), and nested{' '}
      <code>Children</code>. <code>RUI::Link(To, Children, bReplace)</code> renders a navigation
      link; active-state styling comes from <code>UseIsActive</code>.
    </Typography>
    <CodeBlock code={HOOKS_SAMPLE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      The 17 router hooks
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Hooks</TableCell>
            <TableCell>Purpose</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {HOOKS.map(([hook, purpose]) => (
            <TableRow key={hook}>
              <TableCell>
                <code>{hook}</code>
              </TableCell>
              <TableCell>{purpose}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      The router is in-memory (no URL bar) and consumers re-render on location change only —
      navigation state lives outside any one screen, so pushing a settings flow doesn&apos;t
      re-render the HUD.
    </Alert>
  </Box>
)
