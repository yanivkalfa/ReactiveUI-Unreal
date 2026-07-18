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

const BEFORE_AFTER = `// BEFORE (wrapper grammar — still parses, warns UETKX2320)
export component ScoreRow(Label: FString = FString(TEXT("x")), N: int32 = 0) { ... }
export hook UseCountdown(int32 Start) -> TTuple<int32, TFunction<void()>> { ... }
export module Styles {
	inline const FLinearColor Accent{0.36f, 0.75f, 1.0f, 1.0f};
}

// AFTER (plain declarations — the signature IS the kind)
export FRuiNode ScoreRow(FString Label = FString(TEXT("x")), int32 N = 0) { ... }
export TTuple<int32, TFunction<void()>> UseCountdown(int32 Start) { ... }
export FLinearColor Accent = { 0.36f, 0.75f, 1.0f, 1.0f };`

const RUN = `<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUIMigrateEsModules
<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUICompile -check`

const TABLE: Array<[string, string]> = [
  ['component Name(P: T = D) { … }', 'FRuiNode Name(T P = D) { … } — params flip to C++ order; param-less gains ()'],
  ['hook UseX(raw) -> R { … }', 'R UseX(raw) { … } — the return type leads; no -> ⇒ void (explicit)'],
  ['module M { inline const T N = i; }', 'T N = i; hoisted to top level (brace-init normalizes to = { … })'],
  ['module member functions', 'top-level util functions'],
  ['importers of a hoisted module M', 'import { M } flips to import * as M — M::x references keep compiling unchanged'],
]

const DIAG_FIXES: Array<[string, string]> = [
  ['UETKX2320', 'wrapper syntax is deprecated — run the codemod (or rewrite the one declaration by hand)'],
  ['UETKX2321', 'a Use-prefixed function returns FRuiNode — rename it (components are PascalCase, not Use*)'],
  ['UETKX2322', 'type inference needs the initializer to name its type: write `T(...)` / `T{...}`, or declare the type'],
  ['UETKX2323', 'an export list / export default names something this file does not declare — fix the name'],
  ['UETKX2324', 'a name is exported twice (inline + list, or two lists) — drop one'],
  ['UETKX2325', 'an import alias collides with a declaration or another import — rename the alias'],
  ['UETKX2326', 'default-importing a file with no `export default` — use a named import instead'],
  ['UETKX2327', 'two `export default` statements — a file has at most one'],
]

export const EsModulesMigrationPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Migrating to ES modules
    </Typography>
    <Typography variant="body1" paragraph>
      A <code>.uetkx</code> file is a true <strong>ES module</strong>: per-file scope, plain{' '}
      <code>export</code>-prefixed C++-typed declarations, one visibility mechanism (imports), and
      the full ES import surface. The old <code>component</code>/<code>hook</code>/
      <code>module</code> wrapper keywords are deprecated — they keep parsing for one minor
      version (each warns <code>UETKX2320</code>), then get removed.
    </Typography>

    <CodeBlock code={BEFORE_AFTER} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      What changes
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Old</TableCell>
            <TableCell>New</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {TABLE.map(([oldForm, newForm]) => (
            <TableRow key={oldForm}>
              <TableCell>
                <code>{oldForm}</code>
              </TableCell>
              <TableCell>{newForm}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Run the codemod
    </Typography>
    <Typography variant="body1" paragraph>
      One command migrates the whole tree — idempotent (a second run is a no-op), re-runnable on a
      half-migrated tree, and driven by the parsed records (it never regexes over your code). It
      finishes with a zero-diagnostics compile gate, including zero <code>UETKX2320</code> outside
      the skips it explicitly reports.
    </Typography>
    <CodeBlock code={RUN} language="bash" />
    <Alert severity="warning" sx={{ mb: 2 }}>
      A <code>module</code> holding anything besides simple constants and functions (a{' '}
      <code>struct</code>, <code>using</code>, <code>static</code> state…) is <em>left wrapped</em>{' '}
      and named in the summary — migrate those by hand, or move the members to a hand-written C++
      header and <code>import &quot;@Header.h&quot;</code> it.
    </Alert>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Hot-reload note: private members remount on file rename
    </Typography>
    <Typography variant="body1" paragraph>
      Module identity derives from the file path. A <em>private</em> (non-exported)
      declaration&apos;s runtime identity is file-qualified, so renaming a file gives its private
      components fresh identity — they <strong>remount (state resets)</strong> on the next sweep.
      Exported members keep their identity (name-keyed). This is a documented, accepted semantic.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Troubleshooting
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Code</TableCell>
            <TableCell>Fix</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {DIAG_FIXES.map(([code, fix]) => (
            <TableRow key={code}>
              <TableCell>
                <code>{code}</code>
              </TableCell>
              <TableCell>{fix}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      Timeline: 0.12.0 ships the new grammar + the codemod and starts warning; removal of the
      wrapper keywords happens in a later minor, announced in the changelog per the deprecation
      policy in <code>VERSIONING.md</code>.
    </Alert>
  </Box>
)
