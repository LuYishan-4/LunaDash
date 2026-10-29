# Source architecture

NixOS uses the [flake and system module](NIXOS.md). See [CI policy](CI.md) for branch coverage and verification limits.

LunaDash is a development Wayland desktop built with C++20, C11, wlroots and Qt 6. The desktop shell is the separate Slint LunaDash-Shell, run by SlintLu as a Wayland client ([shell](SHELL.md)). LunaDash-owned Welcome, portal selectors and Package Manager windows use Slint/Winit; Qt Widgets remains only in the optional toolkit compatibility client. Qt still supports compositor and service code during the ongoing migration, and Qt Wayland is used by clients, not as the compositor server. Arch Linux is the primary development platform. Passing nested or software-rendered tests does not establish complete hardware or login-session support.

## Source domains

Headers and implementations are colocated. Directories use lowercase domain names, C++ files use PascalCase `.hpp` / `.cpp`, and C interfaces retain `.h` / `.c`. Project C++ uses `LunaDash`; C symbols keep their `ludash_` prefix and existing C type names. Settings keys, resource IDs for translations, executable aliases and protocol names retain their existing compatibility spellings.

```text
src/
  compositor/
    animation/       wlroots scene transitions
    client/          compositor-owned client state
    input/           seat events, keyboard maps and input-method bridge
    ipc/             bounded local JSON control socket
    plugins/         native effect loading and versioned interface
    renderer/        rendering orchestration and render elements
      opengl/        C GL dispatch, context, shader/program/texture/FBO ownership
        shaders/     explicitly embedded built-in GLSL
    session/         compositor startup, child environment and launch policy
    layout/          layout contract and strategy factory
    stacking/        persistent overlapping-window strategy for plugins
    tiling/          bounded split tree, grouped rows and C geometry
    wayland/         runtime, output and surface lifetimes
      wlroots/       version compatibility and external C header boundaries
    window/          rules, switcher and native scene helpers
    xwayland/        authenticated, on-demand X11 compatibility container
    Main.cpp
  config/            command runner, preferences, localization, plugin catalog
  core/              reusable process runner, contracts and listener helpers
  ctl/               control client and Main.cpp
  desktop/           data/actions exposed to shell and native tools
    app/             native executable dispatcher and default application roles
    package/         Slint Package Manager client and pacman argument policy
  service/portal/    file chooser D-Bus service and Main.cpp
  shell/             generic module schema/runtime, media tool and Main.cpp
```

Each `Main.cpp` delegates to a module implementing `run(argc, argv)`. `DesktopApplication` is a native `exec` dispatcher: it starts the compositor or replaces itself with Welcome, Package Manager, the control client, or the configured file manager. `SessionApplication`, `ControlClient`, `ShellTool` and `Portal` own their startup and event loop. The portal's file chooser implementation is separate from D-Bus service registration. Native localization loads JSON catalogs through `config/localization` and shares the merged dictionary with Slint clients and shell IPC. Shell module configuration belongs to `shell/modules`; metrics, status and update checking belong to `desktop/system`.

The root repository contains neither the Astro website nor the QML shell source. LunaDash uses the Shell revision recorded in `packaging/shell-revision`; the install script fetches that revision separately. Website CI and Pages deployment clone LunaDash-docs into a temporary runner directory. Compositor sources and their C++ host APIs and build integration remain in the root repository under `src/compositor/`. See the [repository workflow](COMPONENT_REPOS.md). The compositor is not an independent CMake build.

## Dependency and ownership rules

Dependencies flow from `core` to `config` to runtime domains. Core cannot include any higher domain. Configuration cannot include compositor or desktop services. Desktop code cannot include compositor internals. Renderer code cannot depend on desktop UI. The source-root include style is uniform, for example `"compositor/renderer/Renderer.hpp"`; deep relative includes are rejected.

`WaylandCompositor` owns desktop/session integration. Its private `Runtime.hpp` declares wlroots state; `Runtime.cpp` owns setup/teardown, `Output.cpp` owns output state, `Surface.cpp` owns xdg/layer surfaces, and `input/Input.cpp` owns input event and input-method handling. `input/Keyboard.cpp` applies physical keyboard maps; `desktop/input` validates layout names with xkbcommon without Qt or wlroots device APIs. Version-dependent xdg role events and destruction preserve the remote implementation's wlroots 0.17/0.18+ distinctions.

`core/templates/WaylandListener.hpp` centralizes listener registration and detachment. Listeners disconnect before their wlroots owner is destroyed. Window close transitions retain scene snapshots without retaining dead surface callbacks. `SceneWindowAnimations` remains the active wlroots animation controller. Qt scene-graph adapters are compiled separately and must not be mistaken for active wlroots effects.

