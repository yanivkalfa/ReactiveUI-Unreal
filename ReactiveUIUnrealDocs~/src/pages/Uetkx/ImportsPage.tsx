import type { FC } from 'react'
import {
  Alert,
  Box,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography,
} from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const GRAMMAR = `import { StatusChip } from "./components/StatusChip/StatusChip"
import { UseCounter, CounterStyles } from "~/Screens/SimpleCounter/SimpleCounter.hooks"

export component Screen(Title: FText) { ... }   // exported: cross-file addressable
export hook UseThing(int32 A) -> int32 { ... }  // PascalCase Use* (C++/family)
export module Styles { ... }
component LocalHelper { ... }                    // no export = file-private (tree-shaken)`

const MIGRATE = `<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUIMigrateImports
<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUICompile -check`

const DIAGS: Array<[string, string, string]> = [
  ['UETKX2300', 'err', 'unknown import specifier — no file at that path'],
  ['UETKX2301', 'err', '`X` is not exported by that file — add `export` to its declaration'],
  ['UETKX2302', 'err', '`X` is not declared in that file'],
  ['UETKX2303', 'err', 'duplicate import of `X`'],
  ['UETKX2304', 'warn', 'unused import `X`'],
  ['UETKX2305', 'err', '`X` is defined elsewhere but not imported (with a fix-it import line)'],
  ['UETKX2306', 'err', 'value-import cycle (hooks/modules load eagerly — break the chain)'],
  ['UETKX2307', 'err', '`X` is used like a component/hook but no file exports it'],
  ['UETKX2308', 'err', 'import crosses a module/root boundary (imports are module-scoped in v1)'],
  ['UETKX2309', 'err', 'import must appear in the preamble, before the first declaration'],
]

export const ImportsPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Imports &amp; exports
    </Typography>
    <Typography variant="body1" paragraph>
      A <code>.uetkx</code> file references declarations in other files through{' '}
      <strong>static imports</strong>, and shares its own through <strong>exports</strong>. It is
      strict from day one: a cross-file name that is not imported is a compile error. A file is a
      free sequence of any number of components, hooks and modules.
    </Typography>

    <CodeBlock code={GRAMMAR} language="tsx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Specifiers
    </Typography>
    <Typography variant="body1" paragraph>
      Specifiers are relative (<code>./</code>, <code>../</code>) or use the family root alias{' '}
      <code>~/</code>, are extensionless (<code>.uetkx</code> implied), and name bindings only (no{' '}
      <code>*</code>, no default). <code>~/</code> anchors at the file&apos;s Unreal module root by
      default, or at the <code>&quot;root&quot;</code> directory declared in the nearest{' '}
      <code>uetkx.config.json</code>. Engine-native forms (<code>res://</code>, <code>Assets/</code>
      , <code>Source/</code>) are forbidden in specifiers.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Exports &amp; privacy
    </Typography>
    <Typography variant="body1" paragraph>
      Prefix a declaration with <code>export</code> to make it importable. A non-exported
      declaration is file-private: it emits into a per-file detail namespace and is not registered
      globally (tree-shaken). Privacy is opt-in — the migration codemod exports everything existing.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Migrating an existing project
    </Typography>
    <Typography variant="body1" paragraph>
      <code>-run=RUIMigrateImports</code> is idempotent and re-runnable: it adds <code>export</code>{' '}
      to every declaration and inserts the imports each file needs, then requires a zero-diagnostic
      compile.
    </Typography>
    <CodeBlock code={MIGRATE} language="bash" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Diagnostics
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Code</TableCell>
            <TableCell>Severity</TableCell>
            <TableCell>Meaning</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {DIAGS.map(([code, sev, msg]) => (
            <TableRow key={code}>
              <TableCell>
                <code>{code}</code>
              </TableCell>
              <TableCell>{sev}</TableCell>
              <TableCell>{msg}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      Cross-file <em>component</em> cycles are legal (a forward-declaration pass); <em>value</em>{' '}
      cycles (hooks/modules consumed at load) are a compile error (UETKX2306).
    </Alert>
  </Box>
)
