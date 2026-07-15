import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const EMITTED = `// You write (HelloWorld.uetkx):
<TextBlock Text="Hello, world!" />

// The compiler emits (HelloWorld.uetkx.inl) — a real, gatherable NSLOCTEXT,
// self-namespaced per file so aggregated .inl files never collide:
RUI::TextBlock(NSLOCTEXT("Uetkx.HelloWorld", "HelloWorld_0", "Hello, world!"), ...)`

const GATHER = `; Config/Localization/MyGame_Gather.ini — the stock source-code gather step.
; THE ONE NON-DEFAULT LINE: *.inl — markup text lives in the committed
; *.uetkx.inl files, and the gatherer only scans what the masks name.
[GatherTextStep0]
CommandletClass=GatherTextFromSource
SearchDirectoryPaths=%LOCPROJECTROOT%Source/
FileNameFilters=*.cpp
FileNameFilters=*.h
FileNameFilters=*.inl

; ...then the usual steps: GenerateGatherManifest, GenerateGatherArchive,
; GenerateTextLocalizationResource — or drive it all from the Localization Dashboard.`

const RUN = `:: Gather (repeat after markup changes; the dashboard's Gather button runs the same thing)
<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=GatherText ^
  -config=Config/Localization/MyGame_Gather.ini -unattended -nopause -nullrhi`

export const LocalizationPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Localization
    </Typography>
    <Typography variant="body1" paragraph>
      Markup text is localizable through Unreal&apos;s normal pipeline — no custom gatherer, no
      extra tooling. The <code>.uetkx</code> compiler emits every string literal as a real{' '}
      <code>NSLOCTEXT</code>, so the stock <strong>Localization Dashboard</strong> workflow
      (gather → translate → compile) sees markup text the same way it sees hand-written C++.
    </Typography>
    <CodeBlock code={EMITTED} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      One config line: gather from <code>*.inl</code>
    </Typography>
    <Typography variant="body1" paragraph>
      Generated markup code lives in committed <code>*.uetkx.inl</code> files, and Unreal&apos;s
      source gatherer only scans files its masks name — add <code>*.inl</code> to your
      localization target&apos;s <code>FileNameFilters</code>:
    </Typography>
    <CodeBlock code={GATHER} language="uetkx" />
    <CodeBlock code={RUN} language="bash" />
    <Typography variant="body1" paragraph>
      The demo project ships a complete working target —{' '}
      <code>Config/Localization/RuiDemo_Gather.ini</code> with its gathered output under{' '}
      <code>Content/Localization/RuiDemo/</code> — and the automation battery tripwires on it
      (<code>ReactiveUI.Loc.GatherManifest</code>): if markup text ever stops gathering, a test
      goes red.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Live culture switching
    </Typography>
    <Typography variant="body1" paragraph>
      <code>FText</code> is lazy, so an <code>NSLOCTEXT</code> literal or an{' '}
      <code>RUI::Fmt</code> result re-resolves on a culture switch by itself. On top of that, the
      plugin subscribes to the engine&apos;s text-revision event and <strong>re-renders every
      mounted root once per culture change</strong> — so even values a component baked during
      render (an <code>FString</code> conversion, a culture-dependent branch) heal immediately.
      Nothing to wire up: call{' '}
      <code>FInternationalization::Get().SetCurrentCulture(...)</code> and the UI follows.
    </Typography>

    <Alert severity="info">
      Keys are stable per file and position (<code>&lt;Component&gt;_&lt;n&gt;</code> inside{' '}
      <code>Uetkx.&lt;FileBasename&gt;</code>). Reordering markup can renumber keys — re-gather
      after structural edits, and rely on the archive&apos;s exact-match carry-over for unchanged
      source strings, exactly as with hand-written <code>LOCTEXT</code>.
    </Alert>
  </Box>
)
