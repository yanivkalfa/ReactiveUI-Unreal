// Minimal file-URI <-> fs-path conversion (avoids a vscode-uri dependency; Windows-aware).

export const URI = {
  toFsPath(uri: string): string {
    if (!uri.startsWith("file://")) return uri;
    let p = decodeURIComponent(uri.slice("file://".length));
    // file:///C:/x -> /C:/x -> C:/x
    if (/^\/[A-Za-z]:/.test(p)) p = p.slice(1);
    return p.replace(/\//g, require("node:path").sep);
  },
};
