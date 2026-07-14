import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const INLINE = `<Border
	Padding="12"
	BorderImage="WhiteBrush"
	BorderBackgroundColor={ FLinearColor(0.02f, 0.02f, 0.03f, 0.85f) }
	Slot.Padding="0,10,0,0"
>
	<TextBlock Text={ Label } ColorAndOpacity={ FSlateColor(FLinearColor::White) } />
</Border>`

const SLOT = `<HorizontalBox>
	<Button Slot.Padding="0,0,6,0" Slot.HAlign="HAlign_Fill">Left</Button>
	<Button Slot.HAlign="HAlign_Right">Right</Button>
</HorizontalBox>`

const KEYS: Array<[string, string]> = [
  ['Padding, ContentPadding', 'FMargin — "12" (all), "12,4" (x,y), or "l,t,r,b".'],
  ['BorderBackgroundColor, ColorAndOpacity', 'FSlateColor / FLinearColor.'],
  ['BorderImage', 'A Slate brush (by name or an FSlateBrush).'],
  ['WidthOverride, HeightOverride', 'float — a fixed size on a SizeBox / override.'],
  ['Size', 'FVector2D — e.g. a Spacer’s size.'],
  ['RenderOpacity, RenderTransform', 'Per-widget render-time overrides.'],
  ['Slot.Padding, Slot.HAlign, Slot.VAlign', 'How the PARENT panel lays this child out.'],
]

export const StylingPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Styling
    </Typography>
    <Typography variant="body1" paragraph>
      There is no USS, no stylesheet cascade, and no separate style language. A widget is styled by
      setting its properties directly in markup, and every style key is the exact Unreal setter or
      property name — <code>Padding</code>, <code>ColorAndOpacity</code>, <code>BorderImage</code>.
      Values are engine types (<code>FMargin</code>, <code>FLinearColor</code>,{' '}
      <code>FSlateColor</code>, <code>FVector2D</code>), written as C++ expressions in braces or as
      the compact string forms Slate already accepts.
    </Typography>
    <CodeBlock code={INLINE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Element props vs. slot props
    </Typography>
    <Typography variant="body1" paragraph>
      A plain prop configures the widget itself. A <code>Slot.</code>-prefixed prop configures how
      the <em>parent</em> panel arranges this child — the Slate slot. <code>Slot.Padding</code>,{' '}
      <code>Slot.HAlign</code> and <code>Slot.VAlign</code> map to the panel&apos;s slot setters, so
      the same alignment vocabulary you use in C++ Slate applies unchanged.
    </Typography>
    <CodeBlock code={SLOT} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Common keys
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Key(s)</TableCell>
            <TableCell>Value type</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {KEYS.map(([key, type]) => (
            <TableRow key={key}>
              <TableCell>
                <code>{key}</code>
              </TableCell>
              <TableCell>{type}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      Because keys are the literal Unreal names, discovering what a widget accepts is just reading
      its Slate class — the same <code>SetPadding</code>/<code>SetColorAndOpacity</code> surface,
      exposed as markup props.
    </Alert>
  </Box>
)
