import type { FC } from 'react'
import { Alert, Box, Chip, Stack, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'
import { VersionBadge } from '../../components/VersionBadge/VersionBadge'
import { ELEMENT_VERSIONS } from '../../versionManifest'
import { EVENT_PAYLOADS, HOST_ELEMENTS, SLOT_KEYS, STYLE_KEYS } from '../../hostSchema'

/** One generated reference page per host tag — data-driven from the compiler-exported schema
 *  (the same vocabulary the IDE extensions serve), so it can never drift from the tooling. */
export const ElementPage: FC<{ tag: string }> = ({ tag }) => {
  const el = HOST_ELEMENTS[tag] ?? {}
  const attrs = Object.entries(el.attrs ?? {})
  const events = attrs.filter(([, type]) => type === 'event')
  const props = attrs.filter(([, type]) => type !== 'event')
  const slateClass = `S${tag}` // D-33: tag = Slate class minus the S (SRuiCanvas keeps the Rui mark)

  const sample =
    `<${tag}` +
    (props[0] ? ` ${props[0][0]}={ ... }` : '') +
    (events[0] ? ` ${events[0][0]}={ ... }` : '') +
    (el.children ? `>...</${tag}>` : ' />')

  return (
    <Box>
      <Typography variant="h4" component="h1" gutterBottom>
        {tag}
        <VersionBadge feature={ELEMENT_VERSIONS[tag]} />
      </Typography>
      <Typography variant="body1" paragraph>
        The <code>{slateClass}</code> Slate widget as a markup tag (D-33: the tag is the class
        minus its <code>S</code>; every prop and event below is the exact Unreal name). Factory:{' '}
        <code>{el.factory ?? `RUI::Slate::${tag}`}</code>.{' '}
        {el.children ? 'Accepts children.' : 'A leaf widget — children are a compile error.'}
      </Typography>
      <CodeBlock code={sample} language="uetkx" />

      {props.length > 0 && (
        <>
          <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
            Props
          </Typography>
          <TableContainer sx={{ mb: 2 }}>
            <Table size="small">
              <TableHead>
                <TableRow>
                  <TableCell>Prop</TableCell>
                  <TableCell>Type</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {props.map(([name, type]) => (
                  <TableRow key={name}>
                    <TableCell>
                      <code>{name}</code>
                    </TableCell>
                    <TableCell>{type}</TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          </TableContainer>
        </>
      )}

      {events.length > 0 && (
        <>
          <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
            Events
          </Typography>
          <TableContainer sx={{ mb: 2 }}>
            <Table size="small">
              <TableHead>
                <TableRow>
                  <TableCell>Event</TableCell>
                  <TableCell>Payload (the handler&apos;s Value)</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {events.map(([name]) => (
                  <TableRow key={name}>
                    <TableCell>
                      <code>{name}</code>
                    </TableCell>
                    <TableCell>
                      {EVENT_PAYLOADS[name] === 'void' || !EVENT_PAYLOADS[name] ? (
                        'none'
                      ) : (
                        <code>
                          {EVENT_PAYLOADS[name] === 'text'
                            ? 'Value.TextValue'
                            : EVENT_PAYLOADS[name] === 'bool'
                              ? 'Value.BoolValue'
                              : EVENT_PAYLOADS[name] === 'float'
                                ? 'Value.FloatValue'
                                : EVENT_PAYLOADS[name]}
                        </code>
                      )}
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          </TableContainer>
        </>
      )}

      <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
        Generic style &amp; slot keys
      </Typography>
      <Typography variant="body2" paragraph color="text.secondary">
        Every element also accepts the universal reserved props — <code>key</code> (reconciler
        identity), <code>classes</code> (style classes), <code>Ref=&#123; expr &#125;</code>{' '}
        (host-handle capture) — plus the generic style keys and, under a panel, the parent-slot
        keys (see the Styling guide for values and the cascade):
      </Typography>
      <Stack direction="row" flexWrap="wrap" gap={0.75} sx={{ mb: 1 }}>
        {STYLE_KEYS.map((key) => (
          <Chip key={key} label={key} size="small" variant="outlined" sx={{ fontFamily: 'monospace' }} />
        ))}
      </Stack>
      <Stack direction="row" flexWrap="wrap" gap={0.75} sx={{ mb: 2 }}>
        {SLOT_KEYS.map((key) => (
          <Chip key={key} label={key} size="small" color="info" variant="outlined" sx={{ fontFamily: 'monospace' }} />
        ))}
      </Stack>

      <Alert severity="info">
        This page is generated from the compiler-exported schema — the same vocabulary the VS
        Code / VS2022 extensions complete against — refreshed by the <code>RUIExportSchema</code>{' '}
        commandlet.
      </Alert>
    </Box>
  )
}
