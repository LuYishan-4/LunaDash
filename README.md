<div align="center">

<a href="https://github.com/LuYishan-4/LunaDash">
  <img src="docs/brand/banner.svg" alt="LunaDash — a moonlit, focused Linux desktop" width="880">
</a>

<br>

### A moonlit, focused Linux desktop

<p>
  <img src="https://img.shields.io/badge/license-GPL--3.0--only-9ccbfb?style=flat-square" alt="GPL-3.0-only">
  <img src="https://img.shields.io/badge/platform-Arch%20Linux%20first-9ccbfb?style=flat-square" alt="Arch Linux first">
  <img src="https://img.shields.io/badge/stack-C%2B%2B20%20%C2%B7%20Qt%206%20%C2%B7%20Wayland-9ccbfb?style=flat-square" alt="C++20, Qt 6, Wayland">
  <img src="https://img.shields.io/badge/shell-Quickshell%20QML-9ccbfb?style=flat-square" alt="Quickshell QML shell">
  <a href="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-gate.yml">
    <img src="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-gate.yml/badge.svg" alt="Main required checks">
  </a>
</p>

</div>

LunaDash is a Wayland desktop: a C++20 / OpenGL compositor built on Qt Wayland Compositor, C11 cores for rendering, tiling and metrics, and a Quickshell/QML shell. Windows live in niri-inspired scrollable columns across four workspaces, and the shell keeps the workspace, the launcher and the settings in one compact top panel.

