import type { FC } from 'react'
import { Alert, Box, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const USEFIELD = `#include "RuiFieldHooks.h"

// Read a FieldNotify field reactively — from the MvvmDemo gallery screen.
// Works on ANY INotifyFieldValueChanged UObject: Epic-MVVM viewmodels,
// UMVVMViewModelBase subclasses, or stock FieldNotify-enabled widgets.
export component HealthBar(Vm: UObject*) {
	const float Health = RUI::Umg::UseField<float>(Ctx, Vm, FName(TEXT("Health")), 100.0f);
	return (
		<ProgressBar Percent={ Health / 100.0f } />
	);
}
// Subscribes once, re-renders on broadcast, unsubscribes on unmount;
// a null/stale VM reads the caller's default.`

const REVERSE = `// The reverse direction: OUR state as a viewmodel THEIR views bind.
// URuiSignalViewModel implements INotifyFieldValueChanged with no
// MVVM-plugin dependency — write it from ReactiveUI, bind it from UMG.
URuiSignalViewModel* Vm = NewObject<URuiSignalViewModel>();
Vm->SetInt(Score);            // broadcasts on change, skips on equal

// With the MVVM plugin enabled, register into the global collection so
// UMG views resolve it by context name:
RUI::Mvvm::RegisterGlobalViewModel(GameInstance, TEXT("PlayerStats"), Vm);`

const OWNED = `// Inside a component — create, own, and read a viewmodel in two lines:
URuiSignalViewModel* Vm = RUI::Umg::UseOwnedViewModel<URuiSignalViewModel>(Ctx);
const int32 Score = RUI::Umg::UseField<int32>(Ctx, Vm, "Int", 0);

// Write it from events; anything bound to it (UMG views included) follows:
<Button OnClicked={ Vm->SetInt(Score + 1) }>+1</Button>`

export const MvvmGuidePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      MVVM Interop — they own values, we own structure
    </Typography>
    <Typography variant="body1" paragraph>
      ReactiveUI targets the engine-level <strong>FieldNotification</strong> module directly — no
      dependency on the ModelViewViewModel plugin — so any FieldNotify source is a data source:
      Epic-MVVM viewmodels, custom <code>UMVVMViewModelBase</code> subclasses, or stock widgets.
      The viewmodels keep owning the values; ReactiveUI decides what UI exists because of them.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Their data feeding us — <code>UseField</code>
    </Typography>
    <CodeBlock code={USEFIELD} language="uetkx" />
    <Typography variant="body1" paragraph>
      Broadcasts coalesce to one re-render per frame — many field changes in a frame cost one
      reconcile, the same batching MVVM&apos;s Delayed execution mode gives hand-wired bindings.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Our state feeding them — two bridges
    </Typography>
    <Typography variant="body1" paragraph>
      <code>URuiSignalViewModel</code> (plugin-free) turns ReactiveUI state into a FieldNotify
      source UMG bindings consume. <code>URuiMvvmViewModel</code> +{' '}
      <code>RUI::Mvvm::RegisterGlobalViewModel</code> (in the optional{' '}
      <code>ReactiveUIMVVMBridge</code> module) additionally registers into the MVVM plugin&apos;s{' '}
      <strong>global viewmodel collection</strong>, so existing UMG views bind it by context name —
      no view-side changes at all.
    </Typography>
    <CodeBlock code={REVERSE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Owning a viewmodel — <code>UseOwnedViewModel</code>
    </Typography>
    <Typography variant="body1" paragraph>
      When the component itself should own the viewmodel,{' '}
      <code>RUI::Umg::UseOwnedViewModel&lt;T&gt;(Ctx)</code> creates it on first render, keeps it
      GC-rooted for the component&apos;s lifetime, and releases it on unmount — the{' '}
      <code>UseMemo</code> + <code>TStrongObjectPtr</code> pattern, packaged into one hook.
    </Typography>
    <CodeBlock code={OWNED} language="uetkx" />

    <Alert severity="info">
      Moving single values between Rui state and reflected <code>UPROPERTY</code>s by hand? The
      prop-map&apos;s conversion table is public: <code>RUI::Umg::MarshalToProperty</code> /{' '}
      <code>MarshalFromProperty</code> (bool, int32/64, float/double, FString, FText, FName —
      kind mismatches are skipped, never mangled).
    </Alert>
  </Box>
)
