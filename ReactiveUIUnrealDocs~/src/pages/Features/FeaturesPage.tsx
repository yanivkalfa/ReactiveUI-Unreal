import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const ROUTER = `#include "RuiRouter.h"

// A router owns an in-memory history; consumers read location and navigate.
FRuiNode App(FRuiContext& Ctx) {
    return RUI::Router({}, {
        RUI::Routes({}, {
            RUI::Route("/", HomeRoute),
            RUI::Route("/users/:id", UserRoute),
        }),
    });
}

// Inside a route component: the family's 17 router hooks.
FRuiLocation Loc   = RUI::UseLocation(Ctx);
FRuiParams   P     = RUI::UseParams(Ctx);          // { "id": "42" }
auto         Nav   = RUI::UseNavigate(Ctx);        // Nav("/users/7")
auto         Search = RUI::UseSearchParams(Ctx);   // ?q=... read/write`

const STYLESHEET = `// theme.uss — @theme tokens + $token refs; the cascade is theme < classes < inline.
@theme dark {
    $bg: #10131a;
    $accent: #4f8cff;
}
.panel { RenderOpacity: 1; }

// C++: register + activate, then reference tokens from style dicts.
RUI::Slate::LoadStylesheet(Source);
RUI::Slate::SetActiveTheme("dark");`

const LISTVIEW = `#include "RuiListView.h"

// Virtualized: only visible rows have widgets. Each generated row is its own
// FRuiRoot sub-root rendering RenderItem(item, index).
FRuiListViewProps P;
P.SetItems(Items);                                  // stable TSharedPtr<FRuiValue> array
P.SetRenderItem(RUI::Slate::MakeItemRenderer(
    [](const FRuiValue& V, int32) { return RUI::TextBlock(V.StringValue); }));
P.SetSelectedIndex(0);
return RUI::Slate::ListView(MoveTemp(P));            // or TileView(...)`

const DND = `#include "RuiDragDrop.h"

// A draggable carrying a typed payload; a drop zone filtering by type.
RUI::Slate::DragSource(SourceProps /* DragType, Payload */, { Card });
RUI::Slate::DropTarget(TargetProps /* AcceptTypes, OnDrop */, { Slot });`

const PRESENCE = `#include "RuiPresence.h"

// Children removed from the tree stay mounted until they signal done (or time out).
RUI::Presence({ /* keyed children */ });
FRuiPresenceState S = RUI::UsePresence(Ctx);         // { bPresent, NotifyDone }`

const COMMONUI = `#include "RuiActivatableScreen.h"   // our tree as a CommonUI screen

// Inside the hosted component: react to activation + input method.
bool             bActive = RUI::CommonUI::UseIsActive(Ctx);
ERuiInputMethod  Method  = RUI::CommonUI::UseInputMethod(Ctx);

// MVVM: register a viewmodel globally; UMG views bind it by context name.
RUI::Mvvm::RegisterGlobalViewModel(GameInstance, "PlayerStats", ViewModel);`

const WIDGETS: Array<[string, string]> = [
  ['WidgetSwitcher, ScaleBox, Throbber, WrapBox', 'the batch-2 everyday set'],
  ['MultiLineEditableTextBox, SearchBox', 'multi-line + search text inputs'],
  ['SafeZone, DPIScaler, Separator', 'layout + platform helpers'],
  ['SpinBox, UniformWrapPanel, RichTextBlock', 'numeric drag input, uniform wrap, rich text'],
  ['GridPanel, UniformGridPanel', 'slot.column / slot.row placement'],
  ['ExpandableArea', 'a two-named-slot (header / body) collapsible section'],
  ['SegmentedControl', 'a labelled tab-bar selector'],
  ['NumericEntryBox', 'a controlled numeric field'],
  ['ComboBox', 'a dropdown reusing the ListView render-prop'],
  ['SuggestionTextBox', 'autocomplete with a substring suggestion filter'],
  ['ListView, TileView', 'virtualized item-model views (per-row sub-roots)'],
]

export const FeaturesPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Post-v1 subsystems
    </Typography>
    <Typography variant="body1" paragraph>
      Beyond the core reconciler + Slate host, the Unreal sibling ships the family&apos;s full
      post-v1 surface: a router, stylesheets, exit animations, drag-and-drop, virtualized lists,
      first-class CommonUI/MVVM citizenship, and ~20 additional widgets. Everything below is C++
      today (the <code>.uetkx</code> markup covers the declarative subset); each subsystem is
      covered by the headless automation battery.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Router
    </Typography>
    <Typography variant="body1" paragraph>
      An engine-blind port of the family router: <code>Router</code> / <code>Routes</code> /{' '}
      <code>Link</code> / <code>Outlet</code> over an in-memory history, plus the 17 router hooks
      (<code>UseLocation</code>, <code>UseNavigate</code>, <code>UseParams</code>,{' '}
      <code>UseSearchParams</code>, <code>UseMatch</code>, <code>UseBlocker</code>, …). Consumers
      re-render on location change only.
    </Typography>
    <CodeBlock code={ROUTER} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Stylesheets (<code>@theme</code> / <code>@uss</code>)
    </Typography>
    <Typography variant="body1" paragraph>
      A third style layer on top of inline <code>style</code> + <code>classes</code>:{' '}
      <code>@theme</code> tokens and <code>$token</code> references, loaded from a{' '}
      <code>@uss</code> stylesheet. The cascade is <strong>theme &lt; classes &lt; inline</strong>.
    </Typography>
    <CodeBlock code={STYLESHEET} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Virtualized lists (<code>ListView</code> / <code>TileView</code>)
    </Typography>
    <Typography variant="body1" paragraph>
      Only the rows in view have widgets, so a 100k-item list stays cheap. The API is a render
      prop: a stable item array + a <code>RenderItem</code> closure. Each generated row is its own
      reconciler sub-root; re-handing a fresh closure re-renders live rows in place with no churn.
    </Typography>
    <CodeBlock code={LISTVIEW} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Drag-and-drop &amp; exit animations
    </Typography>
    <Typography variant="body1" paragraph>
      Typed DnD over Slate&apos;s <code>FDragDropOperation</code> — a <code>DragSource</code>{' '}
      carries an <code>FRuiValue</code> payload + type tag, a <code>DropTarget</code> filters by
      accepted types. Exit animations use a <code>&lt;Presence&gt;</code> boundary: a removed child
      stays mounted until it signals done (or a timeout fires).
    </Typography>
    <CodeBlock code={DND} language="uetkx" />
    <CodeBlock code={PRESENCE} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      CommonUI &amp; MVVM citizenship
    </Typography>
    <Typography variant="body1" paragraph>
      <code>URuiActivatableScreen</code> pushes our tree onto a CommonUI activatable stack; the
      hosted component reacts with <code>UseActivation</code> / <code>UseInputMethod</code>. On the
      MVVM side, a viewmodel can be registered in the plugin&apos;s global collection, and a hosted{' '}
      <code>UUserWidget</code> receives Rui props declaratively through the reflection prop-map.
    </Typography>
    <CodeBlock code={COMMONUI} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Widgets added
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Widgets</TableCell>
            <TableCell>Notes</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {WIDGETS.map(([names, note]) => (
            <TableRow key={names}>
              <TableCell>
                <code>{names}</code>
              </TableCell>
              <TableCell>{note}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      The per-widget catalog (one page per host tag) is generated from the widget prop-map schema —
      landing that generation is the remaining docs-site step; this page is the hand-written
      overview until then.
    </Alert>
  </Box>
)
