import type { FC } from 'react'
import { Alert, Box, Chip, Stack, Typography } from '@mui/material'

type Group = { title: string; note: string; tags: string[] }

const TAG_GROUPS: Group[] = [
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
    title: 'Custom (the Rui mark)',
    note: 'Our own widgets carry the Rui prefix.',
    tags: ['RuiCanvas'],
  },
]

const CPP_ONLY: Array<[string, string]> = [
  ['ListView / TileView', 'virtualized item-model views — a render-prop API (see Beyond the tags below)'],
  ['ComboBox / SegmentedControl / NumericEntryBox / SuggestionTextBox / ExpandableArea', 'render-prop / multi-slot specials (ExpandableArea takes header + body roles)'],
  ['DragSource / DropTarget', 'typed drag-and-drop wrappers'],
  ['RUI::Umg::UserWidget', 'host a UMG UUserWidget inside the tree'],
  ['RUI::{Fragment, Portal, Suspense, ErrorBoundary, Presence, Router, Routes, Link}', 'structural primitives — RUI:: calls, not tags'],
]

export const ComponentsOverviewPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Components Overview
    </Typography>
    <Typography variant="body1" paragraph>
      Two vocabularies compose the tree. <strong>Markup tags</strong> (30 of them) map 1:1 to Slate
      widgets — the tag is the Slate class minus its <code>S</code>, so <code>VerticalBox</code> is{' '}
      <code>SVerticalBox</code>. <strong>C++-level factories</strong> cover the widgets whose API
      is a render prop or a structural boundary — you reach them from C++ or inside a{' '}
      <code>{'{ expr }'}</code> child. Your own PascalCase components sit alongside the tags with
      identical syntax.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 2 }}>
      The markup tags
    </Typography>
    {TAG_GROUPS.map((group) => (
      <Box key={group.title} sx={{ mb: 2.5 }}>
        <Typography variant="h6" component="h3">
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

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Beyond the tags (C++ / expression children)
    </Typography>
    {CPP_ONLY.map(([names, note]) => (
      <Box key={names} sx={{ mb: 1.25 }}>
        <Typography variant="body1">
          <code>{names}</code> — {note}
        </Typography>
      </Box>
    ))}

    <Alert severity="info" sx={{ mt: 2 }}>
      Props, style keys and events on each tag are the exact Unreal names, so the authoritative
      reference for any tag is its Slate class. See <strong>Styling</strong> for the key
      vocabulary and <strong>Events &amp; Input</strong> for handlers. Per-widget reference pages
      generate from the compiler-exported schema (the same one the IDE extensions use).
    </Alert>
  </Box>
)
