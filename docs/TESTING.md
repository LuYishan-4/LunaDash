# Testing LuDash

The complete, maintained instructions are in [Testing and file reference](TESTING_AND_FILES.md). They cover dependencies, build commands, CTest, OpenGL/GLES sessions, first-run setup, interactive checks, sanitizer runs, packaging, failure diagnosis and the purpose of every maintained project file.

Quick start after installing the dependencies and completing your edits:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure
LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

Require exit code zero, rendered content, valid geometry and clean shutdown. Inspect `build/wayland-preview.png`, `build/wayland-state.json` and `build/wayland.log`. Do not accept an old success line followed by a crash. Nested software tests do not verify physical GPU or standalone login compatibility.
