// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-020 — the source map between a .uetkx document and its generated C++ virtual document. The
// virtual doc concatenates the embedded-C++ regions (setup blocks, hook/module bodies) into
// synthetic functions clangd can reason about; each region contributes ONE span that maps a
// contiguous run of virtual offsets back to the original .uetkx offsets 1:1. Requests (hover,
// completion, definition) translate .uetkx offsets -> virtual offsets on the way IN, and clangd's
// answer ranges translate virtual -> .uetkx on the way OUT.

/** One contiguous region copied verbatim from the .uetkx source into the virtual C++ doc. */
export interface SourceSpan {
  srcStart: number; // offset in the .uetkx document
  virStart: number; // offset in the virtual C++ document
  length: number; // characters (identical in both, copied verbatim)
}

/** Offset <-> {line, character} over a specific text (UTF-16 offsets, LSP-style). */
export function offsetToPosition(text: string, offset: number): { line: number; character: number } {
  const clamped = Math.max(0, Math.min(offset, text.length));
  let line = 0;
  let lineStart = 0;
  for (let i = 0; i < clamped; i++) {
    if (text.charCodeAt(i) === 10 /* \n */) {
      line++;
      lineStart = i + 1;
    }
  }
  return { line, character: clamped - lineStart };
}

export function positionToOffset(text: string, position: { line: number; character: number }): number {
  let offset = 0;
  let line = 0;
  while (line < position.line && offset < text.length) {
    if (text.charCodeAt(offset) === 10) {
      line++;
    }
    offset++;
  }
  return Math.min(offset + Math.max(0, position.character), text.length);
}

/** A bidirectional, span-based map. Spans are kept sorted by srcStart for binary search. */
export class SourceMap {
  private readonly bySrc: SourceSpan[];
  private readonly byVir: SourceSpan[];

  constructor(spans: readonly SourceSpan[]) {
    this.bySrc = [...spans].sort((a, b) => a.srcStart - b.srcStart);
    this.byVir = [...spans].sort((a, b) => a.virStart - b.virStart);
  }

  get spanCount(): number {
    return this.bySrc.length;
  }

  /** .uetkx offset -> virtual offset, or null when the offset is outside every embedded region. */
  sourceToVirtual(srcOffset: number): number | null {
    const span = SourceMap.find(this.bySrc, (s) => s.srcStart, srcOffset);
    if (span && srcOffset >= span.srcStart && srcOffset <= span.srcStart + span.length) {
      return span.virStart + (srcOffset - span.srcStart);
    }
    return null;
  }

  /** virtual offset -> .uetkx offset, or null when inside synthetic scaffolding (not user code). */
  virtualToSource(virOffset: number): number | null {
    const span = SourceMap.find(this.byVir, (s) => s.virStart, virOffset);
    if (span && virOffset >= span.virStart && virOffset <= span.virStart + span.length) {
      return span.srcStart + (virOffset - span.virStart);
    }
    return null;
  }

  /** Largest span whose keyed start is <= value (binary search on a sorted array). */
  private static find(
    spans: readonly SourceSpan[],
    key: (s: SourceSpan) => number,
    value: number,
  ): SourceSpan | null {
    let lo = 0;
    let hi = spans.length - 1;
    let best: SourceSpan | null = null;
    while (lo <= hi) {
      const mid = (lo + hi) >> 1;
      if (key(spans[mid]) <= value) {
        best = spans[mid];
        lo = mid + 1;
      } else {
        hi = mid - 1;
      }
    }
    return best;
  }
}
