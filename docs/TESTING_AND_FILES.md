# Testing and file reference

This guide explains how to test LuDash, interpret failures and find the purpose of every maintained project file. Run commands from the repository root unless stated otherwise. Complete intended code, packaging and documentation edits before building.

LuDash 0.1 is a development preview, not a production-ready KDE replacement. Test it inside an existing desktop first. The compositor uses C++20, C11 cores and native Wayland; the shell uses Quickshell. Graphics require OpenGL 3.3 compatibility or OpenGL ES 3.0+.

## 1. Dependencies

Arch Linux:

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-wayland \
  qt6-translations quickshell mesa xorg-server-xvfb xorg-xauth xdotool python python-pillow xorg-xwayland
```

Static analysis and sanitizer builds need `clang`. The Astro/TypeScript website needs Node.js 22.12+ and npm. Optional network tools are `networkmanager nm-connection-editor`; do not replace an existing network service just to run a test. Fonts, input methods and full terminals are listed in [the Arch package](../packaging/arch/PKGBUILD).

Ubuntu 24.04 backend dependencies: `build-essential cmake ninja-build pkg-config libwayland-dev qt6-base-dev qt6-declarative-dev qt6-wayland-dev qt6-wayland libqt6opengl6-dev`; tests add `libgl1-mesa-dri xvfb xauth python3 python3-pil`. Fedora uses `gcc-c++ cmake ninja-build wayland-devel qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel mesa-dri-drivers xorg-x11-server-Xvfb xorg-x11-xauth python3 python3-pillow`. Install Quickshell separately where unavailable; the shell targets version 0.3 and may need newer Qt than the C++ backend's 6.4 minimum.

## 2. Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
```

Outputs include `ludash-compositor` (Wayland server), `ludash-desktop` (native tools), `ludashctl` (local control client), test executables and the disabled-by-default fade plugin. Source QML is used automatically when an installed shell is not found. An older installation under your data search path can take precedence; remove or update that installation when validating source changes.

Use the existing generator when reusing a build directory: omit `-G Ninja` if it was configured with Unix Makefiles.

## 3. Automated checks

```sh
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure
```

| CTest | What it checks |
| --- | --- |
| desktop-interactions | Native tools, retired-application removal, tiling, language resources, wallpaper validation, package input and plugin paths |
| security-gate | SARIF findings and missing reports fail the security gate |
| graphics-contexts | Actual desktop GL and GLES contexts with production C wallpaper and blur passes |
| graphics-startup-failure | Invalid API arguments and unavailable GL 3.3 return code 2 instead of aborting |
| c-core | C geometry, bounded parsing, arithmetic overflow and counter resets |
| window-animations | Interrupted visibility transitions, item destruction and reduced motion |
| desktop-preferences | Type/range validation, no partial invalid update, setup completion and link/Internet distinction |
| system-settings | Bounded helper output/timeouts, audio arguments and power-profile parsing |
| shell-modules | Module schema bounds, atomic preservation, template ownership, trust defaults and symlink escape rejection |
| files-and-defaults | File overwrite protection, name validation, default application validation and literal argument preservation |
| source-language | English C/C++/QML, Markdown documentation and website sources |

Require `100% tests passed`. Graphics tests use Mesa software rendering; they are not physical GPU compatibility results.

## 4. Full Wayland sessions

```sh
LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
LUDASH_TEST_OVERVIEW=1 ./scripts/test-wayland.sh
LUDASH_TEST_SETUP=1 ./scripts/test-wayland.sh
xvfb-run -a -s '-screen 0 1440x900x24' python3 tests/wayland/test_shell_interactions.py build
xvfb-run -a -s '-screen 0 1440x900x24' python3 tests/wayland/test_setup.py build
xvfb-run -a -s '-screen 0 1440x900x24' python3 tests/wayland/test_settings.py build
xvfb-run -a -s '-screen 0 1440x900x24' python3 tests/wayland/test_effects.py build
xvfb-run -a -s '-screen 0 1440x900x24' python3 tests/wayland/test_customization.py build
xvfb-run -a python3 tests/wayland/test_xwayland.py build
xvfb-run -a python3 tests/wayland/test_crash_detection.py build
```

The X11 test checks a real mapped XCB client, authentication rejection and socket cleanup.

Each session uses its own runtime/configuration directory. The normal demo runs for eight seconds and allows up to five seconds for clean shutdown. The script removes stale screenshots/JSON before starting. During shutdown Quickshell waits for the compositor to observe panel unmapping and for control helpers to exit. First-run logs are retained in `build/ci-evidence/setup/`; a failed shutdown reports pending processes, clients, layers and animations. It checks actual graphics and blur health, visible client content, non-overlapping geometry and child-process status. Pixel-diversity checks catch blank content but do not prove visual correctness; inspect the screenshot too.

The interaction test clicks workspace, launcher and settings controls, checks language and wallpaper changes, and minimizes/restores a native window. The setup test walks through the offline guide, changes its accent, completes it, rejects invalid mixed preference updates and restarts the compositor to verify persistence. It never changes the host network. The negative crash test deliberately signals one owned test client and requires compositor exit code 2.

`LUDASH_TEST_NO_SHELL=1` tests native clients without Quickshell. `LUDASH_BUILD_DIR=/absolute/build-directory` selects another build. Demo/overview tests bypass first-run setup; `LUDASH_TEST_SETUP=1` explicitly enables it.

| Evidence | Contents |
| --- | --- |
| `build/wayland-preview.png` | Real compositor, shell and demo-window screenshot |
| `build/wayland-state.json` | Geometry, visibility, buffers, preferences, network and graphics state |
| `build/wayland.log` | Full session output |
| `build/desktop-preview.png` / `desktop-state.json` / `desktop.log` | Default desktop without demo apps |
| `build/setup-preview.png` / `setup-state.json` / `setup.log` | First-run guide screenshot and state |
| `build/Testing/Temporary/LastTest.log` | CTest output |

Check `$?` immediately after a command. Success requires exit code zero and the final `clean shutdown` message. A success line followed by crashed processes is a failure, not a partial pass. Do not publish raw state/log files without reviewing local paths and system identifiers.

## 5. Manual acceptance

Inside your existing Wayland desktop:

```sh
env -u MESA_GL_VERSION_OVERRIDE -u MESA_GLSL_VERSION_OVERRIDE \
  QT_QPA_PLATFORM=wayland ./build/ludash-compositor --socket ludash-test
```

To get an isolated first-run configuration without changing your normal settings:

```sh
LUDASH_TEST_CONFIG=$(mktemp -d)
XDG_CONFIG_HOME="$LUDASH_TEST_CONFIG" QT_QPA_PLATFORM=wayland \
  ./build/ludash-compositor --socket ludash-first-run
```

Retain the same configuration directory for a second launch to verify persistence. Remove the temporary directory after both sessions have exited.

