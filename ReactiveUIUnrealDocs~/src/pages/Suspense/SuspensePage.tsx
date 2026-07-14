import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const SUSPENSE = `<Suspense Fallback={ RUI::FC(&Spinner) }>
	<AsyncContent />
</Suspense>`

export const SuspensePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Suspense
    </Typography>
    <Typography variant="body1" paragraph>
      <code>Suspense</code> shows a <strong>fallback</strong> while its content isn&apos;t ready yet,
      then swaps to the real content once it is — the declarative way to handle a loading state for
      an async resource (a streamed asset, a deferred computation).
    </Typography>
    <CodeBlock code={SUSPENSE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      No throw-to-suspend
    </Typography>
    <Typography variant="body1" paragraph>
      React suspends by throwing a promise mid-render; C++ has no equivalent, so ReactiveUI&apos;s
      Suspense is a <strong>declarative boundary polyfill</strong>. Readiness is driven explicitly —
      a poll driver (a self-re-arming frame request) or a signal flips the boundary from fallback to
      content. It is a deliberate family divergence, shared with the Godot and Unity siblings.
    </Typography>

    <Alert severity="info">
      Because there is no throw, a component never &quot;suspends itself&quot; implicitly — you model
      the not-ready state and let the boundary render the fallback until your readiness condition
      holds. The Stress Test demo uses the same poll-driver pattern for its frame loop.
    </Alert>
  </Box>
)
