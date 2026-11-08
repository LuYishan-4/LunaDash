# C core and C++ integration

LunaDash uses C11 for bounded low-level operations and C++20 for object ownership and Linux/wlroots integration. The shell is the separate Slint LunaDash-Shell on SlintLu.

| C implementation | Responsibility | C++ owner |
| --- | --- | --- |
| `compositor/renderer/opengl/GLDispatch.c` | Resolve the required GL entry points with bounded diagnostics | `OpenGL` uses the caller's current EGL context |
| `compositor/tiling/TilingGeometry.c` | Validate sizes and calculate column/member geometry | `TilingLayout` converts Qt values |
| `desktop/system/SystemMetrics.c` | Parse CPU/memory counters and calculate bounded percentages | `SystemStatus` and `SystemMonitor` |

Paths above are relative to `src/`. C functions retain the `ludash_` prefix and existing `LuDash*` C type names. Headers expose declarations inside `namespace LunaDash` with C linkage when included from C++; C compilation sees the same global C ABI. Each implementation is listed explicitly in CMake.

Shader compilation, program linking, texture and framebuffer ownership are C++ RAII objects inside `renderer/opengl`; they are not a second C renderer. wlroots owns the active compositor renderer/allocator and Wayland protocol lifetimes. These boundaries are enforced by the source-layout scanner.

See [graphics](GRAPHICS.md) and [build and source map](TESTING_AND_FILES.md) for build commands and limits.
