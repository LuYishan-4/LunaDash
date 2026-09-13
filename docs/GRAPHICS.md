# OpenGL and OpenGL ES

LuDash requires OpenGL 3.3 Core or OpenGL ES 3.0 or newer. GLES 2 is not supported.

```sh
QT_QPA_PLATFORM=wayland ./build/ludash-compositor --graphics opengl
QT_QPA_PLATFORM=wayland ./build/ludash-compositor --graphics gles
```

`auto` chooses according to Qt's OpenGL module type. It is not a restart-based GPU recovery mechanism. An unavailable requested API produces an error; desktop OpenGL is never reported as GLES.

`RenderBackend` configures `QSurfaceFormat` and Qt Quick before constructing the application. The surface keeps 24-bit depth and 8-bit stencil buffers for Qt Quick ordering and clipping; removing depth can cause parent frames to obscure client content.

`WallpaperItem` exposes a Qt Quick framebuffer item. `WallpaperRenderer` adapts Qt to the C rendering core. It uses `QOpenGLContext::currentContext()` on the render thread, validates the actual API/version and provides that context's function resolver to C. Shader programs, VAO and FBO stay within the render-thread context. Required Qt Quick GL state is reset after drawing. GUI/render health is exchanged through atomic fields.

The vertex and fragment shaders in `data/shaders/` are compiled and linked at runtime. Desktop GL uses `#version 330 core`; GLES uses `#version 300 es` and precision declarations. Quickshell displays the image wallpaper; Dusk/Forest switch its background surface to transparent so the compositor shader is visible. The shell and compositor have separate contexts.

```sh
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build -R graphics-contexts --output-on-failure
LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

The state JSON records actual API/version, `shaderReady` and `graphicsFailed`. Xvfb/Mesa is software-driver validation, not physical NVIDIA/AMD/Intel/ARM coverage.

A `QQuickWindow::sceneGraphError` handler reports initialization failure and exits with code 2 instead of Qt's default abort. Invalid `--graphics` arguments also return 2. The startup-failure test intentionally forces Mesa 3.2 to reject the required 3.3 context and verifies controlled failure.

For manual diagnosis, remove incompatible overrides only from the launched process:

```sh
env -u MESA_GL_VERSION_OVERRIDE -u MESA_GLSL_VERSION_OVERRIDE QT_QPA_PLATFORM=wayland ./build/ludash-compositor --graphics opengl
```

References: [Qt render-thread/context rules](https://doc.qt.io/qt-6/qquickframebufferobject-renderer.html), [QSurfaceFormat](https://doc.qt.io/qt-6/qsurfaceformat.html), [sceneGraphError](https://doc.qt.io/qt-6/qquickwindow.html#sceneGraphError).

Shader compilation, GL dispatch, wallpaper drawing and two-pass backdrop blur now live in C11 under `render_core`. Qt-owned framebuffer items and scene-graph nodes remain C++ adapters. [C core](C_CORE.md) and [Effects](EFFECTS.md) describe ownership and validation.
