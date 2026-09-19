# C core and C++ integration

LunaDash uses C11 for bounded low-level operations and C++20 for object ownership and Qt/wlroots integration. The shell is Quickshell/QML.

| C implementation | Responsibility | C++ owner |
| --- | --- | --- |
| `compositor/renderer/opengl/GLDispatch.c` | Resolve the required GL entry points with bounded diagnostics | `OpenGL` uses Qt's current render-thread context |
| `compositor/tiling/TilingGeometry.c` | Validate sizes and calculate column/member geometry | `TilingLayout` converts Qt values |
| `desktop/system/SystemMetrics.c` | Parse CPU/memory counters and calculate bounded percentages | `SystemStatus` and `SystemMonitor` |

Paths above are relative to `src/`. C functions retain the `ludash_` prefix and existing `LuDash*` C type names. Headers expose declarations inside `namespace LunaDash` with C linkage when included from C++; C compilation sees the same global C ABI. Each implementation is listed explicitly in CMake.

Shader compilation, program linking, texture and framebuffer ownership are C++ RAII objects inside `renderer/opengl`; they are not a second C renderer. wlroots owns the active compositor renderer/allocator and Wayland protocol lifetimes. These boundaries are enforced by the source-layout scanner.

The renderer's failure-injection regression checks partial resource cleanup without a GPU; its optional software OpenGL test compiles embedded shaders and draws with a current context. The file tests and Wayland lifecycle tests cover their respective owners. See [graphics](GRAPHICS.md) and [testing](TESTING_AND_FILES.md) for current commands and limits.