1. Walk through language, network, appearance and completion. Verify offline continuation. Network status must distinguish a link from confirmed Internet connectivity. If desired, manually open the network editor and close it; automated tests do not connect Wi-Fi or alter profiles.
2. Change accent, gaps, panel height, wallpaper and card visibility. Restart with the same configuration and check those choices remain. Reopen the guide from settings.
3. Open Files, Console and Monitor from the launcher. Check real client content, readable translucent backgrounds and no overlap with the panel.
4. Open Settings → Keyboard and pointer. Change a keyboard layout and repeat settings, then use the test field to check input. No bundled Notes application is installed.
5. In Console, run `printf 'hello\n'; exit 7`; expect hello and exit code 7. This console is not a PTY terminal; use the default Terminal (Konsole/Fish) for interactive programs.
6. Navigate into and out of a directory in Files.
7. Switch workspaces by clicking the panel. Test Super + Shift + 2 to move a window, Super + M to minimize, and the launcher's open-window list to restore it.
8. Adjust blur, window opacity and animation duration in settings. Disable animations and rapidly switch workspaces or close windows. Launch an X11-only app through the compatibility dialog when XWayland is available. Choose a local wallpaper file; reject an invalid path without crashing. Test both shader palettes and return to the bundled image.
9. Reopen native apps after a language change. Test preedit, candidates and commit following [Input methods](INPUT_METHODS.md); protocol registration alone is not end-to-end IME verification.
10. Confirm logout can be canceled. Close all windows, then finish the session. Native plugins stay disabled unless you explicitly trust and enable one. Pacman operations require deliberate terminal confirmation and are not executed by tests.

The host desktop may intercept Super combinations. Use shell buttons or adjust host shortcuts before concluding input is broken. An X11 host can run the nested compositor through `QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_egl`; LuDash's clients still use Wayland, not an X11 window manager.

## 6. Static analysis and sanitizers

```sh
cmake -S . -B build-checked -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy -DLUDASH_ENABLE_SANITIZERS=ON
cmake --build build-checked --parallel 4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 \
  xvfb-run -a ctest --test-dir build-checked --output-on-failure
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  LUDASH_BUILD_DIR="$PWD/build-checked" ./scripts/test-wayland.sh
```

Select both Clang compilers: the renderer, tiling and metrics cores and generated protocol code use C. The configured clang-tidy checks also apply to C targets. Use a fresh directory when changing compilers. Any sanitizer report, enabled clang-tidy warning or nonzero test exit fails validation. Leak detection is disabled; these runs do not check leaks. See [Security checks](SECURITY_CHECKS.md) for CodeQL and required PR checks.

On Ubuntu 24.04, select `clang-19`, `clang++-19` and `clang-tidy-19` for combined analysis and install `libclang-rt-19-dev`. Run `python3 tests/security/test_analyzer.py clang-tidy-19` to check valid Qt pointer handling and actual use-after-free detection. CI separately verifies Clang 18 ASan/UBSan with `libclang-rt-18-dev`; older Qt packages omit optional text-input v3 automatically.

## 7. Packaging and website

```sh
DESTDIR=/tmp/ludash-stage cmake --install build
./scripts/make-source.sh
tar -tf packaging/arch/ludash-0.1.0.tar.gz
npm ci --prefix site --include=dev --ignore-scripts
npm run check --prefix site
npm run build --prefix site
python3 tests/site/test_site.py site/dist
npm run preview --prefix site -- --host 127.0.0.1
```

Review installed binaries, QML, wallpaper, translations, plugin SDK and session descriptor. The source archive should contain all CMake inputs and no build/cache files. A full Arch package can be built with `makepkg -Cfs` from `packaging/arch`; staged install success does not prove a pacman transaction or independent login session works.

Review the website at desktop and mobile widths. Test keyboard navigation, accent/gap controls and copy feedback. TypeScript uses strict checking; generated output stays in ignored `site/dist/`. See [Website deployment](WEBSITE.md) for GitHub Pages prerequisites and deployment verification.

## 8. Diagnose failures

| Message or symptom | Interpretation and next action |
| --- | --- |
| `LuDash graphics initialization failed` | Inspect driver and GL overrides. The controlled result is exit code 2. |
| `SIGABRT`, `QMessageLogger::fatal`, `QQuickWindow::event` | Older code could abort when creating a context. Rebuild and run the startup-failure regression. |
| `invalid method 8, object zwlr_layer_surface_v1` | Old layer-shell v1 lacked `set_layer`; the current compositor negotiates v2. Rebuild both runtime and shell configuration. |
| `There is no EGL_WL_bind_wayland_display extension` | Xvfb may use shared-memory client buffers. This alone does not prove context failure or hardware acceleration. Require rendered content and clean exit. |
| `Failed to initialize EGL display` | Inspect the host backend and buffer integration. Run the current script with `xcb_egl`; do not treat a geometry-only pass as GPU validation. |
| Portal app ID warning | The host portal cannot resolve a development app registration. Full portal support is not implemented. |
| Child process crash or timeout | Retain logs and treat the run as failed. Current cleanup requests normal Wayland close and checks exit status. |
| `QWaylandTextInputManagerV3` header missing | Rebuild the updated source: v3 registration is conditional on the installed Qt headers. Qt's input-method protocol and text-input v2 remain available. |
| Clang 18 reports `qsharedpointer_impl.h:130` use-after-free | Use the configured Clang 19 analysis job and run `test_analyzer.py`; keep memory-safety checks enabled. The minimal Qt guard reproduces an old analyzer false positive. |
| Qt 6.4 crash in `zwp_text_input_v2::handle_modifiers_map` | Rebuild with the corrected input-protocol registration order, then require the complete Wayland session test to pass. |
| Blur reports non-finite float-to-integer conversion | Rebuild with bounded blur geometry and run `blur-geometry` plus the Wayland rendering test. |
| `No GLSL shader code found` / `Failed to build graphics pipeline state` | Rebuild with the desktop compatibility profile; Qt's external OES material requires GLSL 120. Try `--graphics gles` if needed. Test the host GPU path with `LUDASH_TEST_HOST_WAYLAND=1`; the session checker rejects these messages even on exit code zero. |
| NetworkManager unavailable | Existing interfaces may still work. Install/configure the appropriate system network tools or continue offline. |
| Guide repeats / preferences reset | Check XDG_CONFIG_HOME, write access and whether a test uses an intentionally fresh directory. |
| Pages returns 404 | Enable Pages, verify repository/plan permissions, inspect deployment workflow and confirm the published URL. |

For boot login installation, optional SDDM auto-login and recovery, see [Login session](LOGIN_SESSION.md). `tests/wayland/test_session_launcher.py` checks launcher isolation, literal arguments, log permissions and installer dry-run behavior with fake commands; it never acquires a GPU or changes services.

## 9. Verification record

<!-- verification-results -->
Verified locally on 2026-09-14 (Asia/Taipei). These results describe executed tests, not merely workflow configuration.

