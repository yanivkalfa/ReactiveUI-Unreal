import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const SCREEN = `// URuiActivatableScreen is a UCommonActivatableWidget hosting a named component.
// Push it onto an existing activatable stack like any other screen — activation,
// input-mode switching, and back-handling stay CommonUI's job.
URuiActivatableScreen* Screen = CreateWidget<URuiActivatableScreen>(OwningPlayer, ScreenClass);
// (set ComponentName in the Blueprint subclass, or on the instance before push)
Stack->AddWidgetInstance(*Screen);   // UCommonActivatableWidgetContainerBase

// The screen re-renders its hosted tree when it (de)activates and when the
// input method changes (mouse/keyboard <-> gamepad <-> touch).`

const HOOKS = `// Inside the hosted component — react to what CommonUI decides.
export component PauseMenu {
	const bool bActive = RUI::CommonUI::UseIsActive();
	const ERuiInputMethod Input = RUI::CommonUI::UseInputMethod();

	return (
		<VerticalBox RenderOpacity={ bActive ? 1.0f : 0.4f }>
			<TextBlock Text={ Input == ERuiInputMethod::Gamepad
				? FText::FromString(TEXT("Press (A) to continue"))
				: FText::FromString(TEXT("Click to continue")) } />
		</VerticalBox>
	);
}`

const PROVIDER = `// Outside a real screen (tests, stand-ins, custom hosts) you can provide
// activation state yourself — the CommonUiDemo gallery screen does exactly this.
FRuiActivationState State;
State.bActive = true;
State.InputMethod = ERuiInputMethod::MouseAndKeyboard;
ProvideContext(RUI::CommonUI::ActivationContext(), State);`

export const CommonUiGuidePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      CommonUI Interop — their stacks, our screens
    </Typography>
    <Typography variant="body1" paragraph>
      CommonUI keeps owning what it already owns: menu stacks, input routing, back-handling, and
      the console-cert-hardened behavior teams rely on. ReactiveUI never installs its own input
      preprocessor and never rebuilds a stack — our screens live <em>inside</em> activatables and{' '}
      <em>read</em> the state CommonUI publishes.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Pushing a screen — <code>URuiActivatableScreen</code>
    </Typography>
    <CodeBlock code={SCREEN} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Reacting to activation &amp; input method
    </Typography>
    <Typography variant="body1" paragraph>
      The screen publishes an <code>FRuiActivationState</code> into its hosted tree; components
      read it with the CommonUI hooks. A live input-device switch (keyboard → gamepad) re-renders
      consumers automatically — glyph and prompt swaps are one conditional away.
    </Typography>
    <CodeBlock code={HOOKS} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Providing activation yourself
    </Typography>
    <CodeBlock code={PROVIDER} language="uetkx" />

    <Alert severity="info" sx={{ mt: 2 }}>
      Beta caveats: wrap plain Common widgets (e.g. <code>UCommonButtonBase</code>) via{' '}
      <code>RUI::Umg::UserWidget</code> as children, but host <em>activatables</em> only through{' '}
      <code>URuiActivatableScreen</code>. The screen does not yet override{' '}
      <code>GetDesiredFocusTarget()</code>, so designate initial gamepad focus yourself (e.g.{' '}
      <code>RUI::Slate::UseFocus</code> + an activation effect) — a first-class focus-target prop
      is tracked for v1.x.
    </Alert>
  </Box>
)
