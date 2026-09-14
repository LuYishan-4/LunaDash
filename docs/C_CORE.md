# C core and C++ integration

LunaDash uses C11 for independent low-level work and C++20 for Qt/Wayland object integration. The shell is Quickshell/QML; the website source is Astro and TypeScript.

| C module | Responsibility | C++ adapter |
| --- | --- | --- |
| `render_core` | Resolve GL functions, compile/link shaders, draw wallpaper, manage blur textures/FBOs and run two blur passes | `renderer` and `blur` own current-context/render-thread integration |
| `tiling_core` | Compute one full-height rectangle per horizontal column into a caller-owned buffer, validate sizes and prevent arithmetic overflow | `tiling` converts QRect/QList values |
| `system_metrics` | Parse bounded CPU/memory records, reject counter overflow and calculate percentages | `system_status` and `system_monitor` read files and present results |

C functions use the `ludash_` prefix. C types use `LuDash` names, and headers expose C linkage inside namespace `LuDash` when included from C++. Each module has matching `include/LuDash/<feature>/` and `src/<feature>/` directories. Headers declare the API; implementation stays in `.c`. CMake lists every source explicitly.

The renderer receives a function resolver from the active Qt OpenGL context. It does not create a second context or call a different GL implementation. The same C functions are tested against OpenGL 3.3 Core, OpenGL 3.3 compatibility and OpenGL ES 3.0+. The compositor requests compatibility on desktop GL so Qt can also render its legacy external-texture material. Shader resources receive the appropriate GLSL version/precision prefix in the adapter.

Rendering objects must be created, used and destroyed while their owning context is current on the render thread. Allocation and shader errors return a failure value with bounded diagnostics; the C++ adapter reports render health. Geometry and metric functions do not depend on Qt or allocate memory, and do not modify output on invalid input.

Qt objects, signals, Wayland client lifetimes, QSettings, JSON IPC and plugin integration remain in C++ because they use Qt's C++ API. Moving these wrappers to C would add another binding layer rather than simplify the implementation.

`c-core` tests compile as C and cover geometry, parser bounds, counter reset and overflow cases. `graphics-contexts` executes the production C shader and blur paths in both GL and GLES. Static analysis and sanitizers apply to `.c` and `.cpp` production sources. Tests prove the exercised behavior, not that changing language alone improves speed or security.
