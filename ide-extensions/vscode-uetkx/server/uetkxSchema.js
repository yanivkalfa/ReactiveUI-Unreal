"use strict";
// The markup vocabulary: SHIPPED default (src/uetkx-schema.json — the committed RUIExportSchema
// output) overridden by the workspace's live export at <workspace>/Saved/ReactiveUI/schema.json
// when present (regenerate with `UnrealEditor-Cmd <proj>.uproject -run=RUIExportSchema`).
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.shippedSchema = shippedSchema;
exports.schemaForFile = schemaForFile;
exports.loadFormatterConfig = loadFormatterConfig;
const fs = __importStar(require("node:fs"));
const path = __importStar(require("node:path"));
let shipped = null;
function shippedSchema() {
    if (!shipped) {
        shipped = JSON.parse(fs.readFileSync(path.join(__dirname, "uetkx-schema.json"), "utf8"));
    }
    return shipped;
}
/** Walk up from fileDir looking for Saved/ReactiveUI/schema.json next to a .uproject. */
function schemaForFile(fileDir) {
    let dir = fileDir;
    for (let depth = 0; depth < 32; depth++) {
        try {
            const entries = fs.readdirSync(dir);
            if (entries.some((e) => e.endsWith(".uproject"))) {
                const live = path.join(dir, "Saved", "ReactiveUI", "schema.json");
                if (fs.existsSync(live)) {
                    try {
                        return JSON.parse(fs.readFileSync(live, "utf8"));
                    }
                    catch {
                        return shippedSchema(); // malformed live export — shipped baseline
                    }
                }
                break;
            }
        }
        catch {
            break;
        }
        const parent = path.dirname(dir);
        if (parent === dir)
            break;
        dir = parent;
    }
    return shippedSchema();
}
/** uetkx.config.json walk-up (nearest wins; malformed = {}). */
function loadFormatterConfig(fileDir) {
    let dir = fileDir;
    for (let depth = 0; depth < 32; depth++) {
        const candidate = path.join(dir, "uetkx.config.json");
        if (fs.existsSync(candidate)) {
            try {
                return JSON.parse(fs.readFileSync(candidate, "utf8"));
            }
            catch {
                return {};
            }
        }
        const parent = path.dirname(dir);
        if (parent === dir)
            break;
        dir = parent;
    }
    return {};
}
//# sourceMappingURL=uetkxSchema.js.map