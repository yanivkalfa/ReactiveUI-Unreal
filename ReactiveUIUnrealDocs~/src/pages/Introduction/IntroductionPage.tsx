import type { FC } from 'react'
import { Alert, Box, Link, List, ListItem, ListItemText, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'
import { ROADMAP_URL } from '../../links'
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
      frame's and patches only what changed. The output is ordinary SWidgets — Widget Reflector and
      the Slate toolchain see normal widgets — and everything of Epic's stays in place: UMG,
      CommonUI, and MVVM either feed data in or host the output.
    </Typography>
    <Typography variant="body1" paragraph>
      On top sits <code>.uetkx</code>, its authoring language — a JSX-like markup format. It
      compiles to native C++ for shipping builds, and hot-reloads live in PIE during development:
      save a file mid-play and see the UI update in under a second, state preserved, no C++
      recompile.
    </Typography>

    <Alert severity="info" sx={{ mb: 2 }}>
      ReactiveUI-Unreal is in <strong>beta</strong>: the product is built end to end (reconciler,
      compiler, hot reload, IDE tooling, Epic interop, localization) and green under the headless
      automation battery. Remaining before v1: docs build-out — see{' '}
      <Link href={ROADMAP_URL} target="_blank" rel="noreferrer">
        plans/ROADMAP.md
      </Link>{' '}
      for the living status.
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
  </Box>
)