The MPRIS media library uses GLib GDBus and nlohmann/json; it can discover players on both the session bus and the login user's runtime bus without linking QtDBus. The audio-spectrum library captures only the playback monitor through `posix_spawnp` and `poll`, and emits bounded JSON frames without Qt process or event-loop APIs. Audio volume/device control, power-profile status/actions, backlight control and DDC/CI monitor brightness use native service targets with `ProcessRunner` and the Wayland event loop. The shell tool still has other Qt-backed modules that remain in the migration queue.

Default application roles, browser discovery, desktop preference validation/storage, and wallpaper selection/history metadata use standard C++ and nlohmann/json without Qt. New files are written atomically under `${XDG_CONFIG_HOME:-$HOME/.config}/LunaDash/`; a GLib `GKeyFile` importer reads legacy values from `LunaDash.conf`/`LunaDash.ini` once and leaves the old file in place.

System telemetry now reads procfs, sysfs, user metadata and disk capacity with standard Linux APIs. Its periodic refresh uses the wlroots event loop, GPU subprocesses use the native process runner, and the status target returns nlohmann/json without QObject or Qt. Update checks use libcurl on a cancellable worker and return through eventfd; install progress is read with standard filesystem and JSON APIs.

Network status and actions use native argument arrays, the Wayland event loop, `ProcessRunner`, nlohmann/json and GLib GDBus. The service no longer depends on QtDBus or Qt's network/process APIs.

Optional weather lookups use libcurl on a cancellable worker thread and return through eventfd to the Wayland loop. Requests require HTTPS and responses are capped at 64 KiB; the target has no Qt Network dependency.

Plugin metadata discovery is configuration (`config/plugins/PluginCatalog`), so desktop settings can inspect plugins without depending on the compositor loader. Native effects remain disabled by default and require explicit user enablement. Metadata is not a sandbox. Rebuild native plugins against the current installed header after this source namespace change; a stable native binary ABI is not promised for development builds.

## Rendering

The active compositor uses wlroots renderer/allocator/scene ownership. Its `--graphics opengl` and `--graphics gles` preferences both request wlroots' GLES2 renderer; wlroots can select another available renderer in automatic mode. Headless CI explicitly selects pixman. These are distinct from the standalone EGL OpenGL render-element library.

Application glass remains in this scene pipeline. `renderer/blur/WindowGlass` owns non-interactive sibling underlays beneath managed window trees; `SceneBackdrop.c` captures only earlier scene content and blurs it through wlroots render passes. The client buffer stays sharp above the backdrop. Output preparation updates cached underlays before committing the scene, while existing animation code retains temporary opacity ownership. Content-commit revisions and scene geometry invalidate the cache; frame-callback-only commits and the window's own underlay do not. Scene destruction listeners release retained buffers before renderer teardown. The existing appearance preferences configure this path; fullscreen, utility and desktop windows are excluded.

The render-element library has one `Renderer`, one compile-time `ActiveGraphics` selection in `RendererConfig.hpp`, and an `OpenGL` implementation. `RendererTypes.hpp` carries common state without exposing GL handles. `Feature`, `Module` and `RendererTemplate` provide small, consumed lifecycle contracts rather than a global singleton framework. Only create additional provider/manager abstractions when there is an actual extension boundary.

All project-owned raw OpenGL implementation lives in `renderer/opengl`, including blur, wallpaper and decoration passes. `GLDispatch.c` is the C11 function-loading core. `Shader`, `Program`, `Texture` and `Framebuffer` own their GL resources. Failed shader compilation, later-stage creation failure and linking failure release previously allocated objects. The calling process supplies the current EGL context; render resources must be destroyed on its render thread while that context is current.

Built-in shaders are listed explicitly in `cmake/modules/Renderer.cmake` and embedded into a generated C++ resource header. They load from the same IDs after installation or relocation. `data/` contains application assets, translations and plugin/portal metadata, not internal GLSL.

## Extending a feature

Choose an existing domain, add a cohesive interface/implementation pair, identify the lifetime owner, and list each source in the owning CMake target. Add shared types only when needed. Keep QML under `LunaDash-Shell/<feature>/`, translation strings in external catalogs, and platform switches near implementation boundaries. The public compile definitions use `LUDASH_*`; `core/Defines.hpp` and `config/BuildConfig.hpp` expose common capabilities and version metadata. Native rendering selection is centralized rather than repeated throughout feature code.