| Environment / check | Result |
| --- | --- |
| Arch development host, Qt 6.11.2, GCC | Full build and all 14 CTest checks passed. |
| Arch host, Clang 22 with clang-tidy, ASan and UBSan | Full build and all 14 CTest checks passed; enabled analyzer warnings remain errors. |
| Arch GL and GLES sessions with Quickshell and sanitizers | Three native clients rendered without overlap, blur rendered, and shutdown was clean for both APIs. |
| Host Wayland GPU path, Qt 6.11.2 | OpenGL 4.6 compatibility and GLES 3.2 rendered three clients and closed cleanly after enabling the global share context. No missing-GLSL, failed-pipeline or missing-current-context diagnostics remained. Qt still reports orphaned EGLStream textures at teardown; resource-leak coverage is not claimed. |
| Arch settings, setup and customization | All 15 settings categories, first-run persistence (three consecutive two-session runs), shell interactions, JSON validation, custom QML replacement/error fallback, live Files palette changes and interactive Fish passed during this change. |
| Arch live effects | Blur/opacity changes, reduced motion, minimize/restore and clean shutdown passed on the final renderer. |
| Ubuntu 24.04.4 container, Qt 6.4.2, Clang 19 | All production translation units passed static analysis. The Qt guard / intentional use-after-free analyzer regression passed. |
| Ubuntu container, Clang 18 ASan/UBSan | All 12 CTest checks passed. The final no-shell Wayland session rendered two clients and 15 blur frames, closed cleanly, and passed authenticated X11 lifecycle and deliberate client-crash detection. |
| Astro/TypeScript website | Type checking: zero errors, warnings or hints. Five built English pages passed asset/link checks. Desktop/mobile browser checks passed, including generated module JSON and clipboard behavior. |
| Packaging and source policy | Staged CMake installation, installed launcher preflight, installer dry-run, package metadata, source archive contents, English source policy and whitespace checks passed. No system installation, service changes or physical boot login were performed. |

The supplied GitHub Actions logs exposed an optional Qt header failure and Clang 18's Qt pointer diagnostic. Ubuntu reproduction also exposed an Xauthority length conversion, Qt 6.4 input-protocol announcement ordering and a render-matrix lifetime bug; the final checks above include their fixes. Successful final Ubuntu session and analysis results are in `build/ci-evidence/ubuntu-final-session-and-analysis.log`; its clean final unit-test report is `build/ci-evidence/ubuntu-asan-tests.log`. Intermediate failure reproductions are under `build/ci-evidence/diagnostics/`. Other local evidence is retained under `build/ci-evidence/`; screenshots are in `build/`.

The later Arch CI first-run failure was not reproduced by four baseline retries. Shutdown now waits for observed layer unmapping and drained control helpers rather than relying on a fixed 300 ms delay; failed shutdowns retain detailed process/layer/animation diagnostics and the workflow uploads the logs. The original GPU shader mismatch was identified in Qt source; host testing then reproduced missing EGLStream import contexts, fixed by enabling the global share group. Current evidence is in `build/ci-evidence/graphics-*.log`; before-fix host failures are in `build/ci-evidence/diagnostics/host-before-share-*.log`.

GitHub Actions and CodeQL have not been rerun remotely from this workspace. Push the reviewed changes and require the remote jobs to pass; the supplied failed runs are not a successful CI result. Fedora remains configured but was not reproduced locally in this run. Xvfb/software rendering does not verify physical GPU drivers or a complete standalone login session. Leak detection was disabled; no memory-leak coverage is claimed.
<!-- /verification-results -->

See [Modules](MODULES.md) for JSON/QML templates, and [Default apps and Files](DEFAULT_APPS_AND_FILES.md) for terminal setup and file-operation limits.

## 10. Every maintained file

Generated build output, dependency caches, source archives and Git internals are excluded. Header rows describe interfaces; matching implementation rows describe behavior.

### Root and automation

| File | Purpose |
| --- | --- |
| [.clang-tidy](../.clang-tidy) | Selected security/crash checks; enabled warnings are errors. |
| [.github/codeql/config.yml](../.github/codeql/config.yml) | CodeQL C/C++ security and quality query configuration. |
| [.github/dependabot.yml](../.github/dependabot.yml) | Weekly GitHub Actions and website npm dependency update configuration. |
| [.github/pull_request_template.md](../.github/pull_request_template.md) | Review template for behavior, validation and security/lifetime changes. |
| [.github/workflows/build.yml](../.github/workflows/build.yml) | Arch/Ubuntu/Fedora builds; Arch shell, graphics and first-run integration. |
| [.github/workflows/pages.yml](../.github/workflows/pages.yml) | TypeScript/site PR checks and main-only GitHub Pages deployment. |
| [.github/workflows/security.yml](../.github/workflows/security.yml) | clang-tidy, ASan/UBSan, negative crash test and CodeQL SARIF gate. |
| [.gitignore](../.gitignore) | Exclude generated builds, package artifacts and website dependencies/output. |
| [AGENTS.md](../AGENTS.md) | Repository implementation and collaboration rules. |
| [CMakeLists.txt](../CMakeLists.txt) | Explicit targets, dependencies, resources, installation and CTest registration. |
| [LICENSE](../LICENSE) | GPL-3.0-only license text. |
| [README.md](../README.md) | Project introduction, dependencies, quick start and limitations. |

### C and C++ headers

