/**
 * Version manifest — single source of truth for Unreal Engine version awareness
 * in the documentation website.
 *
 * When adding a new engine version, add an entry to SUPPORTED_VERSIONS and
 * populate the feature maps below for any non-floor additions.
 *
 * Floor version = the minimum Unreal Engine version the library supports.
 * Features at or below the floor have no entry here (they are always available).
 */

import { HOST_ELEMENTS } from './hostSchema'

// ---------------------------------------------------------------------------
// Version registry
// ---------------------------------------------------------------------------

export interface VersionInfo {
  /** Internal version string, e.g. "5.6". */
  version: string
  /** Human-readable label shown in the UI, e.g. "5.6". */
  label: string
}

/**
 * Ordered list of Unreal Engine versions the library has explicit support for.
 * The first entry is the floor version (minimum supported).
 */
export const SUPPORTED_VERSIONS: VersionInfo[] = [
  { version: '5.6', label: '5.6' },
  { version: '5.7', label: '5.7' },
  { version: '5.8', label: '5.8' },
]

export const FLOOR_VERSION = SUPPORTED_VERSIONS[0]

/** Latest version in the list — used as default when "All versions" is selected. */
export const LATEST_VERSION = SUPPORTED_VERSIONS[SUPPORTED_VERSIONS.length - 1]

// ---------------------------------------------------------------------------
// Feature version tags
// ---------------------------------------------------------------------------

export interface FeatureVersion {
  /** The version where this feature was introduced (e.g. "5.7"). */
  sinceUE: string
  /** The version where this feature was deprecated (optional). */
  deprecatedIn?: string
  /** The version where this feature was removed (optional). */
  removedIn?: string
}

/**
 * Host elements introduced after the floor version — DERIVED from the compiler-exported
 * schema's `sinceUE` annotations (the codegen tag defs are the source of truth; e.g.
 * SearchableComboBox: 5.7), so this map can never drift from the tooling. An element with
 * no entry is available since floor.
 */
export const ELEMENT_VERSIONS: Record<string, FeatureVersion> = Object.fromEntries(
  Object.entries(HOST_ELEMENTS)
    .filter(([, el]) => el.sinceUE)
    .map(([tag, el]) => [tag, { sinceUE: el.sinceUE as string }]),
)

/**
 * Style keys introduced after the floor version.
 */
export const STYLE_PROPERTY_VERSIONS: Record<string, FeatureVersion> = {
  // All documented style keys are available since the floor.
}

/**
 * Doc pages introduced after the floor version.
 * Keys are page canonicalId values from docs.tsx / pages.tsx.
 */
export const PAGE_VERSIONS: Record<string, FeatureVersion> = {
  // When a new page documents a feature introduced in a newer engine version:
  // 'some-page': { sinceUE: '5.8' },
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Compare two version strings numerically (e.g. "5.7" > "5.6"). */
export function compareVersions(a: string, b: string): number {
  const pa = a.split('.').map(Number)
  const pb = b.split('.').map(Number)
  for (let i = 0; i < Math.max(pa.length, pb.length); i++) {
    const diff = (pa[i] ?? 0) - (pb[i] ?? 0)
    if (diff !== 0) return diff
  }
  return 0
}

/** Check if a feature is available for the given selected version. */
export function isAvailableIn(feature: FeatureVersion | undefined, selectedVersion: string): boolean {
  // No version info → floor feature → always available
  if (!feature) return true
  if (compareVersions(feature.sinceUE, selectedVersion) > 0) return false
  if (feature.removedIn && compareVersions(feature.removedIn, selectedVersion) <= 0) return false
  return true
}

/** Get a display label like "5.7+" for a feature version. Returns undefined for floor features. */
export function getVersionBadge(feature: FeatureVersion | undefined): string | undefined {
  if (!feature) return undefined
  const info = SUPPORTED_VERSIONS.find((v) => v.version === feature.sinceUE)
  return info ? `${info.label}+` : `${feature.sinceUE}+`
}

/**
 * Build version-aware search keywords for the Styling page.
 *
 * Slate styling is applied through plain widget setters with no version-gated
 * keys at the moment (STYLE_PROPERTY_VERSIONS is empty), so this returns no
 * extra terms yet. The plumbing stays in place so the styling prose (Phase 2)
 * can version-gate style keys without touching the search machinery.
 */
export function getStyleSearchTerms(selectedVersion: string): string {
  return Object.entries(STYLE_PROPERTY_VERSIONS)
    .filter(([, feature]) => isAvailableIn(feature, selectedVersion))
    .map(([key]) => key)
    .join(' ')
}
