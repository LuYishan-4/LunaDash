# Slint and wlroots migration

## Target architecture

- Keep the independent `LunaDash-Shell` repository unchanged. Its Quickshell/QML UI remains a separate Wayland client and may use its own Qt runtime.
- Keep LunaDash's compositor and built-in services free of Qt linkage and toolkit-specific surface handling.
- wlroots owns the compositor, Wayland protocol implementation, input routing, outputs, and composition. Shells connect as independent Wayland clients through protocols.
- Slint owns LunaDash-built UI outside the independent shell repository, such as settings, welcome, and portal surfaces.
- Standard C++ and Linux system libraries own configuration, process management, networking, D-Bus, and service integration. wlroots is not a replacement for those services.
- Keep Quickshell optional at the compositor/package boundary so users can run other shells without a Quickshell/Qt dependency. The separate LunaDash-Shell installer may continue to require it.

## Migration order

1. Define and implement a toolkit-neutral Wayland protocol compatibility matrix in the compositor. At minimum cover `xdg-shell` application windows and `wlr-layer-shell` shell surfaces; then review activation, decoration, foreign-toplevel/workspace, screencopy, text-input, and virtual-keyboard protocols by client need.
2. Port LunaDash-owned desktop-visible UIs in this repository: welcome, settings/application surfaces, package manager, and portal chooser. Use Slint as Wayland clients; keep portal path handling inside LunaDash and never evaluate selected paths through a shell.
3. Remove Qt Quick and Qt OpenGL wrappers from the compositor renderer. Keep composition in the wlroots scene/render path and use raw OpenGL only for compositor effects that need custom rendering.
4. Replace Qt utility APIs in non-UI modules with C++ standard-library and Linux service APIs, keeping interfaces in their existing source domains.
5. Replace QtTest-dependent native tests with Qt-free tests. Keep the independent shell's QML tests because that repository is explicitly out of scope.
6. Remove Qt linkage from LunaDash compositor/services and make Quickshell an optional external shell dependency. Keep separate shell packaging and installation requirements in `LunaDash-Shell`.

## Compatibility boundary

The compositor should accept shell surfaces as protocol clients, independent of whether the client UI is built with Qt Quick, Slint, or another toolkit. This avoids embedding UI-toolkit runtimes into the compositor. A toolkit does not guarantee a protocol: for example, an ordinary Slint Wayland window uses `xdg-shell`, while a Slint-based panel needs a client implementation of `wlr-layer-shell` (or an `xdg-shell` fallback). Quickshell and Qt clients likewise only work to the extent that they use protocols the compositor implements.

“Perfect compatibility” with every shell cannot be guaranteed because shells may depend on compositor-specific IPC and protocol extensions. Maintain a published capability matrix and test representative clients against each supported protocol; expose LunaDash-specific controls through optional IPC rather than making them prerequisites for rendering or input.

The current compositor registers `xdg-shell` v3, `wlr-layer-shell` v4, foreign-toplevel management, `wlr-screencopy`, ext foreign-toplevel/window capture where the installed wlroots version provides it, `xdg-output`, idle inhibition, input-method-v2, text-input-v3, and virtual-keyboard. `xdg-activation-v1` is now registered when the installed wlroots headers provide it; activation requests must carry a valid serial for the source seat client before they can focus a mapped target. Preserve these while removing Qt from the compositor. Their registration is the current code baseline, not proof of full client compatibility; each advertised protocol still needs behavior-level coverage.

| Client capability | Current advertisement | Compatibility boundary |
| --- | --- | --- |
| Application windows and popups | `xdg-shell` v3 | Standard Wayland clients can map, resize, maximize, and close through the implemented xdg lifecycle. |
| Panels, backgrounds, and shell overlays | `wlr-layer-shell` v4 | Qt Quick, Slint, and other clients must use this protocol for panel roles; ordinary toolkit windows alone do not provide layer-shell roles. |
| Shared buffers and subsurfaces | `wl_compositor` v5, subcompositor, viewporter, shm, renderer-gated linux-dmabuf and explicit sync | Buffer import depends on negotiated formats and renderer support; clients must retain shm fallback. |
| Toplevel discovery and capture | foreign-toplevel protocols and capture globals when available; `wlr-screencopy` v3 | Optional globals vary with the installed wlroots version and are not a guarantee that every portal implementation supports every capture path. |
| Text input and virtual keyboards | input-method-v2, text-input-v3, virtual-keyboard-v1 | Advertised protocol state is implemented in the compositor; third-party input methods still need their own Wayland client integration. |
| Activation | `xdg-activation-v1` when the installed wlroots headers provide it | Requests are accepted only for valid source seat serials and mapped, non-utility targets; clients should keep a launch/focus fallback on older wlroots builds. |
| Server-side decorations, session lock, and standard workspace control | Not currently advertised as dedicated globals | Shells that require these interfaces or private compositor IPC need a fallback or a separately implemented protocol; toolkit choice does not remove this requirement. |

