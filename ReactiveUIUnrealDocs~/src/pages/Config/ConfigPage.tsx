import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const CONFIG = `// uetkx.config.json — walked up from a file; nearest one wins.
{
  "root": "Source",       // what "~/" resolves to for imports in this tree
  "printWidth": 100,       // formatter wrap width
  "indentStyle": "tab",   // "tab" | "space"
  "indentSize": 4
}`

const EDITOR = `// .vscode/settings.json — the extension sets these defaults for .uetkx.
"[uetkx]": {
  "editor.defaultFormatter": "ReactiveUITK.uetkx",
  "editor.formatOnSave": true,
  "editor.insertSpaces": false,
  "editor.detectIndentation": false
}`

const KEYS: Array<[string, string]> = [
  ['root', 'The directory the "~/" import alias anchors to (per Unreal module by default).'],
  ['printWidth', 'Column width the formatter wraps at.'],
  ['indentStyle', '"tab" or "space" — the family default is tabs.'],
  ['indentSize', 'Columns per indent level.'],
]

export const ConfigPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Configuration
    </Typography>
    <Typography variant="body1" paragraph>
      Two things are configurable: how <code>.uetkx</code> files resolve and format
      (<code>uetkx.config.json</code>), and how your editor treats them (the VS Code / VS2022
      extension settings). Neither affects the runtime — they shape authoring only.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      uetkx.config.json
    </Typography>
    <Typography variant="body1" paragraph>
      The compiler and formatter walk up from a file to the nearest{' '}
      <code>uetkx.config.json</code>. It fixes the import root alias and the canonical formatting.
    </Typography>
    <CodeBlock code={CONFIG} language="uetkx" />
    <TableContainer sx={{ my: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Key</TableCell>
            <TableCell>Purpose</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {KEYS.map(([key, purpose]) => (
            <TableRow key={key}>
              <TableCell>
                <code>{key}</code>
              </TableCell>
              <TableCell>{purpose}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Editor defaults
    </Typography>
    <Typography variant="body1" paragraph>
      Installing the extension registers <code>.uetkx</code> and sets sensible defaults: the
      canonical formatter, format-on-save, and tab indentation (the format is golden-corpus locked,
      so on-save keeps every file byte-consistent). The{' '}
      <code>UETKX: Restart Language Server</code> command reloads the analyzer if it ever gets stuck.
    </Typography>
    <CodeBlock code={EDITOR} language="uetkx" />

    <Alert severity="info">
      The formatter is one component per file and byte-for-byte matched to the C++ compiler&apos;s
      golden corpus — so formatting never changes meaning, only layout.
    </Alert>
  </Box>
)