| File | Purpose |
| --- | --- |
| [include/LuDash/animation/WindowAnimations.h](../include/LuDash/animation/WindowAnimations.h) | Declare interfaces/types to animate window visibility and safely cancel interrupted transitions. |
| [include/LuDash/application_catalog/ApplicationCatalog.h](../include/LuDash/application_catalog/ApplicationCatalog.h) | Declare interfaces/types to discover installed application entries and launch requests. |
| [include/LuDash/application_window/ApplicationWindow.h](../include/LuDash/application_window/ApplicationWindow.h) | Declare interfaces/types to host built-in applications and route close requests. |
| [include/LuDash/audio_settings/AudioSettings.h](../include/LuDash/audio_settings/AudioSettings.h) | Declare interfaces to validate audio requests and read/control default PipeWire devices. |
| [include/LuDash/blur/BlurGeometry.h](../include/LuDash/blur/BlurGeometry.h) | Declare interfaces to clip finite blur capture coordinates before integer conversion. |
| [include/LuDash/blur/BlurItem.h](../include/LuDash/blur/BlurItem.h) | Declare interfaces/types to synchronize application blur properties into the scene graph. |
| [include/LuDash/blur/BlurNode.h](../include/LuDash/blur/BlurNode.h) | Declare interfaces/types to bridge Qt scene graph state and the C blur renderer. |
| [include/LuDash/compositor/ClientWindow.h](../include/LuDash/compositor/ClientWindow.h) | Declare interfaces/types to track compositor-owned client state and geometry. |
| [include/LuDash/compositor/WaylandCompositor.h](../include/LuDash/compositor/WaylandCompositor.h) | Declare interfaces/types to own Wayland clients, workspaces, process lifetimes and control commands. |
| [include/LuDash/configuration/DesktopPreferences.h](../include/LuDash/configuration/DesktopPreferences.h) | Declare interfaces/types to validate and persist appearance and first-run completion. |
| [include/LuDash/console/Console.h](../include/LuDash/console/Console.h) | Declare interfaces/types to run bounded shell commands with process-group cleanup. |
| [include/LuDash/default_applications/DefaultApplications.h](../include/LuDash/default_applications/DefaultApplications.h) | Declare interfaces to validate default app argument arrays and prepare the Konsole/Fish profile. |
| [include/LuDash/display_settings/DisplaySettings.h](../include/LuDash/display_settings/DisplaySettings.h) | Declare interfaces to describe the output and validate nested window-size changes. |
| [include/LuDash/fade_plugin/FadePlugin.h](../include/LuDash/fade_plugin/FadePlugin.h) | Declare interfaces/types to animate the opt-in example window effect. |
| [include/LuDash/file_icons/FileIconDelegate.h](../include/LuDash/file_icons/FileIconDelegate.h) | Declare interfaces to decorate file rows and icon grids with themed outline icons. |
| [include/LuDash/file_icons/FileIcons.h](../include/LuDash/file_icons/FileIcons.h) | Declare interfaces to draw a cached, consistent outline icon set. |
| [include/LuDash/file_manager/FileManager.h](../include/LuDash/file_manager/FileManager.h) | Declare interfaces/types to browse the local filesystem. |
| [include/LuDash/file_operations/FileOperations.h](../include/LuDash/file_operations/FileOperations.h) | Declare interfaces to perform guarded asynchronous file operations. |
| [include/LuDash/input_method/InputMethodSupport.h](../include/LuDash/input_method/InputMethodSupport.h) | Declare interfaces/types to register Qt and Wayland text-input protocols. |
| [include/LuDash/input_settings/InputSettings.h](../include/LuDash/input_settings/InputSettings.h) | Declare interfaces to apply validated keyboard maps and repeat settings to the Wayland seat. |
| [include/LuDash/ipc/ControlServer.h](../include/LuDash/ipc/ControlServer.h) | Declare interfaces/types to serve bounded user-only JSON control requests. |
| [include/LuDash/launcher/Launcher.h](../include/LuDash/launcher/Launcher.h) | Declare interfaces/types to present the native application launcher. |
| [include/LuDash/layer_shell/LayerShell.h](../include/LuDash/layer_shell/LayerShell.h) | Declare interfaces/types to bind and negotiate the supported layer-shell global. |
| [include/LuDash/layer_shell/LayerSurface.h](../include/LuDash/layer_shell/LayerSurface.h) | Declare interfaces/types to configure, place and retire layer surfaces. |
| [include/LuDash/localization/JsonTranslator.h](../include/LuDash/localization/JsonTranslator.h) | Declare interfaces/types to adapt the external dictionary to Qt translation. |
| [include/LuDash/localization/Localization.h](../include/LuDash/localization/Localization.h) | Declare interfaces/types to select language and load external dictionary resources. |
| [include/LuDash/network/NetworkStatus.h](../include/LuDash/network/NetworkStatus.h) | Declare interfaces/types to read NetworkManager asynchronously and distinguish link/Internet state. |
| [include/LuDash/packages/PackageManager.h](../include/LuDash/packages/PackageManager.h) | Declare interfaces/types to validate package names and use confirmed terminal pacman operations. |
| [include/LuDash/plugin_settings/PluginSettings.h](../include/LuDash/plugin_settings/PluginSettings.h) | Declare interfaces/types to show metadata and save explicit native-plugin enablement. |
| [include/LuDash/plugins/CompositorPlugin.h](../include/LuDash/plugins/CompositorPlugin.h) | Declare interfaces/types to define the versioned window-effect plugin contract. |
| [include/LuDash/plugins/PluginManager.h](../include/LuDash/plugins/PluginManager.h) | Declare interfaces/types to validate metadata/library paths and load enabled effects. |
| [include/LuDash/power_settings/PowerSettings.h](../include/LuDash/power_settings/PowerSettings.h) | Declare interfaces to read supported power profiles and request allowed changes. |
| [include/LuDash/process_runner/CommandRunner.h](../include/LuDash/process_runner/CommandRunner.h) | Declare interfaces to run bounded asynchronous helper commands with cancellation and output limits. |
| [include/LuDash/render_core/BlurPass.h](../include/LuDash/render_core/BlurPass.h) | Declare interfaces/types to capture the backdrop and draw two Gaussian blur passes. |
| [include/LuDash/render_core/GLDispatch.h](../include/LuDash/render_core/GLDispatch.h) | Declare interfaces/types to resolve OpenGL and GLES function pointers from the current context. |
| [include/LuDash/render_core/ShaderProgram.h](../include/LuDash/render_core/ShaderProgram.h) | Declare interfaces/types to compile and link GLSL with bounded diagnostics and explicit ownership. |
| [include/LuDash/renderer/RenderBackend.h](../include/LuDash/renderer/RenderBackend.h) | Declare interfaces/types to select and configure the graphics API and shared render health. |
| [include/LuDash/renderer/WallpaperItem.h](../include/LuDash/renderer/WallpaperItem.h) | Declare interfaces/types to expose the compositor framebuffer wallpaper item. |
| [include/LuDash/renderer/WallpaperRenderer.h](../include/LuDash/renderer/WallpaperRenderer.h) | Declare interfaces/types to compile GLSL and draw with the current render-thread context. |
| [include/LuDash/shell_modules/ModuleSchema.h](../include/LuDash/shell_modules/ModuleSchema.h) | Declare interfaces to validate and normalize versioned shell module metadata. |
| [include/LuDash/shell_modules/ShellModules.h](../include/LuDash/shell_modules/ShellModules.h) | Declare interfaces to persist module configuration, enforce trust and watch custom entrypoints. |
| [include/LuDash/system_metrics/SystemMetrics.h](../include/LuDash/system_metrics/SystemMetrics.h) | Declare interfaces/types to parse bounded CPU and memory counters with overflow validation. |
| [include/LuDash/system_monitor/SystemMonitor.h](../include/LuDash/system_monitor/SystemMonitor.h) | Declare interfaces/types to show native process/system monitoring. |
| [include/LuDash/system_status/SystemStatus.h](../include/LuDash/system_status/SystemStatus.h) | Declare interfaces/types to sample CPU, memory, disk and battery data for the shell. |
| [include/LuDash/system_tools/SystemTools.h](../include/LuDash/system_tools/SystemTools.h) | Declare interfaces to resolve fixed system/host editor commands and package availability. |
| [include/LuDash/theme/DesktopTheme.h](../include/LuDash/theme/DesktopTheme.h) | Declare interfaces/types to style the native Qt Widgets tools. |
| [include/LuDash/tiling/TilingLayout.h](../include/LuDash/tiling/TilingLayout.h) | Declare interfaces/types to compute master/stack rectangles with bounded gaps. |
| [include/LuDash/tiling_core/TilingGeometry.h](../include/LuDash/tiling_core/TilingGeometry.h) | Declare interfaces/types to calculate bounded master/stack rectangles without Qt. |
| [include/LuDash/wallpaper/WallpaperSettings.h](../include/LuDash/wallpaper/WallpaperSettings.h) | Declare interfaces/types to validate local image paths and select image/shader wallpaper. |
| [include/LuDash/welcome/Welcome.h](../include/LuDash/welcome/Welcome.h) | Declare interfaces/types to provide the optional native welcome/demo application. |
| [include/LuDash/window_frame/WindowFrame.h](../include/LuDash/window_frame/WindowFrame.h) | Declare interfaces/types to paint and handle compositor window decorations. |
| [include/LuDash/xwayland/XWaylandSupport.h](../include/LuDash/xwayland/XWaylandSupport.h) | Declare interfaces/types to manage the optional authenticated XWayland compatibility container. |

