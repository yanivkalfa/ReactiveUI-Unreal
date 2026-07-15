import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const REF = `export component Stopwatch {
	// A ref is a mutable box whose .current survives every render and never
	// triggers one — perfect for a timer handle, a previous value, or a counter.
	const TSharedRef<TRuiRef<int32>>& Renders = UseRef<int32>(0);
	Renders->Current += 1;

	auto [Now, SetNow] = UseState<double>(0.0);
	return <TextBlock Text={ RUI::Fmt(TEXT("render #{}"), Renders->Current) } />;
}`

export const RefsGuidePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Refs Guide
    </Typography>
    <Typography variant="body1" paragraph>
      A ref is escape-hatch storage for a value that must persist across renders but should{' '}
      <em>not</em> cause one when it changes — the opposite of state.{' '}
      <code>UseRef&lt;T&gt;(Initial)</code> returns a stable box; you read and write{' '}
      <code>.Current</code> freely, and the box itself is never re-created.
    </Typography>
    <CodeBlock code={REF} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      When to use a ref
    </Typography>
    <Typography variant="body1" paragraph>
      Reach for a ref to hold a mutable value that isn&apos;t rendered — a subscription handle, the
      previous value of a prop, an animation cursor. If changing the value should update the UI, it
      belongs in <code>UseState</code>, not a ref.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Imperative handles
    </Typography>
    <Typography variant="body1" paragraph>
      <code>UseImperativeHandle</code> lets a component expose a small imperative API to its parent
      through a ref — for the rare cases where a parent must call into a child (focus, scroll,
      play) rather than pass props down. Prefer props and state first; use this only when a
      one-off imperative action genuinely has no declarative equivalent.
    </Typography>

    <Alert severity="info">
      Ref lifecycle follows React&apos;s model exactly: attach on mount, detach on unmount, and a
      re-created callback ref re-fires. See <strong>Different from React</strong> for the full list
      of deliberate decisions.
    </Alert>
  </Box>
)
