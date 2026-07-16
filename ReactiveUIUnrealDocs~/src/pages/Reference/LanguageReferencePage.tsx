import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const PREAMBLE = `import "@MyTypes.h"                        // your own C++ header — library headers are auto-included
import { StatusChip } from "./StatusChip"  // cross-file declarations

export component Screen(Title: FText, Count: int32 = 0) {
	// ...component body...
}`

const FLOW = `<VerticalBox>
	@if (Items.Num() == 0) {
		return ( <TextBlock Text="Empty" /> );
	} @else {
		@for (int32 i = 0; i < Items.Num(); ++i) {
			const FItem& Item = Items[i];          // plain statements are allowed...
			return ( <Row key={ Item.Id } Label={ Item.Name } /> );   // ...markup exits via return
		}
	}
	@match (Tab) {
		@case (0) { return ( <Overview /> ); }
		@default  { return ( <Details /> ); }
	}
</VerticalBox>`

const EARLY_RETURNS = `export component Status(Count: int32 = 0) {
	if (Count < 0) {
		return ( <TextBlock Text="invalid" /> );   // guard clause — plain C++ if
	}
	return (
		<VerticalBox>
			<TextBlock Text={ FText::AsNumber(Count) } />
			{ Count > 10 && <TextBlock Text="high" /> }   // renders only when true
			{ Count > 10 || <Spacer /> }                  // renders only when false
		</VerticalBox>
	);
}`

const DIRECTIVES: Array<[string, string]> = [
  ['@if / @elif / @else', 'Conditional markup.'],
  ['@for', 'Loop — the header is verbatim C++ (classic or ranged for); give children a key.'],
  ['@while', 'Loop while a condition holds.'],
  ['@match / @case / @default', 'Multi-way branch on a value.'],
]

export const LanguageReferencePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Language Reference
    </Typography>
    <Typography variant="body1" paragraph>
      A <code>.uetkx</code> file compiles to committed C++. It is a preamble followed by a free
      sequence of <strong>component</strong>, <strong>hook</strong> and <strong>module</strong>{' '}
      declarations. This page is the syntax at a glance; the guides cover each area in depth.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Preamble &amp; declarations
    </Typography>
    <Typography variant="body1" paragraph>
      The preamble holds <code>import</code> statements only, before the first declaration — file
      imports (<code>./</code>, <code>~/</code>) are name-checked, and{' '}
      <code>import &quot;@Header.h&quot;</code> is a nameless host include for your own C++ headers
      (library headers are auto-included; see <strong>Imports &amp; exports</strong>). A raw{' '}
      <code>#include</code> line still works too. A component takes typed parameters with optional
      defaults and returns exactly one root element.
    </Typography>
    <CodeBlock code={PREAMBLE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Expressions &amp; fragments
    </Typography>
    <Typography variant="body1" paragraph>
      An attribute value is a plain string or a <code>{'{ expr }'}</code> C++ expression (a{' '}
      <code>{'{ ...Props }'}</code> spread parses but is post-v1 — UETKX3003). A{' '}
      <code>{'{ expr }'}</code> child inlines a value or element, and{' '}
      <code>{'{ children }'}</code> splices the children a
      caller passed into your component. Use a fragment <code>&lt;&gt;...&lt;/&gt;</code> to return
      several children without an extra wrapper widget — leaf widgets (e.g.{' '}
      <code>TextBlock</code>) reject children with a compile error. Comments: <code>//</code>,{' '}
      <code>/* */</code>, <code>{'<!-- -->'}</code> between nodes, and <code>{'{/* ... */}'}</code>{' '}
      inside attribute lists.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Control-flow directives
    </Typography>
    <Typography variant="body1" paragraph>
      A directive block is a statement block: declare locals, compute — and emit markup with{' '}
      <code>return ( ... );</code>. The directive header is verbatim C++ (<code>@for</code> takes a
      classic or ranged for-header).
    </Typography>
    <CodeBlock code={FLOW} language="uetkx" />
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Directive</TableCell>
            <TableCell>Meaning</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {DIRECTIVES.map(([d, meaning]) => (
            <TableRow key={d}>
              <TableCell>
                <code>{d}</code>
              </TableCell>
              <TableCell>{meaning}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Early returns &amp; short-circuit rendering
    </Typography>
    <Typography variant="body1" paragraph>
      A component body is verbatim C++ — its own control flow branches. Guard clauses exit early
      with a markup <code>return ( ... );</code> anywhere in the body; the <em>final</em> markup
      return must sit at the top level of the body (UETKX3007). Inside a{' '}
      <code>{'{ expr }'}</code> hole, <code>{'cond && <X/>'}</code> renders the element only when
      the condition holds, and <code>{'cond || <X/>'}</code> renders it only when it does not —
      sugar for the equivalent ternary.
    </Typography>
    <CodeBlock code={EARLY_RETURNS} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Structural attributes
    </Typography>
    <Typography variant="body1" paragraph>
      Any element accepts <code>key</code> (list identity), <code>Slot.*</code> (parent-slot layout),
      and event props (<code>On*</code>). Everything else is the widget&apos;s own Slate property.
    </Typography>

    <Alert severity="info">
      The primary exported component&apos;s name must match its file (UETKX0103), and a component
      must return exactly one root element (UETKX0108). See <strong>Diagnostics</strong>.
    </Alert>
  </Box>
)
