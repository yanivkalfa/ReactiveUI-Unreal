import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const KEYS = `@for (int32 i = 0; i < Current.Num(); ++i) {
	<HorizontalBox key={ FName(*Current[i]) }>
		<TextBlock Text={ FText::FromString(Current[i]) } />
	</HorizontalBox>
}`

export const KeysGuidePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Keys Guide
    </Typography>
    <Typography variant="body1" paragraph>
      When you render a dynamic list, the reconciler needs to know which element of the new frame
      corresponds to which element of the last — so it can move a row instead of destroying and
      rebuilding it. The <code>key</code> prop is that identity.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Give every list item a stable key
    </Typography>
    <Typography variant="body1" paragraph>
      Inside a <code>@for</code>, give each element a <code>key</code> that is stable and unique for
      the item it represents — an <code>FName</code> built from the item&apos;s id, not its index.
      With stable keys, reordering the collection moves the existing widgets (and their component
      state, hooks and refs) rather than resetting them.
    </Typography>
    <CodeBlock code={KEYS} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Why the index is a trap
    </Typography>
    <Typography variant="body1" paragraph>
      Keying by array index means &quot;position 0&quot; is always the same key even after you
      insert at the front — so the reconciler keeps the old widget in place and just changes its
      props, silently transplanting one item&apos;s state onto another. A stable per-item key avoids
      this; the Keyed Diff demo shows the difference side by side.
    </Typography>

    <Alert severity="info">
      Keys only need to be unique among siblings, not globally. Match them to the data&apos;s natural
      identity and reordering, insertion and removal all just work.
    </Alert>
  </Box>
)