Reusable module contracts use composition and C++20 concepts. Runtime virtual dispatch is appropriate for existing render elements and the native plugin boundary. Do not introduce an inheritance hierarchy, empty directory or singleton merely to match a diagram.

## Enforcement and limitations

Run `python3 scripts/check-source-layout.py`. It rejects incorrect source naming, old renderer names, duplicate render trees, forbidden include directions, missing explicit CMake sources, shaders outside their owner and raw OpenGL implementation outside its module. `tests/architecture/test_source_layout.py` verifies rejection behavior. Existing source-style and Ubuntu workflows run these checks.

The renderer regression tests load embedded assets from an unrelated directory, report missing-context errors, inject GL failures to check cleanup, and optionally compile/draw through a software OpenGL context after a staged install. These checks do not establish physical GPU correctness.

The wlroots compositor accepts toolkit clients through Wayland protocols. It registers `xdg-activation-v1` when the installed wlroots headers provide it and checks the source seat client's input serial before honoring activation for a mapped application window. Clients still need a fallback on older wlroots builds. Wayland CI exercises protocol globals, xdg-toplevel commit/map/unmap/destroy, client startup, on-demand XWayland and clean shutdown; the activation serial policy also has a source contract check. Chrome, Zed, Fcitx candidate placement, physical input, multiple physical outputs and display-manager login still need application/hardware testing. The current file chooser portal is not a complete screen-sharing/PipeWire portal. There is no complete screen-locking or polkit-agent implementation. See [testing](TESTING_AND_FILES.md), [graphics](GRAPHICS.md), [input methods](INPUT_METHODS.md) and [release process](RELEASE_PROCESS.md).

Media players are read by SlintLu's MPRIS support inside the shell. See [Window layout templates](WINDOW_LAYOUT_TEMPLATES.md).

## NyxNiri-inspired desktop, Orbit and live wallpapers

The shell's panel, dashboard, launcher and dock live in the LunaDash-Shell repository; `src/config/desktop/` owns desktop preferences such as dock visibility.

Panel workspace filtering is a presentation option and does not change compositor workspace allocation. Centered launcher placement, window titles and CPU/memory readings consume existing shell state. Author-inspired configuration stays editable through LunaDash's current JSON/settings interfaces; no Niri configuration parser, replacement compositor or second desktop shell is introduced. The first-install wizard extends the existing installation scripts and selectively imports application profiles, keeping compositor configuration under LunaDash ownership.

See [the desktop integration guide](NYXNIRI_DESKTOP.md) for settings, shortcuts, dependencies and current verification limits, and [session installation](LOGIN_SESSION.md) for the optional first-install workflow.

File-manager launches resolve the external files role (Dolphin by default). The retired `desktop/filemanager` and `desktop/fileoperations` implementations and build targets are removed. The FileChooser and ScreenCast selectors are separate Slint/Winit Wayland clients owned by `service/portal`; the portal service exports FileChooser v4 and Settings v2 with GLib GDBus. The shell image picker remains independent in LunaDash-Shell.

## Current rendering and shell boundary

Renderer selection is C11 in `renderer/selection/RenderSelection.c`; scene transitions use `window/animation/SceneAnimation`. The standalone Welcome tool is a Slint/Winit Wayland client in `desktop/welcome`; its control-socket requests use the same compositor IPC as the Shell. The first-login Quickshell guide remains in the independent Shell repository. The compositor accepts both as ordinary Wayland clients and does not embed either toolkit. No QtWayland compositor/server API is used. The native process runner uses `posix_spawn` and the Wayland event loop for bounded asynchronous output, timeout and child reaping. Audio, power, brightness, network and NVIDIA status services now call it directly. Application theme synchronization uses native file operations and GDBus for Fcitx reloads. Shell-module and Orbit launcher settings use nlohmann/json and GIO in Qt-free targets; the compositor adapts their JSON at its IPC boundary. Other LunaDash targets still link Qt Core/Gui/Widgets/Quick/Network/DBus; the project migration is in progress.

For shell polling, `status <known-language>` omits the translation dictionary when the language matches. Ordinary `status` continues returning the complete state. QML restores its cached dictionary and suppresses invisible frame/event-loop counters without dropping display mode, geometry, error or confirmation changes. Live metrics resume in the control center or About page.

System telemetry also updates through a separate `systemState` binding. Panel CPU/memory and battery values, power settings and user information stay current while unrelated settings retain their stable desktop snapshot.

Keyboard focus is restored when the first keyboard keymap becomes ready, including a virtual keyboard attached after an exclusive shell surface has mapped. Keymap listeners are detached during keyboard and display teardown.
