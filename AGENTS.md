# LuDash project rules

- Implement a C++20 / OpenGL / Wayland desktop with a C11 low-level rendering core. Do not add an X11 window manager.
- Put all project C++ types and functions in namespace `LunaDash` (except `main`).
- Keep interfaces and implementations together in their owning lowercase source domain. Use PascalCase `.hpp` / `.cpp` filenames and source-root project includes.
- C core functions use the `ludash_` prefix; expose C declarations inside `LunaDash` with C linkage when included from C++.
- Run `scripts/check-source-layout.py` after changing native source layout or build lists.
- Keep raw OpenGL code and built-in shaders under `src/compositor/renderer/opengl/`; embed shaders through CMake.
- Headers declare interfaces and types; `.cpp` files contain implementations. Do not accumulate unrelated features in a shared implementation file.
- Keep small `Main.cpp` entry points in their owning executable domain, and add every source explicitly to CMake targets.
- Finish the intended code, packaging, and documentation changes before building. Do not build after each intermediate edit.
- Target Arch Linux first, with portable CMake support and documented dependencies for other Linux distributions. Distinguish configured CI from actually verified platforms.
- Do not describe this development version as a production-ready KDE replacement. Document missing protocol and session features accurately.
- Implement the desktop shell UI in Quickshell/QML under `qml/<feature>/`; keep the compositor and backend in C++.
- Keep C, C++ and QML source text in English. Traditional Chinese belongs only in the external language pack under `data/translations/`.
- Keep native plugins disabled by default. Plugin metadata is not a sandbox and does not make native code safe.
