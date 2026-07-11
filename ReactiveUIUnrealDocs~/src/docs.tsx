import type { ReactElement } from 'react'
import type { Page as LegacyPage } from './pages'
import { pages as legacySections } from './pages'
import { PAGE_VERSIONS, isAvailableIn, compareVersions } from './versionManifest'
import { IntroductionPage } from './pages/Introduction/IntroductionPage'
import { ImportsPage } from './pages/Uetkx/ImportsPage'

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
    id: 'guides',
    title: 'Guides',
    pages: [
      {
        id: 'uetkx-imports',
        canonicalId: 'uetkx-imports',
        title: 'Imports & exports',
        path: '/uetkx/imports',
        keywords: ['import', 'export', 'module', 'strict', 'codemod', 'uetkx', 'privacy', 'root alias'],
        searchContent:
          'uetkx imports exports static import export module strict resolution codemod RUIMigrateImports ~/ root alias specifier relative extensionless named export privacy tree-shaken UETKX2300 2301 2302 2303 2304 2305 2306 2307 2308 2309 unknown specifier not exported not declared duplicate unused used not imported value cycle module boundary preamble uetkx.config.json root key',
        element: () => <ImportsPage />,
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
