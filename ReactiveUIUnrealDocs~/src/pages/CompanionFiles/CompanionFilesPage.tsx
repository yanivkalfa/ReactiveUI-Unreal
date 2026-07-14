import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const LAYOUT = `SimpleCounter.uetkx          // the component
SimpleCounter.hooks.uetkx    // reusable hooks
SimpleCounter.style.uetkx    // shared constants (a module)`

const HOOKS = `// SimpleCounter.hooks.uetkx
export hook UseCounter(int32 Start) -> TTuple<int32, TFunction<void()>> {
	auto [Count, SetCount] = UseState<int32>(Start);
	TFunction<void()> Increment = [SetCount, Count]() { SetCount(Count + 1); };
	return MakeTuple(Count, Increment);
}`

const MODULE = `// ContextDemo.style.uetkx — a module holds plain C++ constants and helpers.
export module ContextDemoStyle {
	static const FLinearColor CoolTheme = FLinearColor(0.12f, 0.18f, 0.28f);
	static const FLinearColor WarmTheme = FLinearColor(0.28f, 0.16f, 0.10f);
}`

const FILES: Array<[string, string]> = [
  ['Name.uetkx', 'The component. Its name matches the file.'],
  ['Name.hooks.uetkx', 'Custom Use* hooks — reusable state and effect logic.'],
  ['Name.style.uetkx', 'A module of shared constants (colors, sizes, brushes).'],
]

export const CompanionFilesPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Companion Files
    </Typography>
    <Typography variant="body1" paragraph>
      A component&apos;s supporting code lives in <strong>companion files</strong> beside it — same
      base name, a role suffix, the <code>.uetkx</code> extension. They keep hooks and constants next
      to the component that uses them, and each compiles to its own committed C++ just like the
      component does.
    </Typography>
    <CodeBlock code={LAYOUT} language="uetkx" />

    <TableContainer sx={{ my: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>File</TableCell>
            <TableCell>Role</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {FILES.map(([file, role]) => (
            <TableRow key={file}>
              <TableCell>
                <code>{file}</code>
              </TableCell>
              <TableCell>{role}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Hooks companion
    </Typography>
    <CodeBlock code={HOOKS} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Module companion
    </Typography>
    <CodeBlock code={MODULE} language="uetkx" />

    <Alert severity="info">
      Companion files are just files with declarations — the suffix is a convention, not a special
      kind. Export what other files need; anything non-exported stays private. See{' '}
      <strong>Imports &amp; exports</strong>.
    </Alert>
  </Box>
)
