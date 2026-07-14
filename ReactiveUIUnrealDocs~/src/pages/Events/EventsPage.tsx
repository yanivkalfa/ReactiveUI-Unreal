import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const BASIC = `<Button OnClicked={ Submit() }>Save</Button>

<EditableTextBox
	Text={ FText::FromString(Text) }
	HintText="Type here..."
	OnTextChanged={ SetText(Value.TextValue.ToString()) }
/>

<CheckBox bIsChecked={ bChecked } OnCheckStateChanged={ SetChecked(Value.BoolValue) } />`

const EVENTS: Array<[string, string]> = [
  ['OnClicked', 'Button — the click delegate; no payload fields.'],
  ['OnCheckStateChanged', 'CheckBox — the new checked state as Value.BoolValue.'],
  ['OnTextChanged / OnTextCommitted', 'EditableTextBox, SearchBox — per-keystroke / on-commit text as Value.TextValue.'],
  ['OnValueChanged', 'Slider, SpinBox — the new numeric value as Value.FloatValue.'],
]

export const EventsPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Events &amp; Input Handling
    </Typography>
    <Typography variant="body1" paragraph>
      There is one rule: an event prop is <code>On</code> + the exact Slate delegate name. If Slate
      calls it <code>OnClicked</code>, so does the markup — there is no <code>onClick</code> and no{' '}
      <code>onChange</code>. The value in braces is the C++ body that runs when the delegate fires.
    </Typography>
    <CodeBlock code={BASIC} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      The event payload
    </Typography>
    <Typography variant="body1" paragraph>
      Inside a handler body, <code>Value</code> is the event payload — an <code>FRuiValue</code>{' '}
      whose typed field carries what the delegate passed. A text change exposes{' '}
      <code>Value.TextValue</code> (an <code>FText</code>), a checkbox toggle{' '}
      <code>Value.BoolValue</code>, a slider change <code>Value.FloatValue</code>;{' '}
      <code>OnClicked</code> carries nothing. You read the field you need and call a setter,
      exactly like the demos.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Common events
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Prop</TableCell>
            <TableCell>Widget &amp; payload</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {EVENTS.map(([prop, detail]) => (
            <TableRow key={prop}>
              <TableCell>
                <code>{prop}</code>
              </TableCell>
              <TableCell>{detail}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      Because event props reset between renders (unlike plain props), removing a handler genuinely
      disconnects it — no stale delegate is left bound to the widget.
    </Alert>
  </Box>
)
