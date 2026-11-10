# Mandelbrot Project Instructions

## Code style

Apply these rules to every C++ and shader edit in this Bitloop project, including framework-independent algorithms. Minimal implementation means avoiding unnecessary code, not compressing its formatting. Apply style to the code being changed; do not reformat unrelated code.

- Follow the project `.clang-format`: four-space indentation and Allman braces for C++. Use readable names without member prefixes or suffixes such as `m_` or a trailing `_`; resolve ambiguity with `this->` or clearer parameter names.
- Normally put each declaration and operation on its own line. Keep expressions spaced and wrap long expressions at meaningful boundaries. Simple accessors may stay compact when clearly readable.
- Separate distinct logic groups with blank lines. In ImGui code, a control and its setup, change handler, tooltip and disabled scope form one group; start the next independent control after a blank line.
- Put a blank line before a standalone `//` comment introducing a new section after other code. Keep consecutive comment lines together. Comments at the start of a block need no extra leading blank line; trailing inline comments stay attached to their statements.
- Use short `//` comments for assumptions, meaningful stages and non-obvious numerical choices. Avoid narrating self-evident statements.
- Omit braces for a simple unwrapped condition with one single-line body. Use Allman braces if the condition/header wraps or the body spans multiple lines. A lambda callback or nested control statement counts as a multiline body. Apply the same rule to loops and `else` bodies, preserving normal `else if` chains.
- Format embedded shaders as readable source: spaced expressions, one statement per line, indented blocks and separated functions. Preserve arithmetic and reduction order during formatting cleanup.
- Nest embedded WGSL within the surrounding C++ indentation using four spaces per level. For a shader passed as an argument, put `R"wgsl(` on its own continuation line, indent the shader one additional level, and align `)wgsl"` with the opening marker. For a named initializer, the opening marker may stay beside the declaration; indent the shader one level beyond the declaration and align the closing marker with the declaration. Preserve relative indentation within complete shaders and concatenated fragments.
- Review logical grouping, conditional braces and embedded shader layout explicitly after formatting. clang-format does not enforce these semantic rules or format WGSL inside raw strings.

Use the repository-local `bitloop-development` skill for code style on these edits, even when no Bitloop API is involved.

## Project Boundary

This repository owns the Mandelbrot explorer, its `subprojects/Cardioid` child, project data, CMake and vcpkg wiring, architecture reference, and project-local diagnostics.

Keep the repository independently cloneable, configurable, buildable, and runnable even when the gallery includes it as a child. Do not modify the Bitloop framework, gallery parent, registry, or sibling projects unless the user explicitly requests cross-repository work.

When this project is addressed from the gallery root, continue to honor the gallery's action-authorization, safety, and cross-repository ownership rules.

## Bitloop Guidance

Use the repository-local `bitloop-development` skill for Bitloop lifecycle, drawing, ImGui, camera/input, types, threading, performance, assets, CMake integration, traces, snapshots, checkpoints, temporary diagnostics, and visual QA.

Load only the relevant references. In a gallery build, the checked-out `bitloop` source is authoritative. In a standalone package-backed build, inspect already-installed headers for version-sensitive details when available; do not install or update packages merely to answer a documentation question.

## Architecture Routing

`ARCHITECTURE.md` is the durable descriptive reference. Do not read its 100+ KB in full by default. Search its headings and load only the sections relevant to the current task:

- Ownership, build boundary, lifecycle, data flow, and progressive phases: sections 2–6.
- Precision ladder, kernel dispatch, perturbation, stripes, and line-level kernel behavior: sections 7–9.
- Raw field normalization, GPU upload, tone management, and gradients: section 10.
- Custom multi-pass GLSL parsing, generated API, pass execution, and editor workflow: section 11.
- Saved state, bookmarks, WebP XMP, discovery, loading, and batch rendering: section 12.
- Animation, tweening, steady zoom, and capture payloads: section 13.
- Sidebar controls, live state, statistics, mouse orbit, and undo/redo: section 14.
- Cardioid ownership and geometry helpers: section 15.
- Dormant/unversioned experiments: section 16.
- File/function ownership and current source map: section 17.
- Confirmed risks and sharp edges that must be checked before touching affected code: section 18.
- Pre-change maintenance checklist: section 19.

Line anchors in the architecture document describe the audited 2026-07-22 worktree and may have moved. Confirm current code before relying on an anchor.

## Change Discipline

Preserve the separation between expensive orbit/field computation and comparatively cheap normalization/shading. Classify requested options by the invalidation they require before changing state, history, serialization, phase reuse, or shader behavior.

Implement only the requested behavior. Do not activate dormant kernels or unversioned experiments, add new controls, expand telemetry, or redesign compute phases without explicit scope.

Keep simulation mutation out of draw hooks. Respect the current phased-compute ownership, thread partitioning, interruption boundaries, and GUI-thread resource cleanup.

Ask before changing project architecture, adding a package, broadening per-pixel memory, or moving behavior into the framework.

## Build And Validation

The standalone root target is `Mandelbrot`; the gallery root target is `Gallery` when this child is enabled there. Reuse Codex-owned build contexts and preserve user build state.

Use compilation for API/wiring, semantic traces or focused development probes for behavior, and rendered snapshots for visual claims. Do not infer numerical or state correctness from pixels alone.

The current standalone vcpkg dependency may lag the checked-out framework. Until a matching Bitloop commit is tagged and published in the custom registry, do not run package-backed standalone configure/build as part of agent-instruction validation and do not claim standalone compatibility.

## Plans And Diagnostics

Keep Codex artifacts inside this repository:

```text
.codex/plans/
.codex/runs/run0000_YYYYMMDD_HHMMSS/
```

Preserve existing plans, audit evidence, images, and runs. Use the next sequential run index. Put disposable probes behind `BITLOOP_DEV_MODE` and remove objective-only diagnostics when complete. Reusable framework guidance belongs in `.agents/skills`, not `.codex`.
