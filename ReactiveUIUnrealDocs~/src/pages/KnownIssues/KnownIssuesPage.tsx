import type { FC, ReactNode } from 'react'
import { Alert, Box, Typography } from '@mui/material'

const ITEMS: Array<[string, ReactNode]> = [
  [
    'Localization is deferred',
    <>FText gathering for <code>.uetkx</code> is not wired yet — it is the one deliberately deferred
      production gap. Localized text works through the normal Unreal channels in the meantime; the
      markup-level gathering lands post-v1.</>,
  ],
  [
    'Hot reload is Windows-only',
    <>HMR rides Unreal Live Coding, a Windows-editor capability, so live editing is a development-time
      convenience on that platform. Shipping builds are unaffected — they compile the committed{' '}
      <code>*.uetkx.inl</code> ahead of time.</>,
  ],
  [
    'Hooks must be unconditional',
    <>Hook state is positional, so a hook inside an <code>@if</code>/<code>@for</code> desyncs the
      slots. Call every hook at the top level. Enable <code>rui.StrictMode</code> in dev to catch
      slip-ups early.</>,
  ],
  [
    'Transitions are synchronous',
    <><code>UseTransition</code> does not defer work — there is no concurrent renderer on Unreal, so it
      runs synchronously. It exists for API parity and intent, not background scheduling.</>,
  ],
  [
    'Removed plain props do not reset',
    <>Dropping a plain prop between renders leaves the widget&apos;s current value in place (Slate
      setters have no universal unset). Style, events, refs and custom draw <em>do</em> reset. This is
      an intentional family semantic.</>,
  ],
  [
    'Per-widget reference pages are pending',
    <>The Components section is data-driven from the compiler&apos;s prop-map, which is still being
      finalized — until it lands, use <strong>Components Overview</strong> plus the widget&apos;s Slate
      class as the reference.</>,
  ],
]

export const KnownIssuesPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Known Issues
    </Typography>
    <Typography variant="body1" paragraph>
      ReactiveUI-Unreal is pre-alpha. These are the current limitations and intentional divergences
      worth knowing — most are decisions, not bugs.
    </Typography>
    {ITEMS.map(([title, body]) => (
      <Box key={title} sx={{ mb: 2 }}>
        <Typography variant="h6" component="h2">
          {title}
        </Typography>
        <Typography variant="body1">{body}</Typography>
      </Box>
    ))}
    <Alert severity="info">
      The living status — what is built and what remains — is the <strong>Roadmap</strong>, mirrored
      from <code>plans/ROADMAP.md</code> in the repository.
    </Alert>
  </Box>
)
