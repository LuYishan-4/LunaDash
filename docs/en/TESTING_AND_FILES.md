# Testing and source map

Finish code, packaging and documentation changes before building. Arch Linux is the primary development environment; CI configuration is not a substitute for observing a completed run on the intended commit.

## Build and static checks

The toolchain requires CMake 3.21+, Ninja, Python 3, C11/C++20, Qt 6.4+ Core/Gui/Widgets/Quick/OpenGL/Concurrent/Network/DBus, wlroots 0.17–0.20, Wayland protocols/scanner, xkbcommon, GL headers and GLib. `scripts/install-dependencies.sh` provides distribution-specific package selection. The shell additionally needs Quickshell 0.3+. The default terminal is Kitty. XWayland provides optional X11 compatibility. Interactive capture uses grim and slurp; backlight controls use brightnessctl and external-monitor controls use ddcutil.

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
  -DLUDASH_BUILD_RENDERER_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The architecture checker enforces lowercase domains, PascalCase C++ filenames, small domain-local entrypoints, source-root includes, dependency direction, raw GL placement and explicit CMake inventories. Its fixture tests intentionally introduce invalid layouts and ensure rejection. `.clang-format` defines source formatting; use `clang-format -i` on modified C/C++ files. Static analysis remains configured separately in the PR clang-tidy/CodeQL workflows.

## Plugin SDK checks

Native CTest coverage includes plugin validation, enable/disable, settings updates, native revision reload and the stacking layout example. QML tests exercise replacement readiness, augmentation, failure fallback and recovery. The template integration test builds and stages all four SDK templates; it additionally needs Qt Shader Tools. Run it against either the build-tree SDK or a relocated installation:

```sh
python3 tests/plugins/test_sdk.py --sdk build/sdk-build
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software qmltestrunner -input tests/qml
```

These tests use temporary configuration/data directories and do not install plugins into the active user's desktop.

## Runtime and installation

```sh
LUDASH_TEST_NO_SHELL=1 LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh
./scripts/test-xdg-lifecycle.sh "$PWD/build"
python3 tests/renderer/test_startup_failure.py build/lunadash-compositor
xvfb-run -a python3 tests/wayland/test_xwayland.py build
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  ./build/lunadash-renderer-test --opengl
DESTDIR="$PWD/build/stage" cmake --install build --prefix /usr
```

Headless wlroots tests use pixman and private runtime/configuration directories. The lifecycle client exercises map, unmap, role destruction and popup nesting. XWayland checks cover on-demand startup, authenticated application launches, display reuse and cleanup. Renderer checks cover resource relocation, error diagnostics, partial GL allocation cleanup and software OpenGL shader compilation/drawing. Translation checks validate catalog structure and preserve the stored values of translated controls.

For a real desktop, use `LUDASH_TEST_HOST_WAYLAND=1 LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh`. It uses the host Wayland socket and writes `build/wayland.log` and `build/wayland-state.json`. Run without `LUDASH_TEST_NO_SHELL` to check Quickshell. A complete manual session also checks Chrome/Zed, pointer and physical keyboard input, Fcitx preedit/candidate positioning, wallpaper changes, animation, file chooser D-Bus activation and logout. Do not infer these results from successful compilation.

Normal installation includes executables/compatibility aliases, session entries, shell QML, translations, portal configuration, assets and the optional example plugin. Internal GLSL is embedded and needs no installed shader directory. For explicit relocation testing, install `--component Tests` into a temporary prefix and run `libexec/lunadash/tests/lunadash-renderer-test` there; that component is excluded from normal installs.

On Arch, also verify the source archive used by `./scripts/install-session.sh`, since a checkout build cannot detect missing archive contents:

```sh
./scripts/make-source.sh
(cd packaging/arch && makepkg --cleanbuild --force --nosign)
```

This builds packages without installing them. The archive must include `examples/` for native plugin examples and `templates/` for the installed plugin SDK, in addition to the main source tree. Inspect the resulting package for the SDK templates and plugin examples before publishing it.

## CI coverage

| Workflow | Configured coverage |
| --- | --- |
| Main build and integration | One Ubuntu build covering maintained CTest, source contracts, QML parsing/tests, renderer checks, startup failure handling, Wayland/XWayland lifecycle and software OpenGL relocation |
| Linux distribution builds | Arch, Debian 13, Fedora 45, openSUSE Tumbleweed and Alpine Edge source builds |
| Main website build | Astro checking/build and local link/asset tests |
| PR source style | Architecture, English source, shell syntax/ShellCheck and QML design-system usage |
| PR analysis and hygiene | Policy, repository hygiene, path-scoped clang-tidy, CodeQL and Qt lifetime analysis |
| Website Pages deployment | Builds and publishes the release website from main |

Feature-specific workflows are intentionally not maintained. Individual UI interactions and one-off desktop regressions belong in the maintained test suite or release-session verification rather than owning a separate required workflow. The source-build matrix does not verify physical hardware. Void/Gentoo have installer paths without the same configured CI breadth. Inspect completed runs for the exact commit; a skipped/manual-only workflow is not a successful execution. See [security checks](SECURITY_CHECKS.md) and [release process](RELEASE_PROCESS.md).

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
| `src/desktop` | Applications, external default-app policy, audio, network, power and system services |
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

CI focuses on reusable build, protocol, lifecycle, renderer, translation and source-quality checks. Temporary tests and mock applications written for individual desktop bug reports are not part of the maintained suite. Full settings-page coverage, region selection, window motion, hardware brightness and application compatibility require real-session verification when preparing a release.
