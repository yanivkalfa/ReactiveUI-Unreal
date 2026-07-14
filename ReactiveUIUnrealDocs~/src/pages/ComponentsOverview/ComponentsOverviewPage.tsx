import type { FC } from 'react'
import { Alert, Box, Chip, Stack, Typography } from '@mui/material'

type Group = { title: string; note: string; tags: string[] }

const GROUPS: Group[] = [
  {
    title: 'Layout & panels',
    note: 'Arrange children — each is a Slate panel.',
    tags: [
      'VerticalBox', 'HorizontalBox', 'Overlay', 'WrapBox', 'UniformGridPanel', 'GridPanel',
      'UniformWrapPanel', 'ScrollBox', 'WidgetSwitcher', 'Box', 'Border', 'ScaleBox', 'SafeZone',
      'DPIScaler', 'Spacer', 'Separator',
    ],
  },
  {
    title: 'Display',
    note: 'Show text, images and progress.',
    tags: ['TextBlock', 'RichTextBlock', 'Image', 'ProgressBar', 'Throbber'],
  },
  {
    title: 'Input & controls',
    note: 'Interactive widgets that raise On* events.',
    tags: ['Button', 'CheckBox', 'Slider', 'SpinBox', 'EditableTextBox', 'MultiLineEditableTextBox', 'SearchBox'],
  },
  {
    title: 'Interaction',
    note: 'Drag-and-drop and navigation.',
    tags: ['DragSource', 'DropTarget', 'Link'],
  },
  {
    title: 'Custom & structural',
    note: 'The Rui-marked canvas and tree-shaping elements.',
    tags: ['RuiCanvas', 'Fragment', 'Suspense'],
  },
]

export const ComponentsOverviewPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Components Overview
    </Typography>
    <Typography variant="body1" paragraph>
      Host elements are the intrinsic tags you compose with. Each maps 1:1 to a Slate widget: the tag
      is the Slate class with its leading <code>S</code> dropped, so <code>VerticalBox</code> is{' '}
      <code>SVerticalBox</code> and <code>EditableTextBox</code> is <code>SEditableTextBox</code>.
      Our own widgets carry the <code>Rui</code> mark — <code>RuiCanvas</code>. Your PascalCase
      function components sit alongside them with the same syntax.
    </Typography>

    {GROUPS.map((group) => (
      <Box key={group.title} sx={{ mb: 2.5 }}>
        <Typography variant="h6" component="h2">
          {group.title}
        </Typography>
        <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
          {group.note}
        </Typography>
        <Stack direction="row" flexWrap="wrap" gap={0.75}>
          {group.tags.map((tag) => (
            <Chip key={tag} label={tag} size="small" variant="outlined" sx={{ fontFamily: 'monospace' }} />
          ))}
        </Stack>
      </Box>
    ))}

    <Alert severity="info">
      Props, style keys and events on each element are the exact Unreal names — so the authoritative
      reference for any tag is its Slate class. See <strong>Styling</strong> for style keys and{' '}
      <strong>Events &amp; Input</strong> for handlers. Per-widget reference pages are generated from
      the compiler&apos;s prop-map as it lands.
    </Alert>
  </Box>
)
