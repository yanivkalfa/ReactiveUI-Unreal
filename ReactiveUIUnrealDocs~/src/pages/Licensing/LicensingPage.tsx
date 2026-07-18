import type { FC, ReactNode } from 'react'
import {
  Box,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography,
} from '@mui/material'

const QA: Array<[string, ReactNode]> = [
  [
    'I downloaded an older version — do the new terms apply to me?',
    <>No. Every copy keeps the terms it shipped with. The new license applies from{' '}
      <strong>0.13.0</strong> (Unreal) onward — and from the matching releases of the Godot and
      Unity libraries. (Old versions also stop receiving updates and support, so staying current
      is worth it anyway.)</>,
  ],
  [
    "We're a contractor building a game for a client — whose revenue counts?",
    <>The entity that ships the product. If your client publishes the game, their revenue decides
      the tier; put the license in their name.</>,
  ],
  [
    'We crossed $250k mid-project. Are we in trouble?',
    <>No — you have a 60-day window to pick up a license, and nothing you did before crossing
      needs back-payment. The threshold looks at the 12 months before you ship.</>,
  ],
  [
    'Does the free tier limit features?',
    <>No. Same library, same features, same updates. The only difference is the piece of paper.</>,
  ],
  [
    "We can't afford $2,000 but we're just over the threshold / we're a nonprofit / weird case?",
    <>Email us — <a href="mailto:yanivkalfa@gmail.com">yanivkalfa@gmail.com</a>. We&apos;d rather
      you ship with ReactiveUI than not.</>,
  ],
]

export const LicensingPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Licensing
    </Typography>

    <Typography variant="body1" sx={{ mb: 2 }}>
      <strong>ReactiveUI is free for almost everyone.</strong> If you&apos;re a student, a
      hobbyist, a jam team, or an indie studio earning under <strong>$250,000 a year</strong> —
      everything on this page reduces to: <em>use it, ship your game, credit &quot;Made with
      ReactiveUI&quot;, pay nothing.</em>
    </Typography>

    <Typography variant="h6" component="h2" gutterBottom>
      Am I free?
    </Typography>
    <Typography variant="body1" sx={{ mb: 1 }}>
      <strong>Learning, prototyping, game jams, unreleased projects</strong> — free, always, for
      everyone. Even a AAA studio evaluates and develops free; payment only ever applies to{' '}
      <em>shipping</em>.
    </Typography>
    <Typography variant="body1" sx={{ mb: 1 }}>
      <strong>Your company (plus its parents/subsidiaries) earned under $250k in the last 12
      months</strong> — free to ship, commercially, forever. No registration required, no strings
      beyond the credits line.
    </Typography>
    <Typography variant="body1" sx={{ mb: 2.5 }}>
      <strong>Over $250k and shipping a product</strong> — you need a commercial license.
      That&apos;s the whole rule.
    </Typography>

    <Typography variant="h6" component="h2" gutterBottom>
      The commercial license
    </Typography>
    <Typography variant="body1" sx={{ mb: 1 }}>
      The same license, same threshold, and same prices exist for each library — Godot, Unity,
      and Unreal — each licensed per product. Pick whichever shape suits you:
    </Typography>

    <TableContainer sx={{ mb: 2, maxWidth: 640 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell />
            <TableCell>Per-Title</TableCell>
            <TableCell>Studio</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          <TableRow>
            <TableCell>Price</TableCell>
            <TableCell><strong>$2,000</strong> one-time</TableCell>
            <TableCell><strong>$2,500</strong> / year</TableCell>
          </TableRow>
          <TableRow>
            <TableCell>Covers</TableCell>
            <TableCell>one game, forever (patches, DLC, ports included)</TableCell>
            <TableCell>everything you ship while subscribed</TableCell>
          </TableRow>
          <TableRow>
            <TableCell>Best for</TableCell>
            <TableCell>a studio shipping occasionally</TableCell>
            <TableCell>a studio always shipping</TableCell>
          </TableRow>
          <TableRow>
            <TableCell>After it ends</TableCell>
            <TableCell>n/a — perpetual</TableCell>
            <TableCell>shipped games stay licensed forever</TableCell>
          </TableRow>
        </TableBody>
      </Table>
    </TableContainer>

    <Typography variant="body1" sx={{ mb: 2.5 }}>
      To purchase, email <a href="mailto:yanivkalfa@gmail.com">yanivkalfa@gmail.com</a> — you get
      a license certificate PDF, the document your producer files for publisher and platform
      paperwork. The full texts live in the repository:{' '}
      <a
        href="https://github.com/yanivkalfa/ReactiveUI-Unreal/blob/master/LICENSE"
        target="_blank"
        rel="noreferrer"
      >
        ReactiveUI Community License
      </a>{' '}
      and{' '}
      <a
        href="https://github.com/yanivkalfa/ReactiveUI-Unreal/blob/master/LICENSE-COMMERCIAL.md"
        target="_blank"
        rel="noreferrer"
      >
        Commercial License Agreement
      </a>
      .
    </Typography>

    <Typography variant="h6" component="h2" gutterBottom>
      The two asks we make of everyone
    </Typography>
    <Typography variant="body1" sx={{ mb: 1 }}>
      1. <strong>Credits.</strong> Put &quot;Made with ReactiveUI&quot; in your game&apos;s
      credits, wherever you credit other middleware. That line is how the project grows.
    </Typography>
    <Typography variant="body1" sx={{ mb: 2.5 }}>
      2. <strong>Don&apos;t resell the library.</strong> You can ship anything you build{' '}
      <em>with</em> ReactiveUI; you can&apos;t repackage ReactiveUI itself as a competing UI
      framework. (Your game is never a &quot;competing product&quot; — this clause exists purely
      so nobody takes the source and sells it out from under the project.)
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom>
      Common questions
    </Typography>
    {QA.map(([q, a]) => (
      <Box key={q} sx={{ mb: 2.5 }}>
        <Typography variant="h6" component="h2" gutterBottom>
          {q}
        </Typography>
        <Typography variant="body1">{a}</Typography>
      </Box>
    ))}

    <Typography variant="body2" sx={{ mt: 2, fontStyle: 'italic' }}>
      The legally binding texts are the ReactiveUI Community License (the repo&apos;s{' '}
      <code>LICENSE</code>) and the Commercial License Agreement (<code>LICENSE-COMMERCIAL.md</code>).
      This page summarizes them; where they differ, the texts win.
    </Typography>
  </Box>
)
