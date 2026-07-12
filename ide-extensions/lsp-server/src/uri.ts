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
  /** Equality that survives re-serialization: a client/clangd may echo a URI with a different
   *  drive-letter case or `%3A` vs `:` encoding. Compare decoded, slash- and case-normalized so a
   *  virtual-doc location is not mistaken for a real file (bughunt LSP-5). */
  sameUri(a: string, b: string): boolean {
    if (a === b) return true;
    const norm = (u: string): string => {
      let s = u;
      try {
        s = decodeURIComponent(s);
      } catch {
        /* keep raw on malformed escapes */
      }
      return s.replace(/\\/g, "/").toLowerCase();
    };
    return norm(a) === norm(b);
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
