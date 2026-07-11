using System.ComponentModel.Composition;
using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Utilities;

namespace UetkxVsix
{
    // Defines the `uetkx` content type and maps .uetkx to it. Deriving from
    // CodeRemoteContentDefinition is what makes VS's LSP client (ILanguageClient) attach to
    // .uetkx editors; coloring comes from the TextMate grammar registered in uetkx.pkgdef
    // (VS never drives colorization over LSP).
    public static class UetkxContentDefinition
    {
        [Export]
        [Name("uetkx")]
        [BaseDefinition(CodeRemoteContentDefinition.CodeRemoteContentTypeName)]
        internal static ContentTypeDefinition UetkxContentTypeDefinition;

        [Export]
        [FileExtension(".uetkx")]
        [ContentType("uetkx")]
        internal static FileExtensionToContentTypeDefinition UetkxFileExtensionDefinition;
    }
}
