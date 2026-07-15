import type { FC } from 'react'
import { Alert, Box, Chip, Stack, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'
import type { HookEntry } from '../../hooksCatalog'

/** One generated reference page per hook — the data lives in hooksCatalog.ts (authored from
 *  the real headers; the docs-drift gate compares its counts against the code registries). */
export const HookPage: FC<{ hook: HookEntry }> = ({ hook }) => (
  <Box>
    <Stack direction="row" alignItems="center" gap={1.5} sx={{ mb: 1 }}>
      <Typography variant="h4" component="h1">
        {hook.name}
      </Typography>
      <Chip
        size="small"
        label={hook.category === 'router' ? 'router hook' : 'core hook'}
        color={hook.category === 'router' ? 'info' : 'primary'}
        variant="outlined"
      />
    </Stack>

    <Typography variant="body1" paragraph>
      {hook.description}
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Signature
    </Typography>
    <CodeBlock
      code={`// .uetkx (the compiler passes Ctx for you)\n${hook.signature}\n\n// C++\n${hook.cppSignature}`}
      language="uetkx"
    />
    <Typography variant="body2" paragraph color="text.secondary" sx={{ mt: 1 }}>
      Returns: {hook.returns}
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Example
    </Typography>
    <CodeBlock code={hook.example} language="uetkx" />

    {hook.gotchas && hook.gotchas.length > 0 && (
      <Alert severity="info" sx={{ mt: 2 }}>
        {hook.gotchas.map((g) => (
          <Typography key={g} variant="body2" sx={{ '&:not(:last-child)': { mb: 0.5 } }}>
            • {g}
          </Typography>
        ))}
      </Alert>
    )}
  </Box>
)
