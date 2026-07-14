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
    'C++-only factories have no generated pages yet',
    <>The per-tag Components pages generate from the compiler-exported schema (the 29 markup
      tags). The nine C++-only factories (ListView/TileView, drag-and-drop, the render-prop
      specials) are covered by <strong>Subsystems in depth</strong> and the guides until they join
      the schema.</>,
  ],
]

export const KnownIssuesPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Known Issues
    </Typography>
    <Typography variant="body1" paragraph>
      ReactiveUI-Unreal is in beta. These are the current limitations and intentional divergences
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
