import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const ROUTES = `<Router InitialPath="/home">
	<Routes>
		<Route Path="/home" element={ RUI::FC(&HomeScreen) } />
		<Route Path="/settings" element={ RUI::FC(&SettingsScreen) } />
		<Route Index element={ RUI::FC(&Landing) } />
	</Routes>
</Router>

// A parent route renders its matched child at the Outlet.
<VerticalBox>
	<NavBar />
	<Outlet />
</VerticalBox>`

const HOOKS: Array<[string, string]> = [
  ['UseNavigate', 'Imperative navigation — go to a path.'],
  ['UseLocation / UsePathname', 'The current location / path.'],
  ['UseParams', 'Dynamic segments matched by the active route.'],
  ['UseSearchParams', 'Read and update the query string.'],
  ['UseMatch / UseResolvedPath', 'Test/resolve a path against the current route.'],
  ['UseOutlet', 'The element to render at this route’s Outlet.'],
  ['UseBlocker', 'Intercept a navigation (e.g. unsaved-changes guard).'],
]

export const RouterPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Router
    </Typography>
    <Typography variant="body1" paragraph>
      An in-memory router — inspired by React Router — lets you author navigation directly in markup.
      A <code>Router</code> holds the routing state; <code>Routes</code> picks the best match by path;
      each matched <code>Route</code> renders its element, and nested routes render into an{' '}
      <code>Outlet</code>.
    </Typography>
    <CodeBlock code={ROUTES} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Elements
    </Typography>
    <Typography variant="body1" paragraph>
      <code>Router</code> (with an <code>InitialPath</code>), <code>Routes</code>,{' '}
      <code>Route</code> (a <code>Path</code>, or <code>Index</code> for the default child),{' '}
      <code>Outlet</code> (where a nested route renders), and <code>NavLink</code> /{' '}
      <code>Link</code> for declarative navigation with active-state styling.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Hooks
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Hook</TableCell>
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
      The router is in-memory (no URL bar), which suits game UI: menus, settings flows and nested
      screens all navigate the same way, and <code>UseBlocker</code> guards transitions when needed.
    </Alert>
  </Box>
)
