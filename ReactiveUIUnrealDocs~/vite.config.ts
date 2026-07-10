import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import fs from 'node:fs'
import path from 'node:path'

// ─────────────────────────────────────────────────────────────────────────────
// Build-time data generation for the Unreal docs.
//
// Like the Godot sibling (whose config reads the bundled ClassDB dump and guitkx
// schema), this config injects host/widget data as compile-time constants so the
// docs never drift from the toolchain:
//   __PACKAGE_VERSION__  — plugin version from Plugins/ReactiveUI/ReactiveUI.uplugin
//   __UE_MIN__           — the minimum supported Unreal Engine version (floor)
//   __HOST_ELEMENTS__    — per host tag: { tag, slateClass, factory, events, props[], signals[] }
//   __HOST_TAGS__        — ordered list of host tags (nav order = schema order)
//   __STRUCTURAL_ATTRS__/__COMMON_ATTRS__/__CONTROL_FLOW__/__PREAMBLE_DIRECTIVES__ — from the schema
// ─────────────────────────────────────────────────────────────────────────────

const repoRoot = path.resolve(process.cwd(), '..')

// ── plugin version (ReactiveUI.uplugin: "VersionName": "0.1.0-dev") ──────────
function readPluginVersion(): string {
  try {
    const raw = fs
      .readFileSync(path.join(repoRoot, 'Plugins', 'ReactiveUI', 'ReactiveUI.uplugin'), 'utf-8')
      .replace(/^\uFEFF/, '')
    const uplugin = JSON.parse(raw) as { VersionName?: string }
    return uplugin.VersionName || '0.0.0'
  } catch {
    return '0.0.0'
  }
}

// ── host-element tables ───────────────────────────────────────────────────────
// TODO(Phase 2): wire these to templates/prop-map.schema.json (the widget prop-map
// the toolchain ships) the same way the Godot sibling reads its ClassDB dump +
// guitkx schema, so the per-widget prop/event tables never drift from the tooling.
// Until the prop-map data lands, the tables are empty and the docs ship only the
// non-catalog pages.
const hostElements: Record<string, unknown> = {}
const hostTags: string[] = []
const schema: {
  structuralAttributes: unknown[]
  commonAttributes: unknown[]
  controlFlow: unknown[]
  preambleDirectives: unknown[]
} = { structuralAttributes: [], commonAttributes: [], controlFlow: [], preambleDirectives: [] }

// https://vite.dev/config/
export default defineConfig({
  // GitHub Pages PROJECT site: served from /ReactiveUI-Unreal/. Asset URLs are base-prefixed and the
  // router basename mirrors this (see src/main.tsx). Switch both to '/' for a custom domain.
  base: '/ReactiveUI-Unreal/',
  plugins: [react()],
  define: {
    __PACKAGE_VERSION__: JSON.stringify(readPluginVersion()),
    __UE_MIN__: JSON.stringify('5.6'),
    __HOST_ELEMENTS__: JSON.stringify(hostElements),
    __HOST_TAGS__: JSON.stringify(hostTags),
    __STRUCTURAL_ATTRS__: JSON.stringify(schema.structuralAttributes),
    __COMMON_ATTRS__: JSON.stringify(schema.commonAttributes),
    __CONTROL_FLOW__: JSON.stringify(schema.controlFlow),
    __PREAMBLE_DIRECTIVES__: JSON.stringify(schema.preambleDirectives),
  },
  css: {
    postcss: './postcss.config.cjs',
  },
})
