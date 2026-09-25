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


## 1.0.1a desktop UI contract

- Treat `1.0.1a` as the current product/plugin version string. CMake project versions stay numeric (`1.0.1`) where required.
- Dashboard, Settings, Welcome, portal pickers and desktop-visible plugin UI must share the LunaDash design system: strong/glass surfaces, hairline borders, shared radii and bounded layouts.
- Dashboard content must use Qt Quick layouts, bounded card sizes, clipping and text elision; do not use unbounded absolute text placement that can overlap at runtime.
- Settings must remain usable at its default size and when maximized. Search belongs in the header; page implementations must not invent separate window chrome.
- Portal FileChooser UI must keep the LunaDash-owned frameless wrapper, dark header and manual local-path field. Never evaluate a selected path through a shell.
- Desktop-widget plugins render inside the wallpaper Background layer. Do not create Top/Overlay windows for desktop widgets.
- `desktop-widgets` is multi-selection so clocks, visualizers and other wallpaper widgets can coexist.
- The removed Command Console must not return to built-ins, startup settings, tests, docs or launcher entries. Use the configured terminal role.
- “Motion” is not a standalone settings feature. Use “Visual effects” for appearance controls and “Reduced motion” only for accessibility.
- Native plugin metadata is validation, not isolation; never describe native plugins as sandboxed.
- Update the matching English and Traditional Chinese canonical docs whenever visible desktop behavior changes. Root `docs/*.md` compatibility paths remain small forwarding pages.
- After UI changes, run QML design/action audits plus the relevant Qt/Wayland integration workflows; a real-session screenshot is still required for final visual verification.
