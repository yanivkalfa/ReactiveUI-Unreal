import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const ROOT = `#include "RuiRoot.h"

// Create / destroy a UI root.
TSharedPtr<FRuiRoot> Root = FRuiRoot::CreateInViewport(RUI::FC(&MyComponent), /*ZOrder*/ 10);
Root.Reset();   // unmount`

type Entry = [string, string]
const CORE: Entry[] = [
  ['FRuiRoot::CreateInViewport(vnode, z)', 'Mount a component tree into the game viewport; returns a TSharedPtr root.'],
  ['RUI::FC(&Component, props, children, key)', 'Instantiate a function component as a vnode.'],
  ['RUI::Fragment(...)', 'Group children with no wrapper widget.'],
  ['RUI::Suspense(...)', 'Boundary that shows a fallback until its content is ready.'],
  ['RUI::Fmt(TEXT("... {} ..."), args...)', 'Type-generic text interpolation returning FText.'],
  ['RUI::Deps(a, b, ...)', 'Build a dependency list for UseMemo / UseCallback / UseEffect.'],
]
const SLATE: Entry[] = [
  ['RUI::Slate::VerticalBox() / Button() / Border() / …', 'Host-element factories — one per Slate widget (usually written as tags).'],
  ['RUI::Slate::MakeDrawFn(lambda)', 'Wrap a paint lambda into an FRuiDrawFn for RuiCanvas.'],
  ['RUI::Slate::UseShortcut(...) / FRuiShortcut', 'Register keyboard shortcuts from a component.'],
]
const STATE: Entry[] = [
  ['RUI::GetOrCreateSignal<T>(Key, Init)', 'Get the process-wide signal for a key (creates on first use).'],
  ['RUI::UseSignalKey<T>(Ctx, Key, Default)', 'Subscribe a component to a keyed signal.'],
  ['ProvideContext(Key, Value) / UseContext(Key)', 'Provide and consume a value down a subtree.'],
]
const INTEROP: Entry[] = [
  ['RUI::Umg::UserWidget(Class, World)', 'Embed a UMG UUserWidget inside the tree.'],
  ['RUI::Umg::UseField<T>(Ctx, VM, FName, Default)', 'Read a FieldNotify view-model field reactively.'],
  ['RUI::CommonUI::ActivationContext() / UseIsActive()', 'Drive and read CommonUI activation state.'],
]

const Section: FC<{ title: string; rows: Entry[] }> = ({ title, rows }) => (
  <>
    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      {title}
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Symbol</TableCell>
            <TableCell>Purpose</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {rows.map(([sym, purpose]) => (
            <TableRow key={sym}>
              <TableCell sx={{ fontFamily: 'monospace', fontSize: '0.85em' }}>{sym}</TableCell>
              <TableCell>{purpose}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>
  </>
)

export const ApiReferencePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      API Reference
    </Typography>
    <Typography variant="body1" paragraph>
      Everything lives under the <code>RUI::</code> namespace, split by host: <code>RUI::Slate</code>{' '}
      for the default Slate host, <code>RUI::Umg</code> and <code>RUI::CommonUI</code> for the Epic
      interop layers. Hooks are free functions (<code>UseState</code>, <code>UseEffect</code>, …) —
      see the <strong>Hooks Guide</strong> for the full set.
    </Typography>
    <CodeBlock code={ROOT} language="uetkx" />

    <Section title="Core" rows={CORE} />
    <Section title="Slate host" rows={SLATE} />
    <Section title="State sharing" rows={STATE} />
    <Section title="Epic interop" rows={INTEROP} />

    <Alert severity="info">
      In <code>.uetkx</code> markup you rarely call the <code>RUI::Slate::*</code> factories directly
      — you write tags. The factories are what the compiler emits, and what you reach for in
      hand-written C++.
    </Alert>
  </Box>
)
