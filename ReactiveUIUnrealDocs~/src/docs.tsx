import type { ReactElement } from 'react'
import type { Page as LegacyPage } from './pages'
import { pages as legacySections } from './pages'
import { PAGE_VERSIONS, isAvailableIn, compareVersions } from './versionManifest'
import { IntroductionPage } from './pages/Introduction/IntroductionPage'
import { GettingStartedPage } from './pages/GettingStarted/GettingStartedPage'
import { ConceptsPage } from './pages/Concepts/ConceptsPage'
import { MigrationPage } from './pages/Migration/MigrationPage'
import { ImportsPage } from './pages/Uetkx/ImportsPage'
import { HooksGuidePage } from './pages/Hooks/HooksGuidePage'
import { StylingPage } from './pages/Styling/StylingPage'
import { DifferencesPage } from './pages/Differences/DifferencesPage'
import { EventsPage } from './pages/Events/EventsPage'
import { ContextPage } from './pages/Context/ContextPage'
import { RefsGuidePage } from './pages/Refs/RefsGuidePage'
import { KeysGuidePage } from './pages/Keys/KeysGuidePage'
import { CustomRenderingPage } from './pages/CustomRendering/CustomRenderingPage'
import { SignalsPage } from './pages/Signals/SignalsPage'
import { CompanionFilesPage } from './pages/CompanionFiles/CompanionFilesPage'
import { LanguageReferencePage } from './pages/Reference/LanguageReferencePage'
import { ComponentsOverviewPage } from './pages/ComponentsOverview/ComponentsOverviewPage'
import { ApiReferencePage } from './pages/API/ApiReferencePage'
import { DiagnosticsPage } from './pages/Diagnostics/DiagnosticsPage'
import { ConfigPage } from './pages/Config/ConfigPage'
import { AssetsPage } from './pages/Assets/AssetsPage'
import { LocalizationPage } from './pages/Localization/LocalizationPage'
import { RouterPage } from './pages/Router/RouterPage'
import { SuspensePage } from './pages/Suspense/SuspensePage'
import { DebuggingPage } from './pages/Debugging/DebuggingPage'
import { FAQPage } from './pages/FAQ/FAQPage'
import { KnownIssuesPage } from './pages/KnownIssues/KnownIssuesPage'
import { RoadmapPage } from './pages/Roadmap/RoadmapPage'
import { HmrPage } from './pages/Tooling/HMR/HmrPage'
import { PortalsPage } from './pages/Portals/PortalsPage'
import { InteropOverviewPage } from './pages/Interop/InteropOverviewPage'
import { UmgGuidePage } from './pages/Interop/UmgGuidePage'
import { CommonUiGuidePage } from './pages/Interop/CommonUiGuidePage'
import { MvvmGuidePage } from './pages/Interop/MvvmGuidePage'
import { FeaturesPage } from './pages/Features/FeaturesPage'

export type DocPage = {
  id: string
  canonicalId: string
  title: string
  path: string
  keywords?: string[]
  searchContent?: string
  group?: 'basic' | 'advanced'
  sinceUE?: string
  element: () => ReactElement
}

export type DocSection = {
  id: string
  title: string
  pages: DocPage[]
}

const componentPages = legacySections.find((section) => section.id === 'components')?.pages ?? []

