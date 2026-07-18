// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// The VS Code client: starts the bundled uetkx-language-server over IPC and exposes a
// restart command. The server is self-contained JS (no native deps) — one .vsix serves
// every platform.

import * as fs from "fs";
import * as path from "path";
import { commands, ExtensionContext, window, workspace } from "vscode";
import { LanguageClient, LanguageClientOptions, ServerOptions, TransportKind } from "vscode-languageclient/node";

let client: LanguageClient | undefined;

function buildAndStartClient(module: string): LanguageClient {
  const serverOptions: ServerOptions = {
    run: { module, transport: TransportKind.ipc },
    debug: { module, transport: TransportKind.ipc, options: { execArgv: ["--nolazy", "--inspect=6010"] } },
  };
  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: "file", language: "uetkx" }],
    // TB-10: hand the server the clangd override for embedded-C++ intelligence (empty = discover).
    initializationOptions: {
      clangdPath: workspace.getConfiguration("uetkx").get<string>("clangd.path", ""),
    },
  };
  const c = new LanguageClient("uetkx", "UETKX Language Server", serverOptions, clientOptions);
  c.start();
  return c;
}

export function activate(context: ExtensionContext): void {
  const bundled = context.asAbsolutePath(path.join("server", "server.js"));
  if (!fs.existsSync(bundled)) {
    window.showErrorMessage("UETKX: bundled language server missing — run `npm run build` in the extension.");
    return;
  }
  client = buildAndStartClient(bundled);
  context.subscriptions.push(
    commands.registerCommand("uetkx.restartLanguageServer", async () => {
      if (client) {
        await client.restart();
        window.setStatusBarMessage("UETKX language server restarted", 3000);
      }
    }),
  );
}

export function deactivate(): Thenable<void> | undefined {
  return client?.stop();
}
