# Architecture

```text
Existing Wayland host / experimental EGLFS-KMS session
  ludash-compositor: C++20 Qt Wayland integration + C11 OpenGL / GLES core
    xdg-shell: native windows and the XWayland compatibility container
    viewporter: client viewport scaling
    layer-shell v2 subset: Quickshell desktop surfaces
    text-input v2/v3 and Qt input-method protocol
    master/stack tiling, four workspaces, focus and window effects
    asynchronous NetworkManager status and saved desktop preferences
    user-only local JSON control socket
      ludashctl <-> Quickshell status and commands
    quickshell --path qml/shell.qml
      setup / wallpaper / panel / overview / launcher / settings / session
    ludash-desktop --app <id>
      files / console / monitor / packages / plugins / settings
```

Quickshell/QML implements the desktop shell. Built-in applications currently use C++ Qt Widgets and tile alongside other Wayland applications. The compositor owns Wayland window lifetimes and rendering; Quickshell runs in a separate process with its own graphics context.

Each C++ feature has a matching `include/LuDash/<feature>/` and `src/<feature>/` directory. Headers declare types and interfaces; `.cpp` files implement them. Entry points live in `src/entrypoints/`; CMake lists sources explicitly. QML features live under `qml/<feature>/`. Project code is English and uses namespace `LuDash`, except `main`, Qt-generated resource initialization and scanner-generated C protocol symbols.

| Module | Responsibility |
| --- | --- |
| render_core / renderer / blur | C shader/FBO passes with C++ context and scene-graph adapters |
| animation | Safe fade/scale transitions and reduced-motion controls |
| xwayland | Optional authenticated XWayland service and X11 launcher |
| tiling_core / system_metrics | Qt-independent C geometry and bounded proc parsers |
| compositor / tiling / window_frame | Window lifetime, layout and decorations |
| layer_shell | Background, panel and overlay surfaces; negotiated v2 subset |
| ipc | User-only local socket, 64 KiB request limit, three-second timeout |
| configuration | Validated saved appearance and first-run completion |
| network | Nonblocking NetworkManager D-Bus reads, interface fallback |
| system_status / wallpaper | Local system statistics and validated local images |
| localization / input_method | Translation resources and input protocol registration |
| plugins / fade_plugin / plugin_settings | Metadata discovery, explicit enablement and native example |
| packages | Read-only queries and confirmed terminal-based pacman changes |
| remaining app modules | Separate files, console, monitor and application tools |

The shell controls LuDash through allowlisted JSON methods. Appearance updates reject unknown keys, incorrect types and out-of-range numbers before changing settings. Network status is read asynchronously with a 1.5-second D-Bus timeout every five seconds. Passwords are handled by the external network editor, never passed through LuDash's control socket.

The current compositor uses one output. Panel height and tiling gaps define its work area; this is not a general implementation of arbitrary exclusive zones. Layer-shell popups and some double-buffered state behavior remain incomplete. Multiple outputs, locking, portals, PipeWire capture, an audio service, a polkit agent and a full input-method-v2 bridge are not implemented. The EGLFS/KMS launcher is experimental; evaluate nested sessions first.

See [C core](C_CORE.md), [Effects](EFFECTS.md) and [X11 compatibility](XWAYLAND.md) for implementation boundaries.

The Quickshell settings center delegates fixed system-tool IDs to `system_tools`, bounded helper processes to `process_runner`, audio to `audio_settings`, power profiles to `power_settings`, keyboard configuration to `input_settings`, and nested output resizing to `display_settings`. Native settings entry points route to this shared interface.