### C and C++ implementations

| File | Purpose |
| --- | --- |
| [src/animation/WindowAnimations.cpp](../src/animation/WindowAnimations.cpp) | Implement behavior to animate window visibility and safely cancel interrupted transitions. |
| [src/application_catalog/ApplicationCatalog.cpp](../src/application_catalog/ApplicationCatalog.cpp) | Implement behavior to discover installed application entries and launch requests. |
| [src/application_window/ApplicationWindow.cpp](../src/application_window/ApplicationWindow.cpp) | Implement behavior to host built-in applications and route close requests. |
| [src/audio_settings/AudioSettings.cpp](../src/audio_settings/AudioSettings.cpp) | Implement behavior to validate audio requests and read/control default PipeWire devices. |
| [src/blur/BlurGeometry.cpp](../src/blur/BlurGeometry.cpp) | Implement behavior to clip finite blur capture coordinates before integer conversion. |
| [src/blur/BlurItem.cpp](../src/blur/BlurItem.cpp) | Implement behavior to synchronize application blur properties into the scene graph. |
| [src/blur/BlurNode.cpp](../src/blur/BlurNode.cpp) | Implement behavior to bridge Qt scene graph state and the C blur renderer. |
| [src/compositor/WaylandCompositor.cpp](../src/compositor/WaylandCompositor.cpp) | Implement behavior to own Wayland clients, workspaces, process lifetimes and control commands. |
| [src/configuration/DesktopPreferences.cpp](../src/configuration/DesktopPreferences.cpp) | Implement behavior to validate and persist appearance and first-run completion. |
| [src/console/Console.cpp](../src/console/Console.cpp) | Implement behavior to run bounded shell commands with process-group cleanup. |
| [src/default_applications/DefaultApplications.cpp](../src/default_applications/DefaultApplications.cpp) | Implement behavior to validate default app argument arrays and prepare the Konsole/Fish profile. |
| [src/display_settings/DisplaySettings.cpp](../src/display_settings/DisplaySettings.cpp) | Implement behavior to describe the output and validate nested window-size changes. |
| [src/entrypoints/compositor_main.cpp](../src/entrypoints/compositor_main.cpp) | Parse compositor arguments, create the session and coordinate test shutdown. |
| [src/entrypoints/control_main.cpp](../src/entrypoints/control_main.cpp) | Send one bounded local control request and return the response status. |
| [src/entrypoints/desktop_main.cpp](../src/entrypoints/desktop_main.cpp) | Launch a native app or delegate desktop startup to the compositor. |
| [src/fade_plugin/FadePlugin.cpp](../src/fade_plugin/FadePlugin.cpp) | Implement behavior to animate the opt-in example window effect. |
| [src/file_icons/FileIconDelegate.cpp](../src/file_icons/FileIconDelegate.cpp) | Implement behavior to decorate file rows and icon grids with themed outline icons. |
| [src/file_icons/FileIcons.cpp](../src/file_icons/FileIcons.cpp) | Implement behavior to draw a cached, consistent outline icon set. |
| [src/file_manager/FileManager.cpp](../src/file_manager/FileManager.cpp) | Implement behavior to browse the local filesystem. |
| [src/file_operations/FileOperations.cpp](../src/file_operations/FileOperations.cpp) | Implement behavior to perform guarded asynchronous file operations. |
| [src/input_method/InputMethodSupport.cpp](../src/input_method/InputMethodSupport.cpp) | Implement behavior to register Qt and Wayland text-input protocols. |
| [src/input_settings/InputSettings.cpp](../src/input_settings/InputSettings.cpp) | Implement behavior to apply validated keyboard maps and repeat settings to the Wayland seat. |
| [src/ipc/ControlServer.cpp](../src/ipc/ControlServer.cpp) | Implement behavior to serve bounded user-only JSON control requests. |
| [src/launcher/Launcher.cpp](../src/launcher/Launcher.cpp) | Implement behavior to present the native application launcher. |
| [src/layer_shell/LayerShell.cpp](../src/layer_shell/LayerShell.cpp) | Implement behavior to bind and negotiate the supported layer-shell global. |
| [src/layer_shell/LayerSurface.cpp](../src/layer_shell/LayerSurface.cpp) | Implement behavior to configure, place and retire layer surfaces. |
| [src/localization/Localization.cpp](../src/localization/Localization.cpp) | Implement behavior to select language and load external dictionary resources. |
| [src/network/NetworkStatus.cpp](../src/network/NetworkStatus.cpp) | Implement behavior to read NetworkManager asynchronously and distinguish link/Internet state. |
| [src/packages/PackageManager.cpp](../src/packages/PackageManager.cpp) | Implement behavior to validate package names and use confirmed terminal pacman operations. |
| [src/plugin_settings/PluginSettings.cpp](../src/plugin_settings/PluginSettings.cpp) | Implement behavior to show metadata and save explicit native-plugin enablement. |
| [src/plugins/PluginManager.cpp](../src/plugins/PluginManager.cpp) | Implement behavior to validate metadata/library paths and load enabled effects. |
| [src/power_settings/PowerSettings.cpp](../src/power_settings/PowerSettings.cpp) | Implement behavior to read supported power profiles and request allowed changes. |
| [src/process_runner/CommandRunner.cpp](../src/process_runner/CommandRunner.cpp) | Implement behavior to run bounded asynchronous helper commands with cancellation and output limits. |
| [src/render_core/BlurPass.c](../src/render_core/BlurPass.c) | Implement behavior to capture the backdrop and draw two Gaussian blur passes. |
| [src/render_core/GLDispatch.c](../src/render_core/GLDispatch.c) | Implement behavior to resolve OpenGL and GLES function pointers from the current context. |
| [src/render_core/ShaderProgram.c](../src/render_core/ShaderProgram.c) | Implement behavior to compile and link GLSL with bounded diagnostics and explicit ownership. |
| [src/render_core/WallpaperPass.c](../src/render_core/WallpaperPass.c) | Implement behavior to draw the procedural wallpaper using the C dispatch table. |
| [src/renderer/RenderBackend.cpp](../src/renderer/RenderBackend.cpp) | Implement behavior to select and configure the graphics API and shared render health. |
| [src/renderer/WallpaperItem.cpp](../src/renderer/WallpaperItem.cpp) | Implement behavior to expose the compositor framebuffer wallpaper item. |
| [src/renderer/WallpaperRenderer.cpp](../src/renderer/WallpaperRenderer.cpp) | Implement behavior to compile GLSL and draw with the current render-thread context. |
| [src/shell_modules/ModuleSchema.cpp](../src/shell_modules/ModuleSchema.cpp) | Implement behavior to validate and normalize versioned shell module metadata. |
| [src/shell_modules/ShellModules.cpp](../src/shell_modules/ShellModules.cpp) | Implement behavior to persist module configuration, enforce trust and watch custom entrypoints. |
| [src/system_metrics/SystemMetrics.c](../src/system_metrics/SystemMetrics.c) | Implement behavior to parse bounded CPU and memory counters with overflow validation. |
| [src/system_monitor/SystemMonitor.cpp](../src/system_monitor/SystemMonitor.cpp) | Implement behavior to show native process/system monitoring. |
| [src/system_status/SystemStatus.cpp](../src/system_status/SystemStatus.cpp) | Implement behavior to sample CPU, memory, disk and battery data for the shell. |
| [src/system_tools/SystemTools.cpp](../src/system_tools/SystemTools.cpp) | Implement behavior to resolve fixed system/host editor commands and package availability. |
| [src/theme/DesktopTheme.cpp](../src/theme/DesktopTheme.cpp) | Implement behavior to style the native Qt Widgets tools. |
| [src/tiling/TilingLayout.cpp](../src/tiling/TilingLayout.cpp) | Implement behavior to compute master/stack rectangles with bounded gaps. |
| [src/tiling_core/TilingGeometry.c](../src/tiling_core/TilingGeometry.c) | Implement behavior to calculate bounded master/stack rectangles without Qt. |
| [src/wallpaper/WallpaperSettings.cpp](../src/wallpaper/WallpaperSettings.cpp) | Implement behavior to validate local image paths and select image/shader wallpaper. |
| [src/welcome/Welcome.cpp](../src/welcome/Welcome.cpp) | Implement behavior to provide the optional native welcome/demo application. |
| [src/window_frame/WindowFrame.cpp](../src/window_frame/WindowFrame.cpp) | Implement behavior to paint and handle compositor window decorations. |
| [src/xwayland/XWaylandSupport.cpp](../src/xwayland/XWaylandSupport.cpp) | Implement behavior to manage the optional authenticated XWayland compatibility container. |

