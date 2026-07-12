# Changelog

## [0.2.0] - 2026-07-12
- Editor polish (TD-020 tail): a Tools > Options > ReactiveUI > UETKX page with a Format on Save setting; format-on-save runs Format Document on a .uetkx file (via the golden-corpus formatter over LSP) when the option is enabled. Brace completion + smart indent ride the shared language-configuration. The extension now loads a background package on solution-exists to arm the save hook.
