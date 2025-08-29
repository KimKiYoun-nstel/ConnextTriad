# Coding Guidelines (C/C++)

## Style
- Base: Google C++ Style; enforced via `.clang-format` (4 spaces, 120 cols, pointer `*` attached to type).
- Use `.editorconfig` for EOL/whitespace consistency.
- Doxygen comments **required** for all classes, functions, and member data.

## Language Level
- Classic C++ (C++03/limited C++11). Avoid modern constructs that harm portability (concepts/ranges/coroutines).
- Prefer RAII; minimize raw `new/delete`. Smart pointers allowed if ABI/API permits.

## Includes & Headers
- Public headers live under `RtpDdsGateway/include/` (or respective module `include/`).
- Keep headers self-contained and minimal; forward-declare when possible.

## Error Handling
- Return explicit status or boolean; avoid exceptions across DDS call boundaries.
- Convert DDS return codes to clear ACK/ERR responses via `ipc_adapter`.

## Concurrency
- `DkmRtpIpc` owns its recv thread; guard shared state with mutex.
- Listener callbacks must be lightweight; offload processing to converter/IPC.

## Naming & Layout
- Types `PascalCase`, functions `camelCase`, constants `kCamelCase` (Google style).
- One responsibility per file; short functions (<50 lines) where feasible.

## Docs & Examples
- Use Doxygen blocks:
  ```cpp
  /**
   * @brief Initializes DDS subscriber.
   * @param domain_id DDS domain id.
   * @return true on success.
   */
  bool init(int domain_id);
  ```

## Documentation Language
- All comments (including Doxygen), commit messages, and project documentation SHOULD be written in **Korean**.
- Code identifiers (types, functions, enums, constants) and external API names MUST remain in **English**.
- Example Doxygen should use Korean prose while keeping type and API symbols in English.

## Log Message Language
All log messages in source code MUST be written in **English**.

## No Duplication
- Before adding APIs, search `include/` & `src/` for prior art; extend rather than copy.
