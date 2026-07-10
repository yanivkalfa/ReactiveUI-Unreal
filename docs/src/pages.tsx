import type { ReactElement } from 'react'

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

// One page per host tag, in schema (nav) order — the widget catalog is fully
// data-driven in the family shell. TODO(Phase 2): generate from
// templates/prop-map.schema.json (injected by vite.config.ts as __HOST_TAGS__ /
// __HOST_ELEMENTS__) once the widget prop-map lands; until then the catalog is
// empty and only the hand-written pages in docs.tsx exist.
const componentPages: Page[] = []

export const pages: Section[] = [
  {
    id: 'components',
    title: 'Components',
    pages: componentPages,
  },
]

export const flat: Page[] = pages.flatMap((s) => {
  if (s.id === 'components') {
    const common = s.pages.filter((p) => p.group === 'basic')
    const uncommon = s.pages.filter((p) => p.group === 'advanced' || !p.group)
    return [...common, ...uncommon]
  }
  return s.pages
})