### Quickshell UI

| File | Purpose |
| --- | --- |
| [qml/compatibility/X11Launcher.qml](../qml/compatibility/X11Launcher.qml) | Launch an X11 executable through the compatibility service. |
| [qml/components/AnimatedPanel.qml](../qml/components/AnimatedPanel.qml) | Shared animated layer-panel opening and closing. |
| [qml/components/LineIcon.qml](../qml/components/LineIcon.qml) | Scalable outline icons shared by settings categories and search. |
| [qml/components/Segment.qml](../qml/components/Segment.qml) | Rounded animated top-panel button with keyboard access. |
| [qml/components/ShellButton.qml](../qml/components/ShellButton.qml) | Reusable shell button with keyboard and accessibility labels. |
| [qml/components/SoftSlider.qml](../qml/components/SoftSlider.qml) | Rounded accent slider with consistent interaction geometry. |
| [qml/components/SoftSwitch.qml](../qml/components/SoftSwitch.qml) | Accessible compact accent switch for settings toggles. |
| [qml/components/qmldir](../qml/components/qmldir) | Explicit shared UI type registration for dynamic settings imports. |
| [qml/configuration/AppearanceControls.qml](../qml/configuration/AppearanceControls.qml) | Shared accent, gap, panel and information-card controls. |
| [qml/configuration/qmldir](../qml/configuration/qmldir) | Register shared appearance controls for dynamically loaded pages. |
| [qml/effects/EffectsControls.qml](../qml/effects/EffectsControls.qml) | Live blur, transparency and animation preferences. |
| [qml/effects/qmldir](../qml/effects/qmldir) | Register effect controls for dynamically loaded settings pages. |
| [qml/feedback/Message.qml](../qml/feedback/Message.qml) | Dismissible IPC and validation error feedback. |
| [qml/launcher/Launcher.qml](../qml/launcher/Launcher.qml) | Search built-in/installed apps and restore existing windows. |
| [qml/modules/ModuleSurface.qml](../qml/modules/ModuleSurface.qml) | Host built-in or trusted user QML content with style overrides, animations and loading fallback. |
| [qml/overview/Overview.qml](../qml/overview/Overview.qml) | Tabbed dashboard with performance and workspace controls. |
| [qml/panel/PanelSegment.qml](../qml/panel/PanelSegment.qml) | Panel button adapter for per-module height, typography and accent colors. |
| [qml/panel/TopPanel.qml](../qml/panel/TopPanel.qml) | Workspace/app controls, clock, CPU history, memory and battery indicators. |
| [qml/session/LogoutPanel.qml](../qml/session/LogoutPanel.qml) | Confirm or cancel ending the desktop session. |
| [qml/settings/SettingsPanel.qml](../qml/settings/SettingsPanel.qml) | Dedicated settings center with searchable sidebar and dynamically loaded feature pages. |
| [qml/settings/components/DefaultAppEditor.qml](../qml/settings/components/DefaultAppEditor.qml) | Editor for validated terminal/file-manager argument arrays. |
| [qml/settings/components/HelpText.qml](../qml/settings/components/HelpText.qml) | Wrapped localized explanatory text for settings pages. |
| [qml/settings/components/PageTitle.qml](../qml/settings/components/PageTitle.qml) | Localized settings page title. |
| [qml/settings/components/PreferenceSlider.qml](../qml/settings/components/PreferenceSlider.qml) | Validated preference slider with separate pending and saved values. |
| [qml/settings/components/ToolList.qml](../qml/settings/components/ToolList.qml) | System/host tool availability, package guidance and explicit launch buttons. |
| [qml/settings/components/qmldir](../qml/settings/components/qmldir) | Explicit settings component registration for dynamically loaded pages. |
| [qml/settings/pages/about.qml](../qml/settings/pages/about.qml) | Settings page for about; direct controls and explicit system-service availability. |
| [qml/settings/pages/appearance.qml](../qml/settings/pages/appearance.qml) | Settings page for appearance; direct controls and explicit system-service availability. |
| [qml/settings/pages/applications.qml](../qml/settings/pages/applications.qml) | Settings page for applications; direct controls and explicit system-service availability. |
| [qml/settings/pages/bluetooth.qml](../qml/settings/pages/bluetooth.qml) | Settings page for bluetooth; direct controls and explicit system-service availability. |
| [qml/settings/pages/devices.qml](../qml/settings/pages/devices.qml) | Settings page for devices; direct controls and explicit system-service availability. |
| [qml/settings/pages/display.qml](../qml/settings/pages/display.qml) | Settings page for display; direct controls and explicit system-service availability. |
| [qml/settings/pages/general.qml](../qml/settings/pages/general.qml) | Settings page for general; direct controls and explicit system-service availability. |
| [qml/settings/pages/input.qml](../qml/settings/pages/input.qml) | Settings page for input; direct controls and explicit system-service availability. |
| [qml/settings/pages/modules.qml](../qml/settings/pages/modules.qml) | Settings page for modules; direct controls and explicit system-service availability. |
| [qml/settings/pages/network.qml](../qml/settings/pages/network.qml) | Settings page for network; direct controls and explicit system-service availability. |
| [qml/settings/pages/power.qml](../qml/settings/pages/power.qml) | Settings page for power; direct controls and explicit system-service availability. |
| [qml/settings/pages/privacy.qml](../qml/settings/pages/privacy.qml) | Settings page for privacy; direct controls and explicit system-service availability. |
| [qml/settings/pages/sound.qml](../qml/settings/pages/sound.qml) | Settings page for sound; direct controls and explicit system-service availability. |
| [qml/settings/pages/system.qml](../qml/settings/pages/system.qml) | Settings page for system; direct controls and explicit system-service availability. |
| [qml/settings/pages/windows.qml](../qml/settings/pages/windows.qml) | Settings page for windows; direct controls and explicit system-service availability. |
| [qml/setup/SetupWizard.qml](../qml/setup/SetupWizard.qml) | Four-step language, network, appearance and completion guide. |
| [qml/shell.qml](../qml/shell.qml) | Shell root, status polling, serialized commands and panel/setup visibility. |
| [qml/style/Theme.qml](../qml/style/Theme.qml) | Shared palette, font and live accent/panel-height properties. |
| [qml/style/qmldir](../qml/style/qmldir) | Register the Theme singleton. |
| [qml/wallpaper/Wallpaper.qml](../qml/wallpaper/Wallpaper.qml) | Aspect-preserving image layer or transparent shader reveal. |

