import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const PROVIDE = `export FRuiNode ThemeRoot() {
	auto [bDark, SetDark] = UseState<bool>(true);
	const FLinearColor Theme = bDark ? CoolTheme : WarmTheme;

	// ProvideContext exposes a value to this component's whole subtree.
	ProvideContext(GDemoThemeCtx, Theme);

	return (
		<VerticalBox>
			<Button OnClicked={ SetDark(!bDark) }>Toggle theme</Button>
			<Panel />
		</VerticalBox>
	);
}`

const CONSUME = `// Any descendant reads the nearest provided value with UseContext.
export FRuiNode Panel() {
	const FLinearColor Theme = UseContext(GDemoThemeCtx);
	return <Border BorderBackgroundColor={ Theme }><TextBlock Text="Themed" /></Border>;
}`

export const ContextPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Context API
    </Typography>
    <Typography variant="body1" paragraph>
      Context passes a value down a subtree without threading it through every component in between.
      A parent <strong>provides</strong> a value under a context key; any descendant{' '}
      <strong>consumes</strong> it. When the provided value changes, every consumer re-renders.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Providing
    </Typography>
    <Typography variant="body1" paragraph>
      <code>ProvideContext(Key, Value)</code> is a statement in the component body — it makes{' '}
      <code>Value</code> visible to everything this component renders. The key is a stable context
      handle (typically declared once in a shared C++ header and host-included with{' '}
      <code>import &quot;@Header.h&quot;</code>), which also fixes the value&apos;s type.
    </Typography>
    <CodeBlock code={PROVIDE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Consuming
    </Typography>
    <Typography variant="body1" paragraph>
      <code>UseContext(Key)</code> returns the nearest provided value for that key. A nested provider
      shadows an outer one for its own subtree, so you can override a theme or locale locally.
    </Typography>
    <CodeBlock code={CONSUME} language="uetkx" />

    <Alert severity="info">
      Context is scoped to a subtree and re-renders its consumers on change — use it for themes,
      locale, or the current user. For truly global state read from anywhere, reach for a{' '}
      <strong>Signal</strong> instead.
    </Alert>
  </Box>
)
