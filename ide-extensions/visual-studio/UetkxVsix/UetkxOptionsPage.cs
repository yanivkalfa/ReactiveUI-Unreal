// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
using System.ComponentModel;
using Microsoft.VisualStudio.Shell;

namespace UetkxVsix
{
    // Tools > Options > ReactiveUI > UETKX. Persists the extension's editor-polish settings; the
    // package reads FormatOnSave in its Running-Document-Table save hook.
    public sealed class UetkxOptionsPage : DialogPage
    {
        [Category("Formatting")]
        [DisplayName("Format on Save")]
        [Description("Run Format Document on a .uetkx file whenever it is saved (uses the golden-corpus formatter over LSP).")]
        public bool FormatOnSave { get; set; } = false;
    }
}
