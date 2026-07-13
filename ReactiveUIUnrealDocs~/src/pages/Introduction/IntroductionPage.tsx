import type { FC } from 'react'
import { Alert, Box, Link, List, ListItem, ListItemText, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'
import { ROADMAP_URL, SIBLING_GODOT_URL, SIBLING_UNITY_URL } from '../../links'
import Styles from './IntroductionPage.style'

const QUICK_SAMPLE = `component Counter {
	auto [Count, SetCount] = UseState<int32>(0);
	return (
		<VerticalBox>
			<TextBlock Text={ RUI::Fmt(TEXT("Count: {}"), Count) } />
			<Button OnClicked={ SetCount(Count + 1) }>+1</Button>
		</VerticalBox>
	);
}`

export const IntroductionPage: FC = () => (
  <Box sx={Styles.root}>
    <Typography variant="h4" component="h1" gutterBottom>
      Reactive UI for Unreal
    </Typography>
    <Typography variant="body1" paragraph>
      Reactive UI is a React-style UI library for Unreal Engine 5.6+, written in pure C++ — no
      JavaScript VM, no bridge layer. You write function-style components, use hooks for state and
      effects (<code>UseState</code>, <code>UseEffect</code>, and 21 more), and a fiber reconciler
      decides which Slate widgets exist each frame: it diffs this frame's description against last
      frame's and patches only what changed. Everything of Epic's stays in place — UMG, CommonUI,
      and MVVM either feed data in or host the output.
    </Typography>
    <Typography variant="body1" paragraph>
      On top sits <code>.uetkx</code>, its authoring language — a JSX-like markup format with the
      same family grammar as <code>.guitkx</code> (Godot) and <code>.uitkx</code> (Unity). It
      compiles to native C++ for shipping builds, and hot-reloads live in PIE during development:
      save a file mid-play and see the UI update in under a second, state preserved, no C++
      recompile.
    </Typography>

    <Alert severity="warning" sx={{ mb: 2 }}>
      ReactiveUI-Unreal is <strong>pre-alpha</strong>: the plans are written and audited, and
      implementation is underway. Nothing here is ready for production use yet — see{' '}
      <Link href={ROADMAP_URL} target="_blank" rel="noreferrer">
        plans/ROADMAP.md
      </Link>{' '}
      in the GitHub repo for the living status and what ships in v1.0.
    </Alert>

    <CodeBlock language="uetkx" code={QUICK_SAMPLE} />

    <Typography variant="h5" component="h2" gutterBottom>
      Highlights
    </Typography>
    <List>
      <ListItem disablePadding>
        <ListItemText primary="Function-style .uetkx components with hooks and typed props — pure C++, zero VM" />
      </ListItem>
      <ListItem disablePadding>
        <ListItemText primary="A fiber reconciler that diffs and batches updates onto real Slate widgets" />
      </ListItem>
      <ListItem disablePadding>
        <ListItemText primary="Compiles to native C++ for shipping builds; hot-reloads live in PIE during development" />
      </ListItem>
      <ListItem disablePadding>
        <ListItemText primary="Interop by design: host inside UMG, embed UMG widgets, subscribe to MVVM viewmodels, live on CommonUI stacks" />
      </ListItem>
    </List>

    <Typography variant="h5" component="h2" gutterBottom>
      A proven family
    </Typography>
    <Typography variant="body1" paragraph>
      ReactiveUI-Unreal is the third sibling of a shipped family:{' '}
      <Link href={SIBLING_UNITY_URL} target="_blank" rel="noreferrer">
        ReactiveUIToolKit
      </Link>{' '}
      (Unity, C# — the reference) and{' '}
      <Link href={SIBLING_GODOT_URL} target="_blank" rel="noreferrer">
        ReactiveUI-Godot
      </Link>{' '}
      (Godot 4, GDScript). Same component model, same 23 hooks, same markup grammar — retargeted at
      Slate.
    </Typography>
  </Box>
)
