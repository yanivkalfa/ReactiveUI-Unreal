import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const PORTAL = `// 1) Get a target: a portal target is a host handle. Capture one from any
//    mounted widget with the universal Ref={ } attribute (React ref lifecycle:
//    called with the handle on attach, cleared on detach):
const TSharedRef<TRuiRef<FRuiPortalHandle>>& Target = UseRef<FRuiPortalHandle>();

<Overlay Ref={ [Target](const FRuiHostHandle& H) { Target->Current = H; } }>
	{ /* ...app content... */ }
</Overlay>

// 2) Render children into it from anywhere in the tree (an {expr} child):
{ bShowModal && Target->Current
	? RUI::Portal(Target->Current, { RUI::FC(&PauseModal) })
	: RUI::Fragment({}) }`

export const PortalsPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Portals
    </Typography>
    <Typography variant="body1" paragraph>
      <code>RUI::Portal(Target, Children, Key)</code> renders children under a different widget
      than their parent — the escape hatch for modals, tooltips and overlays that must break out
      of their ancestor&apos;s clipping or stacking. The portal&apos;s fiber still belongs to the
      component that rendered it: state, context and effects work normally, and unmounting the
      portal tears the out-of-tree content down with it.
    </Typography>
    <CodeBlock code={PORTAL} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Targets are host handles
    </Typography>
    <Typography variant="body1" paragraph>
      A portal target (<code>FRuiPortalHandle</code>) is a handle to a live host widget — capture
      one with the universal <code>Ref=&#123; expr &#125;</code> attribute (markup) or the
      props-level <code>Ref</code> field (C++); both receive the handle on attach and clear on
      detach. On the Slate host, targets are ordinary widget handles, so any panel you can
      reference can receive portal children.
    </Typography>

    <Alert severity="info">
      Context flows through the portal from its <em>render</em> position, not its target position —
      a themed modal keeps its provider&apos;s theme even when re-parented to an app-level overlay.
    </Alert>
  </Box>
)