The matrix describes protocol surfaces and explicit gaps. Expand behavior-level client tests for xdg-shell and layer-shell before claiming compatibility with a particular shell; registry presence alone is insufficient.

## Chosen implementation defaults

The user approved these defaults for the migration:

- JSON/settings: `nlohmann/json` and XDG configuration files.
- D-Bus: GLib `GDBus`.
- HTTP: `libcurl`.
- Process and timer integration: `posix_spawn` integrated with the Wayland event loop.
- Scene composition: wlroots scene/render APIs where available, with project-owned OpenGL effects where necessary.
- Shell compatibility: standard Wayland protocols plus optional LunaDash IPC for richer settings and window/workspace data.

If implementation uncovers a concrete API limitation that materially changes one of these choices, present the tradeoffs before switching.

## Current audit baseline

The application targets still discover and link Qt 6 Core, Gui, Network, and DBus. Qt Widgets is no longer a runtime dependency of LunaDash applications; it is loaded only by the optional Qt Wayland toolkit-compatibility test when `BUILD_TESTING` is enabled. Shell-module and Orbit launcher configuration services now use nlohmann/json, standard file operations and GIO in Qt-free targets; the compositor adapts their data at the existing IPC boundary. Qt Quick/QuickWidgets/OpenGL were removed from the main renderer build: the retained OpenGL render-element library now uses the caller's EGL context and CMake-generated shader resources, while Qt Quick-only wallpaper/blur wrapper classes were unused by compositor runtime and have been removed. Qt APIs are still used outside visible UI for JSON, filesystem paths, process management, timers, HTTP, DBus, and test support. A source scan still finds Qt identifiers in many native files; the migration must distinguish compositor/service linkage from the separately maintained shell runtime.

The independent `LunaDash-Shell` checkout still contains a Quickshell/QML implementation and is intentionally out of scope. The active compositor already uses wlroots for Wayland, input, and scene composition; the session/control code, built-in desktop UI, service APIs, and parts of the test infrastructure still use Qt. Qt Quick renderer wrappers that had no callers have been removed; the retained compositor render path uses wlroots plus project-owned EGL/OpenGL effects.

## Migration progress

