import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const CREATE = `// A signal is a keyed, process-wide reactive value that lives OUTSIDE the tree.
// GetOrCreateSignal returns the same instance for a given key everywhere.
TSharedRef<TRuiSignal<int32>> Signal = RUI::GetOrCreateSignal<int32>(GDemoCounterSignal, 0);

<Button OnClicked={ Signal->Update([](const int32& V) { return V + 1; }) }>Increment</Button>
<Button OnClicked={ Signal->Set(0) }>Reset</Button>`

const READ = `// Any component reads a signal by key and re-renders when it changes.
export component CounterLabel {
	const int32 Count = RUI::UseSignalKey<int32>(Ctx, GDemoCounterSignal, 0);
	return <TextBlock Text={ RUI::Fmt(TEXT("Count: {}"), Count) } />;
}`

export const SignalsPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Signals
    </Typography>
    <Typography variant="body1" paragraph>
      A <strong>signal</strong> is a lightweight reactive value store that lives outside the
      component tree — the single source of truth for state many, possibly distant, components share.{' '}
      <code>TRuiSignal&lt;T&gt;</code> holds the value; components subscribe and re-render when it
      changes, without prop-drilling or lifting state.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Creating &amp; writing
    </Typography>
    <Typography variant="body1" paragraph>
      <code>RUI::GetOrCreateSignal&lt;T&gt;(Key, Initial)</code> returns the one signal registered
      under a key, creating it on first use. Write with <code>Set(value)</code> or{' '}
      <code>Update(fn)</code> for a functional update against the freshest value.
    </Typography>
    <CodeBlock code={CREATE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Reading in a component
    </Typography>
    <Typography variant="body1" paragraph>
      <code>UseSignalKey&lt;T&gt;(Ctx, Key, Default)</code> subscribes the calling component to the
      keyed signal: it returns the current value and schedules a re-render whenever the signal
      updates. Two sibling panels reading the same key stay in lockstep automatically.
    </Typography>
    <CodeBlock code={READ} language="uetkx" />

    <Alert severity="info">
      Signals are for shared, app-wide state that outlives any one component. For a value scoped to
      one subtree, prefer <strong>Context</strong>; for a component&apos;s own state, use{' '}
      <code>UseState</code>.
    </Alert>
  </Box>
)
