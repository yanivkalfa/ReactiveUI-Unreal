import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const DRAW = `export FRuiNode Chart() {
	auto [Sides, SetSides] = UseState<int32>(3);

	// The draw fn's IDENTITY tracks its inputs via UseMemo deps, so the canvas
	// repaints exactly when 'Sides' changes — nothing else re-renders it.
	const TSharedPtr<FRuiDrawFn>& PolyFn = UseMemo<TSharedPtr<FRuiDrawFn>>(
		[Sides]() {
			return RUI::Slate::MakeDrawFn(
				[Sides](const FGeometry& G, FSlateWindowElementList& Out, int32 Layer)
				{ return DrawPolygon(G, Out, Layer, Sides); });
		},
		RUI::Deps(Sides));

	return (
		<VerticalBox>
			<RuiCanvas DrawFn={ PolyFn } CanvasSize={ FVector2D(360.0f, 110.0f) } />
			<Button OnClicked={ SetSides(Sides + 1) }>Add side</Button>
		</VerticalBox>
	);
}`

const REDRAW = `// Alternatively: a STABLE draw fn, repainted on demand by bumping RedrawKey.
const TSharedPtr<FRuiDrawFn>& ScatterFn =
	UseMemo<TSharedPtr<FRuiDrawFn>>([]() { return RUI::Slate::MakeDrawFn(&DrawScatter); }, RUI::Deps());

<RuiCanvas DrawFn={ ScatterFn } RedrawKey={ Tick } CanvasSize={ FVector2D(360.0f, 110.0f) } />`

export const CustomRenderingPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Custom Rendering
    </Typography>
    <Typography variant="body1" paragraph>
      When you need to draw pixels rather than compose widgets — charts, gauges, gizmos —{' '}
      <code>RuiCanvas</code> gives you a Slate paint surface driven declaratively. You supply a{' '}
      <strong>draw function</strong>; the canvas calls it during Slate&apos;s paint pass with the
      geometry and element list.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      The draw function
    </Typography>
    <Typography variant="body1" paragraph>
      <code>RUI::Slate::MakeDrawFn(...)</code> wraps a lambda of{' '}
      <code>(const FGeometry&amp;, FSlateWindowElementList&amp;, int32 Layer)</code> into an{' '}
      <code>FRuiDrawFn</code>. Build it inside <code>UseMemo</code> so its identity is stable, and
      list the inputs it closes over as deps — the canvas repaints whenever the draw fn&apos;s
      identity changes.
    </Typography>
    <CodeBlock code={DRAW} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Repaint on demand
    </Typography>
    <Typography variant="body1" paragraph>
      If the draw fn is stable but the pixels should change (animation, external data), keep the fn
      memoized with empty deps and bump <code>RedrawKey</code> to force a repaint — the same pattern
      the Custom Rendering demo uses for its scatter plot.
    </Typography>
    <CodeBlock code={REDRAW} language="uetkx" />

    <Alert severity="info">
      Inside a draw fn you have the full <code>FSlateWindowElementList</code> API —{' '}
      <code>MakeLines</code>, <code>MakeBox</code>, custom brushes. The declarative wrapper only
      decides <em>when</em> to repaint; <em>what</em> to paint is ordinary Slate drawing.
    </Alert>
  </Box>
)