- `WindowGlass` and `WindowCorners` now use standard C++ containers, geometry values, strings, and `std::chrono`; their public interfaces no longer include Qt types.
- These wlroots scene effects are built as the separate `ludash-scene-window-effects` target, which links only wlroots, Wayland server, and the C scene-rendering helpers.
- The external shell renderer preference helper now uses a standard C++ environment map and no longer links Qt; the compositor still uses Qt in its broader process/session implementation.
- `lunadashctl` now uses POSIX Unix sockets and nlohmann/json, with the same newline-delimited JSON request/response protocol and runtime socket discovery; the standalone control utility no longer links Qt Network or requires a Qt event loop.
- The compositor control server now uses a private POSIX Unix socket and registers listener and client descriptors with the existing `wl_event_loop`. It preserves newline-delimited JSON framing, the 64 KiB request limit, a three-second client timeout, stale-socket recovery, and user-only socket permissions; the Qt JSON adapter remains at the `WaylandCompositor` boundary while that class is being migrated.
- The renderer's low-level OpenGL path no longer depends on Qt context, Qt resource, or Qt Quick APIs. It consumes the caller's current EGL context, keeps raw GL sources under `renderer/opengl`, and embeds the four built-in shaders in a CMake-generated C++ resource header. Qt Quick-only `QQuickItem`/`QSGRenderNode` wallpaper and blur wrappers were unused outside their own library and are removed; the separate compositor continues to render Wayland clients with wlroots.
- The launch capability policy uses C++ strings/filesystem and nlohmann/json; its built-in rules install as a normal data file and user overrides remain under `$XDG_CONFIG_HOME/lunadash/` (or `~/.config/lunadash/`). The policy target no longer links Qt.
- System-settings tool discovery now uses `std::filesystem`, `PATH`, and executable permissions; the service target no longer links Qt. It can still launch installed KDE/GTK applications as external clients.
- XWayland process management keeps the X server/XWM under wlroots and uses `posix_spawn` plus a wl_event_loop timer to reap explicitly launched X11 applications; `ludash-xwayland` no longer links Qt.
- The compositor advertises `xdg-activation-v1` when supported by the installed wlroots headers and validates the source seat client's event serial before focusing a mapped target surface.
- Window scene animations now use standard C++20 geometry, containers, monotonic timing and easing; `ludash-animation` links only wlroots and Wayland server APIs. Qt animation settings are converted to the toolkit-neutral options at the compositor boundary.
- Session power actions now use GLib `GDBus` for system-bus calls to `systemd-logind`; the service target no longer links QtDBus.
- The standalone Welcome tool now uses Slint 1.18.1 with the Winit Wayland backend and FemtoVG renderer. It talks to the existing compositor control socket, and the legacy `lunadash-desktop --app welcome` command replaces itself with `lunadash-welcome`.
- The Package Manager now runs as the independent `lunadash-packages` Slint/Winit client. `lunadash-desktop` is a native `exec` dispatcher for the compositor, Welcome, Package Manager, settings control requests, and configured file manager; the old QWidget host, theme wrapper, and Qt Wayland shutdown adapter have been removed. Pacman query output is captured without privilege escalation; install, remove, and full-upgrade transactions are shown for confirmation and launched as argument arrays in the configured terminal.
- Runtime localization now uses a native JSON catalog loader and GIO for legacy INI reads. One merged dictionary is shared with Slint clients and shell IPC; language selection is stored in native desktop preferences, and reset suppresses the legacy language value without rewriting the old file. The Qt translation resources and `QTranslator` initialization path have been removed.
- The xdpw ScreenCast source selector now runs as a standalone Slint/Winit Wayland client. The portal executable replaces itself with the sibling helper; the helper preserves the line-based source protocol, local screen/window previews, palette/localization data, keyboard confirmation/cancel, and original UTF-8 labels. The ScreenCast chooser executable has no Qt dependency.
- The FileChooser window now runs in a separate Slint/Winit Wayland helper with bounded list/grid layouts, local path entry, filtering, choices, image previews, and no Qt linkage. The portal service now uses GLib GDBus/GSubprocess for FileChooser v4, Settings v2 and `Request.Close`, without Qt linkage. Headless portal coverage exercises OpenFile, SaveFile, SaveFiles, cancellation, the public frontend, and a FileChooser screenshot when the optional runtime suite is enabled.
- The optional renderer regression executable now uses a headless EGL pbuffer instead of Qt's offscreen context and tests the same embedded shaders and GL resource lifecycle.
- The native process runner now uses `posix_spawn`, bounded nonblocking pipe reads, and `wl_event_loop` fd/timer sources for timeout and child reaping. Audio volume/device control, power-profile status/actions, backlight control and DDC/CI monitor brightness now use Qt-free targets built on that runner.
- Window-switcher background capture now launches `grim` through that runner with the compositor's native client environment. Cancelling a switcher session terminates the helper asynchronously; `QProcess` no longer owns this compositor screenshot process. Thumbnail PNG encoding now uses `std::async` and the compositor no longer links Qt Concurrent; switcher state and timers still use Qt types.
- System telemetry now uses standard C++ filesystem/proc/sys reads, `statvfs`, the C11 metrics core, and `ProcessRunner` for `nvidia-smi`. Its refresh timer is a Wayland event-loop source, its snapshot is nlohmann/json, and `ludash-system-status` has no Qt linkage.
- Update checks now use libcurl on a cancellable worker thread and eventfd for Wayland-loop completion. Stable release and dev commit response validation remain in place, and installer progress uses standard file/JSON APIs; `ludash-update-check` no longer links Qt Network.
- Desktop preference, wallpaper selection/history, and shortcut binding validation and atomic XDG JSON persistence now live in Qt-free targets. The public preference API and keyboard preference application use nlohmann/json and standard strings; a GLib `GKeyFile` importer reads existing desktop, setup-completion, and wallpaper values once while leaving the legacy settings file in place. Resetting desktop preferences preserves setup completion, wallpaper, and shortcut values.
- Network status and actions now use standard C++ argument arrays, nlohmann/json, `ProcessRunner`, GLib GDBus and the Wayland event loop. `ludash-network` no longer links Qt or QtDBus; `nmcli` detail scans run every 30 seconds and LunaDash-issued actions trigger an immediate refresh.
- Application theme synchronization now uses standard filesystem operations with atomic replacement; Fcitx reload requests use GLib GDBus, and the `ludash-theme-sync` target no longer links Qt or QtDBus. The compositor converts its existing JSON types at this service boundary.
- Shell-module and Orbit launcher descriptors, validation and persistence now use nlohmann/json, POSIX atomic file replacement and GIO in separate Qt-free targets. Module change monitoring uses GIO; the compositor adapts their data at its existing Qt IPC boundary. Orbit defaults install as a normal data file instead of a Qt resource.
- Weather requests now use libcurl on a cancellable worker thread and return through an eventfd registered with the Wayland event loop. The response is capped at 64 KiB, HTTPS is required, and the weather target no longer links Qt Network.
- Keyboard layout validation and repeat settings now use `std::string` plus xkbcommon/xkbregistry, so the input-settings target no longer links Qt. wlroots remains the owner of keyboard devices and keymap application.
- Shortcut validation, default bindings and persistence now use `std::string`, nlohmann/json and xkbcommon; `ludash-shortcut-settings` no longer links Qt. The compositor imports the existing QSettings value once at its Qt boundary, then the shortcut service reads and writes the native XDG JSON store.
- Wayland client environment construction, XDG data lookup, icon-theme discovery and proxy preference application use a standard C++ environment map, filesystem paths, INI reads and nlohmann/json. The compositor, XWayland, session services, screenshots and window switcher carry that native map; the remaining QProcess callers convert only at their launch boundary. Portal activation and service refresh use `posix_spawn`, explicit child environments and the wlroots event loop through `ProcessRunner`; both session targets and their native tests no longer link Qt.
- Appearance palette generation and application-theme synchronization use nlohmann/json in Qt-free libraries. The compositor converts values only where its existing Qt-based IPC implementation still requires Qt JSON.
- The MPRIS media discovery/action library now uses GLib GDBus and nlohmann/json. It keeps the existing session-bus plus login-user-bus search, concurrent property requests, player selection, action validation, and bounded local lyrics lookup; the media library no longer links Qt.
- The audio-spectrum helper now uses `posix_spawnp`, `poll`, and nlohmann/json to stream PulseAudio-compatible monitor PCM without Qt process or event-loop APIs. It still monitors playback output only, bounds queued frames, handles parent termination, and does not persist audio.
- Default application roles and browser discovery now use standard C++ and nlohmann/json with no Qt linkage. New values are saved atomically under `${XDG_CONFIG_HOME:-$HOME/.config}/LunaDash/default-applications.json`; existing `defaultApps` entries in `LunaDash.conf`/`LunaDash.ini` remain readable until saved to the native file. Desktop tools and the compositor adapt argument arrays only at their Qt process/IPC boundaries.
- Removed two unused compositor dependencies: the `QWindow` nested-display helper had no callers, and the Qt `CommandRunner` wrapper had no callers after services moved to `ProcessRunner`. Their targets and sources are gone; wlroots owns output configuration and the native process runner remains the subprocess API.
- Custom and virtual outputs without a reported mode now advertise a 60 Hz fallback mode. This keeps Wayland refresh values valid for clients such as GTK 4, whose frame callback calculation divides by the advertised monitor rate.
- `test_toolkits.py` maps Slint Welcome, Package Manager and ScreenCast clients alongside GTK 3, GTK 4 and Qt Wayland clients; it checks that each LunaDash Slint process did not load Qt, verifies a nonzero output refresh, and captures both the combined toolkit session and a standalone Package Manager screenshot. Local verification builds all targets and passes 37 CTest cases plus the pixman toolkit flow. A staged `/usr` install resolves every installed executable's shared libraries without `LD_LIBRARY_PATH`; the Welcome, Package Manager, FileChooser, ScreenCast, control and portal clients have no Qt runtime dependency. A nested host-Wayland run still fails wlroots swapchain testing with both pixman and GLES2, so a real-session screenshot remains unverified.
- The portal FileChooser process now creates no Qt window. The optional runtime suite types paths through Wayland and covers SaveFiles collision handling in addition to OpenFile/SaveFile, request cancellation and the public portal frontend.
- The compositor executable and broader desktop targets still discover and link Qt. The migration is in progress; this target split alone does not make the application Qt-free.

The current translation checker reports 100% static-string coverage for `zh_TW`, `zh_CN`, and `ja_JP`, with no duplicate catalog keys. The QML design/action reviews and pixman toolkit flow pass. A host nested-session swapchain failure is an environment limit and does not establish real GPU or login-session compatibility.

The first dependency choices are fixed: nlohmann/json for JSON at the new toolkit-neutral boundaries, and standard Linux Unix sockets for the control client. Keep migrating one service boundary at a time while preserving the existing external IPC schema until all clients have moved.

## Completion criteria

- A clean compositor/services build does not discover, include, link, or download Qt.
- The compositor handles Qt/Quickshell and Slint shell clients through Wayland protocols, without linking their toolkits.
- The separate LunaDash-Shell runtime may depend on Qt/Quickshell; base compositor packages keep that dependency optional.
- Settings, welcome, and portal UI run through Slint; Quickshell remains the existing independently installed shell.
- Compositor protocols, input, and output lifecycle remain owned by wlroots.
- Configured CI and verified real-session behavior are reported separately.
