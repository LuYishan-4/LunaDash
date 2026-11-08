# Graphics and renderer resources

The active compositor uses wlroots for renderer selection, allocation and scene output commits. `--graphics auto` allows wlroots to choose; `--graphics opengl` and `--graphics gles` both request its GLES2 renderer when `WLR_RENDERER` is unset. Startup fails with a subsystem diagnostic if the backend, renderer, allocator or required globals cannot be initialized. A headless session normally uses `WLR_RENDERER=pixman`.

LunaDash also builds a toolkit-neutral OpenGL render-element library under `src/compositor/renderer`. Its `OpenGL` implementation validates the current EGL OpenGL 3.3+ or OpenGL ES 3.0+ context supplied by its caller; it does not create or own a context. `Renderer` orchestrates elements, while raw GL calls and resource ownership stay under `renderer/opengl`. The active compositor scene remains owned by wlroots. Image wallpaper is supplied by the separate Quickshell process.

`Fullscreen.vert`, `Wallpaper.frag`, `Blur.frag` and `Decoration.frag` live in `src/compositor/renderer/opengl/shaders`. CMake generates a C++ resource header from these explicit files; no source checkout, installed shader directory or particular working directory is required. `ShaderAssetLoader` retains stage aliases and generic GLSL suffix support. `Shader` and `Program` report compilation/link diagnostics and use RAII to release every partial allocation on failure. Textures, framebuffers, programs and vertex arrays must be released with the owning context current.

Software OpenGL and pixman results do not prove NVIDIA/AMD/Intel/ARM hardware, dmabuf interoperability or physical-seat startup.

The shell (SlintLu) has its own rendering policy and process. See [shell rendering](SHELL_RENDERING.md) and [architecture](ARCHITECTURE.md).

## Vulkan selection

`--graphics vulkan` (or `LUDASH_GRAPHICS=vulkan` for the session launcher) requests the wlroots Vulkan renderer. A nonempty `WLR_RENDERER` has precedence, as it does for GLES. `--graphics auto` keeps wlroots selection. Explicit Vulkan failure exits with a diagnostic; it never reports a pixman fallback as Vulkan. `lunadashctl status` exposes `renderer` (`vulkan`, `gles2`, `pixman`, or unavailable) and `vulkanAvailable`, which describes build support, not GPU readiness.

The C11 selection code lives in `renderer/selection/RenderSelection.c`, links only to wlroots and accepts no toolkit objects. `LUDASH_ENABLE_VULKAN=OFF` disables Vulkan integration; the default detects the wlroots Vulkan API at configure time. The standalone render-element target also needs EGL development files. Install Vulkan development headers/loader and an appropriate GPU ICD (Arch: `vulkan-headers`, `vulkan-icd-loader`, and your GPU's Vulkan driver). Other Linux dependencies are handled by `scripts/install-dependencies.sh`; Nix supplies the loader and headers. Vulkan needs a usable DRM render node and the external-memory features required by wlroots; a loader or Lavapipe installation alone does not establish that support.

Both compositor renderers use the existing wlroots scene, allocator, dmabuf feedback and capability-gated explicit synchronization. Client GTK/Qt APIs are independent of the compositor renderer. The retained standalone EGL OpenGL render-element library remains OpenGL-specific. Shell selection is separate: `LUDASH_SHELL_RENDERER=vulkan` uses Qt Quick's Vulkan path only in Quickshell.

Software rendering results are not a Vulkan hardware claim.

Local verification on 2026-10-05: Arch Linux, wlroots 0.20.2, AMD Radeon 680M with RADV/Mesa 26.2.3 passed the nested Vulkan GTK 3/GTK 4/Qt map/capture/close test. This is one GPU in a host Wayland session, not a verified physical login or a claim about other drivers.
