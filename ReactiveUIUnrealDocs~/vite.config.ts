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

// ── host-element tables — read from the compiler-exported schema ─────────────
// The SAME vocabulary the LSP serves (ide-extensions/lsp-server/src/uetkx-schema.json,
// refreshed by the RUIExportSchema commandlet), so the per-widget prop/event tables can
// never drift from the tooling — the docs and the editor read one source of truth.
type UetkxSchema = {
  v?: number
  elements?: Record<string, { factory?: string; children?: boolean; attrs?: Record<string, string> }>
  styleKeys?: string[]
  slotKeys?: string[]
  hooks?: string[]
  eventPayloads?: Record<string, string>
}

function readUetkxSchema(): UetkxSchema {
  try {
    const raw = fs
      .readFileSync(path.join(repoRoot, 'ide-extensions', 'lsp-server', 'src', 'uetkx-schema.json'), 'utf-8')
      .replace(/^﻿/, '')
    return JSON.parse(raw) as UetkxSchema
  } catch {
    return {}
  }
}

const uetkxSchema = readUetkxSchema()
const hostElements: Record<string, unknown> = uetkxSchema.elements ?? {}
const hostTags: string[] = Object.keys(uetkxSchema.elements ?? {})
const styleKeys: string[] = uetkxSchema.styleKeys ?? []
const slotKeys: string[] = uetkxSchema.slotKeys ?? []
const eventPayloads: Record<string, string> = uetkxSchema.eventPayloads ?? {}

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
    __STYLE_KEYS__: JSON.stringify(styleKeys),
    __SLOT_KEYS__: JSON.stringify(slotKeys),
    __EVENT_PAYLOADS__: JSON.stringify(eventPayloads),
  },
  css: {
    postcss: './postcss.config.cjs',
  },
})
