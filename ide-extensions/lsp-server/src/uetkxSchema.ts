// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// The markup vocabulary: SHIPPED default (src/uetkx-schema.json — the committed RUIExportSchema
// output) overridden by the workspace's live export at <workspace>/Saved/ReactiveUI/schema.json
// when present (regenerate with `UnrealEditor-Cmd <proj>.uproject -run=RUIExportSchema`).

import * as fs from "node:fs";
import * as path from "node:path";

export interface UetkxSchema {
  v: number;
  elements: Record<string, { factory: string; children: boolean; attrs: Record<string, string>; sinceUE?: string }>;
  styleKeys: string[];
  slotPrefix: string;
  slotKeys: string[];
  hooks: string[];
  /** TD-016: event attr name -> payload kind (text|bool|float|int|name|color|vector2|void) — the
   *  FRuiValue field an event handler's `Value` carries. Absent in older shipped schemas. */
  eventPayloads?: Record<string, string>;
  /** R10: attr/style/slot key -> the CLOSED set of accepted string values, exported from the
   *  runtime's own parse tables (ParseHAlign et al) — those parses fall back SILENTLY, so a
   *  typo'd value is invisible at compile AND run time without this. FName comparison is
   *  case-insensitive; validate accordingly. Absent in older shipped schemas. */
  attrEnums?: Record<string, string[]>;
  /** R11: typed style/slot key -> value kind (float|int|bool|vector2|margin|color). The runtime
   *  parses well-formed string literals through the SLOT-1 hardened readers; malformed ones
   *  still Atof to 0/false silently — this drives the LSP string-format check. color has no
   *  string form at all. Absent in older shipped schemas. */
  attrKinds?: Record<string, string>;
  /** R12: container tag -> the Slot.* keys its slot-apply actually READS (empty = the parent
   *  passes no slot props to children at all — SingleContent SetContent path). A slot key the
   *  parent never reads is dropped in TOTAL silence at runtime. Root elements behave as
   *  children of the implicit SOverlay root panel. Absent in older shipped schemas. */
  slotConsumption?: Record<string, string[]>;
  /** R13: attrs whose string value is a BRUSH NAME (BorderImage) — resolved at runtime
   *  exclusively through FCoreStyle::Get(), the fixed engine style set. */
  brushAttrs?: string[];
  /** R13: the engine's registered FCoreStyle brush names, enumerated by the editor module at
   *  schema-export time (closed per engine version). Absent when the export ran in a bare
   *  unit context — validation and completion disarm without it. */
  brushNames?: string[];
}

let shipped: UetkxSchema | null = null;

export function shippedSchema(): UetkxSchema {
  if (!shipped) {
    shipped = JSON.parse(fs.readFileSync(path.join(__dirname, "uetkx-schema.json"), "utf8")) as UetkxSchema;
  }
  return shipped;
}

/** Walk up from fileDir looking for Saved/ReactiveUI/schema.json next to a .uproject. */
export function schemaForFile(fileDir: string): UetkxSchema {
  let dir = fileDir;
  for (let depth = 0; depth < 32; depth++) {
    try {
      const entries = fs.readdirSync(dir);
      if (entries.some((e) => e.endsWith(".uproject"))) {
        const live = path.join(dir, "Saved", "ReactiveUI", "schema.json");
        if (fs.existsSync(live)) {
          try {
            return JSON.parse(fs.readFileSync(live, "utf8")) as UetkxSchema;
          } catch {
            return shippedSchema(); // malformed live export — shipped baseline
          }
        }
        break;
      }
    } catch {
      break;
    }
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return shippedSchema();
}

/** R12: the project's engine version from the nearest .uproject's EngineAssociation, as
 *  [major, minor] — null when absent or not a plain "X.Y" (custom-engine GUIDs, source builds).
 *  Drives the sinceUE check: a too-new element renders a NULL SLOT at runtime (the adapter is
 *  compiled out; the tag/factory still compile — LogRuiSlate error at mount is the only
 *  runtime signal). */
export function engineVersionForFile(fileDir: string): [number, number] | null {
  let dir = fileDir;
  for (let depth = 0; depth < 32; depth++) {
    try {
      const entries = fs.readdirSync(dir);
      const up = entries.find((e) => e.endsWith(".uproject"));
      if (up) {
        const parsed = JSON.parse(fs.readFileSync(path.join(dir, up), "utf8")) as { EngineAssociation?: string };
        const m = /^(\d+)\.(\d+)/.exec(parsed.EngineAssociation ?? "");
        return m ? [Number(m[1]), Number(m[2])] : null;
      }
    } catch {
      return null;
    }
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return null;
}

/** The nearest uetkx.config.json and the directory it lives in (nearest wins, NO merge; malformed
 *  = {}). configDir is null when no config is found. Mirrors C++ FUetkxConfig::Load (M1): one
 *  walk-up shared by the formatter (options) and the import resolver (the `~/` root anchor). */
export function loadUetkxConfig(fileDir: string): { config: Record<string, unknown>; configDir: string | null } {
  let dir = fileDir;
  for (let depth = 0; depth < 32; depth++) {
    const candidate = path.join(dir, "uetkx.config.json");
    if (fs.existsSync(candidate)) {
      try {
        return { config: JSON.parse(fs.readFileSync(candidate, "utf8")), configDir: dir };
      } catch {
        return { config: {}, configDir: dir }; // nearest config wins even when malformed
      }
    }
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return { config: {}, configDir: null };
}

/** uetkx.config.json walk-up (nearest wins; malformed = {}). */
export function loadFormatterConfig(fileDir: string): Record<string, unknown> {
  return loadUetkxConfig(fileDir).config;
}

/** The `~/` family-root anchor for a .uetkx file, or null when no config declares `"root"` (the
 *  caller falls back to the module root — the *.Build.cs walk, done engine-side / in M11's
 *  workspace index). The `"root"` string is relative to the config file's own directory (A5g:
 *  nearest-config-wins, no inheritance). */
export function rootAnchorFor(fileDir: string): string | null {
  const { config, configDir } = loadUetkxConfig(fileDir);
  const root = config.root;
  if (configDir && typeof root === "string" && root.trim() !== "") {
    return path.resolve(configDir, root.trim());
  }
  return null;
}
