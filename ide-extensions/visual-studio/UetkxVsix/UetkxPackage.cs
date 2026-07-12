// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
using System;
using System.Runtime.InteropServices;
using System.Threading;
using EnvDTE;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Task = System.Threading.Tasks.Task;

namespace UetkxVsix
{
    // The extension's editor-polish package: hosts the Tools>Options page and the format-on-save
    // hook. Loads in the background on solution-exists (so the RDT save listener is armed without
    // slowing startup). Brace completion + smart indent ride language-configuration.json.
    [PackageRegistration(UseManagedResourcesOnly = true, AllowsBackgroundLoading = true)]
    [Guid(PackageGuidString)]
    [ProvideOptionPage(typeof(UetkxOptionsPage), "ReactiveUI", "UETKX", 0, 0, supportsAutomation: true)]
    [ProvideAutoLoad(VSConstants.UICONTEXT.SolutionExists_string, PackageAutoLoadFlags.BackgroundLoad)]
    public sealed class UetkxPackage : AsyncPackage, IVsRunningDocTableEvents3
    {
        public const string PackageGuidString = "7E2B3D9C-6A41-4C58-9E7A-1F0B2C3D4E5F";

        private RunningDocumentTable _rdt;
        private uint _cookie;

        protected override async Task InitializeAsync(CancellationToken cancellationToken,
                                                      IProgress<ServiceProgressData> progress)
        {
            await JoinableTaskFactory.SwitchToMainThreadAsync(cancellationToken);
            _rdt = new RunningDocumentTable(this);
            _cookie = _rdt.Advise(this);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && _rdt != null && _cookie != 0)
            {
                _rdt.Unadvise(_cookie);
                _cookie = 0;
            }
            base.Dispose(disposing);
        }

        // ── format-on-save ──────────────────────────────────────────────────────────────────────
        public int OnBeforeSave(uint docCookie)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            var options = GetDialogPage(typeof(UetkxOptionsPage)) as UetkxOptionsPage;
            if (options == null || !options.FormatOnSave)
            {
                return VSConstants.S_OK;
            }
            var moniker = _rdt.GetDocumentInfo(docCookie).Moniker;
            if (string.IsNullOrEmpty(moniker) || !moniker.EndsWith(".uetkx", StringComparison.OrdinalIgnoreCase))
            {
                return VSConstants.S_OK;
            }
            // Format the active document via the LSP formatting the .uetkx client provides. Best-effort:
            // a save of a non-active document is a rare case and simply skips (never blocks the save).
            try
            {
                if (GetService(typeof(DTE)) is DTE dte && dte.ActiveDocument != null &&
                    string.Equals(dte.ActiveDocument.FullName, moniker, StringComparison.OrdinalIgnoreCase))
                {
                    dte.ExecuteCommand("Edit.FormatDocument");
                }
            }
            catch
            {
                // Formatting is a convenience — never let it interrupt a save.
            }
            return VSConstants.S_OK;
        }

        // ── remaining IVsRunningDocTableEvents3 members: no-ops ──────────────────────────────────
        public int OnAfterFirstDocumentLock(uint c, uint a, uint b, uint d) => VSConstants.S_OK;
        public int OnBeforeLastDocumentUnlock(uint c, uint a, uint b, uint d) => VSConstants.S_OK;
        public int OnAfterSave(uint c) => VSConstants.S_OK;
        public int OnAfterAttributeChange(uint c, uint a) => VSConstants.S_OK;
        public int OnBeforeDocumentWindowShow(uint c, int f, IVsWindowFrame frame) => VSConstants.S_OK;
        public int OnAfterDocumentWindowHide(uint c, IVsWindowFrame frame) => VSConstants.S_OK;
        public int OnAfterAttributeChangeEx(uint c, uint a, IVsHierarchy oh, uint oid, string om, IVsHierarchy nh,
                                            uint nid, string nm) => VSConstants.S_OK;
    }
}
