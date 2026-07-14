import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const BRUSH = `// Point a Slate brush at a UObject (a UTexture2D, material, etc.) and use it
// anywhere a brush is expected — Image, Border, Button background.
TSharedPtr<FSlateBrush> Icon = RUI::Umg::MakeAssetBrush(
	IconTexture, /*ImageSize*/ FVector2D(32.0f, 32.0f), /*Tint*/ FLinearColor::White);

<Image Image={ Icon } />`

export const AssetsPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Assets
    </Typography>
    <Typography variant="body1" paragraph>
      Slate draws from <strong>brushes</strong>, <strong>fonts</strong> and <strong>colors</strong>,
      not raw asset paths. To show a texture or material you wrap it in an <code>FSlateBrush</code>{' '}
      that references the <code>UObject</code>, then hand that brush to a widget.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Asset brushes
    </Typography>
    <Typography variant="body1" paragraph>
      <code>RUI::Umg::MakeAssetBrush(ResourceObject, ImageSize, Tint)</code> builds a brush pointing
      at any drawable <code>UObject</code> — a <code>UTexture2D</code>, a UI material, a render
      target. Load or resolve the object however you normally would (a{' '}
      <code>TObjectPtr</code>, a soft reference you resolve, an asset manager lookup) and pass it in.
    </Typography>
    <CodeBlock code={BRUSH} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Fonts &amp; colors
    </Typography>
    <Typography variant="body1" paragraph>
      Text takes an <code>FSlateFontInfo</code> (via <code>Font</code> keys) and{' '}
      <code>FSlateColor</code>/<code>FLinearColor</code> (via <code>ColorAndOpacity</code>) — the same
      types you would build in C++ Slate. Because every style key is the literal Unreal name, asset
      wiring in markup is exactly the asset wiring you already know.
    </Typography>

    <Alert severity="info">
      Brushes hold a reference to their resource object, so keep the owning object alive for the
      brush&apos;s lifetime (store it in a ref or a member) — the same lifetime rule as hand-built
      Slate brushes.
    </Alert>
  </Box>
)
