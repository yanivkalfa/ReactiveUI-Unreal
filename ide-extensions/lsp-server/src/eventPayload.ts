// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
// TD-016: typed event-payload intelligence. An event handler body sees the widget's payload as an
// `FRuiValue Value`; the schema's `eventPayloads` map types each event (text/bool/float/…), so the
// LSP can complete `Value.<Field>` with the RIGHT field first and hover an event attr with its
// payload. Payload kind -> the FRuiValue member + its C++ type.

export interface PayloadField {
  field: string;
  type: string;
  kind: string;
}

/** Every FRuiValue payload member, in a stable order. `kind` matches the schema's payload kinds. */
export const PAYLOAD_FIELDS: PayloadField[] = [
  { field: "TextValue", type: "FText", kind: "text" },
  { field: "BoolValue", type: "bool", kind: "bool" },
  { field: "FloatValue", type: "double", kind: "float" },
  { field: "IntValue", type: "int64", kind: "int" },
  { field: "NameValue", type: "FName", kind: "name" },
  { field: "ColorValue", type: "FLinearColor", kind: "color" },
  { field: "Vector2Value", type: "FVector2D", kind: "vector2" },
];

/** The FRuiValue field for a payload kind, or null for `void`/unknown. */
export function fieldForKind(kind: string | undefined): PayloadField | null {
  if (!kind || kind === "void") return null;
  return PAYLOAD_FIELDS.find((f) => f.kind === kind) ?? null;
}

/** The attribute whose `={ … }` value expression encloses `off`, or null. Walks back balancing
 *  braces; the enclosing `{` must be preceded by `=` and an identifier (the attr name). Bails at a
 *  tag boundary. UTF-16 offsets (attr names + markup are ASCII). */
export function enclosingAttrName(text: string, off: number): string | null {
  let depth = 0;
  const limit = Math.max(0, off - 4000);
  // Mark string/char-literal spans so a `{`/`}`/`<`/`>` INSIDE a handler string (e.g.
  // `OnClicked={ SetText(TEXT("}")) }`) never miscounts brace depth or fakes a tag boundary (LSP-4).
  const inStr: boolean[] = new Array(off).fill(false);
  {
    let quote: string | null = null;
    let esc = false;
    for (let i = limit; i < off; i++) {
      const c = text[i];
      if (quote) {
        inStr[i] = true;
        if (esc) esc = false;
        else if (c === "\\") esc = true;
        else if (c === quote) quote = null;
      } else if (c === '"' || c === "'") {
        quote = c;
        inStr[i] = true;
      }
    }
  }
  for (let i = off - 1; i >= limit; i--) {
    if (inStr[i]) continue;
    const c = text[i];
    if (c === "}") {
      depth++;
    } else if (c === "{") {
      if (depth === 0) {
        let j = i - 1;
        while (j >= 0 && (text[j] === " " || text[j] === "\t")) j--;
        if (text[j] !== "=") return null;
        j--;
        while (j >= 0 && (text[j] === " " || text[j] === "\t")) j--;
        const end = j + 1;
        while (j >= 0 && /[A-Za-z0-9_.]/.test(text[j])) j--;
        const name = text.slice(j + 1, end);
        return name || null;
      }
      depth--;
    } else if ((c === "<" || c === ">") && depth === 0) {
      return null; // left the tag without finding an attr `={`
    }
  }
  return null;
}
