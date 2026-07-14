import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'

export const HmrPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Hot Module Replacement
    </Typography>
    <Typography variant="body1" paragraph>
      In development you edit a <code>.uetkx</code> file, save, and the running UI updates in under a
      second with component state preserved — no manual rebuild, no restart. ReactiveUI does not ship
      its own hot-reload engine; it rides <strong>Unreal Live Coding</strong>, so the same mechanism
      that patches your C++ patches your markup.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      How it works
    </Typography>
    <Typography variant="body1" paragraph>
      A save triggers the editor&apos;s ReactiveUI controller (<code>FUetkxHmrController</code>): it
      recompiles the changed <code>.uetkx</code> to its committed C++, then asks Live Coding to
      rebuild. Live Coding is <strong>whole-project</strong> — it patches the loaded modules in place
      — and the reconciler re-renders from the new component functions. Because hook state is stored
      by slot on the fibers (which survive the patch), your counters, inputs and scroll positions
      keep their values across the reload.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      What survives a reload — and what resets
    </Typography>
    <Typography variant="body1" paragraph>
      Edits to <strong>structure, style, and expressions</strong> hot-swap with hook state intact.
      Changing a component&apos;s <strong>hook shape</strong> (adding/removing/reordering hooks)
      resets that component&apos;s state on reload — deliberate, since positional slots can&apos;t
      be safely migrated; the status line reports when it happens. New C++ in handler bodies rides
      the same Live Coding patch as the markup, so it is picked up by the same save.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      The ReactiveUetkx window &amp; settings
    </Typography>
    <Typography variant="body1" paragraph>
      The <strong>ReactiveUetkx</strong> menu and window drive the loop from inside the editor: watch
      status, trigger a compile, and toggle <strong>Follow Play</strong> to tie hot-reload to
      PIE&apos;s Play/Stop. Live Coding must be enabled for the reload half to fire. Tuning lives in{' '}
      <em>Project Settings ▸ Plugins ▸ ReactiveUetkx</em> (watched roots, debounce, notifications,
      follow-PIE, hide the Live Coding console), and <code>ReactiveUetkx.HMR.Start</code> /{' '}
      <code>Stop</code> / <code>Toggle</code> drive it from the console.
    </Typography>

    <Alert severity="info" sx={{ mt: 2 }}>
      Live Coding is a Windows-editor capability, so HMR is a development-time convenience on that
      platform. Shipping builds never touch it: they compile the committed{' '}
      <code>*.uetkx.inl</code> ahead of time like any other C++.
    </Alert>

    <Alert severity="warning" sx={{ mt: 2 }}>
      A <code>.uetkx</code> that fails to compile is skipped — the last good build keeps running and
      the error surfaces in the ReactiveUI message log. Fix and save again to resume.
    </Alert>
  </Box>
)
