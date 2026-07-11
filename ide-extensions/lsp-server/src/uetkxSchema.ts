// The markup vocabulary: SHIPPED default (src/uetkx-schema.json — the committed RUIExportSchema
// output) overridden by the workspace's live export at <workspace>/Saved/ReactiveUI/schema.json
// when present (regenerate with `UnrealEditor-Cmd <proj>.uproject -run=RUIExportSchema`).

import * as fs from "node:fs";
import * as path from "node:path";

export interface UetkxSchema {
  v: number;
  elements: Record<string, { factory: string; children: boolean; attrs: Record<string, string> }>;
  styleKeys: string[];
  slotPrefix: string;
  slotKeys: string[];
  hooks: string[];
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

/** uetkx.config.json walk-up (nearest wins; malformed = {}). */
export function loadFormatterConfig(fileDir: string): Record<string, unknown> {
  let dir = fileDir;
  for (let depth = 0; depth < 32; depth++) {
    const candidate = path.join(dir, "uetkx.config.json");
    if (fs.existsSync(candidate)) {
      try {
        return JSON.parse(fs.readFileSync(candidate, "utf8"));
      } catch {
        return {};
      }
    }
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return {};
}
