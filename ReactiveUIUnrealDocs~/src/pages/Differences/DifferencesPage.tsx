import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const EVENTS = `// Event props are the Slate delegate name, verbatim — not React aliases.
<Button OnClicked={ Submit() }>Save</Button>
<CheckBox OnCheckStateChanged={ SetChecked(!Checked) } />
<Slider OnValueChanged={ SetValue } />
// No onClick, no onChange. Tag = Slate class minus S; prop = the setter/delegate name.`

const ROWS: Array<[string, string]> = [
  ['Language', 'Pure C++ compiled to Slate — no JavaScript VM, no bridge, no reflection in the output.'],
  ['Output', 'Retained Slate widgets patched in place, not a virtual DOM re-painted to pixels.'],
  ['Hooks', 'PascalCase — UseState, UseEffect, UseMemo (C++/family convention).'],
  ['Events', 'The Unreal delegate name — OnClicked, OnCheckStateChanged — never onClick/onChange.'],
  ['Elements', 'The Slate class minus its S — VerticalBox, TextBlock, Slider; custom widgets carry the Rui mark.'],
  ['Transitions', 'UseTransition is synchronous — there is no concurrent renderer to defer to.'],
  ['Error boundaries', 'Structural, declared in markup — C++ cannot try/catch across a render the way JS can.'],
]

export const DifferencesPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Different from React
    </Typography>
    <Typography variant="body1" paragraph>
      The mental model is React&apos;s — components, hooks, a reconciler, keys. The surface is
      Unreal&apos;s. If you know React, these are the deliberate divergences to internalize; they are
      decisions, not gaps.
    </Typography>

    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Area</TableCell>
            <TableCell>How ReactiveUI differs</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {ROWS.map(([area, detail]) => (
            <TableRow key={area}>
              <TableCell>
                <strong>{area}</strong>
              </TableCell>
              <TableCell>{detail}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Naming is loyal to Unreal
    </Typography>
    <Typography variant="body1" paragraph>
      There are no React aliases and no shorthands. A tag is the Slate widget class with its{' '}
      <code>S</code> dropped; a prop, style key, or event is the exact Unreal setter, property, or
      delegate name. This keeps the markup 1:1 with the engine you already know.
    </Typography>
    <CodeBlock code={EVENTS} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Removed props don&apos;t reset
    </Typography>
    <Typography variant="body1" paragraph>
      When a plain prop disappears from one render to the next, ReactiveUI leaves the widget&apos;s
      current value in place rather than snapping it back to a default — Slate setters have no
      universal &quot;unset.&quot; Style, events, refs and custom draw <em>do</em> reset. This is a
      preserved family semantic, shared with the Unity and Godot siblings.
    </Typography>

    <Alert severity="info" sx={{ mt: 2 }}>
      Coming from the Godot sibling, three things differ on Unreal: React-style ref lifecycle,
      subscribe-in-effect for signals, and registry-<code>FName</code> component identity. See the
      Hooks and Signals guides for each.
    </Alert>
  </Box>
)
