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
import "@Doom/DoomTypes.h"                       // your own C++ header (a host include)

export component Screen(Title: FText) { ... }   // exported: cross-file addressable
export hook UseThing(int32 A) -> int32 { ... }  // PascalCase Use* (C++/family)
export module Styles { ... }
component LocalHelper { ... }                    // no export = file-private (tree-shaken)`

const AUTO_INCLUDED = [
  'CoreMinimal.h', 'RuiContext.h', 'RuiCoreElements.h', 'RuiSignal.h', 'RuiSlateElements.h',
  'RuiStyle.h', 'RuiRouter.h', 'RuiAssetBrush.h', 'RuiFieldHooks.h', 'RuiUmgElement.h',
  'RuiSignalViewModel.h', 'RuiHostWidget.h', 'RuiWorldSubsystem.h', 'RuiActivation.h',
  'RuiActivatableScreen.h', 'RuiMvvmViewModel.h', 'UObject/StrongObjectPtr.h', 'Engine/World.h',
]

const MIGRATE = `<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUIMigrateImports
<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUICompile -check

:: Optional: also retire raw #include lines from every preamble (idempotent, re-runnable)
<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUIMigrateImports -tidy`

const DIAGS: Array<[string, string, string]> = [
  ['UETKX2106', 'err', '`X` is exported by two files — one exported name, one file (rename or keep one private)'],
  ['UETKX2300', 'err', 'unknown import specifier — no file at that path'],
  ['UETKX2301', 'err', '`X` is not exported by that file — add `export` to its declaration'],
  ['UETKX2302', 'err', '`X` is not declared in that file'],
  ['UETKX2303', 'err', 'duplicate import of `X` (or duplicate host include of the same header)'],
  ['UETKX2304', 'warn', 'unused import `X`'],
  ['UETKX2305', 'err', '`X` is defined elsewhere but not imported (with a fix-it import line)'],
  ['UETKX2306', 'err', 'value-import cycle (hooks/modules load eagerly — break the chain)'],
  ['UETKX2307', 'err', '`X` is used like a component/hook but no file exports it'],
  ['UETKX2308', 'err', 'import crosses a module/root boundary (imports are module-scoped in v1)'],
  ['UETKX2309', 'err', 'import must appear in the preamble, before the first declaration'],
  ['UETKX2317', 'hint', '`X.h` is auto-included by the generated prelude — this line is redundant'],
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

    <CodeBlock code={GRAMMAR} language="uetkx" />

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
      Host includes — <code>import &quot;@Header.h&quot;</code>
    </Typography>
    <Typography variant="body1" paragraph>
      A third specifier shape, braces/<code>from</code>-less: <code>import &quot;@Header.h&quot;</code>{' '}
      is a nameless <strong>host include</strong> — it compiles to a plain{' '}
      <code>#include &quot;Header.h&quot;</code> in the generated file. It carries no bindings{' '}
      <em>by design</em>: the C++ compiler resolves the symbols a header provides, so there is
      nothing for the toolchain to name-check the way it does for a file import. Reach for it only
      for <strong>your own</strong> project headers — the library headers below are already
      included automatically, in every generated file, so a plain <code>.uetkx</code> file never
      needs an include at all.
    </Typography>
    <Typography variant="body2" color="text.secondary" paragraph>
      Auto-included (writing one of these yourself is harmless but redundant — flagged as a hint,{' '}
      <code>UETKX2317</code>):
    </Typography>
    <Typography variant="body2" paragraph sx={{ fontFamily: 'monospace' }}>
      {AUTO_INCLUDED.join(' · ')}
    </Typography>
    <Alert severity="info" sx={{ mb: 2 }}>
      A raw <code>#include &quot;Header.h&quot;</code> line still parses and compiles exactly as
      before — nothing breaks. <code>import &quot;@Header.h&quot;</code> is simply the form that
      keeps a <code>.uetkx</code> preamble import-only; the <code>-tidy</code> codemod below
      converts existing files for you.
    </Alert>

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
      compile. Its <code>-tidy</code> switch is a separate, also-idempotent pass: it rewrites every
      preamble to hold only import lines — an auto-included header is deleted outright, a surviving{' '}
      raw <code>#include</code> converts to <code>import &quot;@Header.h&quot;</code>. Surgical by
      construction: a line that has anything else on it (a trailing comment) is left completely
      untouched rather than risk losing content.
    </Typography>
    <CodeBlock code={MIGRATE} language="bash" />
    <Alert severity="warning" sx={{ mt: 1 }}>
      Because the codemod exports <em>everything</em>, two files that previously held same-named{' '}
      <em>private</em> declarations now both export that name — a{' '}
      <code>UETKX2106</code> duplicate-export collision. The codemod reports it (and so does{' '}
      <code>RUICompile -check</code>); resolve it by renaming one declaration or keeping one private.
    </Alert>

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
