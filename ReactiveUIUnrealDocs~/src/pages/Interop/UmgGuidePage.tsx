import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const OURS_IN_THEIRS = `// 1) Register a component (compiled .uetkx components self-register by name).
RUI_COMPONENT(InventoryPanel);   // or: RUI::RegisterNamedFactory(TEXT("InventoryPanel"), ...);

// 2) In the UMG Designer: drop a "ReactiveUI Host" (URuiHostWidget) into any
//    UserWidget layout and set its ComponentName to "InventoryPanel".
//    Design time shows a placeholder — live component code never runs in the Designer.
//    At runtime RebuildWidget() mounts the tree; ReleaseSlateResources unmounts it
//    (cleanups run first). Remount() re-resolves after changing ComponentName.

// World-scoped alternative (Blueprint-callable, no UMG wrapper needed):
URuiWorldSubsystem* Rui = World->GetSubsystem<URuiWorldSubsystem>();
int32 Handle = Rui->MountNamed(TEXT("InventoryPanel"), /*ZOrder*/ 10);
// Roots tear down automatically when the world dies (PIE-end safe).`

const THEIRS_IN_OURS = `// A UUserWidget as a child of our tree — from the InteropShowcase demo.
// The widget is created for the owning world and its SObjectWidget slots in
// like any other child; it stays GC-alive while mounted, released on unmount.
{ RUI::Umg::UserWidget(UDemoUmgWidget::StaticClass(), World) }

// With a declarative prop map — name -> value, applied to the widget's
// matching UPROPERTYs by reflection each commit (unknown names are skipped):
FRuiStyleDict WidgetProps;
WidgetProps.Add(FName(TEXT("Score")), FRuiValue(42));
{ RUI::Umg::UserWidget(UScoreCard::StaticClass(), World, MoveTemp(WidgetProps)) }`

export const UmgGuidePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      UMG Interop — a door in both directions
    </Typography>
    <Typography variant="body1" paragraph>
      UMG and ReactiveUI host each other. Designers embed reactive panels inside existing
      UserWidgets without leaving the Designer; reactive trees embed existing UMG widgets without
      rewriting them. Neither side is a second-class citizen.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Our UI inside theirs — <code>URuiHostWidget</code>
    </Typography>
    <Typography variant="body1" paragraph>
      <code>URuiHostWidget</code> is a designer-placeable <code>UWidget</code> (&quot;ReactiveUI
      Host&quot; in the palette). Point it at a registered component name; it mounts the tree when
      the widget builds and unmounts — running effect cleanups — when Slate resources release. For
      HUD-style overlays with no UMG wrapper at all, <code>URuiWorldSubsystem::MountNamed</code>{' '}
      mounts straight into the viewport, Blueprint-callable, with automatic teardown on world death.
    </Typography>
    <CodeBlock code={OURS_IN_THEIRS} language="uetkx" />
    <Alert severity="info" sx={{ my: 2 }}>
      Beta caveat: the host widget carries <em>only</em> <code>ComponentName</code> — there is no
      Blueprint-passed props/viewmodel channel yet. To feed data in today, share state through a{' '}
      <strong>Signal</strong> or a FieldNotify viewmodel the component reads with{' '}
      <code>UseField</code> (see the MVVM guide).
    </Alert>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Their widgets inside ours — <code>RUI::Umg::UserWidget</code>
    </Typography>
    <Typography variant="body1" paragraph>
      Any <code>UUserWidget</code> class drops into the tree as an expression child. The host
      creates it for the owning world, wraps its Slate face, keeps it GC-alive while mounted
      (strong-pointer semantics), and releases it in the deletion commit. Initial properties apply
      through the reflection prop-map — the same diffing rules as native elements.
    </Typography>
    <CodeBlock code={THEIRS_IN_OURS} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Textures &amp; materials
    </Typography>
    <Typography variant="body1" paragraph>
      To show a <code>UTexture2D</code>/material in a Slate <code>Image</code> or{' '}
      <code>Border</code>, wrap it with <code>RUI::Umg::MakeAssetBrush</code> — the brush is
      GC-rooted for its lifetime, no manual rooting. See <strong>Assets</strong>.
    </Typography>
  </Box>
)
