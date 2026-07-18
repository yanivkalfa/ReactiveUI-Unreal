// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-020 — the server-side wiring that routes hover/definition INTO the embedded-C++ layer. A
// .uetkx position that lands inside a component `setup` / `hook` / `module` body is embedded C++;
// this module maps it into the synthesized virtual document (virtualDoc.ts) and forwards the
// request to a clangd proxy, translating result ranges back to .uetkx coordinates. A position
// OUTSIDE any embedded region returns null so the caller falls through to the markup baseline; a
// null is also returned whenever the proxy is unavailable — the family's documented degradation.

import { buildVirtualCpp, ImportSurfaceResolver } from "./virtualDoc";
import { offsetToPosition, positionToOffset } from "./sourceMap";
import { importedSurfaceFor } from "./uetkxWorkspace";
import { URI } from "./uri";
import type { ClangdPosition } from "./clangdProxy";

/** The workspace-backed import resolver for a document, or undefined when no path is known
 *  (tests) — CONSISTENCY RULE: every buildEmbeddedView for one didOpen'd doc must use the same
 *  resolver, or mapped ranges shift against what clangd holds. */
export function surfaceResolverFor(fsPath: string | undefined): ImportSurfaceResolver | undefined {
  if (!fsPath) return undefined;
  return (specifier) => importedSurfaceFor(fsPath, specifier);
}

/** The minimal position-request surface embeddedIntel needs — ClangdProxy satisfies it, and tests
 *  pass a stub. Keeping it structural means neither a live clangd nor a spawned process is required
 *  to exercise the routing/fallback logic. */
export interface PositionResponder {
  isAvailable(): boolean;
  didOpen(uri: string, text: string): void;
  positionRequest(method: string, uri: string, position: ClangdPosition, extra?: object): Promise<unknown | null>;
}

/** The virtual URI the proxy sees for a .uetkx document's synthesized C++ translation unit. */
export function virtualUriOf(sourceUri: string): string {
  return `${sourceUri}.__rui_embedded__.cpp`;
}

/** Inverse of virtualUriOf — the real .uetkx uri behind a virtual one, or null for a uri that
 *  is not ours (TB-10: routes clangd's publishDiagnostics back to the right document). */
export function realUriOfVirtual(virtualUri: string): string | null {
  const suffix = ".__rui_embedded__.cpp";
  return virtualUri.endsWith(suffix) ? virtualUri.slice(0, -suffix.length) : null;
}

/**
 * A build-once view over a .uetkx source: the synthesized virtual C++ + bidirectional position
 * mapping. Rebuilding the virtual doc per request is cheap (a single scan), but callers that do
 * several lookups for one document should reuse a view.
 */
export function buildEmbeddedView(source: string, fsPath?: string) {
  const vd = buildVirtualCpp(source, "doc", surfaceResolverFor(fsPath));
  return {
    /** The synthesized C++ translation unit clangd parses. */
    virtualText: vd.text,
    /** How many embedded regions were lifted (0 = a markup-only file — nothing to forward). */
    regionCount: vd.regionCount,

    /** A source utf16 offset -> the virtual-doc position, or null when not inside embedded C++. */
    positionInVirtual(sourceOffset: number): ClangdPosition | null {
      const virOffset = vd.map.sourceToVirtual(sourceOffset);
      if (virOffset === null) return null;
      return offsetToPosition(vd.text, virOffset);
    },

    /** A virtual-doc position -> the source utf16 offset, or null when it maps outside every region
     *  (e.g. clangd pointed into the prelude scaffolding, or into a real engine header). */
    sourceOffsetOf(virtualPosition: ClangdPosition): number | null {
      const virOffset = positionToOffset(vd.text, virtualPosition);
      return vd.map.virtualToSource(virOffset);
    },

    /** The [start, end) source bounds of the embedded region containing a source offset, or null
     *  outside every region (N6: scopes the rename guard's error check to the ENCLOSING block). */
    sourceRegionOf(sourceOffset: number): { start: number; end: number } | null {
      const span = vd.map.sourceSpanContaining(sourceOffset);
      return span ? { start: span.srcStart, end: span.srcStart + span.length } : null;
    },
  };
}