> **0.1 development preview.** LunaDash is not a production-ready KDE replacement. Start with a nested session inside your existing desktop, and read [Project status](#project-status) before relying on it.

## Highlights

- **Scrollable columns.** Every window opens as its own full-height column below the panel, and the column strip slides horizontally when focus changes instead of covering other windows. A column holds up to four windows; visible members split its height as one full tile, two halves, or three or four quarters.
- **One compact top panel.** Workspace and session controls plus one icon per open column on the left, the centred overview / accent launcher / settings selector in the middle, and the StatusNotifier tray with clock, network and battery on the right.
- **Grouping from the panel.** Drag one column icon onto another to merge them, click a member to focus it, right-click to expel it. Window frames themselves are not draggable.
- **A real settings centre.** Sixteen pages in a fullscreen QML overlay with tokenized search, a shortcut recorder that rejects duplicate or invalid combinations, an in-shell wallpaper picker, and a manual update check against the official release feed.
- **Launcher.** One ranked, token-searchable list with icon, name and description for the built-in tools and installed desktop entries.
- **Appearance.** Accent colour, window gaps, panel height, backdrop blur, window opacity, animation duration and a reduced-motion mode, plus an external Traditional Chinese language pack.
- **Built-in tools.** Files, command console, system monitor, a pacman interface and opt-in metadata plugins.
- **X11 compatibility.** Applications run inside an authenticated XWayland instance, which the default Kitty path already uses.
- **Explicit graphics.** OpenGL 3.3 compatibility or OpenGL ES 3.0 contexts with version-specific shaders, plus a software-friendly shell renderer path for problematic drivers.

## Requirements

Arch Linux is the first target. The backend needs CMake 3.21+, a C11/C++20 compiler, Qt 6.4+, Wayland development headers and the Khronos GL headers. The shell targets [Quickshell 0.3](https://quickshell.org/docs/v0.3.0/guide/install-setup/), which may need a newer Qt than the backend's 6.4 minimum.

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-wayland \
  qt6-translations quickshell kitty fish wayland libglvnd dbus mesa
```

Ubuntu 24.04 backend packages: `build-essential cmake ninja-build pkg-config libwayland-dev qt6-base-dev qt6-declarative-dev qt6-wayland-dev qt6-wayland libqt6opengl6-dev`.
Fedora backend packages: `gcc-c++ cmake ninja-build pkgconf-pkg-config wayland-devel qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel`.

Install Kitty and Fish to use the default terminal, and optionally `networkmanager nm-connection-editor` to configure network profiles. LunaDash reuses your existing connections; installing a package never enables a service.

## Build and run

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
QT_QPA_PLATFORM=wayland ./build/lunadash-compositor --socket ludash-test
```

Run it from inside an existing Wayland or X11 desktop and it opens as a nested desktop in a window. The first-run guide covers language, network and appearance, and works fully offline. The canonical executables are `lunadash-compositor`, `lunadash-desktop`, `lunadashctl` and `lunadash-session`; the older `ludash-*` names remain compatibility aliases.

To register a real login session, `./scripts/install-session.sh` builds and installs a pacman-managed package and adds the session entry. Use `--enable-sddm` only when you want SDDM enabled for the next boot, and `--autologin USER` to enable passwordless login explicitly. See [Boot and login session](docs/LOGIN_SESSION.md) for preflight checks, limits and recovery.

## Keyboard shortcuts

`Super` is the Meta key. Every binding below can be changed or disabled in **Settings → Keyboard shortcuts**.

| Shortcut | Action |
| --- | --- |
| `Super` + `Return` / `E` / `D` | Default terminal / file manager / launcher |
| `Super` + `H` / `L` | Focus the column to the left / right |
| `Super` + `K` / `J` | Focus the previous / next member inside the column |
| `Super` + `Shift` + `H` / `L` | Merge the focused window into the left / right column |
| `Super` + `Shift` + `E` | Expel the focused member into its own column |
| `Super` + `Ctrl` + `H` / `L` | Move the focused column left / right |
| `Super` + `=` / `-` | Widen / narrow the focused column |
| `Super` + `Shift` + `C` | Center the focused column |
| `Super` + `F` | Maximize or restore the window under the pointer |
| `Super` + `C` / `Q` | Close the focused window |
| `Super` + `M` | Minimize the focused window |
| `Super` + `Space` | Toggle floating for the focused window |
| `Super` + `1`–`9` | Switch workspace |
| `Super` + `Shift` + `1`–`9` | Move the focused window to a workspace |

The settings overlay takes exclusive keyboard focus and closes with `Escape` from search or its quick-hide control. Your host desktop may intercept `Super` shortcuts while you are testing in a nested session.

## Settings and the control socket

`lunadashctl` talks to the running compositor, and the settings overlay drives exactly the same methods:

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl open-settings appearance
./build/lunadashctl appearance '{"gap":16,"panelHeight":44}'
./build/lunadashctl shortcuts '{"focusLeft":"Meta+U"}'
./build/lunadashctl check-update
```

Preferences are saved automatically as you change them. Resetting desktop preferences does not delete documents, reset the language or wallpaper, or modify system-service configuration. See [Settings coverage](docs/SETTINGS.md) for every page and its limits.

## Testing

```sh
sudo pacman -S --needed pkgconf python-pillow xorg-server-xvfb xorg-xauth xdotool
./scripts/test-once.sh
```

`test-once.sh` builds, lints QML, then runs a nested session with demonstration windows and writes its log, state JSON and screenshot to `build-once`. The Xvfb integration checks are documented in the [testing guide](docs/TESTING_AND_FILES.md): the settings test alone opens all sixteen pages and applies an accepted and several rejected values to every option each page can change.

A passing software-rendered session proves rendering, geometry and clean shutdown, not physical GPU behaviour or a standalone login. CI configuration is never reported as evidence that a job ran.

## Documentation

- [Testing guide and every maintained file](docs/TESTING_AND_FILES.md)
- [Architecture](docs/ARCHITECTURE.md) · [C core](docs/C_CORE.md) · [Graphics contexts](docs/GRAPHICS.md)
- [Appearance](docs/APPEARANCE.md) · [Blur and animations](docs/EFFECTS.md) · [Shell rendering](docs/SHELL_RENDERING.md)
- [First-run setup and configuration](docs/CONFIGURATION.md) · [Settings coverage](docs/SETTINGS.md)
- [Shell modules and templates](docs/MODULES.md) · [Default apps, Fish and Files](docs/DEFAULT_APPS_AND_FILES.md)
- [Input methods and languages](docs/INPUT_METHODS.md) · [X11 compatibility](docs/XWAYLAND.md)
- [Plugin development](docs/PLUGINS.md) · [Security and crash checks](docs/SECURITY_CHECKS.md)
- [Traditional Chinese boot and login guide](docs/LOGIN_SESSION.zh-TW.md) · [Website](docs/WEBSITE.md)

## Project status

Working and tested in nested sessions: window management and grouping, the panel, launcher, all sixteen settings pages, wallpapers, built-in applications, the Traditional Chinese language pack, and authenticated X11 compatibility.

Missing or incomplete: multiple outputs, full layer-shell coverage, screen locking, portals, notification hosting, a native Wi-Fi credential UI, a polkit agent and complete input-method-v2 integration. The Fcitx tray icon works, but its candidate popup still depends on the incomplete input-method bridge. Native plugins are disabled by default and run without a sandbox when enabled. The packaged EGLFS/KMS login session and a physical SDDM login still need physical-session testing, and no such verification is claimed.

## License

GPL-3.0-only. See [LICENSE](LICENSE).
