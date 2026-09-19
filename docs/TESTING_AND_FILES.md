# Testing and source map

Finish code, packaging and documentation changes before building. Arch Linux is the primary development environment; CI configuration is not a substitute for observing a completed run on the intended commit.

## Build and static checks

The toolchain requires CMake 3.21+, Ninja, C11/C++20, Qt 6.4+ Core/Gui/Widgets/Quick/OpenGL/Concurrent/Network/DBus, wlroots 0.17–0.20, Wayland protocols/scanner, xkbcommon, GL headers and GIO. `scripts/install-dependencies.sh` provides distribution-specific package selection. The shell additionally needs Quickshell 0.3+. XWayland provides optional X11 compatibility. Interactive capture uses grim and slurp; backlight controls use brightnessctl and external-monitor controls use ddcutil.

```sh
python3 scripts/check-source-layout.py
python3 tests/architecture/test_source_layout.py
python3 tests/renderer/test_render_architecture.py
python3 tests/renderer/test_shader_extensions.py
python3 tests/wayland/test_no_qtwayland.py
python3 tests/security/test_source_language.py
python3 scripts/ci/check_qml_actions.py
python3 scripts/ci/check_qml_style.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLUDASH_BUILD_FILES_TESTS=ON -DLUDASH_BUILD_RENDERER_TESTS=ON -DLUDASH_BUILD_DESKTOP_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The architecture checker enforces lowercase domains, PascalCase C++ filenames, small domain-local entrypoints, source-root includes, dependency direction, raw GL placement and explicit CMake inventories. Its fixture tests intentionally introduce invalid layouts and ensure rejection. `.clang-format` defines source formatting; use `clang-format -i` on modified C/C++ files. Static analysis remains configured separately in the PR clang-tidy/CodeQL workflows.

## Runtime and installation

```sh
LUDASH_TEST_NO_SHELL=1 LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh
./scripts/test-xdg-lifecycle.sh "$PWD/build"
python3 tests/wayland/test_desktop_controls.py build
python3 tests/renderer/test_startup_failure.py build/lunadash-compositor
python3 tests/wayland/test_xwayland.py build
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  ./build/lunadash-renderer-test --opengl
DESTDIR="$PWD/build/stage" cmake --install build --prefix /usr
```

Headless wlroots tests use pixman and private runtime/configuration directories. The lifecycle client exercises map, unmap and role destruction, including close animation cleanup. XWayland tests require an installed Xwayland binary, verify no compatibility server starts at login, launch an X11 client on demand, reject unauthenticated connections and verify cleanup. Renderer tests independently cover resource relocation, error diagnostics, partial GL allocation cleanup and real software OpenGL shader compilation/drawing.

For a real desktop, use `LUDASH_TEST_HOST_WAYLAND=1 LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh`. It uses the host Wayland socket and writes `build/wayland.log` and `build/wayland-state.json`. Run without `LUDASH_TEST_NO_SHELL` to check Quickshell. A complete manual session also checks Chrome/Zed, pointer and physical keyboard input, Fcitx preedit/candidate positioning, wallpaper changes, animation, file chooser D-Bus activation and logout. Do not infer these results from successful compilation.

Normal installation includes executables/compatibility aliases, session entries, shell QML, translations, portal configuration, assets and the optional example plugin. Internal GLSL is embedded and needs no installed shader directory. For explicit relocation testing, install `--component Tests` into a temporary prefix and run `libexec/lunadash/tests/lunadash-renderer-test` there; that component is excluded from normal installs.

## CI coverage

| Workflow | Configured coverage |
| --- | --- |
| Main Ubuntu build | Full C/C++ build, file regressions and source architecture |
| Main Qt loading | Native Qt client modules against a wlroots session |
| Main Wayland loading | Protocol globals, xdg lifecycle, headless session, on-demand XWayland |
| Main OpenGL loading | Shader inventory, renderer failure/lifetime tests, staged-install software GL and headless session |
| Main startup checks | Invalid CLI options and wlroots renderer fallback |
| Main website build | Astro checking/build and local link/asset tests |
| Source style / QML reviews | Architecture, English source, shell checks, QML syntax/design/actions |
| Linux distribution builds | Arch, Debian 13, Fedora 45, openSUSE Tumbleweed and Alpine Edge source builds |
| PR analysis and hygiene | Path-scoped clang-tidy, CodeQL, Qt lifetime and repository policy |

The source-build matrix does not verify physical hardware. Void/Gentoo have installer paths, without the same configured CI breadth. The main gate historically aggregates five runtime/build workflows; inspect all runs for the exact commit, including website, architecture/QML and distro jobs. A skipped/manual-only workflow is not a successful execution. See [security checks](SECURITY_CHECKS.md) and [release process](RELEASE_PROCESS.md).

## Source map

| Location | What to edit |
| --- | --- |
| `src/compositor/session` | Startup, child environments and launch policy |
| `src/compositor/wayland` | Runtime, output and surface lifetimes; private wlroots compatibility boundary |
| `src/compositor/input` | Physical keyboard and pointer handling, text-input/input-method bridge |
| `src/compositor/tiling`, `window`, `client` | Layout, rules and client state |
| `src/compositor/animation` | wlroots scene transitions and retained Qt adapters |
| `src/compositor/renderer` | Render orchestration, common types and item adapters |
| `src/compositor/renderer/opengl` | GL dispatch/resources/passes and embedded shaders |
| `src/config` | Preferences, localization, process helper and plugin metadata |
| `src/core` | Shared lifecycle concepts, listener template and compile definitions |
| `src/desktop` | Applications, browser policy, files, audio, network, power and system services |
| `src/service/portal` | D-Bus startup and native file chooser implementation |
| `src/shell` | Generic QML module schema/runtime and media helper |
| `src/ctl` | Bounded local control client |
| `qml` | Shell feature UI and shared controls |
| `data` | User-facing assets, templates, translations and portal/plugin metadata |
| `cmake/modules/Renderer.cmake` | Explicit render sources and embedded shader inventory |
| `cmake/modules/Tests.cmake` | Optional renderer tests and staged test installation |
| `scripts`, `tests` | Installation, diagnostics and executable regression checks |
| `site/src/pages/docs/architecture.astro` | Website architecture guide |

See [architecture](ARCHITECTURE.md) for ownership contracts. Existing historical Python tests that assume the previous Qt compositor or Xvfb keyboard injection are not part of the current wlroots CI gate; use the current lifecycle and protocol tests above and port a historical test before relying on it as release evidence.

Desktop-control regressions use isolated settings and fake brightness/selector helpers. DDC/CI fixtures verify per-monitor targeting, non-100 maxima, coalesced writes, errors, timeout handling and removal. The QML regression preserves slider delegates through status updates and rejects queued changes to a replacement monitor. Region capture verifies actual grim PNG dimensions on a headless output, cancellation and nonblocking IPC; display tests verify invalid requests, confirmation, explicit revert and timeout rollback. Shared controls are checked against English, Traditional Chinese, Simplified Chinese, Japanese and long labels with `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QML_XHR_ALLOW_FILE_READ=1 qmltestrunner -input tests/qml`. These tests do not change host backlight or display settings.

The Arch distribution job also runs `tests/wayland/test_shell_startup.py` with a complete Quickshell session on headless wlroots, checking startup-splash mapping/dismissal, wallpaper/panel mapping and localized Display settings. It requires Quickshell and a private D-Bus session; other distro jobs retain their source/lifecycle checks.
