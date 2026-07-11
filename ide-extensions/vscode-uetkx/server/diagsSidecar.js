"use strict";
// Diagnostics sidecar reader (schema v2): full compiler diagnostics arrive via
// `Foo.uetkx.diags.json`, gated by src_hash — a sidecar for DIFFERENT content is stale and
// must not be shown (the live parse covers the current text until the next compile).
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
exports.readSidecarDiags = readSidecarDiags;
const fs = __importStar(require("node:fs"));
const cppScanner_1 = require("./cppScanner");
function readSidecarDiags(uetkxPath, currentSource) {
    const sidecarPath = uetkxPath + ".diags.json";
    let raw;
    try {
        raw = fs.readFileSync(sidecarPath, "utf8");
    }
    catch {
        return [];
    }
    try {
        const parsed = JSON.parse(raw);
        if (parsed.v !== 2)
            return [];
        if ((parsed.src_hash >>> 0) !== (0, cppScanner_1.srcHash)(currentSource))
            return []; // stale — different content
        return (parsed.diagnostics ?? []);
    }
    catch {
        return [];
    }
}
//# sourceMappingURL=diagsSidecar.js.map