export const sections: DocSection[] = [
  {
    id: 'intro',
    title: 'Introduction',
    pages: [
      {
        id: 'introduction',
        canonicalId: 'introduction',
        title: 'Introduction',
        path: '/',
        keywords: ['introduction', 'markup', 'unreal', 'slate', 'uetkx'],
        searchContent:
          'reactive ui for unreal react-style ui library unreal engine 5.6 pure c++ no javascript vm no bridge layer uetkx authoring language function-style components hooks state effects UseState UseEffect fiber reconciler decides which slate widgets exist each frame diffs patches only what changed widget reflector ordinary swidgets umg commonui mvvm stay in place feeding data hosting output .uetkx markup jsx-like compiles to native c++ shipping builds hot-reloads live in pie save mid-play under a second state preserved beta roadmap component counter UseState int32 VerticalBox TextBlock Button OnClicked RUI::Fmt SetCount',
        element: () => <IntroductionPage />,
      },
    ],
  },
  {
    id: 'getting-started',
    title: 'Getting Started',
    pages: [
      {
        id: 'getting-started',
        canonicalId: 'getting-started',
        title: 'Getting Started',
        path: '/getting-started',
        keywords: ['install', 'setup', 'plugin', 'build.cs', 'mount', 'FRuiRoot', 'RUICompile'],
        searchContent:
          'getting started install setup enable ReactiveUI plugin edit plugins Build.cs PublicDependencyModuleNames ReactiveUICore ReactiveUISlate Slate SlateCore write a .uetkx component one component per file file name matches component HelloWorld UseState VerticalBox TextBlock RUI::Fmt Button OnClicked SetCount mount FRuiRoot CreateInViewport RUI::FC ZOrder TSharedPtr UWorld GameMode PlayerController GameInstance HUD BeginPlay RuiRoot.h compile RUICompile commandlet -check drift gate committed uetkx.inl Uetkx.gen.cpp reflection-free diags.json sidecar gitignored unreal engine 5.6',
        element: () => <GettingStartedPage />,
      },
    ],
  },
  {
    id: 'guides',
    title: 'Guides',
    pages: [
      {
        id: 'concepts',
        canonicalId: 'concepts',
        title: 'Concepts & Environment',
        path: '/concepts',
        keywords: ['concepts', 'reconciler', 'components', 'hooks', 'modules', 'companion', 'cvars'],
        searchContent:
          'concepts environment react-style function components fiber reconciler diffs patches slate widgets pure c++ no javascript no uobject in core components hooks modules declaration kinds companion files .hooks.uetkx export hook UseCounter export module Styles synchronous rendering keyed reconciliation bailout effects after commit positional hooks call order unconditional top level IRuiHostConfig umg commonui mvvm rui.StrictMode rui.HookValidation rui.StrictDiagnostics rui.TimeSlicing rui.FrameBudgetMs rui.HostNodePool stat ReactiveUI console variables cvars',
        element: () => <ConceptsPage />,
      },
      {
        id: 'migration',
        canonicalId: 'migration',
        title: 'Migration & Adoption',
        path: '/guides/migration',
        keywords: ['migration', 'adoption', 'existing project', 'incremental', 'umg', 'convert'],
        searchContent:
          'migration adoption existing project incremental nobody rewrites shipping ui leaf islands panel URuiHostWidget ReactiveUI Host designer ComponentName registered component whole screens URuiActivatableScreen commonui stack AddWidgetInstance MountNamed URuiWorldSubsystem overlay inversion ReactiveUI owns tree RUI::Umg::UserWidget legacy widgets UseField viewmodels feed data convert hand-written c++ components RUI:: builder calls to .uetkx markup mechanical same reconciler hooks gallery reference RUIMigrateImports strict imports codemod deprecation policy VERSIONING changelog',
        element: () => <MigrationPage />,
      },
      {
        id: 'uetkx-imports',
        canonicalId: 'uetkx-imports',
        title: 'Imports & exports',
        path: '/uetkx/imports',
        keywords: ['import', 'export', 'module', 'strict', 'codemod', 'uetkx', 'privacy', 'root alias'],
        searchContent:
          'uetkx imports exports static import export module strict resolution codemod RUIMigrateImports ~/ root alias specifier relative extensionless named export privacy tree-shaken UETKX2106 2300 2301 2302 2303 2304 2305 2306 2307 2308 2309 duplicate export binding unknown specifier not exported not declared duplicate unused used not imported value cycle module boundary preamble uetkx.config.json root key',
        element: () => <ImportsPage />,
      },
      {
        id: 'companion-files',
        canonicalId: 'companion-files',
        title: 'Companion Files',
        path: '/guides/companion-files',
        keywords: ['companion', 'hooks', 'module', 'style', '.hooks.uetkx', '.style.uetkx'],
        searchContent:
          'companion files beside component same base name role suffix .uetkx extension Name.uetkx Name.hooks.uetkx custom Use hooks reusable state effect logic Name.style.uetkx module shared constants colors sizes brushes export hook UseCounter export module ContextDemoStyle static const FLinearColor compiles committed c++ convention not special kind export what other files need private',
        element: () => <CompanionFilesPage />,
      },
      {
        id: 'assets',
        canonicalId: 'assets',
        title: 'Assets',
        path: '/guides/assets',
        keywords: ['assets', 'brush', 'texture', 'font', 'image', 'MakeAssetBrush'],
        searchContent:
          'assets slate brushes fonts colors not raw paths FSlateBrush references UObject RUI::Umg::MakeAssetBrush ResourceObject ImageSize Tint UTexture2D material render target TObjectPtr soft reference asset manager Image Border Button background FSlateFontInfo Font keys FSlateColor FLinearColor ColorAndOpacity literal unreal names lifetime handled process-wide GC root FRuiAssetBrushRoot NumTrackedAssetBrushes no manual rooting',
        element: () => <AssetsPage />,
      },
      {
        id: 'localization',
        canonicalId: 'localization',
        title: 'Localization',
        path: '/guides/localization',
        keywords: ['localization', 'loc', 'FText', 'NSLOCTEXT', 'culture', 'gather', 'translation'],
        searchContent:
          'localization ftext gathering nsloctext uetkx string literals self-namespaced Uetkx file basename localization dashboard gather translate compile stock pipeline no custom gatherer *.inl FileNameFilters GatherTextFromSource file masks committed uetkx.inl demo target RuiDemo_Gather.ini manifest archive locres ReactiveUI.Loc.GatherManifest tripwire culture switching SetCurrentCulture text revision re-render mounted roots lazy ftext RUI::Fmt stable keys carry-over archive translation',
        element: () => <LocalizationPage />,
      },
      {
        id: 'hooks-guide',
        canonicalId: 'hooks-guide',
        title: 'Hooks Guide',
        path: '/guides/hooks',
        keywords: ['hooks', 'UseState', 'UseEffect', 'UseMemo', 'UseRef', 'UseReducer', 'UseContext'],
        searchContent:
          'hooks guide UseState UseReducer UseEffect UseLayoutEffect UseMemo UseCallback UseRef UseImperativeHandle UseContext ProvideContext UseStableCallback UseStableFunc UseStableAction UseDeferredValue UseTransition synchronous UseSignal UseSignalKey UseAnimate UseTween UseTweenValue UseSfx UseSafeArea UsePresence functional updater cleanup dependency list positional slot unconditional top level pascalcase custom hook composition .hooks.uetkx tuple',
        element: () => <HooksGuidePage />,
      },
      {
        id: 'styling',
        canonicalId: 'styling',
        title: 'Styling',
        path: '/styling',
        keywords: ['style', 'padding', 'slot', 'brush', 'color', 'FMargin', 'FLinearColor', 'FSlateColor'],
        searchContent:
          'styling three layers cascade theme tokens classes inline style keys exact unreal names element attrs Padding ContentPadding BorderImage BorderBackgroundColor WidthOverride generic style keys RenderOpacity Visibility Enabled RenderTranslation RenderScale RenderTransformAngle RenderTransformPivot ColorAndOpacity Font.Size Justification AutoWrapText FillColorAndOpacity slot keys Slot.Padding Slot.HAlign Slot.VAlign Slot.Fill Slot.ZOrder fill left center right style classes RegisterStyleClass classes attribute merge left-to-right inline wins themes @theme $token LoadStylesheet SetActiveTheme uss stylesheet RUI::Style RUI::Slot fluent builders setter styling never rebuilds reset on removal FMargin FLinearColor FSlateColor',
        element: () => <StylingPage />,
      },
      {
        id: 'differences',
        canonicalId: 'differences',
        title: 'Different from React',
        path: '/differences',
        keywords: ['react', 'differences', 'events', 'onclicked', 'slate', 'synchronous', 'naming'],
        searchContent:
          'different from react mental model pure c++ compiled to slate no javascript vm no bridge no reflection retained slate widgets not virtual dom hooks pascalcase UseState events unreal delegate name OnClicked OnCheckStateChanged OnValueChanged never onclick onchange elements slate class minus S VerticalBox TextBlock Slider Rui mark custom widgets UseTransition synchronous structural error boundaries removed plain props do not reset style events refs draw reset deliberate semantic divergences react ref lifecycle subscribe-in-effect signals registry fname component identity',
        element: () => <DifferencesPage />,
      },
      {
        id: 'events',
        canonicalId: 'events',
        title: 'Events & Input',
        path: '/guides/events',
        keywords: ['events', 'input', 'OnClicked', 'OnTextChanged', 'OnCheckStateChanged', 'Value', 'delegate'],
        searchContent:
          'events input handling On plus slate delegate name OnClicked OnCheckStateChanged OnTextChanged OnTextCommitted OnValueChanged no onclick no onchange event payload FRuiValue Value.TextValue Value.BoolValue Value.FloatValue bIsChecked Button EditableTextBox SearchBox CheckBox Slider SpinBox handler body c++ setter event props reset between renders disconnect stale delegate',
        element: () => <EventsPage />,
      },
      {
        id: 'context',
        canonicalId: 'context',
        title: 'Context API',
        path: '/guides/context',
        keywords: ['context', 'ProvideContext', 'UseContext', 'provider', 'consumer', 'theme'],
        searchContent:
          'context api ProvideContext statement UseContext consume nearest provided value subtree descendant re-render on change context key stable handle type shared module nested provider shadowing override theme locale current user vs signals global state scoped subtree dependency injection',
        element: () => <ContextPage />,
      },
      {
        id: 'refs-guide',
        canonicalId: 'refs-guide',
        title: 'Refs Guide',
        path: '/guides/refs',
        keywords: ['ref', 'UseRef', 'Current', 'mutable', 'UseImperativeHandle'],
        searchContent:
          'refs guide UseRef mutable box Current survives renders never triggers re-render escape hatch storage subscription handle previous value animation cursor timer handle not rendered vs UseState UseImperativeHandle expose imperative api parent focus scroll play ref lifecycle react model attach on mount detach on unmount',
        element: () => <RefsGuidePage />,
      },
      {
        id: 'keys-guide',
        canonicalId: 'keys-guide',
        title: 'Keys Guide',
        path: '/guides/keys',
        keywords: ['key', 'list', 'reconciler', 'reorder', 'identity', 'FName', '@for'],
        searchContent:
          'keys guide key prop dynamic list reconciler identity which element corresponds move row instead of destroy rebuild @for FName stable unique per item not index reorder moves widgets component state hooks refs preserved index trap position 0 same key insert front transplant state keyed diff demo unique among siblings natural identity insertion removal',
        element: () => <KeysGuidePage />,
      },
      {
        id: 'custom-rendering',
        canonicalId: 'custom-rendering',
        title: 'Custom Rendering',
        path: '/guides/custom-rendering',
        keywords: ['custom rendering', 'RuiCanvas', 'DrawFn', 'MakeDrawFn', 'RedrawKey', 'paint'],
        searchContent:
          'custom rendering RuiCanvas slate paint surface draw function RUI::Slate::MakeDrawFn FGeometry FSlateWindowElementList int32 layer FRuiDrawFn UseMemo stable identity RUI::Deps repaint when draw fn identity changes DrawFn CanvasSize RedrawKey repaint on demand animation external data scatter plot MakeLines MakeBox custom brushes charts gauges gizmos declarative when to repaint slate drawing',
        element: () => <CustomRenderingPage />,
      },
    ],
  },
  {
    id: 'tooling',
    title: 'Tooling',
    pages: [
      {
        id: 'signals',
        canonicalId: 'signals',
        title: 'Signals',
        path: '/tooling/signals',
        keywords: ['signals', 'TRuiSignal', 'GetOrCreateSignal', 'UseSignalKey', 'shared state'],
        searchContent:
          'signals TRuiSignal reactive value store outside component tree single source of truth shared state many distant components subscribe re-render RUI::GetOrCreateSignal key initial one instance registered Set Update functional update freshest value RUI::UseSignalKey Ctx key default subscribe current value schedule re-render sibling panels lockstep app-wide state outlives component vs context subtree vs UseState own state',
        element: () => <SignalsPage />,
      },
      {
        id: 'router',
        canonicalId: 'router',
        title: 'Router',
        path: '/tooling/router',
        keywords: ['router', 'routes', 'route', 'outlet', 'navlink', 'navigation', 'UseNavigate'],
        searchContent:
          'router in-memory react router RUI::Router InitialPath RUI::Routes best match ranked path FRuiRoute Path Element bIndex Children data not tags UseOutlet nested route RUI::Link bReplace active state UseIsActive hooks UseNavigate imperative UseLocation UsePathname UseParams dynamic segments UseSearchParams query UseMatch UseResolvedPath UseOutlet UseBlocker navigation guard unsaved changes game ui menus settings nested screens no url bar',
        element: () => <RouterPage />,
      },
      {
        id: 'hmr',
        canonicalId: 'hmr',
        title: 'Hot Module Replacement',
        path: '/tooling/hmr',
        keywords: ['hmr', 'hot reload', 'live coding', 'follow play', 'ReactiveUetkx'],
        searchContent:
          'hot module replacement hmr live reload edit .uetkx save running ui updates under a second state preserved no manual rebuild no restart unreal live coding rides c++ hot reload FUetkxHmrController recompiles committed c++ whole-project patches loaded modules reconciler re-renders hook state stored by slot on fibers survive patch counters inputs scroll positions ReactiveUetkx menu window follow play PIE play stop live coding enabled windows editor development-time shipping compiles committed uetkx.inl ahead of time compile error skipped last good build message log',
        element: () => <HmrPage />,
      },
      {
        id: 'suspense',
        canonicalId: 'suspense',
        title: 'Suspense',
        path: '/tooling/suspense',
        keywords: ['suspense', 'fallback', 'loading', 'async', 'boundary'],
        searchContent:
          'suspense RUI::Suspense IsReady fallback children structural primitive not a tag expression child fallback content not ready swap loading state async resource streamed asset deferred computation no throw-to-suspend c++ has no equivalent declarative boundary polyfill readiness driven explicitly poll driver self-re-arming frame request signal flips fallback to content deliberate divergence from react model not-ready state stress test poll-driver frame loop RUI::Suspense Fallback',
        element: () => <SuspensePage />,
      },
      {
        id: 'portals',
        canonicalId: 'portals',
        title: 'Portals',
        path: '/tooling/portals',
        keywords: ['portal', 'modal', 'tooltip', 'overlay', 'out-of-tree', 'FRuiPortalHandle'],
        searchContent:
          'portals RUI::Portal target children key render children under different widget escape parent clipping stacking modals tooltips overlays fiber belongs to rendering component state context effects unmount tears down out-of-tree content FRuiPortalHandle host handle Ref receives handle on attach cleared on detach react ref lifecycle slate host widget handles any panel context flows from render position not target position themed modal app-level overlay',
        element: () => <PortalsPage />,
      },
    ],
  },
  {
    id: 'integration',
    title: 'Integration',
    pages: [
      {
        id: 'integration-overview',
        canonicalId: 'integration-overview',
        title: 'Integration Overview',
        path: '/integration',
        keywords: ['integration', 'interop', 'epic', 'umg', 'commonui', 'mvvm', 'slate', 'pillars'],
        searchContent:
          'integration overview four pillars layer decides which slate widgets exist each frame everything epic stays in place feeding data hosting output slate render target ordinary swidgets widget reflector umg door both directions URuiHostWidget RUI::Umg::UserWidget commonui owns menus input routing focus activatable stacks URuiActivatableScreen mvvm fieldnotify UseField they own values we own structure named-component registry ComponentName RUI_COMPONENT RegisterNamedFactory MountNamed designer dropdown incremental adoption leaf panel screen interop showcase demo beta caveats',
        element: () => <InteropOverviewPage />,
      },
      {
        id: 'umg-guide',
        canonicalId: 'umg-guide',
        title: 'UMG (both directions)',
        path: '/integration/umg',
        keywords: ['umg', 'URuiHostWidget', 'UserWidget', 'MountNamed', 'prop map', 'designer'],
        searchContent:
          'umg interop door both directions our ui inside theirs URuiHostWidget designer-placeable ReactiveUI Host palette ComponentName registered component RebuildWidget mounts ReleaseSlateResources unmounts cleanups Remount design time placeholder never runs live code URuiWorldSubsystem MountNamed blueprint-callable viewport zorder teardown world death their widgets inside ours RUI::Umg::UserWidget UUserWidget expression child owning world SObjectWidget gc-alive strong pointer deletion commit FRuiStyleDict WidgetProps reflection prop map UPROPERTYs diffing MakeAssetBrush textures materials beta caveat no blueprint props viewmodel channel signal UseField',
        element: () => <UmgGuidePage />,
      },
      {
        id: 'commonui-guide',
        canonicalId: 'commonui-guide',
        title: 'CommonUI',
        path: '/integration/commonui',
        keywords: ['commonui', 'URuiActivatableScreen', 'activation', 'input method', 'gamepad', 'stack'],
        searchContent:
          'commonui interop their stacks our screens menu stacks input routing back-handling console-cert never installs input preprocessor URuiActivatableScreen UCommonActivatableWidget push AddWidgetInstance UCommonActivatableWidgetContainerBase CreateWidget owning player ComponentName re-renders activation deactivation input method change FRuiActivationState UseIsActive UseInputMethod ERuiInputMethod MouseAndKeyboard Gamepad Touch glyph prompt swap ActivationProvider ProvideContext ActivationContext stand-in tests CommonUiDemo wrap common widgets UCommonButtonBase children activatables only through screen GetDesiredFocusTarget focus target UseFocus activation effect v1.x',
        element: () => <CommonUiGuidePage />,
      },
      {
        id: 'mvvm-guide',
        canonicalId: 'mvvm-guide',
        title: 'MVVM / FieldNotify',
        path: '/integration/mvvm',
        keywords: ['mvvm', 'fieldnotify', 'UseField', 'viewmodel', 'URuiSignalViewModel', 'global collection'],
        searchContent:
          'mvvm interop fieldnotify engine-level FieldNotification module no ModelViewViewModel plugin dependency any INotifyFieldValueChanged data source epic mvvm viewmodels UMVVMViewModelBase stock widgets they own values we own structure UseField subscribe re-render broadcast unsubscribe unmount null stale default coalesce one re-render per frame delayed execution batching reverse direction our state their views URuiSignalViewModel plugin-free SetInt broadcasts on change skips equal URuiMvvmViewModel RegisterGlobalViewModel FindGlobalViewModel global viewmodel collection context name bind no view-side changes ReactiveUIMVVMBridge UseMemo TStrongObjectPtr NewObject gc-rooted released unmount MvvmDemo',
        element: () => <MvvmGuidePage />,
      },
    ],
  },
  {
    id: 'reference',
    title: 'Reference',
    pages: [
      {
        id: 'language-reference',
        canonicalId: 'language-reference',
        title: 'Language Reference',
        path: '/reference',
        keywords: ['reference', 'syntax', 'directives', 'preamble', 'fragment', '@if', '@for', '@match'],
        searchContent:
          'language reference uetkx compiles committed c++ preamble #include import statements component hook module declarations typed parameters defaults return exactly one root element attribute value string or expr brace child fragment <> comments // /* */ {/* */} control flow directives @if @elif @else @for @while @match @case @default structural attributes key Slot event props On widget slate property PascalCase UETKX0103 UETKX0108',
        element: () => <LanguageReferencePage />,
      },
      {
        id: 'components-overview',
        canonicalId: 'components-overview',
        title: 'Components Overview',
        path: '/reference/components',
        keywords: ['components', 'host elements', 'tags', 'slate', 'catalog', 'widgets'],
        searchContent:
          'components overview host elements intrinsic tags 1:1 slate widget tag is slate class minus S VerticalBox HorizontalBox Overlay WrapBox UniformGridPanel GridPanel UniformWrapPanel ScrollBox WidgetSwitcher Box Border ScaleBox SafeZone DPIScaler Spacer Separator TextBlock RichTextBlock Image ProgressBar Throbber Button CheckBox Slider SpinBox EditableTextBox MultiLineEditableTextBox SearchBox DragSource DropTarget Link RuiCanvas Rui mark Fragment Suspense function components pascalcase props style keys events exact unreal names prop-map generated per-widget reference',
        element: () => <ComponentsOverviewPage />,
      },
      {
        id: 'api-reference',
        canonicalId: 'api-reference',
        title: 'API Reference',
        path: '/reference/api',
        keywords: ['api', 'RUI', 'FRuiRoot', 'RUI::FC', 'factories', 'namespace'],
        searchContent:
          'api reference RUI namespace RUI::Slate RUI::Umg RUI::CommonUI hooks free functions FRuiRoot CreateInViewport mount viewport zorder TSharedPtr Reset unmount RUI::FC function component vnode props children key RUI::Fragment RUI::Suspense fallback RUI::Fmt text interpolation FText RUI::Deps dependency list UseMemo UseCallback UseEffect RUI::Slate::VerticalBox Button Border factories MakeDrawFn FRuiDrawFn RuiCanvas UseShortcut FRuiShortcut GetOrCreateSignal UseSignalKey ProvideContext UseContext RUI::Umg::UserWidget UseField FieldNotify RUI::CommonUI::ActivationContext UseIsActive',
        element: () => <ApiReferencePage />,
      },
      {
        id: 'diagnostics',
        canonicalId: 'diagnostics',
        title: 'Diagnostics',
        path: '/reference/diagnostics',
        keywords: ['diagnostics', 'UETKX', 'errors', 'warnings', 'codes'],
        searchContent:
          'diagnostics UETKX code stable catalog compiler build time language server editor as you type same meaning everywhere machine-readable uetkx.diags.json sidecar gitignored ReactiveUI message log bands UETKX01xx structural 0103 name file mismatch 0105 unknown element 0108 multiple roots UETKX03xx syntax 0300 0301 0302 0303 0304 0305 UETKX21xx declarations 2100 pascalcase 2101 no declaration 2106 duplicate export UETKX22xx hooks modules 2200 2201 2202 2203 2204 2205 UETKX23xx imports 2300 2309 UETKX25xx directives 2506 2508 UETKX30xx codegen 3006 template hook',
        element: () => <DiagnosticsPage />,
      },
      {
        id: 'configuration',
        canonicalId: 'configuration',
        title: 'Configuration',
        path: '/reference/config',
        keywords: ['config', 'uetkx.config.json', 'formatter', 'settings', 'editor', 'root'],
        searchContent:
          'configuration uetkx.config.json walk up nearest wins root import alias printWidth formatter wrap indentStyle tab space indentSize editor settings vscode vs2022 extension editor.defaultFormatter ReactiveUITK.uetkx editor.formatOnSave editor.insertSpaces false editor.detectIndentation false restart language server command golden corpus locked format on save byte consistent one component per file authoring only not runtime',
        element: () => <ConfigPage />,
      },
      {
        id: 'debugging',
        canonicalId: 'debugging',
        title: 'Debugging Guide',
        path: '/reference/debugging',
        keywords: ['debugging', 'generated code', 'diags.json', 'message log', 'cvars', 'breakpoints'],
        searchContent:
          'debugging guide generated code committed uetkx.inl readable c++ RUI::FC RUI::Slate lowered breakpoints compiler diagnostics UETKX code uetkx.diags.json sidecar gitignored ReactiveUI message log editor extensions inline restart language server runtime cvars rui.StrictMode rui.HookValidation rui.StrictDiagnostics rui.TimeSlicing rui.FrameBudgetMs rui.HostNodePool stat ReactiveUI counters hook called conditionally desync positional slots wrong state re-render unconditional top level live coding',
        element: () => <DebuggingPage />,
      },
    ],
  },
  {
    id: 'subsystems',
    title: 'Subsystems in depth',
    pages: [
      {
        id: 'subsystems-in-depth',
        canonicalId: 'post-v1-subsystems',
        title: 'Subsystems in depth',
        path: '/features/subsystems',
        keywords: [
          'router',
          'stylesheet',
          'theme',
          'uss',
          'listview',
          'tileview',
          'virtualized',
          'drag',
          'drop',
          'presence',
          'commonui',
          'mvvm',
          'activatable',
          'widgets',
        ],
        searchContent:
          'router routes link outlet 17 hooks UseLocation UseNavigate UseParams UseSearchParams UseMatch UseBlocker stylesheet @theme @uss tokens cascade theme classes inline virtualized listview tileview render prop sub-root drag and drop dragsource droptarget payload exit animation presence usepresence commonui activatable screen useactivation useinputmethod mvvm global collection viewmodel umg prop-map widgets widgetswitcher scalebox throbber wrapbox multilineeditabletextbox searchbox safezone dpiscaler separator spinbox uniformwrappanel richtextblock gridpanel uniformgridpanel expandablearea segmentedcontrol numericentrybox combobox suggestiontextbox',
        element: () => <FeaturesPage />,
      },
    ],
  },
  {
    id: 'component-reference',
    title: 'Components',
    pages: componentPages.map((page: LegacyPage) => ({
      id: page.id,
      canonicalId: page.id,
      title: page.title,
      path: page.path,
      keywords: page.keywords,
      group: page.group,
      sinceUE: page.sinceUE,
      element: page.element,
    })),
  },
  {
    id: 'faq',
    title: 'FAQ',
    pages: [
      {
        id: 'faq',
        canonicalId: 'faq',
        title: 'FAQ',
        path: '/faq',
        keywords: ['faq', 'questions', 'help', 'production', 'versions', 'umg', 'mvvm'],
        searchContent:
          'faq frequently asked questions what is reactiveui for unreal react-style ui library pure c++ fiber reconciler slate widgets .uetkx compiles committed c++ which unreal versions 5.6 plugin no external native dependency no external runtime production ready beta pre-1.0 docs release scripting vm no javascript engine bridge reflection-free existing umg widgets UUserWidget RUI::Umg::UserWidget MVVM view-models FieldNotify UseField CommonUI activatable event props OnClicked not onClick loyal to unreal hot reload live coding windows state preserved editor support vs code visual studio 2022 extensions offline',
        element: () => <FAQPage />,
      },
    ],
  },
  {
    id: 'known-issues',
    title: 'Known Issues',
    pages: [
      {
        id: 'known-issues',
        canonicalId: 'known-issues',
        title: 'Known Issues',
        path: '/known-issues',
        keywords: ['known issues', 'limitations', 'hmr', 'hooks', 'transitions'],
        searchContent:
          'known issues limitations beta hot reload windows only live coding shipping unaffected committed uetkx.inl hooks unconditional positional slots @if @for desync rui.StrictMode transitions synchronous UseTransition no concurrent renderer api parity removed plain props do not reset style events refs draw reset intentional family semantic per-widget reference pages pending prop-map components overview slate class decisions not bugs',
        element: () => <KnownIssuesPage />,
      },
    ],
  },
  {
    id: 'roadmap',
    title: 'Roadmap',
    pages: [
      {
        id: 'roadmap',
        canonicalId: 'roadmap',
        title: 'Roadmap',
        path: '/roadmap',
        keywords: ['roadmap', 'status', 'phases', 'progress', 'release'],
        searchContent:
          'roadmap living status plans/ROADMAP.md phases core reconciler hooks signals suspense slate host widgets style uetkx compiler build committed inl schema RUICompile formatter hot reload live coding ide extensions language server vs code vs2022 umg commonui mvvm interop UseField activatables production gaps virtualized lists focus animation portals drag and drop widget batch 2 localization gather culture switch demos gallery benchmarks docs site in progress release publishing owner gated ship gate fab marketplace done in progress planned',
        element: () => <RoadmapPage />,
      },
    ],
  },
]

