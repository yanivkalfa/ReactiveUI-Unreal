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

const GRAMMAR = `import "@Doom/DoomTypes.h"                        // your own C++ header (a host include)
import { StatusChip } from "./StatusChip"          // named import
import { UseCounter as UseTick } from "~/Hooks"    // rename-on-import
import * as Palette from "./Palette"               // namespace import — Palette::Cool
import Screen from "./Screen"                      // default import
import Card, { UseDeck } from "./Deck"             // combined: default + named
import Fallback, * as Deck from "./Deck"           // combined: default + namespace

export FLinearColor Cool = FLinearColor(0.2f, 0.6f, 0.9f, 1.0f);  // value export (typed)
export Accent = FLinearColor(0.9f, 0.2f, 0.2f, 1.0f);             // inference: T(...) / T{...}
FLinearColor RowTint = FLinearColor(1, 1, 1, 1);                   // no export = file-private
export FString FormatScore(int32 S) { ... }        // util function
export int32 UseThing(int32 A) { ... }             // Use-prefix = a hook (Ctx auto-injected)
export FRuiNode StatusChip(FString Label) {        // FRuiNode return = a component
	return ( <Border> ... </Border> );
}
export { RowTint };                                // deferred export list
export default StatusChip;                         // default export (one per file)`

const AUTO_INCLUDED = [
  'CoreMinimal.h', 'RuiContext.h', 'RuiCoreElements.h', 'RuiSignal.h', 'RuiSlateElements.h',
  'RuiStyle.h', 'RuiRouter.h', 'RuiAssetBrush.h', 'RuiFieldHooks.h', 'RuiUmgElement.h',
  'RuiSignalViewModel.h', 'RuiHostWidget.h', 'RuiWorldSubsystem.h', 'RuiActivation.h',
  'RuiActivatableScreen.h', 'RuiMvvmViewModel.h', 'UObject/StrongObjectPtr.h', 'Engine/World.h',
]

const MIGRATE = `<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUIMigrateEsModules
<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUICompile -check`

const DIAGS: Array<[string, string, string]> = [
  ['UETKX2106', 'err', '`X` is exported by two files — one exported name, one file (rename or keep one private)'],
  ['UETKX2300', 'err', 'unknown import specifier — no file at that path'],
  ['UETKX2301', 'err', '`X` is not exported by that file — add `export` to its declaration'],
  ['UETKX2302', 'err', '`X` is not declared in that file'],
  ['UETKX2303', 'err', 'duplicate import of `X` (or duplicate host include of the same header)'],
  ['UETKX2304', 'warn', 'unused import `X` (usage counts the local alias)'],
  ['UETKX2305', 'err', '`X` is defined elsewhere but not imported (with a fix-it import line)'],
  ['UETKX2306', 'err', 'value-import cycle (hooks/modules/values/utils load eagerly — break the chain)'],
  ['UETKX2307', 'err', '`X` is used like a component/hook but no file exports it'],
  ['UETKX2308', 'err', 'import crosses a module/root boundary (imports are module-scoped in v1)'],
  ['UETKX2309', 'err', 'import must appear in the preamble, before the first declaration'],
  ['UETKX2317', 'hint', '`X.h` is auto-included by the generated prelude — this line is redundant'],
  ['UETKX2320', 'warn', '`component`/`hook`/`module` wrapper syntax is deprecated — run -run=RUIMigrateEsModules (removed in the next minor)'],
  ['UETKX2321', 'err', '`UseX` is Use-prefixed but returns FRuiNode — did you mean a component?'],
  ['UETKX2322', 'err', 'cannot infer the type of `X` — the initializer must name the type (T(...) / T{...}), or declare it'],
  ['UETKX2323', 'err', '`export` names `X`, which is not declared in this file'],
  ['UETKX2324', 'err', 'duplicate export of `X` (already exported inline or in a previous export list)'],
  ['UETKX2325', 'err', 'import alias `X` collides with a declaration or another import in this file'],
  ['UETKX2326', 'err', 'the target has no default export — use a named import'],
  ['UETKX2327', 'err', 'duplicate `export default` (a file has at most one default export)'],
]

