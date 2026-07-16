import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'

const BANDS: Array<[string, string]> = [
  ['UETKX01xx', 'Structural — name/file mismatch (0103), unknown element (0105), multiple roots (0108).'],
  ['UETKX03xx', 'Syntax — bad tag/attribute tokens (0300), unclosed tag (0301), mismatched close (0302), unexpected EOF (0303), unclosed brace/paren/comment (0304), unknown @directive (0305).'],
  ['UETKX21xx', 'Declarations — component not PascalCase (2100), no declaration or no markup return (2101), duplicate export collision (2106).'],
  ['UETKX22xx', 'Hooks & modules — missing hook name (2200) / params (2201) / body (2202), Use* naming (2203), missing module name (2204) / body (2205).'],
  ['UETKX23xx', 'Imports & exports — resolution and privacy (2300–2309); a redundant auto-included host include (2317, hint). See the Imports guide. (2316 reserved, post-v1.)'],
  ['UETKX25xx', 'Directives — malformed @if/@for/@match shape (2506–2508).'],
  ['UETKX30xx', 'Codegen — compiler-stage errors, e.g. template hook declarations are unsupported (3006), the final markup return must be top-level (3007).'],
]

const EXAMPLES: Array<[string, string, string]> = [
  ['UETKX0103', 'warn', 'component `Foo` differs from file name `Bar`'],
  ['UETKX0108', 'err', 'a component must return exactly one root element'],
  ['UETKX0302', 'err', 'mismatched tag </X> (expected </Y>)'],
  ['UETKX2100', 'err', 'component name must be PascalCase'],
  ['UETKX2101', 'err', 'no `component`, `hook`, or `module` declaration found'],
  ['UETKX2203', 'warn', 'hook name should start with `Use`'],
  ['UETKX3006', 'err', 'template `hook` declarations are not supported — use a C++ template in a `module`'],
  ['UETKX3007', 'err', "the component's final markup `return ( ... )` must be at the top level of the body"],
  ['UETKX2317', 'hint', '`X.h` is auto-included by the generated prelude — this line is redundant'],
]

export const DiagnosticsPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Diagnostics
    </Typography>
    <Typography variant="body1" paragraph>
      Every problem the compiler or language server reports carries a stable <code>UETKX</code> code.
      The same catalog powers both the C++ compiler (at build time) and the editor extensions (as you
      type), so a code means the same thing everywhere. Machine-readable diagnostics are written to a
      gitignored <code>*.uetkx.diags.json</code> sidecar and surfaced in the ReactiveUI message log.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Code bands
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Band</TableCell>
            <TableCell>Category</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {BANDS.map(([band, cat]) => (
            <TableRow key={band}>
              <TableCell>
                <code>{band}</code>
              </TableCell>
              <TableCell>{cat}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Representative codes
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Code</TableCell>
            <TableCell>Severity</TableCell>
            <TableCell>Message</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {EXAMPLES.map(([code, sev, msg]) => (
            <TableRow key={code}>
              <TableCell>
                <code>{code}</code>
              </TableCell>
              <TableCell>{sev}</TableCell>
              <TableCell>{msg}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      Import diagnostics (UETKX23xx) have their own walkthrough on the <strong>Imports &amp;
      exports</strong> page, including the fix-it for the duplicate-export collision (UETKX2106).
    </Alert>
  </Box>
)
