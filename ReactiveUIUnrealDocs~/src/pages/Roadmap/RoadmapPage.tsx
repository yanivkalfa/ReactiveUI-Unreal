import type { FC } from 'react'
import { Alert, Box, Chip, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'

type Status = 'done' | 'progress' | 'planned'
const PHASES: Array<[string, string, Status, string]> = [
  ['1', 'Core reconciler + hooks', 'done', 'Fiber reconciler (subtree-skip, keyed diff, error boundaries), all hooks, signals + Suspense.'],
  ['2', 'Slate host + first widgets', 'done', 'Adapter registry, event proxies, mount surfaces, the first widget set, style v1, demo gallery.'],
  ['3', '.uetkx compiler + build', 'done', 'Lexer/parser → committed reflection-free .inl, schema sidecars, RUICompile commandlets, formatter.'],
  ['4', 'Hot reload', 'done', 'Live Coding-driven HMR, editor watcher, the ReactiveUetkx window, status line.'],
  ['5', 'IDE extensions', 'done', 'uetkx-language-server + VS Code and VS2022 extensions (completion, hover, diagnostics, formatting).'],
  ['6', 'UMG / CommonUI / MVVM interop', 'done', 'URuiHostWidget, RUI::Umg::UserWidget, UseField, CommonUI activatables, MVVM collection, UMG prop-map.'],
  ['7', 'Production gaps', 'done', 'Virtualized lists, focus, animation/media hooks, portals, drag-and-drop, widget batch 2. Localization deferred.'],
  ['8', 'Demos, docs, benchmarks', 'progress', 'Gallery and benchmark baselines done; the docs site content (this site) is being written.'],
  ['9', 'Release & publishing', 'planned', 'Owner-gated: the v1 ship gate, the merge to release, and the Fab/marketplace uploads.'],
]

const LABEL: Record<Status, { text: string; color: 'success' | 'warning' | 'default' }> = {
  done: { text: 'Done', color: 'success' },
  progress: { text: 'In progress', color: 'warning' },
  planned: { text: 'Planned', color: 'default' },
}

export const RoadmapPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Roadmap
    </Typography>
    <Typography variant="body1" paragraph>
      The living status of the project, mirrored from <code>plans/ROADMAP.md</code>. The runtime,
      compiler, tooling and Epic interop are built; the docs and the v1 release are the remaining
      work.
    </Typography>

    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>#</TableCell>
            <TableCell>Phase</TableCell>
            <TableCell>Status</TableCell>
            <TableCell>Notes</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {PHASES.map(([n, phase, status, notes]) => (
            <TableRow key={n}>
              <TableCell>{n}</TableCell>
              <TableCell>
                <strong>{phase}</strong>
              </TableCell>
              <TableCell>
                <Chip size="small" label={LABEL[status].text} color={LABEL[status].color} />
              </TableCell>
              <TableCell>{notes}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      Post-v1 subsystems and the deferred localization work are tracked on the{' '}
      <strong>Beyond v1</strong> and <strong>Known Issues</strong> pages.
    </Alert>
  </Box>
)
