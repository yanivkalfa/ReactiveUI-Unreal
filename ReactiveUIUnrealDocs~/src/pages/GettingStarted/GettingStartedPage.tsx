import type { FC } from 'react'
import { Alert, Box, Link, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'
import { GITHUB_URL } from '../../links'

const BUILD_CS = `// YourModule.Build.cs — depend on the runtime + the Slate host.
PublicDependencyModuleNames.AddRange(new[]
{
    "Core", "CoreUObject", "Engine", "Slate", "SlateCore",
    "ReactiveUICore",   // vnodes, fibers, hooks, the reconciler (no UObject)
    "ReactiveUISlate",  // the Slate host + per-widget adapters
});`

const HELLO = `// HelloWorld.uetkx — one component per file; the file name matches the component.
export component HelloWorld {
	auto [Count, SetCount] = UseState<int32>(0);
	return (
		<VerticalBox>
			<TextBlock Text={ RUI::Fmt(TEXT("Hello, ReactiveUI — {}"), Count) } />
			<Button OnClicked={ SetCount(Count + 1) }>+1</Button>
		</VerticalBox>
	);
}`

const MOUNT = `// Mount the root anywhere you have a UWorld — a GameMode, PlayerController,
// or GameInstance. FRuiRoot owns the reconciler and the Slate widget it drives.
#include "RuiRoot.h"

void AMyHUD::BeginPlay()
{
    Super::BeginPlay();
    Root = FRuiRoot::CreateInViewport(RUI::FC(&HelloWorld), /*ZOrder*/ 10);
}
// Root is a TSharedPtr<FRuiRoot> member — drop it to unmount.`

const COMPILE = `:: Compile every .uetkx to committed C++ (*.uetkx.inl + <Module>.Uetkx.gen.cpp),
:: then fail on any drift between markup and the generated code.
<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RUICompile -check`

export const GettingStartedPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Install &amp; Setup
    </Typography>
    <Typography variant="body1" paragraph>
      ReactiveUI for Unreal is a normal engine plugin — no GDExtension, no external runtime. You
      enable the plugin, depend on two modules, write a <code>.uetkx</code> component, and mount it
      onto a <code>UWorld</code>. It targets <strong>Unreal Engine 5.6+</strong>.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      1. Enable the plugin
    </Typography>
    <Typography variant="body1" paragraph>
      Drop <code>Plugins/ReactiveUI/</code> into your project (or install it from Fab) and enable{' '}
      <strong>ReactiveUI</strong> under <em>Edit ▸ Plugins</em>. Then add the runtime modules your
      game module builds against:
    </Typography>
    <CodeBlock code={BUILD_CS} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      2. Write a component
    </Typography>
    <Typography variant="body1" paragraph>
      A <code>.uetkx</code> file holds one component whose name matches the file. It returns a single
      markup tree; hooks such as <code>UseState</code> hold its state. Element tags are loyal to
      Slate — <code>VerticalBox</code> is <code>SVerticalBox</code>, <code>TextBlock</code> is{' '}
      <code>STextBlock</code> — and event props are the Slate delegate name (<code>OnClicked</code>).
    </Typography>
    <CodeBlock code={HELLO} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      3. Mount it
    </Typography>
    <Typography variant="body1" paragraph>
      <code>FRuiRoot</code> is the bridge between the reconciler and a real Slate widget.{' '}
      <code>CreateInViewport</code> adds it to the game viewport; keep the returned{' '}
      <code>TSharedPtr</code> alive for as long as the UI should exist.
    </Typography>
    <CodeBlock code={MOUNT} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      4. Compile the markup
    </Typography>
    <Typography variant="body1" paragraph>
      In development, editing a <code>.uetkx</code> hot-reloads live via Unreal Live Coding (see{' '}
      <strong>Hot Module Replacement</strong>). For shipping, the markup compiles to committed C++
      next to each file. The <code>RUICompile</code> commandlet regenerates it and{' '}
      <code>-check</code> gates any drift in CI:
    </Typography>
    <CodeBlock code={COMPILE} language="bash" />

    <Alert severity="info" sx={{ mt: 2 }}>
      The generated <code>*.uetkx.inl</code> and <code>&lt;Module&gt;.Uetkx.gen.cpp</code> are{' '}
      <strong>committed</strong> and reflection-free — they are the shipping artifact. The{' '}
      <code>*.uetkx.diags.json</code> sidecars are machine-local and gitignored.
    </Alert>

    <Typography variant="body1" paragraph sx={{ mt: 2 }}>
      From here, see <strong>Concepts &amp; Environment</strong> for the model, the{' '}
      <strong>Hooks Guide</strong> for state, and the full source at the{' '}
      <Link href={GITHUB_URL} target="_blank" rel="noreferrer">
        GitHub repository
      </Link>
      .
    </Typography>
  </Box>
)
