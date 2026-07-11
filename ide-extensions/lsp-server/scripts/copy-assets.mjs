// tsc compiles only .ts — runtime assets (the shipped schema) ride along here.
import { copyFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
mkdirSync(join(root, "out"), { recursive: true });
copyFileSync(join(root, "src", "uetkx-schema.json"), join(root, "out", "uetkx-schema.json"));
console.log("assets copied");
