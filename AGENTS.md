# LuDash project rules

- Implement a C++20 / OpenGL / Wayland desktop with a C11 low-level rendering core. Use wlroots for compositor, input, and Wayland protocol ownership; do not add an X11 window manager.
- Keep the LunaDash compositor and built-in desktop services toolkit-neutral and free of Qt linkage. The independent LunaDash-Shell repository remains unchanged and may continue to use Quickshell/Qt as a separate Wayland client.
- Use wlroots and standard Wayland protocols for compositor compatibility; do not add toolkit-specific compositor integrations. Use Slint for LunaDash-owned built-in UI outside the independent shell repository.
- Put all project C++ types and functions in namespace `LunaDash` (except `main`).
- Keep interfaces and implementations together in their owning lowercase source domain. Use PascalCase `.hpp` / `.cpp` filenames and source-root project includes.
- C core functions use the `ludash_` prefix; expose C declarations inside `LunaDash` with C linkage when included from C++.
- Run `scripts/check-source-layout.py` after changing native source layout or build lists.
- Keep raw OpenGL code and built-in shaders under `src/compositor/renderer/opengl/`; embed shaders through CMake.
- Name project types, directories and targets by their concrete responsibility; do not introduce generic Backend names. External API/environment spellings remain unchanged.
- Headers declare interfaces and types; `.cpp` files contain implementations. Do not accumulate unrelated features in a shared implementation file.
- Keep small `Main.cpp` entry points in their owning executable domain, and add every source explicitly to CMake targets.
- Finish the intended code, packaging, and documentation changes before building. Do not build after each intermediate edit.
- Target Arch Linux first, with portable CMake support and documented dependencies for other Linux distributions. Distinguish configured CI from actually verified platforms.
- Do not describe this development version as a production-ready KDE replacement. Document missing protocol and session features accurately.
- Keep the independent LunaDash-Shell repository unchanged. Its QML source remains English; keep C, C++, and Slint source text in English as well. Traditional Chinese belongs only in the external language pack under `data/translations/`.
- Keep native plugins disabled by default. Plugin metadata is not a sandbox and does not make native code safe.


## 1.0.1a desktop UI contract

- Treat `1.0.1a` as the current product/plugin version string. CMake project versions stay numeric (`1.0.1`) where required.
- Dashboard, Settings, Welcome, portal pickers and desktop-visible plugin UI must share the LunaDash design system: strong/glass surfaces, hairline borders, shared radii and bounded layouts.
- LunaDash-owned Slint dashboard content must use layouts, bounded card sizes, clipping and text elision. The independent Quickshell UI follows the same bounds and overflow requirements using QML layouts.
- Settings must remain usable at its default size and when maximized. Search belongs in the header; page implementations must not invent separate window chrome.
- Portal FileChooser UI must keep the LunaDash-owned frameless wrapper, dark header and manual local-path field. Never evaluate a selected path through a shell.
- Desktop-widget plugins render inside the wallpaper Background layer. Do not create Top/Overlay windows for desktop widgets.
- `desktop-widgets` is multi-selection so clocks, visualizers and other wallpaper widgets can coexist.
- The removed Command Console must not return to built-ins, startup settings, tests, docs or launcher entries. Use the configured terminal role.
- “Motion” is not a standalone settings feature. Use “Visual effects” for appearance controls and “Reduced motion” only for accessibility.
- Native plugin metadata is validation, not isolation; never describe native plugins as sandboxed.
- Update the matching English and Traditional Chinese canonical docs whenever visible desktop behavior changes. Root `docs/*.md` compatibility paths remain small forwarding pages.
- After LunaDash-owned UI changes, run Slint design/action audits; shell compatibility changes require Qt/Quickshell and Slint Wayland client integration workflows. A real-session screenshot is still required for final visual verification.
