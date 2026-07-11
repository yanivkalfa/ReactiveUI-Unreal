// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;
using Task = System.Threading.Tasks.Task;

namespace UetkxVsix
{
    // Launches the shared TypeScript uetkx-language-server (the same bundle the VS Code
    // extension ships) as a Node child process over stdio. Prefers a bundled server\node.exe;
    // falls back to `node` on PATH.
    [ContentType("uetkx")]
    [Export(typeof(ILanguageClient))]
    public sealed class UetkxLanguageClient : ILanguageClient
    {
        public string Name => "UETKX Language Server";

        public IEnumerable<string> ConfigurationSections => new[] { "uetkx" };
        public object InitializationOptions => null;
        public IEnumerable<string> FilesToWatch => null;
        public bool ShowNotificationOnInitializeFailed => true;

        public event AsyncEventHandler<EventArgs> StartAsync;
        public event AsyncEventHandler<EventArgs> StopAsync;

        public async Task<Connection> ActivateAsync(CancellationToken token)
        {
            await Task.Yield();
            var dir = Path.GetDirectoryName(typeof(UetkxLanguageClient).Assembly.Location);
            var serverDir = Path.Combine(dir, "server");
            var serverPath = Path.Combine(serverDir, "server.js");
            if (!File.Exists(serverPath))
                return null;

            var bundledNode = Path.Combine(serverDir, "node.exe");
            var nodeExe = File.Exists(bundledNode) ? bundledNode : "node";

            var info = new ProcessStartInfo
            {
                FileName = nodeExe,
                Arguments = $"\"{serverPath}\" --stdio",
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true,
                WorkingDirectory = dir,
            };

            var process = new Process { StartInfo = info };
            if (process.Start())
            {
                return new Connection(process.StandardOutput.BaseStream, process.StandardInput.BaseStream);
            }
            return null;
        }

        public async Task OnLoadedAsync()
        {
            if (StartAsync != null)
                await StartAsync.InvokeAsync(this, EventArgs.Empty);
        }

        public Task OnServerInitializedAsync() => Task.CompletedTask;

        public Task<InitializationFailureContext> OnServerInitializeFailedAsync(ILanguageClientInitializationInfo initializationState)
        {
            return Task.FromResult(new InitializationFailureContext
            {
                FailureMessage = "UETKX language server failed to start. The extension expects a bundled Node " +
                                 "runtime (server\\node.exe) or Node.js on PATH. " + initializationState.StatusMessage,
            });
        }
    }
}
