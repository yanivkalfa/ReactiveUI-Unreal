// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// Minimal file-URI <-> fs-path conversion (avoids a vscode-uri dependency; Windows-aware).

export const URI = {
  toFsPath(uri: string): string {
    if (!uri.startsWith("file://")) return uri;
    let p = decodeURIComponent(uri.slice("file://".length));
    // file:///C:/x -> /C:/x -> C:/x
    if (/^\/[A-Za-z]:/.test(p)) p = p.slice(1);
    return p.replace(/\//g, require("node:path").sep);
  },
  /** fs path -> file URI (go-to-def target). `C:\x\y` -> `file:///C:/x/y`; each segment encoded. */
  fromFsPath(fsPath: string): string {
    let p = fsPath.replace(/\\/g, "/");
    const encoded = p
      .split("/")
      .map((seg) => encodeURIComponent(seg).replace(/%3A/gi, ":"))
      .join("/");
    return encoded.startsWith("/") ? "file://" + encoded : "file:///" + encoded;
  },
};
