# Testing LunaDah

The complete, maintained instructions are in [Testing and file reference](TESTING_AND_FILES.md). They cover dependencies, build commands, CTest, OpenGL/GLES sessions, first-run setup, interactive checks, sanitizer runs, packaging, failure diagnosis and the purpose of every maintained project file.

One-shot window test from an existing Wayland desktop:

```sh
./scripts/test-once.sh
```

It configures `build-once`, builds LunaDah, runs the non-display CTest set, lints QML, starts a nested host-Wayland compositor, opens demonstration windows, validates scrollable geometry and rendered content, and writes `host-wayland.log`, `host-wayland-state.json`, and `host-wayland-preview.png` under `build-once`. It sets `LUNADAH_DISABLE_FCITX=1` so the nested session cannot replace the host input-method daemon. Override with `LUDASH_GRAPHICS=opengl ./scripts/test-once.sh` or choose another build directory with `LUNADAH_TEST_BUILD_DIR=/absolute/path ./scripts/test-once.sh`.

Manual quick start after installing the dependencies:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure
LUNADAH_DISABLE_FCITX=1 LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUNADAH_DISABLE_FCITX=1 LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

Require exit code zero, rendered content, valid geometry and clean shutdown. Inspect `build/wayland-preview.png`, `build/wayland-state.json` and `build/wayland.log`. Do not accept an old success line followed by a crash. `LUNADAH_DISABLE_FCITX=1` keeps nested UI checks independent of the host input-method daemon. Manually verify the single compact `TopPanel`: left workspace/session controls plus inline grouped-app icons, the centered overview / Lambda (`Λ`) launcher / settings selector and downward launcher reveal, and the right SNI plus clock/network/battery area. Also check grouped-column keyboard/pointer behavior, confirm application frames cannot be dragged, verify only top-panel icons drag for grouping, inspect resolved icons in `lunadahctl status`, window y geometry below the panel, Kitty's initial maximize and `Super+F`, and the bounded LunaDah image-picker preview. The full steps are in [Manual acceptance](TESTING_AND_FILES.md#5-manual-acceptance). Nested software tests do not verify physical GPU, candidate popups or standalone login compatibility.
