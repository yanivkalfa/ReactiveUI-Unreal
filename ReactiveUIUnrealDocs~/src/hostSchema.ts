// The compiler-exported .uetkx vocabulary, injected at build time by vite.config.ts from
// ide-extensions/lsp-server/src/uetkx-schema.json — the SAME file the language server serves,
// so the generated component catalog can never drift from the tooling.

declare const __HOST_ELEMENTS__: Record<string, HostElement>
declare const __HOST_TAGS__: string[]
declare const __STYLE_KEYS__: string[]
declare const __SLOT_KEYS__: string[]
declare const __EVENT_PAYLOADS__: Record<string, string>

export interface HostElement {
  factory?: string
  children?: boolean
  attrs?: Record<string, string>
  /** Minimum engine version the widget exists in (e.g. "5.7"); absent = all supported engines. */
  sinceUE?: string
}

export const HOST_ELEMENTS: Record<string, HostElement> = __HOST_ELEMENTS__
export const HOST_TAGS: string[] = __HOST_TAGS__
export const STYLE_KEYS: string[] = __STYLE_KEYS__
export const SLOT_KEYS: string[] = __SLOT_KEYS__
export const EVENT_PAYLOADS: Record<string, string> = __EVENT_PAYLOADS__

/** The Phase-2 core set — rendered as "Common" in the sidebar; everything else is "Uncommon". */
export const CORE_TAGS = new Set([
  'TextBlock', 'Button', 'Border', 'Box', 'VerticalBox', 'HorizontalBox', 'Overlay', 'Image',
  'ScrollBox', 'Spacer', 'EditableTextBox', 'CheckBox', 'Slider', 'ProgressBar', 'RuiCanvas',
])
