import type { ReactElement } from 'react'
import { ElementPage } from './pages/Components/ElementPage'
import { CORE_TAGS, HOST_ELEMENTS, HOST_TAGS } from './hostSchema'

export type Page = {
  id: string
  title: string
  path: string
  keywords?: string[]
  searchContent?: string
  group?: 'basic' | 'advanced'
  sinceUE?: string
  element: () => ReactElement
}

export type Section = {
  id: string
  title: string
  pages: Page[]
}

// One page per host tag, GENERATED from the compiler-exported schema injected by
// vite.config.ts (__HOST_TAGS__ / __HOST_ELEMENTS__ ← ide-extensions/lsp-server/src/
// uetkx-schema.json, refreshed by RUIExportSchema) — the docs and the LSP read one
// vocabulary, so the catalog can never drift from the tooling.
const componentPages: Page[] = HOST_TAGS.map((tag) => {
  const attrs = Object.keys(HOST_ELEMENTS[tag]?.attrs ?? {})
  return {
    id: `element-${tag.toLowerCase()}`,
    title: tag,
    path: `/components/${tag.toLowerCase()}`,
    keywords: [tag, 'element', 'widget', 'tag'],
    searchContent: `${tag} S${tag} slate widget host element tag props events ${attrs.join(' ')}`,
    group: CORE_TAGS.has(tag) ? ('basic' as const) : ('advanced' as const),
    element: () => <ElementPage tag={tag} />,
  }
})

export const pages: Section[] = [
  {
    id: 'components',
    title: 'Components',
    pages: componentPages,
  },
]