export const ImportsPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Imports &amp; exports
    </Typography>
    <Typography variant="body1" paragraph>
      A <code>.uetkx</code> file <strong>is a module</strong>: its exports are its entire public
      surface, and it references other files&apos; declarations through <strong>static
      imports</strong> — strict from day one, a cross-file name that is not imported is a compile
      error. Declarations are plain C++-typed signatures; the kind is read from the signature
      alone: an <code>FRuiNode</code>-returning function is a <strong>component</strong>{' '}
      (PascalCase enforced), a <code>Use</code>-prefixed function is a <strong>hook</strong>, a
      name followed by <code>=</code> is a <strong>value export</strong>, anything else callable
      is a <strong>util function</strong>.
    </Typography>

    <CodeBlock code={GRAMMAR} language="uetkx" />

    <Alert severity="info" sx={{ mb: 2 }}>
      The old <code>component</code>/<code>hook</code>/<code>module</code> wrapper keywords still
      parse for one minor version (each warns <code>UETKX2320</code>);{' '}
      <code>-run=RUIMigrateEsModules</code> rewrites a whole tree in one idempotent pass. See the
      migration section below.
    </Alert>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Specifiers &amp; import forms
    </Typography>
    <Typography variant="body1" paragraph>
      Specifiers are relative (<code>./</code>, <code>../</code>) or use the family root alias{' '}
      <code>~/</code>, and are extensionless (<code>.uetkx</code> implied). The full ES surface is
      available: named imports, rename-on-import (<code>{'{ A as B }'}</code>), namespace imports
      (<code>import * as X</code> — members reachable as <code>X::Member</code>), default
      imports binding a file&apos;s <code>export default</code>, and the combined forms{' '}
      <code>{'import Def, { a, b as c } from'}</code> /{' '}
      <code>import Def, * as X from</code> — one declaration carrying the default binding plus
      the named or namespace surface, exactly as in ES. <code>~/</code> anchors at the
      file&apos;s Unreal module root by default, or at the <code>&quot;root&quot;</code> directory
      declared in the nearest <code>uetkx.config.json</code>. Engine-native forms (
      <code>res://</code>, <code>Assets/</code>, <code>Source/</code>) are forbidden in specifiers.
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
      Prefix a declaration with <code>export</code> to make it importable, list names in a
      deferred <code>export {'{ a, b }'};</code> statement, or mark one declaration{' '}
      <code>export default X;</code> (default-exporting does <em>not</em> also name-export it — ES
      parity). A non-exported declaration is file-private: it emits into a per-file detail
      namespace, its <em>runtime identity</em> is file-qualified (two files&apos; private{' '}
      <code>Row</code>s can never collide in HMR), and it is not registered globally (tree-shaken).
    </Typography>
    <Alert severity="info" sx={{ mb: 2 }}>
      Renaming a file renames its private namespace — private components get a fresh runtime
      identity and <strong>remount (state resets)</strong> on the next sweep. Exported members keep
      their identity (name-keyed). A documented, accepted semantic.
    </Alert>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Migrating to the ES-modules grammar
    </Typography>
    <Typography variant="body1" paragraph>
      <code>-run=RUIMigrateEsModules</code> is idempotent and re-runnable, and never regexes over
      raw text (it rewrites from the parsed records): it tidies preambles, exports everything
      existing, flips wrapper declarations to the plain grammar (<code>component X(P: T)</code>{' '}
      → <code>FRuiNode X(T P)</code>; <code>hook UseX(...) -&gt; R</code> → <code>R UseX(...)</code>
      ), hoists <code>module</code> members to value/util exports (importers&apos;{' '}
      <code>{'{ M }'}</code> imports flip to <code>import * as M</code> so their{' '}
      <code>M::x</code> references keep compiling with zero body edits), inserts any missing
      imports, and finishes with a zero-diagnostics compile gate — including zero{' '}
      <code>UETKX2320</code> outside explicitly-reported skips. A module holding anything but
      simple constants/functions (a <code>struct</code>, <code>using</code>, <code>static</code>{' '}
      state) is left wrapped and named in the summary — migrate those by hand or move them to a
      C++ header.
    </Typography>
    <CodeBlock code={MIGRATE} language="bash" />
    <Alert severity="warning" sx={{ mt: 1 }}>
      Because migration exports <em>everything</em>, two files that previously held same-named{' '}
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
