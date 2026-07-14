import type { FC, ReactNode } from 'react'
import { Box, Typography } from '@mui/material'

const QA: Array<[string, ReactNode]> = [
  [
    'What is ReactiveUI for Unreal?',
    <>A React-style UI library for Unreal Engine 5.6+, in pure C++. You write function components with
      hooks; a fiber reconciler patches real Slate widgets. Its markup, <code>.uetkx</code>, compiles
      to committed C++.</>,
  ],
  [
    'Which Unreal versions are supported?',
    <><strong>UE 5.6 and up</strong> — the full automation battery runs green on 5.6, 5.7, and 5.8
      (5.6 is the floor; new engine versions go through a scripted catch-up process). It is a
      normal engine plugin — no GDExtension-style native dependency, no external runtime.</>,
  ],
  [
    'Is it production-ready?',
    <>It is in <strong>beta</strong> — built end to end and green under the headless automation
      battery, but pre-1.0: localization (FText gathering) is deferred and the docs are still
      building out. Expect minor API settling before v1.</>,
  ],
  [
    'Does it ship a scripting VM?',
    <>No. There is no JavaScript engine and no bridge. <code>.uetkx</code> lowers to reflection-free
      C++ that compiles like any other module.</>,
  ],
  [
    'Can I use my existing UMG widgets and MVVM view-models?',
    <>Yes. Embed a <code>UUserWidget</code> with <code>RUI::Umg::UserWidget</code>, read FieldNotify
      view-model fields with <code>UseField</code>, and host on CommonUI activatable stacks. Interop
      is a first-class design pillar.</>,
  ],
  [
    'Why are the event props named OnClicked and not onClick?',
    <>Naming is 1:1 loyal to Unreal. A tag is the Slate class minus its <code>S</code>; a prop, style
      key or event is the exact Unreal name. No React aliases.</>,
  ],
  [
    'Is there hot reload?',
    <>Yes, in development on Windows — it rides Unreal Live Coding, so a saved <code>.uetkx</code>{' '}
      updates the running UI with state preserved. See <strong>Hot Module Replacement</strong>.</>,
  ],
  [
    'What editor support exists?',
    <>VS Code and Visual Studio 2022 extensions: syntax highlighting, schema-driven completion and
      hover, import intelligence, diagnostics and canonical formatting — fully offline.</>,
  ],
]

export const FAQPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      FAQ
    </Typography>
    {QA.map(([q, a]) => (
      <Box key={q} sx={{ mb: 2.5 }}>
        <Typography variant="h6" component="h2" gutterBottom>
          {q}
        </Typography>
        <Typography variant="body1">{a}</Typography>
      </Box>
    ))}
  </Box>
)
