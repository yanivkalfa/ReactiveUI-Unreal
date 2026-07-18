import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const SUSPENSE = `// Suspense is a RUI:: call (a structural primitive, not a tag). You drive
// readiness explicitly — the IsReady callable decides fallback vs content.
export FRuiNode LoadingScreen() {
	auto [bReady, SetReady] = UseState<bool>(false);
	UseEffect([SetReady]() { StartStreaming([SetReady]() { SetReady(true); }); }, RUI::Deps());
	const bool bReadyNow = bReady;

	return (
		<VerticalBox>
			{ RUI::Suspense([bReadyNow]() { return bReadyNow; },
			                RUI::FC(&Spinner),
			                { RUI::FC(&AsyncContent) }) }
		</VerticalBox>
	);
}`

export const SuspensePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Suspense
    </Typography>
    <Typography variant="body1" paragraph>
      <code>RUI::Suspense(IsReady, Fallback, Children)</code> shows a <strong>fallback</strong>{' '}
      while its content isn&apos;t ready, then swaps to the real children once it is — the
      declarative way to handle a loading state for an async resource (a streamed asset, a
      deferred computation). Like Portal and ErrorBoundary, it is a <code>RUI::</code> call used
      inside a <code>{'{ expr }'}</code> child, not a markup tag.
    </Typography>
    <CodeBlock code={SUSPENSE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      No throw-to-suspend
    </Typography>
    <Typography variant="body1" paragraph>
      React suspends by throwing a promise mid-render; C++ has no equivalent, so this Suspense is a{' '}
      <strong>declarative boundary polyfill</strong>: readiness is driven explicitly by the{' '}
      <code>IsReady</code> callable, re-evaluated as the tree updates — flip your state (or use a
      self-re-arming frame request, the poll-driver pattern the Stress Test demo uses) and the
      boundary swaps. A deliberate divergence from React, not a limitation to work around.
    </Typography>

    <Alert severity="info">
      A component never &quot;suspends itself&quot; implicitly — you model the not-ready state and
      let the boundary render the fallback until your readiness condition holds.
    </Alert>
  </Box>
)