### Resources, protocol and packaging

| File | Purpose |
| --- | --- |
| [data/ludash.desktop.in](../data/ludash.desktop.in) | Template for the experimental Wayland login-session descriptor. |
| [data/modules/templates/overview/Main.qml](../data/modules/templates/overview/Main.qml) | Original custom dashboard template displaying actual session statistics. |
| [data/modules/templates/panel/Main.qml](../data/modules/templates/panel/Main.qml) | Original custom taskbar template with workspace and settings controls. |
| [data/modules/templates/shell-modules.json](../data/modules/templates/shell-modules.json) | Versioned JSON example with a floating bottom taskbar. |
| [data/plugins/fade/metadata.json](../data/plugins/fade/metadata.json) | Example effect identity, library and API metadata. |
| [data/shaders/blur/blur.frag](../data/shaders/blur/blur.frag) | Separable Gaussian sampling and alpha composition. |
| [data/shaders/blur/blur.vert](../data/shaders/blur/blur.vert) | Full-screen blur pass vertex shader. |
| [data/shaders/wallpaper.frag](../data/shaders/wallpaper.frag) | Dusk/Forest procedural wallpaper fragment shader. |
| [data/shaders/wallpaper.vert](../data/shaders/wallpaper.vert) | Full-screen triangle vertex shader. |
| [data/terminal/ludash.fish](../data/terminal/ludash.fish) | Session-local Fish prompt and colors without rewriting the user profile. |
| [data/translations/en_US.json](../data/translations/en_US.json) | English dictionary entry; empty mappings use source strings. |
| [data/translations/zh_TW.json](../data/translations/zh_TW.json) | External Traditional Chinese translations, including first-run setup. |
| [data/wallpapers/README.md](../data/wallpapers/README.md) | Bundled wallpaper provenance and usage notes. |
| [data/wallpapers/florist.png](../data/wallpapers/florist.png) | Original generated florist wallpaper without baked-in UI. |
| [packaging/arch/PKGBUILD](../packaging/arch/PKGBUILD) | Arch dependencies and local source build/package instructions. |
| [protocols/wlr-layer-shell-unstable-v1.xml](../protocols/wlr-layer-shell-unstable-v1.xml) | Upstream layer-shell wire definition; LuDash implements a negotiated v2 subset. |

### Scripts and tests

| File | Purpose |
| --- | --- |
| [scripts/install-session.sh](../scripts/install-session.sh) | Build and install the Arch package; optionally enable SDDM and explicit boot auto-login. |
| [scripts/ludash-session](../scripts/ludash-session) | Experimental EGLFS/KMS standalone session launcher. |
| [scripts/make-source.sh](../scripts/make-source.sh) | Create the local Arch source archive without build/Python caches. |
| [scripts/security/check_sarif.py](../scripts/security/check_sarif.py) | Fail closed on missing SARIF or security/quality findings. |
| [scripts/test-wayland.sh](../scripts/test-wayland.sh) | Isolated demo/overview/setup rendering tests and screenshot/state evidence. |
| [scripts/testing/check_graphics_log.py](../scripts/testing/check_graphics_log.py) | Reject missing GLSL and failed Qt pipeline diagnostics in session logs. |
| [tests/DesktopTests.cpp](../tests/DesktopTests.cpp) | Behavioral tests for native apps, layout and validated external inputs. |
| [tests/animation/AnimationTests.cpp](../tests/animation/AnimationTests.cpp) | Interrupted transitions, item destruction and reduced-motion tests. |
| [tests/blur/BlurGeometryTests.cpp](../tests/blur/BlurGeometryTests.cpp) | Verify blur capture bounds, pixel scaling and rejection of non-finite coordinates. |
| [tests/configuration/PreferenceTests.cpp](../tests/configuration/PreferenceTests.cpp) | Preference validation and network-state classification tests. |
| [tests/core/CoreTests.c](../tests/core/CoreTests.c) | C geometry and parser boundary, overflow and counter-reset tests. |
| [tests/file_operations/FileTests.cpp](../tests/file_operations/FileTests.cpp) | Overwrite prevention and literal default-app argument tests. |
| [tests/renderer/RenderTests.cpp](../tests/renderer/RenderTests.cpp) | Real GL/GLES context and production shader checks. |
| [tests/renderer/test_shader_diagnostics.py](../tests/renderer/test_shader_diagnostics.py) | Verify that pipeline errors cannot yield a passing integration result. |
| [tests/renderer/test_startup_failure.py](../tests/renderer/test_startup_failure.py) | Regression for controlled graphics initialization failure. |
| [tests/security/test_analyzer.py](../tests/security/test_analyzer.py) | Verify that static analysis accepts valid Qt guards and rejects real use-after-free. |
| [tests/security/test_sarif_gate.py](../tests/security/test_sarif_gate.py) | Negative and positive SARIF gate cases. |
| [tests/security/test_source_language.py](../tests/security/test_source_language.py) | Keep code, documentation and website text in English. |
| [tests/shell_modules/ModuleTests.cpp](../tests/shell_modules/ModuleTests.cpp) | Module bounds, trust defaults, atomic preservation and symlink escape tests. |
| [tests/site/test_site.py](../tests/site/test_site.py) | Check compiled website assets, fragments, language and image descriptions. |
| [tests/system_settings/SettingsTests.cpp](../tests/system_settings/SettingsTests.cpp) | Audio/profile validation, command allowlists and bounded helper output/timeouts. |
| [tests/wayland/test_crash_detection.py](../tests/wayland/test_crash_detection.py) | Crash one owned client and require session failure. |
| [tests/wayland/test_customization.py](../tests/wayland/test_customization.py) | Actual custom QML replacement/fallback, Files recoloring and interactive Fish under Wayland. |
| [tests/wayland/test_effects.py](../tests/wayland/test_effects.py) | Live blur, opacity and reduced-motion preferences; private-safe window screenshot. |
| [tests/wayland/test_session_launcher.py](../tests/wayland/test_session_launcher.py) | Verify login preflight, environment isolation, argument handling and private logs without starting a real desktop. |
| [tests/wayland/test_settings.py](../tests/wayland/test_settings.py) | Load all fifteen settings pages and verify real keyboard/workspace updates without modifying host services. |
| [tests/wayland/test_setup.py](../tests/wayland/test_setup.py) | Walk through offline setup and check preferences across a restart. |
| [tests/wayland/test_shell_interactions.py](../tests/wayland/test_shell_interactions.py) | Click the live shell and verify workspace/app/settings/window behavior. |
| [tests/wayland/test_xwayland.py](../tests/wayland/test_xwayland.py) | Authenticated X11 mapping, denied unauthenticated access and shutdown cleanup. |

