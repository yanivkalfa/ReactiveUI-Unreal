import type { FC } from 'react'
import { Alert, Box, Chip, Stack, Typography } from '@mui/material'

type Group = { title: string; note: string; tags: string[] }

// TAG_GROUPS chips: 63 tags — every schema tag appears in exactly one group (docs-drift-gated).
const TAG_GROUPS: Group[] = [
  {
    title: 'Layout & panels',
    note: 'Arrange children — each is a Slate panel.',
    tags: [
      'VerticalBox', 'HorizontalBox', 'Overlay', 'Canvas', 'ConstraintCanvas', 'Splitter',
      'Splitter2x2', 'WrapBox', 'UniformGridPanel', 'GridPanel', 'UniformWrapPanel', 'RadialBox',
      'ScrollBox', 'WidgetSwitcher', 'Box', 'Border', 'ScaleBox', 'SafeZone', 'DPIScaler',
      'Spacer', 'Separator', 'LinkedBox', 'EnableBox', 'ScissorRectBox', 'InvalidationPanel',
      'WindowTitleBarArea',
    ],
  },
  {
    title: 'Display',
    note: 'Show text, images, colors and progress.',
    tags: [
      'TextBlock', 'RichTextBlock', 'Image', 'LayeredImage', 'ProgressBar', 'Throbber',
      'TextScroller', 'ColorBlock', 'SimpleGradient', 'ComplexGradient', 'BackgroundBlur',
    ],
  },
  {
    title: 'Input & controls',
    note: 'Interactive widgets that raise On* events.',
    tags: [
      'Button', 'Hyperlink', 'CheckBox', 'Slider', 'SpinBox', 'EditableTextBox',
      'MultiLineEditableTextBox', 'EditableText', 'InlineEditableTextBlock', 'SearchBox',
      'NumericDropDown', 'SearchableComboBox', 'InputKeySelector', 'VolumeControl',
      'VectorInputBox', 'RotatorInputBox', 'VirtualJoystick', 'VirtualKeyboardEntry',
      'ExpandableButton',
    ],
  },
  {
    title: 'Color pickers',
    note: 'Controlled color editing (capture begin/end + OnValueChanged).',
    tags: ['ColorWheel', 'ColorSpectrum', 'ColorGradingWheel'],
  },
  {
    title: 'Popups, toasts & navigation',
    note: 'The protocol widgets — anchored popups, notifications, breadcrumbs.',
    tags: ['MenuAnchor', 'NotificationList', 'BreadcrumbTrail'],
  },
  {
    title: 'Custom (the Rui mark)',
    note: 'Our own widgets carry the Rui prefix.',
    tags: ['RuiCanvas'],
  },
]

const CPP_ONLY: Array<[string, string]> = [
  ['ListView / TileView', 'virtualized item-model views — a render-prop API (see Beyond the tags below)'],
  [
    'TreeView',
    'the hierarchical item-model view: Items + RenderItem + a GetChildren accessor (RUI::Slate::MakeChildAccessor), CONTROLLED expansion (ExpandedItems in, OnExpansionChanged out), optional header Columns (construct-only). C++-first like ListView.',
  ],
  [
    'RUI::Slate::PushNotification',
    'the toast command — RUI::Slate::PushNotification(Handle, Text, Duration) fires a toast at a mounted <NotificationList> (capture its handle via Ref).',
  ],
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
      Two vocabularies compose the tree. <strong>Markup tags</strong> (63 of them) map 1:1 to Slate
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
