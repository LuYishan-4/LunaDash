# Testing LunaDah

The complete, maintained instructions are in [Testing and file reference](TESTING_AND_FILES.md). They cover dependencies, build commands, CTest, OpenGL/GLES sessions, first-run setup, interactive checks, sanitizer runs, packaging, failure diagnosis and the purpose of every maintained project file.

Quick start after installing the dependencies and completing your edits:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure
LUNADAH_DISABLE_FCITX=1 LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUNADAH_DISABLE_FCITX=1 LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

Require exit code zero, rendered content, valid geometry and clean shutdown. Inspect `build/wayland-preview.png`, `build/wayland-state.json` and `build/wayland.log`. Do not accept an old success line followed by a crash. `LUNADAH_DISABLE_FCITX=1` keeps nested UI checks independent of the host input-method daemon. Manually verify the centered cropped BrandIcon and downward launcher reveal, unified ranked app list, left workspace/session controls, and right SNI/compact-status area. Nested software tests do not verify physical GPU, candidate popups or standalone login compatibility.