/**
 * Forward a hover/definition-style position request for a .uetkx position through the proxy.
 * Returns:
 *   - null when the position is NOT embedded C++ (caller uses the markup baseline),
 *   - null when the proxy is unavailable (graceful degradation),
 *   - otherwise clangd's raw result (ranges are in VIRTUAL coordinates — translate with the view
 *     when the result points back into the virtual doc; engine-header locations pass through).
 */
export async function embeddedPositionRequest(
  proxy: PositionResponder,
  method: string,
  sourceUri: string,
  source: string,
  sourceOffset: number,
  extra?: object,
): Promise<unknown | null> {
  if (!proxy.isAvailable()) return null;
  let fsPath: string | undefined;
  try {
    fsPath = URI.toFsPath(sourceUri);
  } catch {
    fsPath = undefined; // a non-file uri (tests) — the view simply omits imported surfaces
  }
  const view = buildEmbeddedView(source, fsPath);
  const virtualPosition = view.positionInVirtual(sourceOffset);
  if (virtualPosition === null) return null; // markup — not our layer
  const uri = virtualUriOf(sourceUri);
  proxy.didOpen(uri, view.virtualText);
  return proxy.positionRequest(method, uri, virtualPosition, extra);
}

/** True when `sourceOffset` lands inside an embedded C++ region of `source` (the routing gate the
 *  server uses before deciding whether to consult the proxy at all). */
export function isEmbeddedOffset(source: string, sourceOffset: number): boolean {
  return buildEmbeddedView(source).positionInVirtual(sourceOffset) !== null;
}

// ── embedded-C++ local rename (LSP_COMPLETION_PLAN N6, decision N-10) ───────────────────────

export type EmbeddedRenameResult =
  | { kind: "edits"; edits: Array<{ range: EmbeddedRange; newText: string }> }
  | { kind: "refused"; reason: string };

/**
 * Translate clangd's rename WorkspaceEdit (virtual coordinates) back into THIS .uetkx document.
 * SAFETY over reach (N-10): the rename is single-file by construction — the virtual doc is one
 * TU per .uetkx — and it must be ALL-OR-NOTHING:
 *   - an edit targeting any OTHER uri (an engine header — the cursor was on an engine symbol)
 *     refuses the whole rename; partially renaming a symbol that lives outside the file would
 *     corrupt the build,
 *   - an edit landing in the virtual doc's GLUE (the synthesized prelude/wrappers, unmapped by
 *     the source map) refuses the whole rename — that occurrence has no .uetkx spelling to edit.
 * Returns null when clangd had no result at all (caller degrades silently).
 */
