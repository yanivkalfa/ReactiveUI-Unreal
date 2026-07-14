import type { FC } from 'react'
import { Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const COMPANION = `SimpleCounter.uetkx          // the component (returns markup)
SimpleCounter.hooks.uetkx    // export hook UseCounter(...) -> ...   (reusable state logic)
SimpleCounter.style.uetkx    // export module Styles { ... }         (shared constants)`

const HOOK = `// SimpleCounter.hooks.uetkx — a hook is just a function whose name is Use*.
export hook UseCounter(int32 Start) -> TTuple<int32, TFunction<void()>> {
	auto [Count, SetCount] = UseState<int32>(Start);
	TFunction<void()> Increment = [SetCount, Count]() { SetCount(Count + 1); };
	return MakeTuple(Count, Increment);
}`

const CVARS: Array<[string, string]> = [
  ['rui.StrictMode', 'Double-invoke render in dev to surface impure components and effect mistakes.'],
  ['rui.Stats', 'Print per-commit reconciler stats (fibers visited, widgets created/patched).'],
  ['rui.DumpTree', 'Dump the current vnode/fiber tree for inspection.'],
]

export const ConceptsPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Concepts &amp; Environment
    </Typography>
    <Typography variant="body1" paragraph>
      ReactiveUI is a React-style model over Slate. You write <strong>function components</strong>{' '}
      that return a description of the UI; a <strong>fiber reconciler</strong> diffs this frame&apos;s
      description against the last and patches only the real Slate widgets that changed. Everything
      is pure C++ — there is no JavaScript engine and no UObject in the core runtime.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Components, hooks, modules
    </Typography>
    <Typography variant="body1" paragraph>
      A <code>.uetkx</code> file is a free sequence of three declaration kinds.{' '}
      <strong>Components</strong> return markup. <strong>Hooks</strong> (PascalCase{' '}
      <code>Use*</code>) encapsulate reusable state and effects. <strong>Modules</strong> hold shared
      constants and helpers. Companion files keep a component&apos;s logic beside it:
    </Typography>
    <CodeBlock code={COMPANION} language="uetkx" />
    <CodeBlock code={HOOK} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      The reconciler
    </Typography>
    <Typography variant="body1" paragraph>
      Rendering is <strong>synchronous</strong>: a state change requests an update, and the next
      reconcile builds the new tree, diffs it against the committed fibers, and applies a minimal set
      of widget mutations in one atomic commit. Children are matched by <code>key</code> for stable
      identity across reorders, and a component whose props and state are unchanged{' '}
      <strong>bails out</strong> without re-rendering its subtree. Effects run after commit.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Hooks are positional
    </Typography>
    <Typography variant="body1" paragraph>
      Hook state is stored in call order, so hooks must run unconditionally at the top of a component
      or hook — never inside <code>if</code>/<code>for</code>. This is the same rule as React, and it
      is what lets state, refs and effects line up slot-for-slot across every render.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Environment &amp; diagnostics
    </Typography>
    <Typography variant="body1" paragraph>
      The core talks to the engine only through <code>IRuiHostConfig</code>, so the same runtime
      drives Slate today and the Epic-interop hosts (UMG, CommonUI, MVVM). Behavior is tuned at
      runtime with <code>rui.</code> console variables:
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>CVar</TableCell>
            <TableCell>Effect</TableCell>
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
  </Box>
)
