// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// Diagnostics sidecar reader (schema v2+): full compiler diagnostics arrive via
// `Foo.uetkx.diags.json`, gated by src_hash — a sidecar for DIFFERENT content is stale and
// must not be shown (the live parse covers the current text until the next compile).
// TD-024 (fixed, ES-modules M6): the gate is `v >= 2` — the strict `!== 2` check silently
// dropped EVERY compiler sidecar once the driver moved to v3 (the M8 staleness-graph schema);
// live-parse diags were all users saw. Additive v3+ fields (export_hash, dep_hashes, default)
// are simply ignored here.

import * as fs from "node:fs";
import { srcHash } from "./cppScanner";

export interface SidecarDiag {
  code: string;
  severity: number; // 0 err / 1 warn / 2 hint
  message: string;
  off: number; // code points
  len: number;
}

export function readSidecarDiags(uetkxPath: string, currentSource: string): SidecarDiag[] {
  const sidecarPath = uetkxPath + ".diags.json";
  let raw: string;
  try {
    raw = fs.readFileSync(sidecarPath, "utf8");
  } catch {
    return [];
  }
  try {
    const parsed = JSON.parse(raw);
    if (typeof parsed.v !== "number" || parsed.v < 2) return [];
    if ((parsed.src_hash >>> 0) !== srcHash(currentSource)) return []; // stale — different content
    return (parsed.diagnostics ?? []) as SidecarDiag[];
  } catch {
    return [];
  }
}
