# Cardioid Project Instructions

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
