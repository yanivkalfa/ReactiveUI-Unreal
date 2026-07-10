// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ── docs-sync TEMPLATE: one hook reference page ─────────────────────────────────────────
// Copy to docs/src/pages/Hooks/<HookName>Page.tsx, replace __TOKENS__, register the route
// in docs/src/docs.tsx + the nav in pages.tsx + searchIndex.ts (docs-sync's new-hook row).
// Haiku-class: fill from the hook's source doc comment; a sonnet-class pass checks the
// SEMANTIC claims (dependency behavior, cleanup timing) against the implementation.

import CodeBlock from '../../components/CodeBlock/CodeBlock';

export default function __HOOK__Page() {
  return (
    <>
      <h1><code>__HOOK__</code></h1>

      {/* One-paragraph summary: what it does, when it re-renders the component. */}
      <p>__SUMMARY__</p>

      <h2>Signature</h2>
      <CodeBlock language="cpp">{`__SIGNATURE__ /* from the runtime hooks header — parity-tested against HookStubs.h */`}</CodeBlock>

      <h2>Usage</h2>
      <CodeBlock language="uetkx">{`component Example() {
	__USAGE_SNIPPET__
	return (
		<VBox>
			...
		</VBox>
	)
}`}</CodeBlock>

      <h2>Rules & behavior</h2>
      <ul>
        <li>Rules of hooks apply: top level of the component, stable order, never inside <code>@if</code>/loops.</li>
        <li>__DEPS_BEHAVIOR__ {/* deps: value-equality for value types, identity for shared refs (MASTER_PLAN §5) */}</li>
        <li>__CLEANUP_TIMING__ {/* effects: two-pass cleanup-then-setup at commit end; unmount runs cleanups */}</li>
      </ul>

      <h2>Differences vs the siblings</h2>
      {/* Only if any — cite MASTER_PLAN §5's documented divergences; otherwise: */}
      <p>Behaves identically to the Unity/Godot siblings.</p>
    </>
  );
}
