# Architecture

```text
Existing Wayland host / experimental EGLFS-KMS session
  lunadash-compositor: C++20 Qt Wayland integration + C11 OpenGL / GLES core
    xdg-shell: native windows and the XWayland compatibility container
    viewporter: client viewport scaling
    layer-shell v2 subset: Quickshell desktop surfaces
    text-input v2, optional v3 and Qt input-method protocol
    niri-inspired scrollable grouped columns, four workspaces, focus and window effects
    asynchronous NetworkManager status and saved desktop preferences
    user-only local JSON control socket
      lunadashctl <-> Quickshell status and commands
    quickshell --path qml/shell.qml (source) or /usr/share/lunadash/shell/shell.qml (installed)
      setup / wallpaper / single panel with inline grouped cells / overview / launcher / fullscreen settings / wallpaper picker / session
    lunadash-desktop --app <id>
      files / console / monitor / packages / plugins / settings
```

Quickshell/QML implements the desktop shell. Built-in applications currently use C++ Qt Widgets and tile alongside other Wayland applications. A tiled column has an independently resizable width and can contain up to four total members, including minimized windows. The C layout divides the compositor `workArea` equally among visible members; focus scrolls the selected column/member into view, and columns can be grouped, expelled, reordered or centered. The compositor owns Wayland window lifetimes, resolved per-window icon names and rendering; Quickshell runs in a separate process with its own graphics context.

Each C++ feature has a matching `include/LuDash/<feature>/` and `src/<feature>/` directory. Headers declare types and interfaces; `.cpp` files implement them. Entry points live in `src/entrypoints/`; CMake lists sources explicitly. QML features live under `qml/<feature>/`. Project code is English and uses namespace `LuDash`, except `main`, Qt-generated resource initialization and scanner-generated C protocol symbols.

| Module | Responsibility |
| --- | --- |
| render_core / renderer / blur | C shader/FBO passes with C++ context and scene-graph adapters |
| animation | Safe fade/scale transitions and reduced-motion controls |
| xwayland | Optional authenticated XWayland service and X11 launcher |
| tiling_core / system_metrics | Qt-independent C geometry and bounded proc parsers |
| compositor / tiling / window_frame / window_rules | Window lifetime, grouped-column layout, initial window policy and decorations |
| file_manager / file_operations | Local filesystem browsing and guarded asynchronous file operations for the built-in Files app |
| default_applications | Built-in Fish terminal or user-selected launch commands |
| layer_shell | Background, panel and overlay surfaces; negotiated v2 subset |
| ipc | User-only local socket, 64 KiB request limit, three-second timeout |
| configuration | Validated saved appearance and first-run completion |
| network | Nonblocking NetworkManager D-Bus reads, interface fallback |
| session_environment | Prepare isolated Wayland, desktop and toolkit-module variables for compositor children |
| session_actions | Query logind action availability and request suspend, reboot or poweroff over D-Bus without shell execution |
| system_status / wallpaper | Local system statistics and validated local images |
| localization / input_method | Translation resources and input protocol registration |
| plugins / fade_plugin / plugin_settings | Metadata discovery, explicit enablement and native example |
| packages | Read-only queries and confirmed terminal-based pacman changes |
| remaining app modules | Separate files, console, monitor and application tools |

The shell controls LunaDash through allowlisted JSON methods. It instantiates one compact, niri-inspired `TopPanel`: workspace/session controls and inline grouped-application cells on the left, a centered three-part overview / accent launcher / settings selector with a Lambda (`Λ`) glyph, and StatusNotifier items plus clock/network/battery on the right. The former `ColumnStrip` is not instantiated as a second layer; `TopPanel` embeds `ColumnCell`/`MemberIcon` behavior inline. One cell represents each column and one icon represents each member. Exact-member focus, cross-column drag/drop grouping and right-click/minus expulsion route through compositor IPC. The launcher merges four built-ins with installed `DesktopEntries` into one metadata-ranked, token-searchable list. Settings uses a fullscreen QML overlay with quick hide and a top margin below `Theme.barHeight`. Appearance updates reject unknown keys, incorrect types and out-of-range numbers before changing settings. Network status is read asynchronously with a 1.5-second D-Bus timeout every five seconds. Passwords are handled by the external network editor, never passed through LunaDash's control socket.

The current compositor uses one output. `ShellModules::panelExtent()` reports the active panel's total reserved extent, corresponding to `TopPanel`'s layer-shell exclusive area. `WaylandCompositor::workArea()` starts application geometry below that extent for a top panel (then applies the configured gap), so tiled, maximized and normally placed windows do not cover it. `SettingsPanel` independently starts below `Theme.barHeight`, keeping the overlay off the panel. This is not a general implementation of arbitrary exclusive zones. Layer-shell popups and some double-buffered state behavior remain incomplete. Multiple outputs, locking, portals, PipeWire capture, an audio service, a polkit agent and a full input-method-v2 bridge are not implemented. StatusNotifier hosting is implemented through Quickshell with Fcitx/Discord/Docker icon fallbacks; this does not imply a notification daemon or complete input-method integration. The EGLFS/KMS launcher is experimental; evaluate nested sessions first.

See [C core](C_CORE.md), [Effects](EFFECTS.md) and [X11 compatibility](XWAYLAND.md) for implementation boundaries.

The Quickshell settings center delegates fixed system-tool IDs to `system_tools`, bounded helper processes to `process_runner`, audio to `audio_settings`, power profiles to `power_settings`, keyboard configuration to `input_settings`, and nested output resizing to `display_settings`. Native settings entry points route to this shared interface. Wallpaper selection opens the in-shell picker inside the settings surface (`qml/imagepicker`); accepted local paths are returned through `lunadashctl wallpaper-image`, while cancellation leaves the wallpaper unchanged.
