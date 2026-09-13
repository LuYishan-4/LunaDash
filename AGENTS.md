# LuDash project rules

- Implement a C++20 / OpenGL / Wayland desktop. Do not add an X11 window manager.
- Put all project C++ types and functions in namespace `LuDash` (except `main`).
- Give every new feature a dedicated directory pair: `include/LuDash/<feature>/` and `src/<feature>/`.
- Headers declare interfaces and types; `.cpp` files contain implementations. Do not accumulate unrelated features in a shared implementation file.
- Keep entry points in `src/entrypoints/`, and add sources explicitly to CMake targets.
- Finish the intended code, packaging, and documentation changes before building. Do not build after each intermediate edit.
- Target Arch Linux first, with portable CMake support and documented dependencies for other Linux distributions. Distinguish configured CI from actually verified platforms.
- Do not describe this development version as a production-ready KDE replacement. Document missing protocol and session features accurately.
- Implement the desktop shell UI in Quickshell/QML under `qml/<feature>/`; keep the compositor and backend in C++.
- Keep C++ and QML source text in English. Traditional Chinese belongs only in the external language pack under `data/translations/`.
- Keep native plugins disabled by default. Plugin metadata is not a sandbox and does not make native code safe.
