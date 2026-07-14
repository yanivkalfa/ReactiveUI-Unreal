import { Prism } from 'prism-react-renderer'

// prism-react-renderer ships a fixed, curated set of Prism grammars (markup, jsx, tsx, json,
// python, yaml, css, go, rust, …) but NOT uetkx. `.uetkx` is a JSX-like markup format with
// embedded C++-flavoured expressions (the same family grammar as Godot's `.guitkx` and Unity's
// `.uitkx`), so any `language="uetkx"` code block would otherwise render as a single
// un-tokenized run — no tag/keyword/string colour at all. We register a small uetkx grammar on
// the SAME Prism instance that <Highlight> uses (prism-react-renderer re-exports it) so those
// blocks highlight like the bundled languages.
//
// Deliberately approximate (flat, no nested embedded-language regions), mirroring how the Godot
// docs register their GDScript grammar. TODO(Phase 2): tighten against the real toolchain
// grammar (ide-extensions/grammar) once the .uetkx compiler lands.

if (!Prism.languages.uetkx) {
  Prism.languages.uetkx = {
    comment: {
      pattern: /\/\/.*|\/\*[\s\S]*?(?:\*\/|$)/,
      greedy: true,
    },
    string: {
      pattern: /"(?:\\[\s\S]|[^\\"\r\n])*"/,
      greedy: true,
    },
    directive: {
      // Markup control flow: @if, @elif, @else, @for, @while, @match, @case, @default.
      pattern: /@[A-Za-z_]\w*/,
      alias: 'keyword',
    },
    preprocessor: {
      // Preamble includes: #include "X.h" (the raw-C++ half of a .uetkx preamble).
      pattern: /^#\s*\w+/m,
      alias: 'keyword',
    },
    tag: {
      // Opening/closing markup tags: <VerticalBox, </TextBlock, and the closers /> and >.
      pattern: /<\/?[A-Za-z][\w.]*|\/?>/,
      inside: {
        punctuation: /^<\/?|\/?>$/,
      },
    },
    // Dotted attr names colorize whole: Slot.Padding=, Font.Size=, not just the tail.
    'attr-name': /\b[A-Za-z_][\w.]*(?==)/,
    keyword:
      /\b(?:component|hook|module|export|import|from|return|if|else|elif|for|in|while|switch|match|case|break|continue|auto|const|constexpr|nullptr|this|void|new|delete)\b/,
    'class-name': [
      {
        // Type positions: `(Title: FText, Count: int32 = 0)`.
        pattern: /(:[ \t]*)[A-Za-z_][\w:<>*]*/,
        lookbehind: true,
      },
      // Unreal-style type names used bare (FText, TArray, EVisibility), PascalCase ctor calls,
      // and namespaces before :: (RUI::Fmt, RuiDemo::GetDemoWorld).
      /\b(?:[FUTE][A-Z]\w*|[A-Z]\w*(?=\s*(?:::|[.(])))\b/,
    ],
    builtin:
      /\b(?:int8|int16|int32|int64|uint8|uint16|uint32|uint64|float|double|bool|char)\b/,
    function: /\b[a-z_]\w*(?=\s*\()/i,
    number: /\b0x[\da-fA-F]+\b|(?:\b\d+(?:\.\d*)?|\B\.\d+)(?:e[+-]?\d+)?f?\b/i,
    boolean: /\b(?:false|true)\b/,
    constant: /\b[A-Z][A-Z_\d]*\b/,
    operator: /->|\+\+|--|&&|\|\||[-+*/%&|^!<>=]=?|[?~]/,
    punctuation: /[{}[\];(),.:]/,
  }
}

export { Prism }