// ---------------------------------------------------------------------------
// Flat page lists
// ---------------------------------------------------------------------------

export const allFlat: DocPage[] = sections.flatMap((section) => {
  if (section.title === 'Components') {
    const common = section.pages.filter((page) => page.group === 'basic')
    const uncommon = section.pages.filter((page) => page.group === 'advanced' || !page.group)
    return [...common, ...uncommon]
  }
  return section.pages
})

// ---------------------------------------------------------------------------
// Version-aware filtering
// ---------------------------------------------------------------------------

/** Check if a page is available for the given Unreal Engine version. */
const isPageAvailable = (page: DocPage, selectedVersion: string): boolean => {
  if (page.sinceUE) {
    return compareVersions(page.sinceUE, selectedVersion) <= 0
  }
  const pv = PAGE_VERSIONS[page.canonicalId]
  return isAvailableIn(pv, selectedVersion)
}

/** Filter sections to only include pages available in the selected version. */
export const getFilteredSections = (selectedVersion: string): DocSection[] =>
  sections
    .map((section) => ({
      ...section,
      pages: section.pages.filter((page) => isPageAvailable(page, selectedVersion)),
    }))
    .filter((section) => section.pages.length > 0)

/** Flat page list filtered by version — for search and sidebar. */
export const getFilteredFlat = (selectedVersion: string): DocPage[] =>
  getFilteredSections(selectedVersion).flatMap((section) => {
    if (section.title === 'Components') {
      const common = section.pages.filter((page) => page.group === 'basic')
      const uncommon = section.pages.filter((page) => page.group === 'advanced' || !page.group)
      return [...common, ...uncommon]
    }
    return section.pages
  })
