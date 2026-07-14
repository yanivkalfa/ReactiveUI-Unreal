import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const STATE = `auto [Count, SetCount]   = UseState<int32>(0);
auto [State, Dispatch]   = UseReducer(&Reducer, FCartState{});

// Functional updater — read the freshest value without capturing a stale one.
SetCount([](int32 Prev) { return Prev + 1; });`

const EFFECT = `// Runs after commit; the returned lambda is the cleanup. Deps gate re-runs.
UseEffect([=]() -> TFunction<void()>
{
	auto Handle = Store->Subscribe(OnChange);
	return [=]() { Store->Unsubscribe(Handle); };   // cleanup on unmount / dep change
}, { PlayerId });`

const HOOKS: Array<[string, string, string]> = [
  ['UseState<T>', 'State', 'A value and its setter; the setter accepts a new value or an updater lambda.'],
  ['UseReducer', 'State', 'State driven by a reducer + dispatch, for complex transitions.'],
  ['UseEffect', 'Effects', 'Side effect after commit, with a cleanup and a dependency list.'],
  ['UseLayoutEffect', 'Effects', 'Like UseEffect, but before the frame is presented — for measurement/focus.'],
  ['UseMemo<T>', 'Memo', 'Cache an expensive computed value across renders by deps.'],
  ['UseCallback', 'Memo', 'Cache a lambda identity across renders by deps.'],
  ['UseRef<T>', 'Refs', 'A mutable box whose .current survives renders and never triggers one.'],
  ['UseImperativeHandle', 'Refs', 'Expose an imperative handle to a parent via a ref.'],
  ['UseContext', 'Context', 'Read the nearest provided value; pair with the ProvideContext statement.'],
  ['UseStableCallback / UseStableFunc / UseStableAction', 'Identity', 'A callable whose identity never changes but always calls the latest closure.'],
  ['UseDeferredValue', 'Scheduling', 'A lagging copy of a value to keep interactions responsive.'],
  ['UseTransition', 'Scheduling', 'Mark an update non-urgent — synchronous on Unreal (no concurrent renderer).'],
  ['UseSignal / UseSignalKey', 'Shared state', 'Subscribe to a process-wide reactive store outside the tree (see Signals).'],
  ['UseAnimate / UseTween / UseTweenValue', 'Animation', 'Drive animated values from a component.'],
  ['UseSfx', 'Media', 'Fire a sound effect as a declarative side effect.'],
  ['UseSafeArea', 'Platform', 'Read the display safe-area insets.'],
  ['UsePresence', 'Lifecycle', 'Keep an exiting subtree mounted long enough to animate out.'],
  ['RUI::Slate::UseFocus', 'Input', 'A stable focus handle — Ref for the widget, Focus(), IsFocused().'],
  ['RUI::Slate::UseShortcut', 'Input', 'Register a keyboard chord for the component’s lifetime.'],
  ['RUI::Umg::UseField<T>', 'Interop', 'Reactively read a FieldNotify viewmodel field (see the MVVM guide).'],
]

export const HooksGuidePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Hooks Guide
    </Typography>
    <Typography variant="body1" paragraph>
      Hooks give a function component memory. They are PascalCase (<code>UseState</code>, not{' '}
      <code>useState</code>) and must be called <strong>unconditionally at the top</strong> of a
      component or another hook — state is stored by call order, so a conditional hook desyncs the
      slots. Router hooks live in the <strong>Router</strong> guide.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      State
    </Typography>
    <CodeBlock code={STATE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Effects
    </Typography>
    <Typography variant="body1" paragraph>
      An effect runs after the commit. Return a cleanup lambda; it runs on unmount and before the
      effect re-runs. The dependency list decides when that happens: <code>RUI::Deps(A, B)</code>{' '}
      re-runs on change, empty <code>RUI::Deps()</code> runs once on mount, and{' '}
      <code>RUI::EveryCommit()</code> runs after every commit.
    </Typography>
    <CodeBlock code={EFFECT} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      The full set
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Hook</TableCell>
            <TableCell>Group</TableCell>
            <TableCell>Purpose</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {HOOKS.map(([hook, group, purpose]) => (
            <TableRow key={hook}>
              <TableCell>
                <code>{hook}</code>
              </TableCell>
              <TableCell>{group}</TableCell>
              <TableCell>{purpose}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      Hooks compose: a custom <code>Use*</code> hook may call other hooks, so shared state logic
      lives in a <code>.hooks.uetkx</code> companion and returns a plain tuple — exactly how{' '}
      <code>UseCounter</code> wraps <code>UseState</code> in the Concepts guide.
    </Alert>
  </Box>
)
