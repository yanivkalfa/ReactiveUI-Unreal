import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const LEAF = `// Stage 1 — in the UMG Designer: drop a "ReactiveUI Host" (URuiHostWidget)
// into your EXISTING UserWidget layout and set ComponentName to a registered
// component. Nothing else about your screen changes.
RUI_COMPONENT(InventoryPanel);   // compiled .uetkx components self-register by name`

const SCREEN = `// Stage 2 — whole screens. Your CommonUI stack stays; the screen it pushes
// is now ours (see the CommonUI guide):
Stack->AddWidgetInstance(*CreateWidget<URuiActivatableScreen>(OwningPlayer, ScreenClass));

// Or overlay screens with no UMG wrapper at all (Blueprint-callable):
World->GetSubsystem<URuiWorldSubsystem>()->MountNamed(TEXT("PauseMenu"), /*ZOrder*/ 10);`

const INVERT = `// Stage 3 — inversion: ReactiveUI owns the tree; what remains of the old UI
// lives INSIDE it, and your viewmodels keep feeding data unchanged.
{ RUI::Umg::UserWidget(ULegacyMinimapWidget::StaticClass(), World) }
const float Health = RUI::Umg::UseField<float>(Ctx, Vm, FName(TEXT("Health")), 100.0f);`

const CONVERT = `// Hand-written C++ component (verbose builder calls)...
FRuiNodeArray ScoreRow(FRuiContext& Ctx, const FRuiEmptyProps&, const TArray<FRuiNode>&)
{
	auto [Score, SetScore] = Ctx.UseState<int32>(0);
	FRuiButtonProps P;
	P.SetContentPadding(FMargin(12, 4));
	// ...pages of props structs and Ch.Add(...)...
}

// ...becomes a .uetkx file (same reconciler, same hooks — readable markup):
export component ScoreRow {
	auto [Score, SetScore] = UseState<int32>(0);
	return (
		<HorizontalBox>
			<TextBlock Text={ RUI::Fmt(TEXT("Score: {}"), Score) } />
			<Button OnClicked={ SetScore(Score + 10) } ContentPadding="12,4">+10</Button>
		</HorizontalBox>
	);
}`

export const MigrationPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Migration &amp; Adoption
    </Typography>
    <Typography variant="body1" paragraph>
      Nobody rewrites a shipping UI. ReactiveUI is adopted <strong>incrementally</strong> — it
      emits the widgets Epic&apos;s stack already governs, so every stage below ships on its own
      and nothing forces the next one.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Stage 1 — leaf islands (a panel inside your existing screens)
    </Typography>
    <Typography variant="body1" paragraph>
      Start with one self-contained panel — an inventory grid, a stat block, a settings group.
      Author it as a <code>.uetkx</code> component and drop a <code>URuiHostWidget</code> where the
      old panel sat. Your screen, your stack, your input handling — one reactive island inside it.
    </Typography>
    <CodeBlock code={LEAF} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Stage 2 — whole screens (their stacks, our screens)
    </Typography>
    <Typography variant="body1" paragraph>
      When a full screen is worth it, push a <code>URuiActivatableScreen</code> onto your existing
      CommonUI stack — activation, back-handling and input routing stay CommonUI&apos;s job — or
      mount overlays straight into the viewport with <code>MountNamed</code>.
    </Typography>
    <CodeBlock code={SCREEN} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Stage 3 — inversion (ours hosting theirs)
    </Typography>
    <Typography variant="body1" paragraph>
      Eventually the tree flips: ReactiveUI owns the screen, surviving UMG widgets embed via{' '}
      <code>RUI::Umg::UserWidget</code>, and your FieldNotify viewmodels keep feeding data through{' '}
      <code>UseField</code> — they own values, we own structure. No stage ever requires deleting
      working UI.
    </Typography>
    <CodeBlock code={INVERT} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Converting hand-written components to <code>.uetkx</code>
    </Typography>
    <Typography variant="body1" paragraph>
      If you started with C++ <code>RUI::</code> builder calls, moving to markup is mechanical —
      the reconciler and hooks are identical; only the authoring changes. The demo gallery&apos;s
      17 screens are all compiled <code>.uetkx</code> and double as conversion references.
    </Typography>
    <CodeBlock code={CONVERT} language="uetkx" />

    <Alert severity="info" sx={{ mt: 2 }}>
      Upgrading an existing <code>.uetkx</code> tree to strict imports is one command —{' '}
      <code>-run=RUIMigrateImports</code> — covered on the <strong>Imports &amp; exports</strong>{' '}
      page. Version-to-version API changes follow the deprecation policy in{' '}
      <code>VERSIONING.md</code> (one minor version of warnings before removal) and are always
      listed in the CHANGELOG.
    </Alert>
  </Box>
)