export function translateEmbeddedRename(
  result: unknown,
  sourceUri: string,
  source: string,
  fsPath?: string,
): EmbeddedRenameResult | null {
  if (!result || typeof result !== "object") return null;
  const we = result as {
    changes?: Record<string, Array<{ range: EmbeddedRange; newText: string }>>;
    documentChanges?: Array<{
      textDocument?: { uri?: string };
      edits?: Array<{ range: EmbeddedRange; newText: string }>;
    }>;
  };
  // Normalize both WorkspaceEdit shapes into uri → edits (clangd sends `changes` for our
  // capabilities, but the documentChanges form costs nothing to accept).
  const byUri = new Map<string, Array<{ range: EmbeddedRange; newText: string }>>();
  for (const [uri, edits] of Object.entries(we.changes ?? {})) {
    if (Array.isArray(edits)) byUri.set(uri, [...(byUri.get(uri) ?? []), ...edits]);
  }
  for (const dc of we.documentChanges ?? []) {
    const uri = dc?.textDocument?.uri;
    if (typeof uri === "string" && Array.isArray(dc.edits)) {
      byUri.set(uri, [...(byUri.get(uri) ?? []), ...dc.edits]);
    }
  }
  if (byUri.size === 0) return null;

  const virtualUri = virtualUriOf(sourceUri);
  const view = buildEmbeddedView(source, fsPath);
  const out: Array<{ range: EmbeddedRange; newText: string }> = [];
  for (const [uri, edits] of byUri) {
    if (!URI.sameUri(uri, virtualUri)) {
      return { kind: "refused", reason: "the symbol is declared outside this file (an engine/header symbol) — rename it at its declaration" };
    }
    for (const edit of edits) {
      if (!edit || !edit.range || typeof edit.newText !== "string") continue;
      const startOff = view.sourceOffsetOf(edit.range.start);
      const endOff = view.sourceOffsetOf(edit.range.end);
      if (startOff === null || endOff === null) {
        return { kind: "refused", reason: "an occurrence lives in generated glue outside the .uetkx source — rename would be partial" };
      }
      out.push({
        range: { start: offsetToPosition(source, startOff), end: offsetToPosition(source, endOff) },
        newText: edit.newText,
      });
    }
  }
  if (out.length === 0) return null;
  return { kind: "edits", edits: out };
}

// ── completion translation (the TD-020 tail: hover/definition shipped first) ────────────────

type EmbeddedRange = { start: ClangdPosition; end: ClangdPosition };

/**
 * Translate a clangd completion result (CompletionItem[] or CompletionList) from VIRTUAL-doc
 * coordinates back to .uetkx coordinates:
 *   - a `textEdit` range (both the classic and the insert/replace form) is remapped; an edit that
 *     lands in the prelude scaffolding is DROPPED from its item (the client then inserts the
 *     label at the cursor — still useful) rather than dropping the item,
 *   - `additionalTextEdits` are dropped wholesale (clangd's auto-include insertions target the
 *     virtual prelude — meaningless in a .uetkx file).
 * Returns null for an empty/absent result so the caller falls through to the markup baseline.
 */
export function translateEmbeddedCompletion(
  result: unknown,
  source: string,
  fsPath?: string,
): { isIncomplete: boolean; items: object[] } | null {
  if (!result) return null;
  const list = Array.isArray(result)
    ? { isIncomplete: false, items: result as unknown[] }
    : (result as { isIncomplete?: boolean; items?: unknown[] });
  const rawItems = Array.isArray(list.items) ? list.items : [];
  if (rawItems.length === 0) return null;

  const view = buildEmbeddedView(source, fsPath);
  const mapRange = (range: EmbeddedRange): { start: ClangdPosition; end: ClangdPosition } | null => {
    const startOff = view.sourceOffsetOf(range.start);
    const endOff = view.sourceOffsetOf(range.end);
    if (startOff === null || endOff === null) return null;
    return { start: offsetToPosition(source, startOff), end: offsetToPosition(source, endOff) };
  };

  const items: object[] = [];
  for (const raw of rawItems) {
    if (!raw || typeof raw !== "object") continue;
    const item = { ...(raw as Record<string, unknown>) };
    delete item.additionalTextEdits;
    const edit = item.textEdit as
      | { newText: string; range?: EmbeddedRange; insert?: EmbeddedRange; replace?: EmbeddedRange }
      | undefined;
    if (edit) {
      if (edit.range) {
        const range = mapRange(edit.range);
        if (range) item.textEdit = { range, newText: edit.newText };
        else delete item.textEdit;
      } else if (edit.insert && edit.replace) {
        const insert = mapRange(edit.insert);
        const replace = mapRange(edit.replace);
        if (insert && replace) item.textEdit = { insert, replace, newText: edit.newText };
        else delete item.textEdit;
      } else {
        delete item.textEdit;
      }
    }
    items.push(item);
  }
  if (items.length === 0) return null;
  return { isIncomplete: list.isIncomplete === true, items };
}
