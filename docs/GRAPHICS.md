# OpenGL and OpenGL ES

LunaDah requires OpenGL 3.3 compatibility or OpenGL ES 3.0 or newer. GLES 2 is not supported.

```sh
QT_QPA_PLATFORM=wayland ./build/lunadah-compositor --graphics opengl
QT_QPA_PLATFORM=wayland ./build/lunadah-compositor --graphics gles
```

`auto` chooses according to Qt's OpenGL module type. It is not a restart-based GPU recovery mechanism. An unavailable requested API produces an error; desktop OpenGL is never reported as GLES.

`RenderBackend` configures `QSurfaceFormat` and Qt Quick before constructing the application. The surface keeps 24-bit depth and 8-bit stencil buffers for Qt Quick ordering and clipping; removing depth can cause parent frames to obscure client content.

It also enables `Qt::AA_ShareOpenGLContexts` before constructing `QGuiApplication`. Qt Wayland imports EGLStream buffers during GUI-thread commits using an offscreen context in the global share group. Without that group, imports can fail with `creating texture with no current context`, missing QSG textures and client EGL-surface errors even though LunaDah's own shaders rendered successfully. This path is implemented by Qt's [EGL client buffer integration](https://github.com/qt/qtwayland/blob/v6.11.2/src/hardwareintegration/compositor/wayland-egl/waylandeglclientbufferintegration.cpp).

Desktop GL requests a compatibility profile with deprecated functions enabled. Qt Wayland's external OES buffer material supplies only GLSL 120 and GLSL ES 100 variants. Qt's RHI excludes GLSL 120 when the context is Core Profile, producing `No GLSL shader code found` and `Failed to build graphics pipeline state` for affected GPU buffers. Compatibility allows Qt's GLSL 120 material and LunaDah's GLSL 330 shaders to coexist; it does not lower the OpenGL 3.3 requirement. GLES still requests version 3.0 and uses LunaDah's GLSL ES 300 shaders. The C renderer is also tested independently in a Core Profile context.

If a driver cannot provide the requested desktop compatibility profile, LunaDah reports the mismatch; try `--graphics gles` with an ES 3 capable driver. See Qt's [external OES material](https://github.com/qt/qtwayland/blob/v6.11.2/src/compositor/compositor_api/qwaylandquickitem.cpp) and [RHI shader selection](https://github.com/qt/qtbase/blob/v6.11.2/src/gui/rhi/qrhigles2.cpp).

`WallpaperItem` exposes a Qt Quick framebuffer item. `WallpaperRenderer` adapts Qt to the C rendering core. It uses `QOpenGLContext::currentContext()` on the render thread, validates the actual API/version and provides that context's function resolver to C. Shader programs, VAO and FBO stay within the render-thread context. Required Qt Quick GL state is reset after drawing. GUI/render health is exchanged through atomic fields.

The vertex and fragment shaders in `data/shaders/` are compiled and linked at runtime. Desktop GL uses `#version 330 core`; GLES uses `#version 300 es` and precision declarations. Quickshell displays the image wallpaper; Dusk/Forest switch its background surface to transparent so the compositor shader is visible. The shell and compositor have separate contexts.

```sh
LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
# From an existing Wayland desktop, open a temporary nested GPU-backed session:
LUDASH_TEST_HOST_WAYLAND=1 LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_TEST_HOST_WAYLAND=1 LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

The state JSON records actual API/version, `shaderReady` and `graphicsFailed`. Xvfb/Mesa is software-driver validation, not physical NVIDIA/AMD/Intel/ARM coverage.

The session test now rejects missing-GLSL and pipeline-failure diagnostics even if the process exits with zero. Host Wayland mode preserves the absolute host socket before creating a private runtime/configuration directory, removes the software-rendering override, and saves `build/host-wayland.log`, `host-wayland-state.json` and `host-wayland-preview.png`. It requires a running Wayland desktop and is a manual hardware check, not a headless CI claim. A passing run only covers the buffer formats actually produced by that host and driver; it does not prove every external OES/dmabuf format works.

A `QQuickWindow::sceneGraphError` handler reports initialization failure and exits with code 2 instead of Qt's default abort. Invalid `--graphics` arguments also return 2. The startup-failure test intentionally forces Mesa 3.2 to reject the required 3.3 context and verifies controlled failure.

For manual diagnosis, remove incompatible overrides only from the launched process:

```sh
env -u MESA_GL_VERSION_OVERRIDE -u MESA_GLSL_VERSION_OVERRIDE QT_QPA_PLATFORM=wayland ./build/lunadah-compositor --graphics opengl
```

References: [Qt render-thread/context rules](https://doc.qt.io/qt-6/qquickframebufferobject-renderer.html), [QSurfaceFormat](https://doc.qt.io/qt-6/qsurfaceformat.html), [sceneGraphError](https://doc.qt.io/qt-6/qquickwindow.html#sceneGraphError).

Shader compilation, GL dispatch, wallpaper drawing and two-pass backdrop blur now live in C11 under `render_core`. Qt-owned framebuffer items and scene-graph nodes remain C++ adapters. [C core](C_CORE.md) and [Effects](EFFECTS.md) describe ownership and validation.

On the tested host, Qt also reports an `m_orphanedTextures container is not empty` warning during EGLStream teardown. Rendering and process exit checks pass, but that warning is still unresolved. ASan leak detection remains disabled. A separate sustained descriptor/fence regression now covers the observed NVIDIA shell leak; see [Shell rendering](SHELL_RENDERING.md) for the default compatibility mode and its limits.

Native tool shutdown on Qt 6.5+ uses the public [Wayland application interface](https://doc.qt.io/qt-6/qnativeinterface-qwaylandapplication.html) to finish a private `wl_display.sync` barrier after the event loop and window lifetimes end. The barrier completes prepared reads before Qt joins its readers sequentially, addressing the observed Qt 6.11.2 teardown deadlock. It dispatches only its private callback queue, leaves the socket owned by Qt and uses a one-second polling deadline. Qt 6.4 retains its existing cleanup path. This guard is for native tools and does not modify an external Quickshell installation.
