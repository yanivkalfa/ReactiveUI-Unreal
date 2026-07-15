import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const ATTRS = `<Border
	Padding="12"
	BorderImage="WhiteBrush"
	BorderBackgroundColor={ FLinearColor(0.02f, 0.02f, 0.03f, 0.85f) }
	Slot.Padding="0,10,0,0"
>
	<TextBlock Text={ Label } Font.Size={ 16.0f } ColorAndOpacity={ FLinearColor(0.4f, 0.8f, 1.0f) } />
</Border>`

const CLASSES = `// Register a named style class once (startup, a module, a theme file):
RUI::Slate::RegisterStyleClass(TEXT("dim"), Style);

// Any element can wear one or more classes; inline style wins over them.
<TextBlock Text={ Mirror } classes="dim" />`

const THEME = `// A .uss-style stylesheet source: @theme token blocks + .class rules.
// Tokens are declared bare and referenced with a leading $.
@theme dark {
	bg: #10131a;
	accent: #4f8cff;
}
.panel { RenderOpacity: 1; ColorAndOpacity: $accent; }

// C++: load + activate, then $tokens resolve against the active theme.
RUI::Slate::LoadStylesheet(Source);
RUI::Slate::SetActiveTheme(TEXT("dark"));`

const STYLE_KEYS: Array<[string, string]> = [
  ['RenderOpacity', 'float 0..1'],
  ['Visibility', 'Visible | Collapsed | Hidden | HitTestInvisible | SelfHitTestInvisible'],
  ['Enabled', 'bool'],
  ['RenderTranslation / RenderScale / RenderTransformAngle / RenderTransformPivot', 'render-transform channels (FVector2D / float)'],
  ['Clipping', 'inherit | clipToBounds | clipToBoundsWithoutIntersecting | clipToBoundsAlways | onDemand — Slate never clips to bounds by default'],
  ['ColorAndOpacity', 'FSlateColor / FLinearColor (TextBlock, Image, Separator)'],
  ['Font.Size', 'text size (TextBlock and friends)'],
  ['Justification / AutoWrapText', 'text layout'],
  ['FillColorAndOpacity', 'ProgressBar fill'],
]

const SLOT_KEYS: Array<[string, string]> = [
  ['Slot.Padding', 'FMargin — "12", "12,4", or "l,t,r,b"'],
  ['Slot.HAlign / Slot.VAlign', 'fill | left | center | right (top/bottom for VAlign), case-insensitive'],
  ['Slot.Fill', 'stretch weight in a box panel'],
  ['Slot.ZOrder', 'stacking order in an Overlay'],
]

export const StylingPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Styling
    </Typography>
    <Typography variant="body1" paragraph>
      Styling has three layers, merged in a fixed cascade: <strong>theme tokens</strong> &lt;{' '}
      <strong>classes</strong> &lt; <strong>inline</strong>. Every key is the exact Unreal name
      (D-33) and applies through the widget&apos;s setter — a style tweak patches the live widget,
      it never rebuilds one. Style values reset when removed (unlike plain props — a preserved
      family semantic).
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Element attrs, style keys, slot keys
    </Typography>
    <Typography variant="body1" paragraph>
      Three kinds of markup attribute do the styling work. <strong>Element attrs</strong> are the
      widget&apos;s own properties (<code>Padding</code>, <code>BorderImage</code>,{' '}
      <code>ContentPadding</code>, <code>WidthOverride</code> — per-widget, from its Slate class).{' '}
      <strong>Generic style keys</strong> work on any widget. <strong>Slot keys</strong> (the{' '}
      <code>Slot.</code> prefix) tell the <em>parent</em> panel how to lay this child out.
    </Typography>
    <CodeBlock code={ATTRS} language="uetkx" />

    <TableContainer sx={{ my: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Generic style key</TableCell>
            <TableCell>Value</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {STYLE_KEYS.map(([key, val]) => (
            <TableRow key={key}>
              <TableCell>
                <code>{key}</code>
              </TableCell>
              <TableCell>{val}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Slot key</TableCell>
            <TableCell>Value</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {SLOT_KEYS.map(([key, val]) => (
            <TableRow key={key}>
              <TableCell>
                <code>{key}</code>
              </TableCell>
              <TableCell>{val}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Classes
    </Typography>
    <Typography variant="body1" paragraph>
      A <strong>style class</strong> is a named, registered bundle of style keys. Elements opt in
      with the <code>classes</code> attribute; multiple classes merge left-to-right, and inline
      style always wins. The Styled Panels demo&apos;s <code>classes=&quot;rui-demo-dim&quot;</code>{' '}
      is exactly this.
    </Typography>
    <CodeBlock code={CLASSES} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Themes &amp; stylesheets
    </Typography>
    <Typography variant="body1" paragraph>
      The lowest cascade layer is <strong>theme tokens</strong>. A stylesheet source declares{' '}
      <code>@theme</code> token blocks and <code>.class</code> rules;{' '}
      <code>LoadStylesheet</code> registers both, <code>SetActiveTheme</code> picks the active
      token set, and a <code>$token</code> string value anywhere in a style resolves against it —
      switch the theme and every consumer restyles.
    </Typography>
    <CodeBlock code={THEME} language="uetkx" />

    <Alert severity="info">
      From C++, the fluent builders mirror the markup: <code>RUI::Style().FontSize(16).
      ColorAndOpacity(Color)</code> and <code>RUI::Slot().Padding(...).HAlign(...)</code> produce
      the same dictionaries the compiler emits.
    </Alert>
  </Box>
)