### Website

| File | Purpose |
| --- | --- |
| [site/astro.config.mjs](../site/astro.config.mjs) | Configure static Astro output and the GitHub Pages base path. |
| [site/package-lock.json](../site/package-lock.json) | Reproducible npm dependency resolution and integrity metadata. |
| [site/package.json](../site/package.json) | Pinned Astro, checker and TypeScript dependencies and check/build scripts. |
| [site/public/assets/desktop.png](../site/public/assets/desktop.png) | Actual desktop screenshot, captured with identity display off. |
| [site/public/assets/mark.svg](../site/public/assets/mark.svg) | Local LuDash diamond mark and favicon. |
| [site/src/app.ts](../site/src/app.ts) | Strict TypeScript for validated accent/gap controls and clipboard feedback. |
| [site/src/components/CodeBlock.astro](../site/src/components/CodeBlock.astro) | Escaped code examples with clipboard feedback. |
| [site/src/data/api.ts](../site/src/data/api.ts) | Typed local IPC method reference and argument contracts. |
| [site/src/data/settings.ts](../site/src/data/settings.ts) | Typed settings guide content with actual capability boundaries. |
| [site/src/docs.css](../site/src/docs.css) | Responsive documentation typography, tables and module playground styling. |
| [site/src/layouts/DocsLayout.astro](../site/src/layouts/DocsLayout.astro) | Shared English documentation layout and base-aware navigation. |
| [site/src/pages/docs/api.astro](../site/src/pages/docs/api.astro) | Local socket, QML and native plugin API documentation. |
| [site/src/pages/docs/modules.astro](../site/src/pages/docs/modules.astro) | JSON style playground, user QML contract and recovery guide. |
| [site/src/pages/docs/settings.astro](../site/src/pages/docs/settings.astro) | Guide to desktop settings, default applications and Files. |
| [site/src/pages/docs/start.astro](../site/src/pages/docs/start.astro) | Arch setup, nested-session instructions and test commands. |
| [site/src/pages/index.astro](../site/src/pages/index.astro) | Accessible English introduction, actual desktop screenshot and documentation links. |
| [site/src/styles.css](../site/src/styles.css) | Responsive desktop/mobile layout and interactive theme preview styles. |
| [site/tsconfig.json](../site/tsconfig.json) | Strict browser TypeScript settings and generated output directory. |

### Documentation

| File | Purpose |
| --- | --- |
| [docs/APPEARANCE.md](../docs/APPEARANCE.md) | Shell visual design and wallpaper/appearance behavior. |
| [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md) | Process, module and protocol boundaries and missing features. |
| [docs/CONFIGURATION.md](../docs/CONFIGURATION.md) | First-run flow, network boundaries, saved keys and IPC customization. |
| [docs/C_CORE.md](../docs/C_CORE.md) | C11 module boundaries, ownership contracts and checks. |
| [docs/DEFAULT_APPS_AND_FILES.md](../docs/DEFAULT_APPS_AND_FILES.md) | Default terminal and file-manager setup, Fish profile and Files features/limits. |
| [docs/EFFECTS.md](../docs/EFFECTS.md) | Default blur, window transparency, animations and limitations. |
| [docs/GRAPHICS.md](../docs/GRAPHICS.md) | Context, shader, render-thread and graphics-failure behavior. |
| [docs/INPUT_METHODS.md](../docs/INPUT_METHODS.md) | Language registration and honest Fcitx/IBus validation guidance. |
| [docs/LOGIN_SESSION.md](../docs/LOGIN_SESSION.md) | Boot session installation, explicit auto-login, physical-session limits and recovery. |
| [docs/MODULES.md](../docs/MODULES.md) | Shell module schema, QML contract, templates, trust and recovery. |
| [docs/PLUGINS.md](../docs/PLUGINS.md) | Plugin metadata, SDK, loading and native trust boundary. |
| [docs/SECURITY_CHECKS.md](../docs/SECURITY_CHECKS.md) | PR gates, local analysis commands and branch protection instructions. |
| [docs/SETTINGS.md](../docs/SETTINGS.md) | Settings coverage, direct controls, system/host integrations, saved keys and limitations. |
| [docs/TESTING.md](../docs/TESTING.md) | Short entry point to the full testing guide. |
| [docs/TESTING_AND_FILES.md](../docs/TESTING_AND_FILES.md) | This testing guide, evidence record and complete maintained-file map. |
| [docs/WEBSITE.md](../docs/WEBSITE.md) | TypeScript site preview, build, deployment and rollback instructions. |
| [docs/XWAYLAND.md](../docs/XWAYLAND.md) | Optional XWayland setup, authenticated X11 launch and verification. |

## 11. Where to make a change

For panel styling, start with `qml/panel`, `qml/components` and `qml/style`. For stored appearance, use the paired `configuration` module and shared QML controls. For window layout/lifetimes, use `tiling` and `compositor`. For graphics, use `render_core`, `renderer`, `blur` and `data/shaders`. For Qt-free logic, use `tiling_core` and `system_metrics`. Add each new C++ feature in a matching header/implementation directory pair and list it in CMake.

Run checks appropriate to the affected behavior. Context, window-lifetime, protocol and IPC changes need integration and sanitizer coverage. Use actual GitHub job results to report remote CI, never just the presence of workflow files.
