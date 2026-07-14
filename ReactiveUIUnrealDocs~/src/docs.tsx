import type { ReactElement } from 'react'
import type { Page as LegacyPage } from './pages'
import { pages as legacySections } from './pages'
import { PAGE_VERSIONS, isAvailableIn, compareVersions } from './versionManifest'
import { IntroductionPage } from './pages/Introduction/IntroductionPage'
import { GettingStartedPage } from './pages/GettingStarted/GettingStartedPage'
import { ConceptsPage } from './pages/Concepts/ConceptsPage'
import { ImportsPage } from './pages/Uetkx/ImportsPage'
import { HooksGuidePage } from './pages/Hooks/HooksGuidePage'
import { StylingPage } from './pages/Styling/StylingPage'
import { DifferencesPage } from './pages/Differences/DifferencesPage'
import { HmrPage } from './pages/Tooling/HMR/HmrPage'
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
          'reactive ui for unreal react-style ui library unreal engine 5.6 pure c++ no javascript vm no bridge layer uetkx authoring language function-style components hooks state effects UseState UseEffect fiber reconciler decides which slate widgets exist each frame diffs patches only what changed umg commonui mvvm stay in place feeding data hosting output .uetkx markup jsx-like same family grammar guitkx godot uitkx unity compiles to native c++ shipping builds hot-reloads live in pie save mid-play under a second state preserved siblings ReactiveUIToolKit unity c# ReactiveUI-Godot godot gdscript pre-alpha roadmap component state count int32 VBox Text Button onClick fmt set',
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
        title: 'Install & Setup',
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
          'concepts environment react-style function components fiber reconciler diffs patches slate widgets pure c++ no javascript no uobject in core components hooks modules declaration kinds companion files .hooks.uetkx export hook UseCounter export module Styles synchronous rendering keyed reconciliation bailout effects after commit positional hooks call order unconditional top level IRuiHostConfig umg commonui mvvm rui.StrictMode rui.Stats rui.DumpTree console variables cvars',
        element: () => <ConceptsPage />,
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
          'styling no uss no css no cascade no stylesheet language widget properties directly in markup style key exact unreal setter property name Padding ContentPadding ColorAndOpacity BorderImage BorderBackgroundColor WidthOverride HeightOverride RenderOpacity RenderTransform Size FMargin FLinearColor FSlateColor FVector2D brush slot props Slot.Padding Slot.HAlign Slot.VAlign parent panel slot setters HAlign_Fill HAlign_Right compact string forms SetPadding SetColorAndOpacity',
        element: () => <StylingPage />,
      },
      {
        id: 'differences',
        canonicalId: 'differences',
        title: 'Different from React',
        path: '/differences',
        keywords: ['react', 'differences', 'events', 'onclicked', 'slate', 'synchronous', 'naming'],
        searchContent:
          'different from react mental model pure c++ compiled to slate no javascript vm no bridge no reflection retained slate widgets not virtual dom hooks pascalcase UseState events unreal delegate name OnClicked OnCheckStateChanged OnValueChanged never onclick onchange elements slate class minus S VerticalBox TextBlock Slider Rui mark custom widgets UseTransition synchronous structural error boundaries removed plain props do not reset style events refs draw reset preserved family semantic godot divergences react ref lifecycle subscribe-in-effect signals registry fname component identity',
        element: () => <DifferencesPage />,
      },
    ],
  },
  {
    id: 'tooling',
    title: 'Tooling',
    pages: [
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
    ],
  },
  {
    id: 'beyond-v1',
    title: 'Beyond v1',
    pages: [
      {
        id: 'post-v1-subsystems',
        canonicalId: 'post-v1-subsystems',
        title: 'Post-v1 subsystems',
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
