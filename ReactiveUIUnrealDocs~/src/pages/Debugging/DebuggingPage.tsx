import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'

const CVARS: Array<[string, string]> = [
  ['rui.StrictMode', 'Double-invoke render to surface impure components and effect mistakes.'],
  ['rui.HookValidation', 'Detect hook-order mismatches between renders (on by default in dev).'],
  ['rui.StrictDiagnostics', 'Warn on misuse such as setting state during render (on by default in dev).'],
  ['rui.TimeSlicing / rui.FrameBudgetMs', 'Chunk render work across frames / the per-frame budget.'],
  ['rui.HostNodePool', 'Recycle childless leaf widgets (turn off to bisect pooling suspicions).'],
]

export const DebuggingPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Debugging Guide
    </Typography>
    <Typography variant="body1" paragraph>
      When a component misbehaves, ReactiveUI gives you three vantage points: the generated C++, the
      compiler&apos;s diagnostics, and the runtime CVars. Because the markup compiles to ordinary
      committed C++, everything you already use to debug Unreal — breakpoints, the log, Live Coding —
      works unchanged.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Read the generated code
    </Typography>
    <Typography variant="body1" paragraph>
      Each <code>.uetkx</code> compiles to a committed <code>*.uetkx.inl</code> beside it. It is
      plain, readable C++ — the <code>RUI::FC</code> / <code>RUI::Slate::*</code> calls your markup
      lowered to. Reading it is the fastest way to confirm what the compiler actually produced, and
      you can set breakpoints in it directly.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Compiler diagnostics
    </Typography>
    <Typography variant="body1" paragraph>
      Every problem carries a <code>UETKX</code> code (see <strong>Diagnostics</strong>), written to
      a gitignored <code>*.uetkx.diags.json</code> sidecar and surfaced in the ReactiveUI message
      log. The editor extensions show the same diagnostics inline as you type — restart the language
      server (<code>UETKX: Restart Language Server</code>) if they ever look stale.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Runtime CVars &amp; stats
    </Typography>
    <Typography variant="body1" paragraph>
      <code>stat ReactiveUI</code> shows the live reconciler counters — Renders, Commits,
      Placements, Updates, Deletions — the fastest way to see whether a change re-renders more
      than it should. The <code>rui.*</code> console variables tune behavior:
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>CVar</TableCell>
            <TableCell>What it shows</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {CVARS.map(([cvar, effect]) => (
            <TableRow key={cvar}>
              <TableCell>
                <code>{cvar}</code>
              </TableCell>
              <TableCell>{effect}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="warning">
      A hook called conditionally desyncs the positional slots — the most common source of
      &quot;wrong state after a re-render.&quot; If state seems to jump between components, check that
      every hook runs unconditionally at the top (see the <strong>Hooks Guide</strong>).
    </Alert>
  </Box>
)
